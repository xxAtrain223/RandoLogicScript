#pragma once

namespace rls::lsp {

class CompletionService;
class DocumentSynchronizationService;
class HoverService;
class JsonRpcRouter;
class LifecycleService;
class NavigationService;
class SemanticTokensService;
class SignatureHelpService;
class WorkspaceService;

void RegisterLifecycleRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace);
void RegisterDocumentSynchronizationRoutes(
    JsonRpcRouter& router, DocumentSynchronizationService& synchronization);
void RegisterAuthoringRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, CompletionService& completion,
    SignatureHelpService& signatureHelp, HoverService& hover);
void RegisterNavigationRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, NavigationService& navigation,
    WorkspaceService& workspace);
void RegisterSemanticTokenRoutes(
    JsonRpcRouter& router, SemanticTokensService& semanticTokens);
void RegisterWorkspaceRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace);

} // namespace rls::lsp
