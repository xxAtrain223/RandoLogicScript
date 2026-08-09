## Detailed Plan: Completion, Signatures, Hover, and Documentation

### Goal

Offer context-aware suggestions, callable signatures, inferred type information, and concise documentation while source is being edited.

### Dependencies and Boundary

Consume parser context, semantic scope/type/call queries from [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) and route through [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md). This plan does not define source indexing, scope calculation, or generic LSP request infrastructure.

### 1. Shared Presentation Model

1. Define compiler-neutral presentation values for type names, enum identities, symbols, parameters, defaults, callable signatures, provenance, and documentation blocks.
2. Implement one renderer used by hover, completion detail/documentation, and signature help.
3. Keep presentation text stable and compact. Preserve source ranges separately from rendered strings.
4. Render extern/built-in provenance clearly without claiming unavailable source documentation.

### 2. Completion

1. Implement `textDocument/completion` using parser context first, then semantic visible-symbol/expected-type queries.
2. Support contexts:
   - Top-level declarations/keywords.
   - Region body keys and section names.
   - Expressions: visible parameters, defines, extern defines, regions/entries where valid, literals/keywords.
   - Type positions: built-in and user enum types.
   - Member access after `.`: members of the resolved enum type only.
   - Named argument labels from resolved callable parameters.
3. Rank candidates by syntactic context, expected type, enum identity, scope proximity, and typed prefix. Do not return every global name as an undifferentiated list.
4. Use the SourceText replacement range only for the active partial token; never derive candidate identity lexically.
5. Provide snippets only where inserted syntax is unambiguous and clients advertise snippet support.

### 3. Signature Help

1. Implement `textDocument/signatureHelp` from `callAt` query results.
2. Calculate active parameter from parsed argument ranges, supporting positional and named arguments.
3. Display parameter types, enum identities, defaults, optionality, and return types.
4. When a call is unresolved, show no fabricated signature. When syntax recovery identifies a known callee but incomplete arguments, provide the known signature with conservative active-argument behavior.

### 4. Hover

1. Implement `textDocument/hover` from symbol/type/occurrence queries.
2. Support declarations, parameter uses, calls, enum types/members, region/section entries where modeled, member expressions, and typed expressions.
3. Show signature/type, enum identity, defaults, declaration provenance/location, and synthesized explanatory text.
4. Never show stale snapshot data for a current unsaved version.

### 5. Documentation Model

1. Initial release documentation is synthesized from declarations, signatures, types, defaults, and provenance.
2. Design a later language feature for `##` immediately preceding a declaration/member:
   - Grammar/builder preserves documentation text and range.
   - AST stores it on documentable declarations/members.
   - Renderer emits Markdown for hover, completion, and signatures.
3. Do not reinterpret existing `#` comments as API docs.

### Tests

- Completion contexts and expected-type/enum filtering.
- Qualified versus ambiguous enum completion.
- Scoped parameters and cross-file declarations.
- Partial token replacement, incomplete calls, named arguments, defaults, and nested calls.
- Hover/signature rendering for user and extern declarations.
- Malformed source, stale snapshots, and unsupported client capabilities.

### Definition of Done

Suggestions and information are context-aware, semantically resolved, safe under incomplete source, and share one renderer instead of endpoint-specific formatting logic.
