#include "rls/lsp/document_store.h"

#include <utility>

#include "rls/lsp/document_uri.h"

namespace rls::lsp {

DocumentUpdateResult DocumentStore::open(
    std::string uri, std::string languageId, int64_t version, std::string text) {
    auto normalizedUri = NormalizeDocumentUri(uri);
    auto key = DocumentUriKey(uri);
    if (!normalizedUri || !key) {
        return DocumentUpdateResult::InvalidUri;
    }

    const auto existing = documents_.find(*key);
    if (existing != documents_.end() && version <= existing->second.version) {
        return DocumentUpdateResult::StaleVersion;
    }

    documents_.insert_or_assign(*key, TextDocument{
        std::move(*normalizedUri), std::move(languageId), version, std::move(text)});
    return DocumentUpdateResult::Applied;
}

DocumentUpdateResult DocumentStore::applyFullChange(
    std::string_view uri, int64_t version, std::string text) {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return DocumentUpdateResult::InvalidUri;
    }

    const auto document = documents_.find(*key);
    if (document == documents_.end()) {
        return DocumentUpdateResult::NotOpen;
    }
    if (version <= document->second.version) {
        return DocumentUpdateResult::StaleVersion;
    }

    document->second.version = version;
    document->second.text = std::move(text);
    return DocumentUpdateResult::Applied;
}

bool DocumentStore::close(std::string_view uri) {
    const auto key = DocumentUriKey(uri);
    return key && documents_.erase(*key) > 0;
}

const TextDocument* DocumentStore::find(std::string_view uri) const {
    const auto key = DocumentUriKey(uri);
    if (!key) {
        return nullptr;
    }
    const auto document = documents_.find(*key);
    return document == documents_.end() ? nullptr : &document->second;
}

size_t DocumentStore::size() const {
    return documents_.size();
}

} // namespace rls::lsp