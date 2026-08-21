# Rando Logic Script for VS Code

This extension contributes RLS syntax support and launches the native RLS language server over stdio for live diagnostics, completion, signature help, hover, navigation, and semantic highlighting.

## Development

1. Configure and build the repository with CMake.
2. Run `npm ci` in this directory.
3. Run `npm test` for the extension-host integration test, or use the repository's **Run RLS Language Extension** launch configuration.

During repository development, the extension discovers common CMake outputs under `build/` and `build-vs/`. Set `randoLogicScript.server.path` to use another executable. The `RLS_LANGUAGE_SERVER_PATH` environment variable is available for automated tests.

Use **Rando Logic Script: Restart Language Server** after changing the configured executable or arguments.

## Packaging

The extension is published as targeted VSIX packages for Windows x64, Linux x64, and
Intel macOS. Each package contains a native Release server under
`server/<platform>-<architecture>/rls_language_server[.exe]`. VS Code selects the
package matching the extension host platform.

Native servers and VSIX files are generated artifacts and are not committed. See the
[release guide](../../docs/RELEASING.md) for the CI/CD and Marketplace publication
process.