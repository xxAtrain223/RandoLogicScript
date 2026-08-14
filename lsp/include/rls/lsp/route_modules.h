#pragma once

namespace rls::lsp {

class DocumentSynchronizationService;
class JsonRpcRouter;
class LifecycleService;
class NavigationService;
class WorkspaceService;

void RegisterLifecycleRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace);
void RegisterDocumentSynchronizationRoutes(
    JsonRpcRouter& router, DocumentSynchronizationService& synchronization);
void RegisterNavigationRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, NavigationService& navigation);
void RegisterWorkspaceRoutes(
    JsonRpcRouter& router, LifecycleService& lifecycle, WorkspaceService& workspace);

} // namespace rls::lsp