#pragma once

#include <string_view>
#include <vector>

#include "rls/lsp/document_store.h"
#include "rls/lsp/json_rpc_router.h"

namespace rls::lsp {

class ServerCompositionRoot {
public:
    ServerCompositionRoot();

    std::vector<std::string> handlePayload(std::string_view payload) const;
    bool shouldExit() const;
    int exitCode() const;

    const DocumentStore& documents() const;
    const JsonRpcRouter& router() const;

private:
    void registerLifecycleRoutes();
    void registerDocumentRoutes();
    void requireInitialized() const;

    JsonRpcRouter router_;
    DocumentStore documents_;
    bool initialized_ = false;
    bool shutdownRequested_ = false;
    bool exitRequested_ = false;
};

} // namespace rls::lsp