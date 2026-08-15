#include "rls/lsp/lifecycle_service.h"

#include <stdexcept>

namespace rls::lsp {

void LifecycleService::initialize(
    bool definitionLinkSupport, bool documentSymbolHierarchySupport,
    bool completionSnippetSupport) {
    if (initializeRequested_) {
        throw std::logic_error("initialize was already requested");
    }
    initializeRequested_ = true;
    definitionLinkSupport_ = definitionLinkSupport;
    documentSymbolHierarchySupport_ = documentSymbolHierarchySupport;
    completionSnippetSupport_ = completionSnippetSupport;
}

void LifecycleService::initialized() {
    if (!initializeRequested_ || initialized_ || shutdownRequested_) {
        throw std::logic_error("initialized is not valid in the current state");
    }
    initialized_ = true;
}

void LifecycleService::shutdown() {
    if (!initializeRequested_ || shutdownRequested_) {
        throw std::logic_error("shutdown is not valid in the current state");
    }
    shutdownRequested_ = true;
}

void LifecycleService::exit() {
    exitRequested_ = true;
}

bool LifecycleService::acceptsDocumentUpdates() const {
    return initialized_ && !shutdownRequested_;
}

bool LifecycleService::supportsDefinitionLinks() const {
    return definitionLinkSupport_;
}

bool LifecycleService::supportsDocumentSymbolHierarchy() const {
    return documentSymbolHierarchySupport_;
}

bool LifecycleService::supportsCompletionSnippets() const {
    return completionSnippetSupport_;
}

bool LifecycleService::shouldExit() const {
    return exitRequested_;
}

int LifecycleService::exitCode() const {
    return shutdownRequested_ ? 0 : 1;
}

} // namespace rls::lsp