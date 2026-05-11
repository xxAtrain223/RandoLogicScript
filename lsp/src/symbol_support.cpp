#include "symbol_support.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "parser.h"

namespace rls::lsp::detail::endpoints::support {

namespace {

// Convert a zero-based line number into the byte offset of that line start.
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

// Translate a byte offset back into the zero-based LSP line/character pair.
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

// Map AST declaration variants onto the LSP SymbolKind values the editor
// expects for workspace and document symbol responses.
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

// Extract the display name for declarations that should surface as symbols.
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

// Provide a short category label used by hover and symbol-style responses.
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

} // namespace

bool isIdentifierChar(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '_';
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

    // Clamp oversized cursor positions back to the last character on the line
    // so requests at end-of-line can still resolve the preceding identifier.
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

    // Completion wants the already-typed prefix immediately before the cursor,
    // even if the cursor sits past the current line buffer.
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

    // Walk backward on the current line to find the innermost still-open call.
    // Encountering ')' or ';' means the cursor is no longer in an active call.
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

            // Preserve the declaration span exactly so downstream definition,
            // hover, and symbol requests all point at the same source range.
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
                    // Repeated names append into the same entry so completion
                    // can surface parameters across all matching declarations.
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

bool symbolMatchesQuery(const std::string& name, const std::string& query) {
    if (query.empty()) {
        return true;
    }

    // Workspace symbol matching is a case-insensitive substring search rather
    // than a prefix search so short queries can still find embedded matches.
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

json locationFromSpan(const std::string& uri, const rls::ast::Span& span) {
    // Declarations may carry their own source file, so prefer that over the
    // caller-provided fallback URI when building the LSP location object.
    const std::string rawUri = span.file.empty() ? uri : span.file;
    return {
        {"uri", normalizeUri(rawUri)},
        {"range", toLspRange(span)},
    };
}

json findWordRange(const std::string& uri, const std::string& text, const std::string& needle, size_t offset) {
    // Convert the raw match offset into an LSP range that spans exactly the
    // matched identifier text.
    const auto [line, character] = offsetToLineCharacter(text, offset);
    return {
        {"uri", uri},
        {"range", {
            {"start", {{"line", line}, {"character", character}}},
            {"end", {{"line", line}, {"character", character + static_cast<int>(needle.size())}}},
        }}
    };
}

} // namespace rls::lsp::detail::endpoints::support
