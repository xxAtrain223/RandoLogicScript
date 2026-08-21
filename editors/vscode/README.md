# Rando Logic Script for VS Code

Language support for Rando Logic Script (`.rls`) files and `rls.json` projects.

## Features

- Syntax highlighting, bracket matching, comments, and editor configuration.
- Live parser, semantic, and project-configuration diagnostics.
- Context-aware completion and snippets.
- Signature help and hover information.
- Go to definition, references, document highlights, and symbol search.
- Semantic highlighting and project-wide rename.
- Automatic `rls.json` project discovery and file watching.

The extension starts a bundled native language server and communicates with it over
stdio. Analysis remains local to the VS Code extension host.

## Requirements

- VS Code 1.82 or later.
- A supported x64 extension host: Windows, a glibc-based Linux distribution, or
	macOS 12 or later on Intel hardware.

When using Remote SSH, WSL, or a Dev Container, install the extension in the remote
environment. The native server runs where the VS Code workspace extension host runs.

## Configuration

| Setting | Default | Description |
| --- | --- | --- |
| `randoLogicScript.server.path` | Empty | Override the bundled language-server executable. Relative paths resolve from the first workspace folder. |
| `randoLogicScript.server.arguments` | `[]` | Additional arguments passed to the language server. |
| `randoLogicScript.completion.sectionSnippetIndentation` | `client` | Choose whether VS Code or the server supplies multiline snippet indentation. |
| `randoLogicScript.trace.server` | `off` | Log language-client protocol messages for troubleshooting. |

After changing the server path, arguments, or snippet-indentation mode, run
**Rando Logic Script: Restart Language Server** from the Command Palette.

## Troubleshooting

If the bundled server cannot be found or launched, configure
`randoLogicScript.server.path` with an executable built from the
[RandoLogicScript repository](https://github.com/xxAtrain223/RandoLogicScript).
Enable `randoLogicScript.trace.server` when collecting protocol logs for an
[issue report](https://github.com/xxAtrain223/RandoLogicScript/issues).

## Development

1. Configure and build the repository with CMake.
2. Run `npm ci` in this directory.
3. Run `npm test` for the extension-host integration test, or use the repository's **Run RLS Language Extension** launch configuration.

During repository development, the extension discovers common CMake outputs under `build/` and `build-vs/`. Set `randoLogicScript.server.path` to use another executable. The `RLS_LANGUAGE_SERVER_PATH` environment variable is available for automated tests.

Use **Rando Logic Script: Restart Language Server** after changing the configured executable or arguments.

### Packaging

The extension is published as targeted VSIX packages for Windows x64, Linux x64, and
Intel macOS. Each package contains a native Release server under
`server/<platform>-<architecture>/rls_language_server[.exe]`. VS Code selects the
package matching the extension host platform.

Native servers and VSIX files are generated artifacts and are not committed. See the
[release guide](../../docs/RELEASING.md) for the CI/CD and Marketplace publication
process.