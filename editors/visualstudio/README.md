# Rando Logic Script for Visual Studio

This directory contains the Visual Studio language client compatibility prototype.
It is a Visual Studio 2022-style extension compiled against the 17.14 VSSDK and
Language Server Client APIs, then built and tested with Visual Studio 2026.

## Compatibility Proof

The prototype was tested with Visual Studio Community 2026 18.9.1 on Windows x64.

- A VSIX with an amd64 Visual Studio `[17.0,18.0)` installation target installed
  successfully through the Visual Studio 2026 VSIX installer.
- Visual Studio discovered the `.rls` MEF content type and launched the existing
  `rls_language_server.exe` over redirected standard input and output.
- The client sent full document synchronization and advertised support for hover,
  signature help, definition, references, document highlights, document symbols,
  workspace symbols, rename preparation, and workspace edits with
  `documentChanges`.
- Visual Studio advertised full and range semantic-token requests. It issued a
  `textDocument/semanticTokens/full` request and accepted the server response.
- Visual Studio received the expected `RLS-T006` diagnostic for the compatibility
  fixture.
- Visual Studio advertised `completionItem.snippetSupport` as `false`. The server
  therefore correctly falls back to plain completion text even though the client
  reports its private Visual Studio snippet version.
- Visual Studio advertised flat document symbols rather than hierarchical document
  symbols.
- A standalone-file initialization sent `rootUri: null`. Open Folder workspace-root
  and workspace-folder notification behavior still needs an automated host check.

No Visual Studio 2026-specific runtime API is required by the prototype. The project
uses current 18.x VSSDK BuildTools only to build the package with the installed
Visual Studio 2026 toolchain.

## Build

Build with the full-framework MSBuild installed by Visual Studio:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  editors\visualstudio\RandoLogicScript.VisualStudio.sln `
  /restore /t:Build /p:Configuration=Debug
```

The prototype VSIX is written to:

```text
editors\visualstudio\src\bin\Debug\net472\RandoLogicScript.VisualStudio.vsix
```

## Run the Prototype

The language server is not bundled during this compatibility phase. Set the
development override before launching an experimental Visual Studio instance:

```powershell
$env:RLS_LANGUAGE_SERVER_PATH = (Resolve-Path 'build\lsp\rls_language_server.exe').Path
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.exe' `
  editors\vscode\test-fixture\diagnostic.rls `
  /RootSuffix RLSExp
```

Production packaging, process recovery, settings UI, grammar registration, and
automated host tests belong to the later implementation phases.