## Detailed Plan: Completion, Signatures, Hover, and Documentation

### Goal

Offer context-aware suggestions, callable signatures, inferred type information, and concise documentation while source is being edited.

### Dependencies and Boundary

Consume parser context, semantic scope/type/call queries from [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) and route through [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md). This plan does not define source indexing, scope calculation, or generic LSP request infrastructure.

### 1. Shared Presentation Model

- [x] Define compiler-neutral presentation values for type names, enum identities, symbols, parameters, defaults, callable signatures, provenance, and documentation blocks.
- [x] Implement one renderer for hover, completion detail/documentation, and signature help to share.
- [x] Keep presentation text stable and compact. Preserve source ranges separately from rendered strings.
- [x] Render extern/built-in provenance clearly without claiming unavailable source documentation.

### 2. Completion

- [x] Implement `textDocument/completion` using parser context first, then semantic visible-symbol/expected-type queries.
- [x] Support top-level declarations and keywords.
- [x] Support project-observed region data keys and language-defined section names, excluding entries already present in the body.
- [x] Support visible parameters, defines, extern defines, Boolean literals, and core expression keywords.
- [x] Support the region-only `here` expression keyword where valid.
- [x] Support declared region, event, and location expression values using semantic domain types and expected-type filtering.
- [x] Complete event and location entry labels from previously declared values of the matching kind, excluding entries already contributed to the active region.
- [x] Complete exit labels from declared and recovered regions, excluding the active region and targets already contributed to it.
- [x] Support built-in and user enum types in type positions.
- [x] Support explicit enum members after `.` for the resolved enum type only; do not offer extern wildcard patterns as concrete members.
- [x] Support named argument labels from resolved callable parameters, excluding parameters already bound positionally or by name.
- [x] Rank candidates by syntactic context, expected type, enum identity, scope proximity, and typed prefix.
- [x] Use the SourceText replacement range only for the active partial token; never derive candidate identity lexically.
- [x] Provide snippets only where inserted syntax is unambiguous and clients advertise snippet support; retain plain-text fallbacks and configurable client/server multiline indentation.

### 3. Signature Help

- [ ] Implement `textDocument/signatureHelp` from `callAt` query results.
- [ ] Calculate active parameter from parsed argument ranges, supporting positional and named arguments.
- [ ] Display parameter types, enum identities, defaults, optionality, and return types.
- [ ] Show no fabricated signature for unresolved calls.
- [ ] Provide known signatures with conservative active-argument behavior for recoverable incomplete calls.

### 4. Hover

- [ ] Implement `textDocument/hover` from symbol/type/occurrence queries.
- [ ] Support declarations, parameter uses, and calls.
- [ ] Support enum types/members and member expressions.
- [ ] Support modeled region/section entries and typed expressions.
- [ ] Show signature/type, enum identity, defaults, declaration provenance/location, and synthesized explanatory text.
- [ ] Never show stale snapshot data for a current unsaved version.

### 5. Documentation Model

- [ ] Synthesize initial documentation from declarations, signatures, types, defaults, and provenance.
- [ ] Design `##` documentation immediately preceding a declaration/member.
- [ ] Preserve `##` documentation text and range in the grammar/builder.
- [ ] Store documentation on documentable AST declarations/members.
- [ ] Emit documentation Markdown through the shared renderer.
- [ ] Do not reinterpret existing `#` comments as API docs.

### Tests

- [x] Declared `Region`, `Event`, and `Location` value typing, same-named host-enum fallback compatibility, semantic indexing, transpiler output, and completion filtering.
- [x] Shared presentation rendering for types, enum identities, defaults, documentation, provenance, and source-range separation.
- [x] Top-level, region-body, type-position, and expression completion contexts with expected-type/enum filtering.
- [x] Qualified versus ambiguous enum completion, including cross-file declarations and unknown qualifiers.
- [x] Scoped parameter completion.
- [x] Cross-file declaration completion for regions, events, locations, defines, enums, and enum members.
- [x] Cross-file and malformed same-file event/location entry-label completion, kind filtering, snippets, blank labels, comment-aware recovery, and canonical-region duplicate suppression.
- [x] Cross-file and malformed same-file exit-label completion, snippets, blank labels, comment-aware region recovery, self suppression, and canonical-region duplicate suppression.
- [x] Partial token replacement.
- [x] Incomplete and parsed call completion for named argument labels.
- [x] Expected-value completion for incomplete positional and named calls using resolved parameter type and enum identity.
- [x] Named argument binding and nested-call isolation.
- [ ] Defaults in completion/signature presentation from compiler query metadata.
- [ ] Hover/signature rendering for user and extern declarations.
- [x] Malformed top-level/region-body/member-access/call source and stale snapshot completion behavior.
- [x] Supported and unsupported completion snippet capability behavior.

### Definition of Done

- [ ] Suggestions and information are context-aware and semantically resolved.
- [ ] Authoring features are safe under incomplete source.
- [ ] Hover, completion, and signature help share one renderer instead of endpoint-specific formatting logic.
