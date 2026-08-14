#pragma once

namespace rls::lsp {

class LifecycleService {
public:
    void initialize(bool definitionLinkSupport = false);
    void initialized();
    void shutdown();
    void exit();

    bool acceptsDocumentUpdates() const;
    bool supportsDefinitionLinks() const;
    bool shouldExit() const;
    int exitCode() const;

private:
    bool initializeRequested_ = false;
    bool definitionLinkSupport_ = false;
    bool initialized_ = false;
    bool shutdownRequested_ = false;
    bool exitRequested_ = false;
};

} // namespace rls::lsp