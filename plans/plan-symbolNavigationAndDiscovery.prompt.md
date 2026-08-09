## Detailed Plan: Symbol Navigation and Discovery

### Goal

Expose RLS declarations and usages through definition, references, document highlights, document symbols, and workspace symbols.

### Dependencies and Boundary

Consume [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) query APIs and [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md) routing/snapshot services. This plan owns endpoint semantics and response shaping only; it does not build symbol indexes or implement raw cursor lookup.

### Features

1. **Definition**
   - Implement `textDocument/definition` from `symbolAt` then `declaration`.
   - Return a location link with origin selection range when supported.
   - Resolve `extend region` targets to canonical region declarations.
   - Resolve extern declarations to their source declaration.
   - Return no definition for unresolved names or pattern-derived external enum values without a concrete source declaration.

2. **References and document highlights**
   - Implement `textDocument/references` from stable `SymbolId -> occurrences` queries.
   - Respect the client request to include declarations.
   - Implement document highlights by filtering references to the active document.
   - Preserve occurrence kind where the protocol supports read/write/text distinctions; do not invent write semantics for declarative RLS.

3. **Document symbols**
   - Implement `textDocument/documentSymbol` from parser/source declaration records.
   - Present regions, defines, extern defines, enums, enum members, and appropriate children without exposing internal AST layout.
   - Use full declaration spans and name selection ranges consistently.
   - Decide/document whether extend-region blocks appear as top-level extension symbols, children of virtual region groups, or both; use one stable representation.

4. **Workspace symbols**
   - Implement `workspace/symbol` from project declaration records only.
   - Support case-insensitive query filtering and stable category-aware ordering.
   - Scope results to the requesting workspace/project according to client context; never leak symbols from a separate discovered project.

### Edge Cases

- Same-name parameters in distinct define scopes remain distinct symbols.
- Ambiguous bare enum values return no arbitrary navigation target.
- Unresolved symbols return empty responses, not textual best matches.
- Invalid/incomplete active files can use the current snapshot only when the source index identifies the same current occurrence; otherwise return no result.
- Cross-file and unsaved-overlay locations use current snapshot paths/ranges.
- Snapshot generations are checked before returning results.

### Tests

- Definition/reference navigation across files for defines, regions, enums, members, parameters, and externs.
- Base-region versus extension behavior.
- Include-declaration reference flag.
- Document-symbol structure and selection ranges.
- Workspace-symbol filtering/category ordering/project isolation.
- Ambiguous, unresolved, malformed, and stale-document cases.

### Definition of Done

All navigation responses derive from stable semantic/source queries, work across files in one RLS project, and never depend on text matching or AST traversal inside endpoint code.
