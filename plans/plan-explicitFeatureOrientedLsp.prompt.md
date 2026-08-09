## Detailed Plan: Explicit Feature-Oriented LSP and Live Diagnostics

### Goal

Expose the compiler query model through a robust, portable LSP server. This plan owns JSON-RPC transport, document synchronization, analysis scheduling, explicit route composition, and diagnostic publishing. It consumes project-loading and compiler-query APIs; it does not redefine them.

### Dependencies

- [plan-rlsProjectFilesAndLoading.prompt.md](plan-rlsProjectFilesAndLoading.prompt.md) supplies project discovery and source membership.
- [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) supplies immutable snapshots and diagnostics/query APIs.

### 1. Port Infrastructure Selectively

1. Salvage or reimplement historical branch components that are protocol-only:
   - Content-Length JSON-RPC framing.
   - Request/notification/response handling.
   - URI normalization.
   - Versioned document storage.
   - Protocol integration tests.
2. Audit all imported code for Windows assumptions, case sensitivity, URI escaping, JSON errors, and stdout logging.
3. Keep transport independent of AST, sema, and query records.
4. Send protocol frames only on stdout. Send logs to stderr or an opt-in file.

### 2. Service Boundaries

1. `DocumentStore` owns client text buffers and client versions.
2. `ProjectManager` maps documents to project or standalone states using the project-loading service.
3. `AnalysisScheduler` receives source-set changes, debounces them, builds snapshots off the protocol loop, and discards stale work.
4. `DiagnosticPublisher` compares accepted snapshots and publishes changed/cleared diagnostics.
5. `ClientConnection` owns protocol notifications/responses.
6. Handler modules depend on these interfaces, not globals or `ast::Project`.

### 3. Explicit Router and Composition Root

1. Create one `ServerCompositionRoot` that constructs all services and registers every route explicitly.
2. Group typed routes into modules:
   - Lifecycle.
   - Document synchronization.
   - Diagnostics.
   - Future placeholders: navigation, authoring, highlighting, refactoring, formatting.
3. Handler rules:
   - Validate/decode protocol DTOs.
   - Invoke injected service APIs.
   - Translate results to protocol DTOs.
   - Never scan source, navigate ASTs, or mutate analysis state directly.
4. Validate duplicate/missing route registration at startup.
5. Remove static endpoint auto-registration, linker force-load flags, global registries, and hidden singletons.

### 4. Lifecycle and Synchronization

1. Implement `initialize`, `initialized`, `shutdown`, and `exit`.
2. Advertise only capabilities implemented by registered modules. Initial scope is text synchronization and diagnostics, not future navigation/authoring capabilities.
3. Implement `didOpen`, `didChange`, and `didClose` with full-document synchronization first.
4. Reject stale document versions. Closing an overlay returns the project to disk content on the next snapshot.
5. Handle workspace-folder and watched-file notifications needed to reload manifests, adjust project membership, and react to disk changes.
6. Reassign/clear state when a document moves between project roots or becomes standalone.

### 5. Scheduling and Stale Results

1. Schedule one debounced analysis stream per project.
2. Capture document and manifest generations before work starts.
3. Support cancellation tokens and cancellation at read, parse, sema, and indexing boundaries.
4. Publish a snapshot only when every triggering generation remains current. Discard older results without client notifications.
5. Begin with whole-project analysis. Hide this policy behind scheduler interfaces so later incremental work does not affect handlers.
6. Bound concurrent analyses across projects.

### 6. Diagnostics

1. Convert compiler/configuration diagnostics to LSP ranges through the shared SourceText conversion API.
2. Preserve severity, stable code, source, related information, and structured future-action data.
3. Publish diagnostics grouped by document for accepted snapshots.
4. Publish empty diagnostics to clear resolved diagnostics, removed files, and closed standalone documents.
5. Publish manifest errors against `rls.json`; cross-file semantic errors use the primary span plus related declaration locations.
6. Use push diagnostics first for broad client support. Defer pull diagnostics until snapshot consistency is proven.

### Tests

- JSON-RPC framing, malformed messages, and clean stdout.
- Explicit router registration without static initialization/linker flags.
- Initialize capability negotiation and shutdown behavior.
- Open/change/close version behavior and overlay-versus-disk behavior.
- Per-project debounce, cancellation, and stale-result suppression.
- Nested/multiple project assignment and manifest reload behavior.
- Parser, sema, configuration, cross-file, and diagnostic-clearing flows.
- Windows, Linux, and macOS process/URI smoke tests.

### Definition of Done

- A standard LSP client starts the server over stdio and receives accurate live diagnostics for a discovered RLS project.
- Unsaved text supersedes disk text and stale analysis never republishes results.
- All handlers are explicitly registered and service-injected.
- No stdout logging, static registrar, linker force-load, endpoint-local AST traversal, or endpoint-local text lookup remains.
