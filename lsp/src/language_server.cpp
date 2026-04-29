#include "language_server.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "ast.h"
#include "parser.h"
#include "sema.h"

namespace rls::lsp {

namespace {

using json = nlohmann::json;

enum class TraceLevel {
    Off,
    Error,
    Info,
    Debug,
};

TraceLevel configuredTraceLevel() {
    static const TraceLevel level = [] {
        std::string traceValue;
#ifdef _MSC_VER
        char* value = nullptr;
        size_t valueLen = 0;
        if (_dupenv_s(&value, &valueLen, "RLS_LSP_TRACE") == 0 && value != nullptr) {
            traceValue.assign(value);
            free(value);
        }
#else
        const char* value = std::getenv("RLS_LSP_TRACE");
        if (value != nullptr) {
            traceValue.assign(value);
        }
#endif

        if (traceValue.empty()) {
            return TraceLevel::Off;
        }

        std::string v = traceValue;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (v == "error") {
            return TraceLevel::Error;
        }
        if (v == "info") {
            return TraceLevel::Info;
        }
        if (v == "debug") {
            return TraceLevel::Debug;
        }
        return TraceLevel::Off;
    }();

    return level;
}

bool shouldTrace(TraceLevel level) {
    return static_cast<int>(configuredTraceLevel()) >= static_cast<int>(level)
        && configuredTraceLevel() != TraceLevel::Off;
}

void trace(TraceLevel level, std::string_view message, const json& data = nullptr) {
    if (!shouldTrace(level)) {
        return;
    }

    const char* levelName = "debug";
    switch (level) {
    case TraceLevel::Error:
        levelName = "error";
        break;
    case TraceLevel::Info:
        levelName = "info";
        break;
    case TraceLevel::Debug:
        levelName = "debug";
        break;
    case TraceLevel::Off:
        return;
    }

    json payload = {
        {"component", "rls_lsp"},
        {"level", levelName},
        {"message", message},
    };
    if (!data.is_null()) {
        payload["data"] = data;
    }

    std::cerr << payload.dump() << "\n";
}

struct WordAtPosition {
    std::string value;
    int startCharacter = 0;
    int endCharacter = 0;
};

struct IndexedSymbol {
    std::string name;
    std::string uri;
    rls::ast::Span span;
    int kind = 13; // Variable
    std::string detail;
};

bool isIdentifierChar(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '_';
}

std::optional<size_t> lineStartOffset(const std::string& text, int targetLine) {
    if (targetLine < 0) {
        return std::nullopt;
    }

    size_t line = 0;
    size_t offset = 0;
    while (line < static_cast<size_t>(targetLine)) {
        const auto next = text.find('\n', offset);
        if (next == std::string::npos) {
            return std::nullopt;
        }
        offset = next + 1;
        ++line;
    }

    return offset;
}

std::optional<WordAtPosition> extractWordAtPosition(const std::string& text, int line, int character) {
    if (character < 0) {
        return std::nullopt;
    }

    const auto lineStart = lineStartOffset(text, line);
    if (!lineStart.has_value()) {
        return std::nullopt;
    }

    size_t lineEnd = text.find('\n', *lineStart);
    if (lineEnd == std::string::npos) {
        lineEnd = text.size();
    }

    size_t absolute = *lineStart + static_cast<size_t>(character);
    if (absolute >= lineEnd) {
        if (lineEnd == *lineStart) {
            return std::nullopt;
        }
        absolute = lineEnd - 1;
    }

    if (!isIdentifierChar(text[absolute])) {
        return std::nullopt;
    }

    size_t start = absolute;
    while (start > *lineStart && isIdentifierChar(text[start - 1])) {
        --start;
    }

    size_t end = absolute;
    while (end + 1 < lineEnd && isIdentifierChar(text[end + 1])) {
        ++end;
    }

    WordAtPosition result;
    result.value = text.substr(start, end - start + 1);
    result.startCharacter = static_cast<int>(start - *lineStart);
    result.endCharacter = static_cast<int>(end - *lineStart + 1);
    return result;
}

std::string prefixAtPosition(const std::string& text, int line, int character) {
    const auto lineStart = lineStartOffset(text, line);
    if (!lineStart.has_value() || character <= 0) {
        return {};
    }

    size_t pos = *lineStart + static_cast<size_t>(character);
    if (pos > text.size()) {
        pos = text.size();
    }
    if (pos == 0) {
        return {};
    }

    size_t start = pos;
    while (start > *lineStart && isIdentifierChar(text[start - 1])) {
        --start;
    }

    if (start >= pos) {
        return {};
    }

    return text.substr(start, pos - start);
}

std::optional<std::string> callsiteFunctionNameAtPosition(const std::string& text, int line, int character) {
    const auto lineStart = lineStartOffset(text, line);
    if (!lineStart.has_value()) {
        return std::nullopt;
    }

    size_t pos = *lineStart + static_cast<size_t>(std::max(character, 0));
    if (pos > text.size()) {
        pos = text.size();
    }

    const size_t lineEnd = [&] {
        const auto e = text.find('\n', *lineStart);
        return e == std::string::npos ? text.size() : e;
    }();

    if (pos > lineEnd) {
        pos = lineEnd;
    }

    size_t openParen = std::string::npos;
    for (size_t i = pos; i > *lineStart; --i) {
        const char ch = text[i - 1];
        if (ch == '(') {
            openParen = i - 1;
            break;
        }
        if (ch == ')' || ch == ';') {
            break;
        }
    }

    if (openParen == std::string::npos || openParen == *lineStart) {
        return std::nullopt;
    }

    size_t end = openParen;
    while (end > *lineStart && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    if (end == *lineStart) {
        return std::nullopt;
    }

    size_t start = end;
    while (start > *lineStart && isIdentifierChar(text[start - 1])) {
        --start;
    }

    if (start == end) {
        return std::nullopt;
    }

    return text.substr(start, end - start);
}

std::pair<int, int> offsetToLineCharacter(const std::string& text, size_t offset) {
    int line = 0;
    int character = 0;

    for (size_t i = 0; i < offset && i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++line;
            character = 0;
        } else {
            ++character;
        }
    }

    return {line, character};
}

int severityFromDiagnosticLevel(rls::ast::DiagnosticLevel level) {
    switch (level) {
    case rls::ast::DiagnosticLevel::Error:
        return 1;
    case rls::ast::DiagnosticLevel::Warning:
        return 2;
    case rls::ast::DiagnosticLevel::Info:
        return 3;
    }

    return 3;
}

json toLspRange(const rls::ast::Span& span) {
    auto toLine = [](uint32_t oneBased) {
        return oneBased > 0 ? static_cast<int>(oneBased - 1) : 0;
    };

    auto toCharacter = [](uint32_t oneBased) {
        return oneBased > 0 ? static_cast<int>(oneBased - 1) : 0;
    };

    const int startLine = toLine(span.start.line);
    const int startChar = toCharacter(span.start.column);

    int endLine = toLine(span.end.line);
    int endChar = toCharacter(span.end.column);

    if (span.end.line == 0 && span.end.column == 0) {
        endLine = startLine;
        endChar = startChar;
    }

    return {
        {"start", {{"line", startLine}, {"character", startChar}}},
        {"end", {{"line", endLine}, {"character", endChar}}},
    };
}

json toLspDiagnostic(const rls::ast::Diagnostic& diagnostic) {
    return {
        {"range", toLspRange(diagnostic.span)},
        {"severity", severityFromDiagnosticLevel(diagnostic.level)},
        {"source", "rls"},
        {"message", diagnostic.message},
    };
}

int symbolKindForDecl(const rls::ast::Decl& decl) {
    return std::visit([](const auto& d) -> int {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, rls::ast::DefineDecl> || std::is_same_v<T, rls::ast::ExternDefineDecl>) {
            return 12; // Function
        }
        if constexpr (std::is_same_v<T, rls::ast::RegionDecl> || std::is_same_v<T, rls::ast::ExtendRegionDecl>) {
            return 5; // Class
        }
        return 13; // Variable
    }, decl);
}

std::string symbolNameForDecl(const rls::ast::Decl& decl) {
    return std::visit([](const auto& d) -> std::string {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, rls::ast::RegionDecl>) {
            return d.key;
        }
        if constexpr (std::is_same_v<T, rls::ast::ExtendRegionDecl>) {
            return d.name;
        }
        if constexpr (std::is_same_v<T, rls::ast::DefineDecl>) {
            return d.name;
        }
        if constexpr (std::is_same_v<T, rls::ast::ExternDefineDecl>) {
            return d.name;
        }
        return {};
    }, decl);
}

std::string symbolDetailForDecl(const rls::ast::Decl& decl) {
    return std::visit([](const auto& d) -> std::string {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, rls::ast::DefineDecl>) {
            return "define";
        }
        if constexpr (std::is_same_v<T, rls::ast::ExternDefineDecl>) {
            return "extern define";
        }
        if constexpr (std::is_same_v<T, rls::ast::RegionDecl>) {
            return "region";
        }
        if constexpr (std::is_same_v<T, rls::ast::ExtendRegionDecl>) {
            return "extend region";
        }
        return "symbol";
    }, decl);
}

std::vector<IndexedSymbol> collectSymbols(const DocumentStore& documents) {
    std::vector<IndexedSymbol> symbols;

    for (const auto& uri : documents.openUris()) {
        const auto* doc = documents.find(uri);
        if (doc == nullptr) {
            continue;
        }

        auto file = rls::parser::ParseString(doc->text, uri);
        for (const auto& decl : file.declarations) {
            const std::string name = symbolNameForDecl(decl);
            if (name.empty()) {
                continue;
            }

            const rls::ast::Span span = std::visit([](const auto& d) {
                return d.span;
            }, decl);

            symbols.push_back({
                .name = name,
                .uri = uri,
                .span = span,
                .kind = symbolKindForDecl(decl),
                .detail = symbolDetailForDecl(decl),
            });
        }
    }

    return symbols;
}

std::map<std::string, std::vector<std::string>> collectFunctionParams(const DocumentStore& documents) {
    std::map<std::string, std::vector<std::string>> params;

    for (const auto& uri : documents.openUris()) {
        const auto* doc = documents.find(uri);
        if (doc == nullptr) {
            continue;
        }

        auto file = rls::parser::ParseString(doc->text, uri);
        for (const auto& decl : file.declarations) {
            std::visit([&](const auto& d) {
                using T = std::decay_t<decltype(d)>;
                if constexpr (std::is_same_v<T, rls::ast::DefineDecl> || std::is_same_v<T, rls::ast::ExternDefineDecl>) {
                    auto& out = params[d.name];
                    for (const auto& p : d.params) {
                        out.push_back(p.name);
                    }
                }
            }, decl);
        }
    }

    return params;
}

bool startsWithCaseInsensitive(const std::string& value, const std::string& prefix) {
    if (prefix.empty()) {
        return true;
    }
    if (prefix.size() > value.size()) {
        return false;
    }

    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

json locationFromSpan(const std::string& uri, const rls::ast::Span& span) {
    return {
        {"uri", span.file.empty() ? uri : span.file},
        {"range", toLspRange(span)},
    };
}

bool symbolMatchesQuery(const std::string& name, const std::string& query) {
    if (query.empty()) {
        return true;
    }

    std::string loweredName = name;
    std::string loweredQuery = query;
    std::transform(loweredName.begin(), loweredName.end(), loweredName.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(loweredQuery.begin(), loweredQuery.end(), loweredQuery.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return loweredName.find(loweredQuery) != std::string::npos;
}

json findWordRange(const std::string& uri, const std::string& text, const std::string& needle, size_t offset) {
    const auto [line, character] = offsetToLineCharacter(text, offset);
    return {
        {"uri", uri},
        {"range", {
            {"start", {{"line", line}, {"character", character}}},
            {"end", {{"line", line}, {"character", character + static_cast<int>(needle.size())}}},
        }}
    };
}

json makeResponse(const json& id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result},
    };
}

json makeErrorResponse(const json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message},
        }},
    };
}

json makeNotification(std::string_view method, const json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
    };
}

} // namespace

LanguageServer::LanguageServer() = default;

std::vector<std::string> LanguageServer::handlePayload(std::string_view payload) {
    json message;
    try {
        message = json::parse(payload);
    } catch (const std::exception& e) {
        trace(TraceLevel::Error, "json parse failure", { {"error", e.what()} });
        return {makeErrorResponse(nullptr, -32700, "Parse error").dump()};
    }

    if (!message.is_object()) {
        return {makeErrorResponse(nullptr, -32600, "Invalid Request").dump()};
    }

    if (!message.contains("jsonrpc") || message["jsonrpc"] != "2.0") {
        const bool hasId = message.contains("id");
        const json id = hasId ? message["id"] : json(nullptr);
        return {makeErrorResponse(id, -32600, "Invalid Request").dump()};
    }

    const bool hasId = message.contains("id");
    const json id = hasId ? message["id"] : json(nullptr);

    if (!message.contains("method") || !message["method"].is_string()) {
        if (hasId) {
            return {makeErrorResponse(id, -32600, "Invalid Request").dump()};
        }
        return {};
    }

    const std::string method = message.value("method", "");
    trace(TraceLevel::Debug, "handle method", { {"method", method} });

    try {
        if (method == "initialize") {
            return handleInitialize(hasId, id);
        }

        if (method == "shutdown") {
            return handleShutdown(hasId, id);
        }

        if (method == "exit") {
            return handleExit();
        }

        if (method == "textDocument/didOpen") {
            return handleDidOpen(message);
        }

        if (method == "textDocument/didChange") {
            return handleDidChange(message);
        }

        if (method == "textDocument/didClose") {
            return handleDidClose(message);
        }

        if (method == "textDocument/definition") {
            return handleDefinition(hasId, id, message);
        }

        if (method == "textDocument/references") {
            return handleReferences(hasId, id, message);
        }

        if (method == "textDocument/hover") {
            return handleHover(hasId, id, message);
        }

        if (method == "textDocument/completion") {
            return handleCompletion(hasId, id, message);
        }

        if (method == "textDocument/documentSymbol") {
            return handleDocumentSymbol(hasId, id, message);
        }

        if (method == "workspace/symbol") {
            return handleWorkspaceSymbol(hasId, id, message);
        }
    } catch (const std::exception& e) {
        trace(TraceLevel::Error, "handler failure", { {"method", method}, {"error", e.what()} });
        if (hasId) {
            return {makeErrorResponse(id, -32603, "Internal error").dump()};
        }
        return {};
    }

    if (hasId) {
        return {makeErrorResponse(id, -32601, "Method not found").dump()};
    }

    return {};
}

std::vector<std::string> LanguageServer::handleInitialize(bool hasId, const json& id) const {
    const json result = {
        {"capabilities", {
            {"textDocumentSync", static_cast<int>(capabilities_.textDocumentSync)},
            {"definitionProvider", capabilities_.definitionProvider},
            {"referencesProvider", capabilities_.referencesProvider},
            {"hoverProvider", capabilities_.hoverProvider},
            {"completionProvider", {
                {"resolveProvider", false},
                {"triggerCharacters", json::array({"(", ",", ":"})}
            }},
            {"documentSymbolProvider", capabilities_.documentSymbolProvider},
            {"workspaceSymbolProvider", capabilities_.workspaceSymbolProvider},
        }}
    };

    if (!hasId) {
        return {};
    }

    return {makeResponse(id, result).dump()};
}

std::vector<std::string> LanguageServer::handleShutdown(bool hasId, const json& id) {
    isShutdown_ = true;
    if (!hasId) {
        return {};
    }

    return {makeResponse(id, nullptr).dump()};
}

std::vector<std::string> LanguageServer::handleExit() {
    shouldExit_ = true;
    return {};
}

std::vector<std::string> LanguageServer::handleDidOpen(const json& message) {
    if (!message.contains("params") || !message["params"].contains("textDocument")) {
        return {};
    }

    const json& textDocument = message["params"]["textDocument"];
    const std::string uri = textDocument.value("uri", "");
    const std::string languageId = textDocument.value("languageId", "");
    const int version = textDocument.value("version", 0);
    const std::string text = textDocument.value("text", "");

    if (uri.empty()) {
        return {};
    }

    documents_.open(uri, languageId, version, text);
    return publishDiagnostics(uri);
}

std::vector<std::string> LanguageServer::handleDidChange(const json& message) {
    if (!message.contains("params") || !message["params"].contains("textDocument")) {
        return {};
    }

    const json& params = message["params"];
    const json& textDocument = params["textDocument"];
    const std::string uri = textDocument.value("uri", "");
    const int version = textDocument.value("version", 0);

    if (uri.empty() || !params.contains("contentChanges") || !params["contentChanges"].is_array()) {
        return {};
    }

    const auto& changes = params["contentChanges"];
    if (changes.empty() || !changes[0].contains("text")) {
        return {};
    }

    const std::string text = changes[0].value("text", "");
    if (!documents_.applyFullChange(uri, version, text)) {
        return {};
    }

    return publishDiagnostics(uri);
}

std::vector<std::string> LanguageServer::handleDidClose(const json& message) {
    if (!message.contains("params") || !message["params"].contains("textDocument")) {
        return {};
    }

    const std::string uri = message["params"]["textDocument"].value("uri", "");
    if (uri.empty() || !documents_.close(uri)) {
        return {};
    }

    return {
        makeNotification(
            "textDocument/publishDiagnostics",
            {
                {"uri", uri},
                {"diagnostics", json::array()},
            }).dump()
    };
}

std::vector<std::string> LanguageServer::handleDefinition(bool hasId, const json& id, const json& message) const {
    if (!hasId || !message.contains("params") || !message["params"].contains("textDocument")
        || !message["params"].contains("position")) {
        return {};
    }

    const std::string uri = message["params"]["textDocument"].value("uri", "");
    const int line = message["params"]["position"].value("line", -1);
    const int character = message["params"]["position"].value("character", -1);

    const TextDocument* doc = documents_.find(uri);
    if (doc == nullptr) {
        return {makeResponse(id, json::array()).dump()};
    }

    const auto word = extractWordAtPosition(doc->text, line, character);
    if (!word.has_value()) {
        return {makeResponse(id, json::array()).dump()};
    }

    const auto symbols = collectSymbols(documents_);
    json locations = json::array();
    for (const auto& symbol : symbols) {
        if (symbol.name == word->value) {
            locations.push_back(locationFromSpan(symbol.uri, symbol.span));
            break;
        }
    }

    return {makeResponse(id, locations).dump()};
}

std::vector<std::string> LanguageServer::handleReferences(bool hasId, const json& id, const json& message) const {
    if (!hasId || !message.contains("params") || !message["params"].contains("textDocument")
        || !message["params"].contains("position")) {
        return {};
    }

    const std::string uri = message["params"]["textDocument"].value("uri", "");
    const int line = message["params"]["position"].value("line", -1);
    const int character = message["params"]["position"].value("character", -1);
    const bool includeDeclaration = message["params"].contains("context")
        ? message["params"]["context"].value("includeDeclaration", true)
        : true;

    const TextDocument* sourceDoc = documents_.find(uri);
    if (sourceDoc == nullptr) {
        return {makeResponse(id, json::array()).dump()};
    }

    const auto word = extractWordAtPosition(sourceDoc->text, line, character);
    if (!word.has_value()) {
        return {makeResponse(id, json::array()).dump()};
    }

    const auto symbols = collectSymbols(documents_);
    std::vector<std::pair<std::string, rls::ast::Span>> declarationSpans;
    for (const auto& symbol : symbols) {
        if (symbol.name == word->value) {
            declarationSpans.push_back({symbol.uri, symbol.span});
        }
    }

    json locations = json::array();
    for (const auto& openUri : documents_.openUris()) {
        const TextDocument* doc = documents_.find(openUri);
        if (doc == nullptr) {
            continue;
        }

        size_t pos = doc->text.find(word->value);
        while (pos != std::string::npos) {
            const bool leftOk = pos == 0 || !isIdentifierChar(doc->text[pos - 1]);
            const size_t rightIndex = pos + word->value.size();
            const bool rightOk = rightIndex >= doc->text.size() || !isIdentifierChar(doc->text[rightIndex]);

            if (leftOk && rightOk) {
                auto loc = findWordRange(openUri, doc->text, word->value, pos);
                bool isDeclarationLocation = false;

                if (!includeDeclaration) {
                    for (const auto& [declUri, declSpan] : declarationSpans) {
                        const auto declUriValue = declSpan.file.empty() ? declUri : declSpan.file;
                        const auto& range = loc["range"];
                        const int startLine = range["start"]["line"].get<int>();
                        const int startCharacter = range["start"]["character"].get<int>();
                        if (declUriValue == openUri
                            && startLine == static_cast<int>(declSpan.start.line > 0 ? declSpan.start.line - 1 : 0)
                            && startCharacter == static_cast<int>(declSpan.start.column > 0 ? declSpan.start.column - 1 : 0)) {
                            isDeclarationLocation = true;
                            break;
                        }
                    }
                }

                if (!isDeclarationLocation) {
                    locations.push_back(std::move(loc));
                }
            }

            pos = doc->text.find(word->value, pos + word->value.size());
        }
    }

    return {makeResponse(id, locations).dump()};
}

std::vector<std::string> LanguageServer::handleHover(bool hasId, const json& id, const json& message) const {
    if (!hasId || !message.contains("params") || !message["params"].contains("textDocument")
        || !message["params"].contains("position")) {
        return {};
    }

    const std::string uri = message["params"]["textDocument"].value("uri", "");
    const int line = message["params"]["position"].value("line", -1);
    const int character = message["params"]["position"].value("character", -1);

    const TextDocument* doc = documents_.find(uri);
    if (doc == nullptr) {
        return {makeResponse(id, nullptr).dump()};
    }

    const auto word = extractWordAtPosition(doc->text, line, character);
    if (!word.has_value()) {
        return {makeResponse(id, nullptr).dump()};
    }

    const auto symbols = collectSymbols(documents_);
    for (const auto& symbol : symbols) {
        if (symbol.name == word->value) {
            const json hover = {
                {"contents", {
                    {"kind", "plaintext"},
                    {"value", symbol.detail + " " + symbol.name},
                }},
                {"range", {
                    {"start", {{"line", line}, {"character", word->startCharacter}}},
                    {"end", {{"line", line}, {"character", word->endCharacter}}},
                }}
            };
            return {makeResponse(id, hover).dump()};
        }
    }

    return {makeResponse(id, nullptr).dump()};
}

std::vector<std::string> LanguageServer::handleCompletion(bool hasId, const json& id, const json& message) const {
    if (!hasId || !message.contains("params") || !message["params"].contains("textDocument")
        || !message["params"].contains("position")) {
        return {};
    }

    const std::string uri = message["params"]["textDocument"].value("uri", "");
    const int line = message["params"]["position"].value("line", -1);
    const int character = message["params"]["position"].value("character", -1);

    const TextDocument* doc = documents_.find(uri);
    if (doc == nullptr) {
        return {makeResponse(id, json{{"isIncomplete", false}, {"items", json::array()}}).dump()};
    }

    const std::string prefix = prefixAtPosition(doc->text, line, character);
    const auto callsiteName = callsiteFunctionNameAtPosition(doc->text, line, character);
    const auto symbols = collectSymbols(documents_);
    const auto paramsByFunction = collectFunctionParams(documents_);

    const std::vector<std::string> keywords = {
        "define", "extern", "region", "extend", "match", "shared", "any_age",
        "true", "false", "always", "never", "and", "or", "not"
    };

    json items = json::array();
    std::set<std::string> seen;

    for (const auto& kw : keywords) {
        if (!startsWithCaseInsensitive(kw, prefix)) {
            continue;
        }
        if (!seen.insert("kw:" + kw).second) {
            continue;
        }
        items.push_back({
            {"label", kw},
            {"kind", 14},
            {"detail", "keyword"},
        });
    }

    for (const auto& symbol : symbols) {
        if (!startsWithCaseInsensitive(symbol.name, prefix)) {
            continue;
        }
        if (!seen.insert("sym:" + symbol.name).second) {
            continue;
        }
        items.push_back({
            {"label", symbol.name},
            {"kind", symbol.kind == 12 ? 3 : symbol.kind},
            {"detail", symbol.detail},
        });
    }

    if (callsiteName.has_value()) {
        auto it = paramsByFunction.find(*callsiteName);
        if (it != paramsByFunction.end()) {
            for (const auto& param : it->second) {
                const std::string label = param + ": ";
                if (!startsWithCaseInsensitive(param, prefix)) {
                    continue;
                }
                if (!seen.insert("param:" + label).second) {
                    continue;
                }
                items.push_back({
                    {"label", label},
                    {"kind", 5},
                    {"detail", "parameter"},
                });
            }
        }
    }

    return {
        makeResponse(id, json{
            {"isIncomplete", false},
            {"items", std::move(items)}
        }).dump()
    };
}

std::vector<std::string> LanguageServer::handleDocumentSymbol(bool hasId, const json& id, const json& message) const {
    if (!hasId || !message.contains("params") || !message["params"].contains("textDocument")) {
        return {};
    }

    const std::string uri = message["params"]["textDocument"].value("uri", "");
    const TextDocument* doc = documents_.find(uri);
    if (doc == nullptr) {
        return {makeResponse(id, json::array()).dump()};
    }

    auto file = rls::parser::ParseString(doc->text, uri);
    json symbols = json::array();

    for (const auto& decl : file.declarations) {
        const std::string name = symbolNameForDecl(decl);
        const int kind = symbolKindForDecl(decl);
        const rls::ast::Span span = std::visit([](const auto& d) {
            return d.span;
        }, decl);

        symbols.push_back({
            {"name", name},
            {"kind", kind},
            {"range", toLspRange(span)},
            {"selectionRange", toLspRange(span)},
        });
    }

    return {makeResponse(id, symbols).dump()};
}

std::vector<std::string> LanguageServer::handleWorkspaceSymbol(bool hasId, const json& id, const json& message) const {
    if (!hasId || !message.contains("params")) {
        return {};
    }

    const std::string query = message["params"].value("query", "");
    const auto symbols = collectSymbols(documents_);

    json results = json::array();
    for (const auto& symbol : symbols) {
        if (!symbolMatchesQuery(symbol.name, query)) {
            continue;
        }

        results.push_back({
            {"name", symbol.name},
            {"kind", symbol.kind},
            {"location", locationFromSpan(symbol.uri, symbol.span)},
        });
    }

    return {makeResponse(id, results).dump()};
}

const ServerCapabilities& LanguageServer::capabilities() const {
    return capabilities_;
}

DocumentStore& LanguageServer::documents() {
    return documents_;
}

const DocumentStore& LanguageServer::documents() const {
    return documents_;
}

bool LanguageServer::shouldExit() const {
    return shouldExit_;
}

std::vector<std::string> LanguageServer::publishDiagnostics(const std::string& uri) const {
    std::vector<std::string> outbound;

    const TextDocument* doc = documents_.find(uri);
    if (doc == nullptr) {
        return outbound;
    }

    rls::ast::File file;
    try {
        file = rls::parser::ParseString(doc->text, uri);
    } catch (const std::exception& e) {
        trace(TraceLevel::Error, "parse exception", { {"uri", uri}, {"error", e.what()} });
        const json lspDiagnostics = json::array({
            {
                {"range", {
                    {"start", {{"line", 0}, {"character", 0}}},
                    {"end", {{"line", 0}, {"character", 0}}},
                }},
                {"severity", 1},
                {"source", "rls"},
                {"message", std::string("internal parser failure: ") + e.what()},
            }
        });

        outbound.push_back(makeNotification(
            "textDocument/publishDiagnostics",
            {
                {"uri", uri},
                {"diagnostics", lspDiagnostics},
            }).dump());
        return outbound;
    }

    std::vector<rls::ast::Diagnostic> diagnostics;
    diagnostics.reserve(file.diagnostics.size());
    for (const auto& diagnostic : file.diagnostics) {
        diagnostics.push_back(diagnostic);
    }

    bool hasParseErrors = false;
    for (const auto& diagnostic : file.diagnostics) {
        if (diagnostic.level == rls::ast::DiagnosticLevel::Error) {
            hasParseErrors = true;
            break;
        }
    }

    if (!hasParseErrors) {
        rls::ast::Project project;
        project.files.push_back(std::move(file));

        try {
            auto semaDiagnostics = rls::sema::analyze(project);
            diagnostics.insert(
                diagnostics.end(),
                std::make_move_iterator(semaDiagnostics.begin()),
                std::make_move_iterator(semaDiagnostics.end()));
        } catch (const std::exception& e) {
            trace(TraceLevel::Error, "sema exception", { {"uri", uri}, {"error", e.what()} });
            diagnostics.push_back({
                rls::ast::DiagnosticLevel::Error,
                std::string("internal semantic analysis failure: ") + e.what(),
                rls::ast::Span{uri, {1, 1}, {1, 1}},
            });
        }
    }

    json lspDiagnostics = json::array();
    for (const auto& diagnostic : diagnostics) {
        lspDiagnostics.push_back(toLspDiagnostic(diagnostic));
    }

    outbound.push_back(makeNotification(
        "textDocument/publishDiagnostics",
        {
            {"uri", uri},
            {"diagnostics", std::move(lspDiagnostics)},
        }).dump());

    return outbound;
}

} // namespace rls::lsp
