## Detailed Plan: Symbol Navigation and Discovery

### Goal

Expose RLS declarations and usages through definition, references, document highlights, document symbols, and workspace symbols.

### Dependencies and Boundary

Consume [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) query APIs and [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md) routing/snapshot services. This plan owns endpoint semantics and response shaping only; it does not build symbol indexes or implement raw cursor lookup.

- [x] Compiler query APIs are available through immutable analysis snapshots.
- [x] Explicit LSP routing, project assignment, and accepted-snapshot services are available.
- [x] Keep navigation handlers limited to protocol validation, query invocation, and response shaping.

### Features

1. **Definition**
   - [x] Register and advertise `textDocument/definition`.
   - [x] Implement `textDocument/definition` from `symbolAt` then `declaration`.
   - [x] Return a location link with origin selection range when supported.
   - [x] Resolve `extend region` targets to canonical region declarations.
   - [x] Resolve extern declarations to their source declaration.
   - [x] Return no definition for unresolved names or pattern-derived external enum values without a concrete source declaration.

2. **References and document highlights**
   - [x] Implement `textDocument/references` from stable `SymbolId -> occurrences` queries.
   - [x] Respect the client request to include declarations.
   - [x] Implement document highlights by filtering references to the active document.
   - [x] Preserve occurrence kind where the protocol supports read/write/text distinctions; do not invent write semantics for declarative RLS.

3. **Document symbols**
   - [x] Implement `textDocument/documentSymbol` from parser/source declaration records.
   - [x] Present regions, defines, extern defines, enums, enum members, and appropriate children without exposing internal AST layout.
   - [x] Use full declaration spans and name selection ranges consistently.
   - [x] Present extend-region blocks once as top-level extension symbols, with their own entries as children; do not also nest them under canonical base regions.

4. **Workspace symbols**
   - [x] Implement `workspace/symbol` from project declaration records only.
   - [x] Support case-insensitive query filtering and stable category-aware ordering.
   - [x] Scope results to the requesting workspace/project according to client context; never leak symbols from a separate discovered project.

### Edge Cases

- [x] Same-name parameters in distinct define scopes remain distinct symbols.
- [x] Ambiguous bare enum values return no arbitrary navigation target.
- [x] Unresolved symbols return empty responses, not textual best matches.
- [ ] Invalid/incomplete active files can use the current snapshot only when the source index identifies the same current occurrence; otherwise return no result.
- [x] Cross-file and unsaved-overlay locations use current snapshot paths/ranges.
- [x] Snapshot generations are checked before returning results.

### Tests

- [ ] Definition/reference navigation across files for defines, regions, enums, members, parameters, and externs.
- [x] Base-region versus extension behavior.
- [x] Include-declaration reference flag.
- [x] Document-symbol structure and selection ranges.
- [x] Workspace-symbol filtering/category ordering/project isolation.
- [ ] Ambiguous, unresolved, malformed, and stale-document cases.

### Definition of Done

- [x] All navigation responses derive from stable semantic/source queries.
- [x] Navigation works across files in one RLS project.
- [x] Endpoint code never depends on text matching or AST traversal.
