#include "language_server.h"

#include <cstdint>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "ast.h"
#include "parser.h"
#include "sema.h"

namespace rls::lsp {

namespace {

using json = nlohmann::json;

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
    } catch (const std::exception&) {
        return {};
    }

    if (!message.is_object() || !message.contains("jsonrpc") || message["jsonrpc"] != "2.0") {
        return {};
    }

    const bool hasId = message.contains("id");
    const json id = hasId ? message["id"] : json(nullptr);
    const std::string method = message.value("method", "");

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
            {"completionProvider", json::object()},
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

    auto file = rls::parser::ParseString(doc->text, uri);

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

        auto semaDiagnostics = rls::sema::analyze(project);
        diagnostics.insert(
            diagnostics.end(),
            std::make_move_iterator(semaDiagnostics.begin()),
            std::make_move_iterator(semaDiagnostics.end()));
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
