## Detailed Plan: Explicit Feature-Oriented LSP and Live Diagnostics

### Goal

Expose the compiler query model through a robust, portable LSP server. This plan owns JSON-RPC transport, document synchronization, analysis scheduling, explicit route composition, and diagnostic publishing. It consumes project-loading and compiler-query APIs; it does not redefine them.

### Dependencies

- [plan-rlsProjectFilesAndLoading.prompt.md](plan-rlsProjectFilesAndLoading.prompt.md) supplies project discovery and source membership.
- [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) supplies immutable snapshots and diagnostics/query APIs.

### 1. Port Infrastructure Selectively

- [x] Salvage or reimplement historical branch components that are protocol-only:
   - [x] Content-Length JSON-RPC framing.
   - [x] Request/notification/response handling.
   - [x] URI normalization.
   - [x] Versioned document storage.
   - [x] Protocol integration tests.
- [x] Audit all imported code for Windows assumptions, case sensitivity, URI escaping, JSON errors, and stdout logging.
- [x] Keep transport independent of AST, sema, and query records.
- [x] Send protocol frames only on stdout. Send logs to stderr or an opt-in file.

### 2. Service Boundaries

- [x] `DocumentStore` owns client text buffers and client versions.
- [x] `ProjectManager` maps documents to project or standalone states using the project-loading service.
- [x] `AnalysisScheduler` receives source-set changes, debounces them, builds snapshots off the protocol loop, and discards stale work.
- [ ] `DiagnosticPublisher` compares accepted snapshots and publishes changed/cleared diagnostics.
- [x] `ClientConnection` owns protocol notifications/responses.
- [x] Handler modules depend on these interfaces, not globals or `ast::Project`.

### 3. Explicit Router and Composition Root

- [x] Create one `ServerCompositionRoot` that constructs all services and registers every route explicitly.
- [ ] Group typed routes into modules:
   - [x] Lifecycle.
   - [x] Document synchronization.
   - [ ] Diagnostics.
   - [ ] Future placeholders: navigation, authoring, highlighting, refactoring, formatting.
- [x] Apply handler rules:
   - [x] Validate/decode protocol DTOs.
   - [x] Invoke injected service APIs.
   - [x] Translate results to protocol DTOs.
   - [x] Never scan source, navigate ASTs, or mutate analysis state directly.
- [x] Validate duplicate/missing route registration at startup.
- [x] Remove static endpoint auto-registration, linker force-load flags, global registries, and hidden singletons.

### 4. Lifecycle and Synchronization

- [x] Implement `initialize`, `initialized`, `shutdown`, and `exit`.
- [x] Advertise only capabilities implemented by registered modules. Initial scope is text synchronization and diagnostics, not future navigation/authoring capabilities.
- [x] Implement `didOpen`, `didChange`, and `didClose` with full-document synchronization first.
- [x] Reject stale document versions. Closing an overlay returns the project to disk content on the next snapshot.
- [ ] Handle workspace-folder and watched-file notifications needed to reload manifests, adjust project membership, and react to disk changes.
- [ ] Reassign/clear state when a document moves between project roots or becomes standalone.

### 5. Scheduling and Stale Results

- [x] Schedule one debounced analysis stream per project.
- [ ] Capture document and manifest generations before work starts.
- [ ] Support cancellation tokens and cancellation at read, parse, sema, and indexing boundaries.
- [x] Publish a snapshot only when every triggering generation remains current. Discard older results without client notifications.
- [x] Begin with whole-project analysis. Hide this policy behind scheduler interfaces so later incremental work does not affect handlers.
- [x] Bound concurrent analyses across projects.

### 6. Diagnostics

- [ ] Convert compiler/configuration diagnostics to LSP ranges through the shared SourceText conversion API.
- [ ] Preserve severity, stable code, source, related information, and structured future-action data.
- [ ] Publish diagnostics grouped by document for accepted snapshots.
- [ ] Publish empty diagnostics to clear resolved diagnostics, removed files, and closed standalone documents.
- [ ] Publish manifest errors against `rls.json`; cross-file semantic errors use the primary span plus related declaration locations.
- [ ] Use push diagnostics first for broad client support. Defer pull diagnostics until snapshot consistency is proven.

### Tests

- [x] JSON-RPC framing, malformed messages, and clean stdout.
- [x] Explicit router registration without static initialization/linker flags.
- [x] Initialize capability negotiation and shutdown behavior.
- [x] Open/change/close version behavior and overlay-versus-disk behavior.
- [x] Per-project debounce, cancellation, and stale-result suppression.
- [ ] Nested/multiple project assignment and manifest reload behavior.
- [ ] Parser, sema, configuration, cross-file, and diagnostic-clearing flows.
- [ ] Windows, Linux, and macOS process/URI smoke tests.

### Definition of Done

- [ ] A standard LSP client starts the server over stdio and receives accurate live diagnostics for a discovered RLS project.
- [ ] Unsaved text supersedes disk text and stale analysis never republishes results.
- [x] All handlers are explicitly registered and service-injected.
- [x] No stdout logging, static registrar, linker force-load, endpoint-local AST traversal, or endpoint-local text lookup remains.
