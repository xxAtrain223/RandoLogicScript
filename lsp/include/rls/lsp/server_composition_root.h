#pragma once

#include <string_view>
#include <vector>

#include "rls/lsp/analysis_scheduler.h"
#include "rls/lsp/diagnostic_publisher.h"
#include "rls/lsp/document_synchronization_service.h"
#include "rls/lsp/document_store.h"
#include "rls/lsp/json_rpc_router.h"
#include "rls/lsp/lifecycle_service.h"
#include "rls/lsp/navigation_service.h"
#include "rls/lsp/outbound_message_queue.h"
#include "rls/lsp/project_manager.h"
#include "rls/lsp/workspace_service.h"

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
    AnalysisScheduler& scheduler();
    const WorkspaceService& workspace() const;
    OutboundMessageQueue& outbound();
    const JsonRpcRouter& router() const;

private:
    JsonRpcRouter router_;
    OutboundMessageQueue outbound_;
    DocumentStore documents_;
    ProjectManager projects_;
    LifecycleService lifecycle_;
    DiagnosticPublisher diagnostics_;
    AnalysisScheduler scheduler_;
    NavigationService navigation_;
    WorkspaceService workspace_;
    DocumentSynchronizationService synchronization_;
};

} // namespace rls::lsp