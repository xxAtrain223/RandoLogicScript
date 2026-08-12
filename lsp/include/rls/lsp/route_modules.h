#pragma once

namespace rls::lsp {

class DocumentSynchronizationService;
class JsonRpcRouter;
class LifecycleService;

void RegisterLifecycleRoutes(JsonRpcRouter& router, LifecycleService& lifecycle);
void RegisterDocumentSynchronizationRoutes(
    JsonRpcRouter& router, DocumentSynchronizationService& synchronization);

} // namespace rls::lsp