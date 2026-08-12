#pragma once

namespace rls::lsp {

class LifecycleService {
public:
    void initialize();
    void initialized();
    void shutdown();
    void exit();

    bool acceptsDocumentUpdates() const;
    bool shouldExit() const;
    int exitCode() const;

private:
    bool initializeRequested_ = false;
    bool initialized_ = false;
    bool shutdownRequested_ = false;
    bool exitRequested_ = false;
};

} // namespace rls::lsp