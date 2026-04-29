#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rls::lsp {

struct TextDocument {
    std::string uri;
    std::string languageId;
    int version = 0;
    std::string text;
};

class DocumentStore {
public:
    void open(std::string uri, std::string languageId, int version, std::string text);

    // Applies full-document synchronization. Returns false for stale versions
    // or unknown documents.
    bool applyFullChange(const std::string& uri, int version, std::string text);

    bool close(const std::string& uri);

    const TextDocument* find(const std::string& uri) const;
    TextDocument* find(const std::string& uri);

    std::vector<std::string> openUris() const;
    size_t size() const;

private:
    std::unordered_map<std::string, TextDocument> documents_;
};

} // namespace rls::lsp
