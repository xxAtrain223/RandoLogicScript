#include "language_server.h"

namespace rls::lsp {

LanguageServer::LanguageServer() = default;

const ServerCapabilities& LanguageServer::capabilities() const {
    return capabilities_;
}

DocumentStore& LanguageServer::documents() {
    return documents_;
}

const DocumentStore& LanguageServer::documents() const {
    return documents_;
}

} // namespace rls::lsp
