#include "document_store.h"

#include <algorithm>

namespace rls::lsp {

void DocumentStore::open(std::string uri, std::string languageId, int version, std::string text) {
    TextDocument doc;
    doc.uri = uri;
    doc.languageId = std::move(languageId);
    doc.version = version;
    doc.text = std::move(text);

    documents_.insert_or_assign(uri, std::move(doc));
}

bool DocumentStore::applyFullChange(const std::string& uri, int version, std::string text) {
    auto it = documents_.find(uri);
    if (it == documents_.end()) {
        return false;
    }

    if (version < it->second.version) {
        return false;
    }

    it->second.version = version;
    it->second.text = std::move(text);
    return true;
}

bool DocumentStore::close(const std::string& uri) {
    return documents_.erase(uri) > 0;
}

const TextDocument* DocumentStore::find(const std::string& uri) const {
    auto it = documents_.find(uri);
    return it == documents_.end() ? nullptr : &it->second;
}

TextDocument* DocumentStore::find(const std::string& uri) {
    auto it = documents_.find(uri);
    return it == documents_.end() ? nullptr : &it->second;
}

std::vector<std::string> DocumentStore::openUris() const {
    std::vector<std::string> uris;
    uris.reserve(documents_.size());

    for (const auto& [uri, _] : documents_) {
        uris.push_back(uri);
    }

    std::sort(uris.begin(), uris.end());
    return uris;
}

size_t DocumentStore::size() const {
    return documents_.size();
}

} // namespace rls::lsp
