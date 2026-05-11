#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rls::lsp {

/// In-memory representation of one text document currently tracked by the
/// language server.
struct TextDocument {
    /// Canonical document URI used as the store key.
    std::string uri;
    /// Language identifier supplied by the client.
    std::string languageId;
    /// Most recent document version observed from the client.
    int version = 0;
    /// Full current text contents of the document.
    std::string text;
};

/// Stores the set of open documents synchronized from the client.
class DocumentStore {
public:
    /// Inserts or replaces an open document with the provided contents and
    /// version.
    void open(std::string uri, std::string languageId, int version, std::string text);

    /// Applies a full-document update to an existing document.
    /// Returns false when the URI is unknown or the incoming version is older
    /// than the stored version.
    bool applyFullChange(const std::string& uri, int version, std::string text);

    /// Removes a document from the store and returns true when one was present.
    bool close(const std::string& uri);

    /// Returns a read-only pointer to the stored document, or nullptr when the
    /// URI is not open.
    const TextDocument* find(const std::string& uri) const;
    /// Returns a mutable pointer to the stored document, or nullptr when the
    /// URI is not open.
    TextDocument* find(const std::string& uri);

    /// Returns the currently open document URIs in sorted order.
    std::vector<std::string> openUris() const;
    /// Returns the number of open documents currently tracked.
    size_t size() const;

private:
    /// Mapping from canonical URI to the synchronized document state.
    std::unordered_map<std::string, TextDocument> documents_;
};

} // namespace rls::lsp
