#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rls::lsp {

struct TextDocument {
    std::string uri;
    std::string languageId;
    int64_t version = 0;
    std::string text;
};

enum class DocumentUpdateResult {
    Applied,
    InvalidUri,
    NotOpen,
    StaleVersion,
};

class DocumentStore {
public:
    DocumentUpdateResult open(
        std::string uri, std::string languageId, int64_t version, std::string text);
    DocumentUpdateResult applyFullChange(
        std::string_view uri, int64_t version, std::string text);
    bool close(std::string_view uri);

    const TextDocument* find(std::string_view uri) const;
    size_t size() const;

private:
    std::unordered_map<std::string, TextDocument> documents_;
};

} // namespace rls::lsp