#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "language_server_endpoints_private.h"
#include "document_store.h"

namespace rls::lsp::detail::endpoints::support {

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

bool isIdentifierChar(char ch);
std::optional<WordAtPosition> extractWordAtPosition(const std::string& text, int line, int character);
std::string prefixAtPosition(const std::string& text, int line, int character);
std::optional<std::string> callsiteFunctionNameAtPosition(const std::string& text, int line, int character);

std::vector<IndexedSymbol> collectSymbols(const DocumentStore& documents);
std::map<std::string, std::vector<std::string>> collectFunctionParams(const DocumentStore& documents);

bool startsWithCaseInsensitive(const std::string& value, const std::string& prefix);
bool symbolMatchesQuery(const std::string& name, const std::string& query);

json locationFromSpan(const std::string& uri, const rls::ast::Span& span);
json findWordRange(const std::string& uri, const std::string& text, const std::string& needle, size_t offset);

} // namespace rls::lsp::detail::endpoints::support
