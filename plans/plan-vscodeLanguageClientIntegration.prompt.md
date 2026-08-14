## Detailed Plan: VS Code Language Client Integration

### Goal

Provide a thin VS Code adapter that launches the portable native RLS language server, forwards editor lifecycle events, and exposes server diagnostics without moving compiler or protocol behavior into the extension.

### Ownership

- The VS Code adapter owns extension activation, native executable discovery, settings, restart behavior, file watching, packaging, and extension-host tests.
- The LSP architecture plan owns JSON-RPC, document/project synchronization, scheduling, diagnostics, and server capabilities.
- The syntax plan owns VS Code language registration, TextMate grammar, and language configuration.

### Runtime Adapter

- [x] Add a TypeScript extension entry point using `vscode-languageclient`.
- [x] Activate for RLS documents and launch the native server over stdio.
- [x] Restrict the client document selector to file-backed RLS documents supported by the server.
- [x] Forward `.rls` and `rls.json` file changes through a VS Code file-system watcher.
- [x] Dispose the client cleanly during extension deactivation.
- [x] Add an explicit language-server restart command.

### Executable Discovery and Settings

- [x] Add `randoLogicScript.server.path` and `randoLogicScript.server.arguments` settings.
- [x] Discover common CMake development outputs on Windows, Linux, and macOS.
- [x] Support a test-only `RLS_LANGUAGE_SERVER_PATH` override.
- [x] Reserve `server/<platform>-<architecture>/` for bundled release binaries.
- [ ] Build, sign where required, and package native server binaries into platform-specific VSIX artifacts.
- [ ] Define the supported platform/architecture release matrix and unsupported-platform message.

### Tests and CI

- [x] Add a VS Code extension-host test that activates the extension against the real native server.
- [x] Verify live compiler diagnostics from an RLS workspace fixture.
- [x] Verify the restart command reconnects to the server.
- [x] Compile and run the extension-host test on the existing Ubuntu, Windows, and macOS CI matrix.
- [x] Keep runtime dependencies audit-clean.

### Definition of Done

- [x] A repository build can be launched from the VS Code extension and produces live diagnostics.
- [x] Server discovery failures give an actionable setting link instead of silently disabling language support.
- [x] The client remains thin: it does not parse RLS, perform project discovery, or interpret diagnostic message strings.
- [ ] Published VSIX artifacts contain a compatible native server for every declared target platform.