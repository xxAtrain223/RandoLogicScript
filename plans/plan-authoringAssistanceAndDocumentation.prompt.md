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

- [ ] Implement `textDocument/completion` using parser context first, then semantic visible-symbol/expected-type queries.
- [ ] Support top-level declarations and keywords.
- [ ] Support region body keys and section names.
- [ ] Support expression symbols, literals, and keywords valid at the cursor.
- [ ] Support built-in and user enum types in type positions.
- [ ] Support enum members after `.` for the resolved enum type only.
- [ ] Support named argument labels from resolved callable parameters.
- [ ] Rank candidates by syntactic context, expected type, enum identity, scope proximity, and typed prefix.
- [ ] Use the SourceText replacement range only for the active partial token; never derive candidate identity lexically.
- [ ] Provide snippets only where inserted syntax is unambiguous and clients advertise snippet support.

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

- [x] Shared presentation rendering for types, enum identities, defaults, documentation, provenance, and source-range separation.
- [ ] Completion contexts and expected-type/enum filtering.
- [ ] Qualified versus ambiguous enum completion.
- [ ] Scoped parameters and cross-file declarations.
- [ ] Partial token replacement and incomplete calls.
- [ ] Named arguments, defaults, and nested calls.
- [ ] Hover/signature rendering for user and extern declarations.
- [ ] Malformed source, stale snapshots, and unsupported client capabilities.

### Definition of Done

- [ ] Suggestions and information are context-aware and semantically resolved.
- [ ] Authoring features are safe under incomplete source.
- [ ] Hover, completion, and signature help share one renderer instead of endpoint-specific formatting logic.
