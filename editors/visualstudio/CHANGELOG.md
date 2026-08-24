# Changelog

All notable changes to the Rando Logic Script Visual Studio extension are documented
in this file.

## 0.1.0

- Added `.rls` activation for amd64 Visual Studio 2022-compatible installations.
- Bundled the statically linked x64 RLS language server in the VSIX.
- Added diagnostics, completion, hover, signature help, semantic tokens, definition,
  references, document/workspace symbols, highlights, and rename support through LSP.
- Added shared TextMate syntax highlighting and language configuration for comments,
  brackets, auto-closing, surrounding pairs, and word selection.
- Added Open Folder, CMake-folder, and standalone-file project discovery behavior.
- Added Activity Log reporting, initialization failure notifications, deterministic
  process cleanup, and Visual Studio-managed bounded crash recovery.
- Added client-configurable semantic-token legend mapping during initialization, so
  Visual Studio receives its method, enum, enum-member, parameter, property, local,
  and operator classification names. Standard LSP clients retain the canonical legend.
- Added static native binary/package validation, xUnit tests, expanded process smoke
  coverage, and a Visual Studio 2026 Experimental Instance harness.

### Known Limitations

- Windows x64 only; ARM64 and Visual Studio 2019 are not supported.
- Visual Studio 2026 reports completion snippet support as unavailable, so completion
  uses plain text with server-side indentation.
- Visual Studio uses flat document symbols rather than hierarchical document symbols.
- No custom project system, debugger integration, or designer is included.
- Visual Studio Marketplace publication is manual; GitHub Release publication and
  provenance attestation are automated.
