#include "rls/lsp/server_composition_root.h"

#include <utility>

#include "rls/lsp/route_modules.h"

namespace rls::lsp {

ServerCompositionRoot::ServerCompositionRoot(ProjectManager::Resolver resolver)
    : projects_(documents_, std::move(resolver)),
    synchronization_(lifecycle_, documents_, projects_, scheduler_) {
    RegisterLifecycleRoutes(router_, lifecycle_);
    RegisterDocumentSynchronizationRoutes(router_, synchronization_);
    router_.requireRoutes({
        "initialize",
        "initialized",
        "shutdown",
        "exit",
        "textDocument/didOpen",
        "textDocument/didChange",
        "textDocument/didClose",
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

const JsonRpcRouter& ServerCompositionRoot::router() const {
    return router_;
}

} // namespace rls::lsp