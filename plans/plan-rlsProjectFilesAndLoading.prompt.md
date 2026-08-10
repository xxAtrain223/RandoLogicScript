## Detailed Plan: RLS Project Files and Shared Project Loading

### Goal

Define `rls.json` as the canonical RLS project description and make the CLI and editor tooling resolve the same root, sources, exclusions, transpiler configurations, and outputs.

### Ownership

This plan owns manifest format, discovery, validation, source membership, and shared loading/execution configuration. It does not own compiler semantic indexes, editor synchronization, or LSP routing.

### Manifest Design

1. [x] Use a versioned JSON object:

```json
{
  "version": 1,
  "sources": ["src", "stdlib/host.rls"],
  "exclude": ["generated/**"],
  "transpilers": {
    "soh": { "output": "generated/soh" },
    "ap": { "output": "generated/ap" }
  }
}
```

2. [x] Publish a JSON Schema that validates required fields, types, known keys, relative-path rules, and manifest version. Registered transpiler names are validated by the console.
3. [x] Resolve every relative source, exclusion, and output path from the manifest directory. That directory is the project root.
4. Define duplicate and overlap rules:
   - [x] Canonicalize explicit input paths before de-duplication.
   - [x] Explicit sources can override default exclusion rules only when intentional and documented.
   - [x] Outputs must not be treated as sources unless explicitly included.
   - [x] Reject outputs that escape the project root unless an explicit future escape-hatch is designed.

### Discovery and Membership

1. [x] Given an edited `.rls` file, walk parent directories to the nearest `rls.json`.
2. [x] Treat nested manifests as separate projects. A file belongs to the nearest parent manifest, not every ancestor.
3. [ ] Support multiple manifests in an editor workspace without mixing their source sets or diagnostics. This requires an LSP project manager, which is not present in this checkout.
4. [x] For files with no discovered manifest, return a standalone configuration that analyzes only that file and does not promise cross-file resolution.
5. [x] Define default discovery exclusions for build/VCS/cache directories and apply manifest exclusions before source loading.
6. [x] Produce deterministic source ordering for explicit CLI inputs so diagnostics, tests, and generated output are stable.

### Shared Compiler/CLI Integration

1. [x] Introduce a project-loading library used by the console and later by the LSP project manager.
2. [x] Move explicit CLI source collection from `console/main.cpp` into the library.
3. [x] Represent a loaded project as configuration plus canonical source paths; do not read or parse source contents in the configuration layer.
4. Add CLI behavior:
   - [x] `--project <path>` loads a specified manifest.
   - [x] Invocation from a project directory discovers the nearest manifest by default.
   - [x] Existing explicit files/folders remain supported for compatibility.
   - [x] Explicit files/folders form an ephemeral project configuration.
   - [x] Command-line transpiler/output arguments override or complement manifest rules according to explicit documented precedence.
5. [x] Keep transpiler execution outside manifest parsing. The manifest describes intent; the console uses registered transpiler implementations to execute it.

### Diagnostics and Tests

1. [ ] Emit configuration diagnostics with manifest URI/ranges for schema and path errors. This requires the absent editor/LSP diagnostic transport; the shared loader currently returns actionable configuration errors.
2. Test:
   - [x] Manifest version/unknown-field errors.
   - [ ] Relative paths from nested working directories.
   - [x] Missing sources and empty source sets.
   - [x] Duplicate paths via relative aliases.
   - [x] Nested project discovery.
   - [x] Exclude patterns and output-directory exclusion.
   - [x] Invalid output paths.
   - [x] Unknown transpiler validation in the console.
   - [x] CLI manifest discovery and explicit-input compatibility.
3. [x] Validate example projects in CI and document the format in user-facing project setup docs.

### Definition of Done

- [ ] CLI and editor tooling receive identical project membership for the same `rls.json`. The shared resolver is ready for editor integration, but no editor project manager exists in this checkout.
- [x] A file can be mapped deterministically to its nearest project or standalone state.
- [ ] Manifest mistakes produce actionable diagnostics instead of silently analyzing an unintended file set.
- [x] No project loader accidentally parses build or generated output as RLS source.
