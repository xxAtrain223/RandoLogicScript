#pragma once

namespace rls::lsp {

enum class SectionSnippetIndentation {
    Client,
    Server,
};

class LifecycleService {
public:
    void initialize(
        bool definitionLinkSupport = false,
        bool documentSymbolHierarchySupport = false,
        bool completionSnippetSupport = false,
        SectionSnippetIndentation sectionSnippetIndentation =
            SectionSnippetIndentation::Server,
        bool workspaceDocumentChangesSupport = false);
    void initialized();
    void shutdown();
    void exit();

    bool acceptsDocumentUpdates() const;
    bool supportsDefinitionLinks() const;
    bool supportsDocumentSymbolHierarchy() const;
    bool supportsCompletionSnippets() const;
    bool supportsWorkspaceDocumentChanges() const;
    SectionSnippetIndentation sectionSnippetIndentation() const;
    bool shouldExit() const;
    int exitCode() const;

private:
    bool initializeRequested_ = false;
    bool definitionLinkSupport_ = false;
    bool documentSymbolHierarchySupport_ = false;
    bool completionSnippetSupport_ = false;
    bool workspaceDocumentChangesSupport_ = false;
    SectionSnippetIndentation sectionSnippetIndentation_ =
        SectionSnippetIndentation::Server;
    bool initialized_ = false;
    bool shutdownRequested_ = false;
    bool exitRequested_ = false;
};

} // namespace rls::lsp