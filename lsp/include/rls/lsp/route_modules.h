#pragma once

namespace rls::lsp {

class CompletionService;
class DocumentSynchronizationService;
class JsonRpcRouter;
class LifecycleService;
class NavigationService;
class SignatureHelpService;
class WorkspaceService;

void RegisterLifecycleRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace);
void RegisterDocumentSynchronizationRoutes(
    JsonRpcRouter& router, DocumentSynchronizationService& synchronization);
void RegisterAuthoringRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, CompletionService& completion,
    SignatureHelpService& signatureHelp);
void RegisterNavigationRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, NavigationService& navigation,
    WorkspaceService& workspace);
void RegisterWorkspaceRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace);

} // namespace rls::lsp
