# Rando Logic Script for Visual Studio

This directory contains the Visual Studio language client. It is a Visual Studio
2022-compatible extension compiled against the 17.14 VSSDK and Language Server
Client APIs, then built and tested with Visual Studio 2026.

## Compatibility Proof

The extension was tested with Visual Studio Community 2026 18.9.1 on Windows x64.

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

No Visual Studio 2026-specific runtime API is required. The project uses current
18.x VSSDK BuildTools only to build the package with the installed Visual Studio
2026 toolchain.

## Editor Integration

- `.rls` files activate the MEF language client.
- The VSIX links the canonical TextMate grammar and language configuration from
  `editors/vscode`; the syntax and local editor behavior have one source of truth.
- The language configuration supplies comments, bracket matching, auto-closing,
  surrounding pairs, and RLS word boundaries.
- The client supplies semantic token type and modifier name mappings in
  `initializationOptions.semanticTokens.legend`. The server applies those mappings
  while constructing the initialize response, before Visual Studio caches the
  legend. Token indexes, modifier bits, ranges, and semantic analysis are unchanged.
- Visual Studio maps the canonical RLS token names to its method, C++ value-type,
  C++ enumerator, parameter, property, local, operator, and string classifications. Unsupported
  modifiers map to an unregistered no-style name, so they do not overlay resolved
  symbols with plain text.
- Server stderr, startup failures, initialization failures, and unexpected exits
  are written to the Visual Studio Activity Log. Visual Studio supplies the
  initialization-failure InfoBar.
- The client owns one server process at a time and cleans it up on cancellation or
  host shutdown. Unexpected exits use Visual Studio's bounded language-server
  recovery policy; a forced-crash host test verified exactly one replacement.
- `RLS_LANGUAGE_SERVER_PATH` is available only as a development override. Release
  packages use the server bundled at `Server\rls_language_server.exe`.

Visual Studio LSP tracing is available for Open Folder workspaces. Add the following
to `.vs\VSWorkspaceSettings.json`:

```json
{
  "randoLogicScript.trace.server": "Verbose"
}
```

Trace files are written under `%TEMP%\VisualStudio\LSP`. Extension startup, stderr,
initialization failures, and unexpected exits are also written to the Visual Studio
Activity Log.

## Known Limitations

- Windows x64 only; Visual Studio 2019 and ARM64 are not supported.
- Visual Studio 2026 advertises no completion snippet support, so completions use plain
  text and server-side indentation.
- Visual Studio requests flat document symbols rather than hierarchical symbols.
- No custom project system, debugger integration, or designer is included.
- Visual Studio Marketplace publication is currently manual.

## Build

Build the native server from an x64 Visual Studio developer shell. CMake 4.1 does
not expose a Visual Studio 18 generator, so Visual Studio 2026 uses Ninja:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1' `
  -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
cmake -S . -B build-visualstudio-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=OFF `
  -DRLS_STATIC_MSVC_RUNTIME=ON
cmake --build build-visualstudio-release --target rls_language_server --parallel
```

Then build the VSIX with full-framework MSBuild:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  editors\visualstudio\RandoLogicScript.VisualStudio.sln `
  /restore /t:Build /p:Configuration=Release
```

The VSIX is written to:

```text
editors\visualstudio\src\bin\Release\net472\RandoLogicScript.VisualStudio.vsix
```

The project defaults `RlsLanguageServerPath` to the Ninja output above. Override it
for another generator or CI build:

```powershell
/p:RlsLanguageServerPath=C:\path\to\rls_language_server.exe
```

The build fails if the server is absent, is not an x64 PE, or imports the dynamic
MSVC runtime. Generated executables and VSIX files remain ignored build artifacts.

## Validate the Package

```powershell
& editors\visualstudio\scripts\validate-vsix.ps1 `
  -VsixPath editors\visualstudio\src\bin\Release\net472\RandoLogicScript.VisualStudio.vsix
```

The validator checks package identity, install target, architecture, required
assets, exactly one bundled server, and static MSVC runtime linkage. It also requires
the Visual Studio manifest version to match `editors/vscode/package.json`. Release
validation can additionally enforce the tag:

```powershell
& editors\visualstudio\scripts\validate-vsix.ps1 `
  -VsixPath editors\visualstudio\src\bin\Release\net472\RandoLogicScript.VisualStudio.vsix `
  -ExpectedTag v0.1.0
```

## Run the Development Build

Set the development override to test a different server without rebuilding the VSIX:

```powershell
$env:RLS_LANGUAGE_SERVER_PATH = (Resolve-Path 'build\lsp\rls_language_server.exe').Path
& 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\devenv.exe' `
  editors\vscode\test-fixture\diagnostic.rls `
  /RootSuffix RLSExp
```

## Test

Run the client unit tests without launching Visual Studio or a real server:

```powershell
dotnet test editors\visualstudio\tests\RandoLogicScript.VisualStudio.Tests.csproj `
  --configuration Release
```

Run the editor-neutral executable protocol matrix against the packaged server:

```powershell
python lsp\tests\process_smoke.py `
  --server build-visualstudio-release\lsp\rls_language_server.exe
```

Run the Visual Studio 2026 Experimental Instance harness from PowerShell 7:

```powershell
& editors\visualstudio\scripts\test-experimental-instance.ps1 `
  -Configuration Release `
  -RootSuffix RLSPhase4
```

Interactive local runs are strict. Hosted CI additionally passes
`-SkipWhenDteUnavailable` because a runner can have Visual Studio installed without an
interactive desktop capable of registering its DTE automation object. That switch
only skips the initial DTE acquisition timeout; it does not suppress host assertion
failures after DTE becomes available.

The host harness validates VSIX deployment, bundled-server activation, encoded Open
Folder roots, standalone null roots, diagnostics, semantic-token and document-symbol
requests, Visual Studio semantic classification legend adaptation,
authoring/navigation/refactoring capability negotiation, watched manifest
notifications, one-process crash recovery, Unicode/spaced URIs, and cleanup. The
process smoke test sends actual completion, signature, hover, definition, references,
document/workspace symbol, prepare-rename, rename, and semantic-token requests.

Visual rendering, keyboard accessibility, and UI workflow checks remain manual. See
`editors/visualstudio/MANUAL-TESTING.md`.

CI and release publication belong to the remaining implementation phase.
