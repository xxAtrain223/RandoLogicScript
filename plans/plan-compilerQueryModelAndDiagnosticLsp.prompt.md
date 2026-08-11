## Detailed Plan: Compiler Query Model

### Goal

Create the compiler-owned, immutable query model that later editor features consume. It answers what syntax or symbol is at a position, what it resolves to, which symbols are visible, what types are involved, and where declarations/references occur.

Despite the historical main-plan label, this document deliberately does **not** plan LSP transport, document synchronization, endpoint routing, project discovery, or diagnostic publication. Those concerns belong to [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md) and [plan-rlsProjectFilesAndLoading.prompt.md](plan-rlsProjectFilesAndLoading.prompt.md).

### Ownership Boundary

- Parser/builder owns source spans, recoverable syntax structure, and source-level cursor queries.
- Sema owns symbol identity, scope, type/enum identity, call resolution, declaration links, and references.
- `AnalysisSnapshot` owns one coherent analyzed source set and every index derived from it.
- Consumers use public value-query results. They do not retain AST pointers or recreate symbol resolution from text.

### 1. Canonical SourceText

- [x] Introduce immutable `SourceText` with canonical UTF-8 content and precomputed line-start byte offsets.
- [x] Centralize byte offset to/from `ast::Position`.
- [x] Centralize UTF-8 and UTF-16 position conversion for external consumers.
- [x] Centralize full-document and ranged edit application.
- [x] Define and enforce an explicit invalid-UTF-8 policy.
- [x] Normalize or preserve CRLF consistently and test the chosen contract.
- [x] Keep a narrowly lexical incomplete-token replacement-range helper for future completion use; it can locate a fragment but cannot identify a semantic symbol.
- [ ] Forbid duplicated offset/range logic elsewhere.

### 2. Parser Source Index

- [x] Audit `ast::Name`, expression spans, declaration spans, `CallExpr`, `MemberExpr`, parameters, entries, sections, and enum nodes in [ast/include/ast.h](../ast/include/ast.h).
- [x] Extend builder output in [parser/src/builder.cpp](../parser/src/builder.cpp) or a post-parse pass to construct a per-file `SourceIndex`.
- [x] Index name tokens and source-level categories.
- [x] Index expressions and enclosing declaration/section context.
- [x] Index calls, arguments, and argument labels.
- [x] Index declarations and selection ranges.
- [x] Index region data, sections, and entries.
- [x] Expose parser-only `syntaxAt(position)`.
- [x] Expose parser-only `nameAt(position)`.
- [x] Expose parser-only `enclosingExpression(position)`.
- [x] Expose parser-only `enclosingCall(position)` with structural argument index/ranges.
- [x] Expose parser-only `declarationsIn(file)`.
- [x] Preserve partial indexes only for trustworthy recovery nodes; return empty/unknown context rather than fabricated syntax meaning.

### 3. Stable Semantic Identity

- [x] Define opaque `SymbolId`, stable for the lifetime of an `AnalysisSnapshot`, never derived from an AST pointer.
- [x] Define `SymbolRecord` with identity, category, display name, declaration URI/path and ranges, container, signature/type/enum metadata, and provenance.
- [x] Define `OccurrenceRecord` with referenced `SymbolId` when resolved, source range, and occurrence kind: declaration, reference, call, type reference, member access, extension target, or unresolved.
- [x] Model regions, extension contributions/targets, defines, extern defines, enum types/members, parameters, and navigable region/section entries.
- [x] Preserve extern/pattern provenance; a pattern-matched external enum value can be typed/referenced without pretending it has a source declaration.

### 4. Semantic Index Construction

- [x] In [sema/src/collect_declarations.cpp](../sema/src/collect_declarations.cpp), assign top-level declaration identities, record canonical region/extension relations, and attach duplicate-related locations.
- [x] In [sema/src/resolve_types.cpp](../sema/src/resolve_types.cpp), record parameter scopes, identifier uses, enum/member resolutions, callable targets, argument bindings, inferred types, enum identities, and expected types.
- [ ] In [sema/src/validate_declarations.cpp](../sema/src/validate_declarations.cpp), produce stable diagnostic codes and structured related data for later consumers.
- [x] Build `SymbolId -> SymbolRecord` indexes.
- [x] Build `SymbolId -> sorted occurrences` indexes.
- [x] Build file/range -> occurrence indexes.
- [x] Build syntax node/range -> inferred and expected type indexes.
- [x] Build call node/range -> resolved target and normalized binding indexes.
- [x] Build scope context -> visible symbols, or retain sufficient parent data to derive them.
- [x] Keep pointer-keyed `TypeTable`, `EnumTypeTable`, and `ResolvedCallArgs` internal; copy required values into stable snapshot records before exposing queries.

### 5. AnalysisSnapshot

- [x] Define an immutable snapshot that owns source text, parsed files, parser diagnostics/indexes, analyzed `ast::Project`, semantic diagnostics/indexes, project identity, and a monotonic generation number.
- [x] Construct it from an explicit source set supplied by the project-loading/LSP layers; it must not perform parent-directory discovery.
- [x] Support disk content and caller-supplied in-memory overlays through the same source-set API.
- [x] Define degraded parse-failure behavior: retain parser diagnostics, exclude unreliable declarations from sema, and keep indexes for unaffected/recoverable source only.
- [x] Use shared ownership so readers see one consistent snapshot while a later snapshot is built.

### Required Query API

```text
syntaxAt(document, position) -> optional<SyntaxContext>
nameAt(document, position) -> optional<SourceNameContext>
symbolAt(document, position) -> optional<SymbolId>
occurrenceAt(document, position) -> optional<OccurrenceRecord>
declaration(symbol) -> optional<SymbolRecord>
references(symbol, options) -> vector<OccurrenceRecord>
visibleSymbolsAt(document, position) -> vector<SymbolId>
typeAt(document, position) -> optional<TypeInfo>
expectedTypeAt(document, position) -> optional<TypeInfo>
callAt(document, position) -> optional<CallContext>
diagnosticsFor(document) -> vector<CompilerDiagnostic>
```

`CallContext` includes resolved/unresolved callable state, argument ranges, active argument index when structurally known, parameter metadata, normalized bindings when valid, and expected parameter type/enum identity when available.

### Tests

- [x] Add SourceText round trips for ASCII, UTF-8, UTF-16, CRLF, ranged edits, and the invalid-input policy.
- [ ] Add source-index tests for declarations, calls, arguments, members, comments, strings, whitespace, malformed syntax, and recovery.
- [ ] Add symbol tests for same-spelled parameters in separate scopes, cross-file declarations, externs, enums, member resolution, ambiguous enum values, and unknown identifiers.
- [ ] Add region tests for base/extension relations and references.
- [ ] Add snapshot tests proving open overlays override disk input and public query results contain no AST pointers.
- [ ] Add regression tests confirming a parse error in one file does not corrupt queries for unaffected files.

### Definition of Done

- [ ] Compiler services answer tested syntax, symbol, type, scope, call, declaration, reference, and diagnostic queries from one immutable snapshot.
- [ ] No public query result depends on AST pointer lifetime.
- [ ] No consumer needs raw word-boundary scanning to determine source or semantic meaning.
- [ ] Project/LSP layers can supply a complete source set and consume query results without depending on parser/sema internals.
