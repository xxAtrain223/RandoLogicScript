#pragma once

#include <string_view>
#include <vector>

#include "rls/lsp/document_synchronization_service.h"
#include "rls/lsp/document_store.h"
#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/project_manager.h"

namespace rls::lsp {

class ServerCompositionRoot {
public:
    explicit ServerCompositionRoot(
        ProjectManager::Resolver resolver = project::ResolveFileProject);

    std::vector<std::string> handlePayload(std::string_view payload) const;
    bool shouldExit() const;
    int exitCode() const;

    const DocumentStore& documents() const;
    const ProjectManager& projects() const;
    const JsonRpcRouter& router() const;

private:
    JsonRpcRouter router_;
    DocumentStore documents_;
    ProjectManager projects_;
    LifecycleService lifecycle_;
    DocumentSynchronizationService synchronization_;
};

} // namespace rls::lsp