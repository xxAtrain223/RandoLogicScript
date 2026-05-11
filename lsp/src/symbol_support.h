#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "language_server_endpoints_private.h"
#include "document_store.h"

namespace rls::lsp::detail::endpoints::support {

/// Describes the identifier found under or immediately before a cursor
/// position on a single line of text.
struct WordAtPosition {
    /// Identifier text extracted from the source buffer.
    std::string value;
    /// Zero-based start column of the identifier.
    int startCharacter = 0;
    /// Zero-based exclusive end column of the identifier.
    int endCharacter = 0;
};

/// Symbol metadata collected from an open document for workspace-style LSP
/// queries.
struct IndexedSymbol {
    /// Display name of the symbol.
    std::string name;
    /// Document URI that defines the symbol.
    std::string uri;
    /// Source span covering the declaration.
    rls::ast::Span span;
    /// LSP SymbolKind numeric value associated with the declaration.
    int kind = 13; // Variable
    /// Supplemental display text, such as a signature or declaration category.
    std::string detail;
};

/// Returns true when the character may appear in an identifier.
bool isIdentifierChar(char ch);

/// Extracts the full identifier at the given zero-based line and character,
/// or returns std::nullopt when the cursor is not on an identifier.
std::optional<WordAtPosition> extractWordAtPosition(const std::string& text, int line, int character);

/// Returns the identifier prefix immediately preceding the given cursor
/// position on a line, which is used for completion filtering.
std::string prefixAtPosition(const std::string& text, int line, int character);

/// Walks backward from the cursor to detect the function name for the active
/// call site on the current line.
std::optional<std::string> callsiteFunctionNameAtPosition(const std::string& text, int line, int character);

/// Parses all open documents and returns the declarations that should be
/// surfaced by definition, hover, and workspace symbol endpoints.
std::vector<IndexedSymbol> collectSymbols(const DocumentStore& documents);

/// Parses all open documents and maps function names to the parameter names
/// declared for each overload-like definition encountered.
std::map<std::string, std::vector<std::string>> collectFunctionParams(const DocumentStore& documents);

/// Performs an ASCII case-insensitive prefix comparison.
bool startsWithCaseInsensitive(const std::string& value, const std::string& prefix);

/// Returns true when the query appears anywhere within the symbol name using
/// case-insensitive matching.
bool symbolMatchesQuery(const std::string& name, const std::string& query);

/// Builds an LSP location object from a declaration span, preferring the span's
/// own file path when one is available.
json locationFromSpan(const std::string& uri, const rls::ast::Span& span);

/// Builds an LSP location whose range covers the provided word starting at the
/// given byte offset in the source text.
json findWordRange(const std::string& uri, const std::string& text, const std::string& needle, size_t offset);

} // namespace rls::lsp::detail::endpoints::support
