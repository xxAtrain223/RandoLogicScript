#include "rls/lsp/server_composition_root.h"

#include <utility>

#include "rls/lsp/route_modules.h"

namespace rls::lsp {

ServerCompositionRoot::ServerCompositionRoot(ProjectManager::Resolver resolver)
    : projects_(documents_, std::move(resolver)),
      diagnostics_(outbound_),
    navigation_(projects_, scheduler_),
      workspace_(projects_, scheduler_, diagnostics_),
      synchronization_(lifecycle_, documents_, projects_, scheduler_, diagnostics_) {
    scheduler_.setAcceptedHandler(
        [this](std::string projectId, AnalysisScheduler::Snapshot snapshot) {
            diagnostics_.acceptedSnapshot(std::move(projectId), std::move(snapshot));
        });
    RegisterLifecycleRoutes(router_, lifecycle_, workspace_);
    RegisterDocumentSynchronizationRoutes(router_, synchronization_);
    RegisterNavigationRoutes(router_, lifecycle_, navigation_, workspace_);
    RegisterWorkspaceRoutes(router_, lifecycle_, workspace_);
    router_.requireRoutes({
        "initialize",
        "initialized",
        "shutdown",
        "exit",
        "textDocument/didOpen",
        "textDocument/didChange",
        "textDocument/didClose",
        "textDocument/definition",
        "textDocument/references",
        "textDocument/documentHighlight",
        "textDocument/documentSymbol",
        "workspace/symbol",
        "workspace/didChangeWorkspaceFolders",
        "workspace/didChangeWatchedFiles",
    });
}

std::vector<std::string> ServerCompositionRoot::handlePayload(std::string_view payload) const {
    return router_.handlePayload(payload);
}

bool ServerCompositionRoot::shouldExit() const {
    return lifecycle_.shouldExit();
}

int ServerCompositionRoot::exitCode() const {
    return lifecycle_.exitCode();
}

const DocumentStore& ServerCompositionRoot::documents() const {
    return documents_;
}

const ProjectManager& ServerCompositionRoot::projects() const {
    return projects_;
}

AnalysisScheduler& ServerCompositionRoot::scheduler() {
    return scheduler_;
}

const WorkspaceService& ServerCompositionRoot::workspace() const {
    return workspace_;
}

OutboundMessageQueue& ServerCompositionRoot::outbound() {
    return outbound_;
}

const JsonRpcRouter& ServerCompositionRoot::router() const {
    return router_;
}

} // namespace rls::lsp