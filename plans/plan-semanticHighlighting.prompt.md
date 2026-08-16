## Detailed Plan: Semantic Highlighting

### Goal

Overlay semantic distinctions unavailable to TextMate or Tree-sitter while preserving those lexical grammars as immediate fallbacks.

### Dependencies and Boundary

Consume compiler occurrence/symbol records from [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) and server capability/routing services from [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md). This plan does not own lexical syntax highlighting or re-tokenize documents.

### Token Design

- [x] Define a small standard LSP semantic token legend:
   - Function for defines/extern defines where appropriate.
   - Parameter for parameters.
   - Enum and enumMember for enum types/members.
   - Property/variable only where an RLS source category maps honestly.
- [x] Define modifiers only when semantically true: declaration, definition, readonly, defaultLibrary, deprecated.
- [x] Map every semantic token selector, including modifier-specific cases, to the existing RLS TextMate scopes and avoid custom token types that common clients/themes will ignore.
- [x] Emit functions, parameters, enums, and enum members where lexical fallback scopes are unambiguous; omit regions, extension targets, section entries, region data keys, unresolved/ambiguous names, and wildcard patterns rather than applying unstable or misleading classifications.

### Implementation

- [x] Implement `textDocument/semanticTokens/full` from current-snapshot occurrence records and `Name` spans.
- [x] Classify declarations and references consistently, including parameters, calls, enum/member expressions, extern/default-library symbols, and source-level entries where the model supports them.
- [x] Sort, validate non-overlap, and delta-encode tokens centrally. Convert source ranges using the shared position converter.
- [x] Advertise a standard legend and reject stale snapshot/document generations.
- [x] Do not scan document text or use identifier-prefix rules in the endpoint.
- [x] Start with full-document results. Defer range and delta requests until profiling demonstrates a need.

### Tests

- [x] Encoded stream snapshots for representative files.
- [x] Declaration/reference modifier correctness.
- [x] Enum/member, parameter, call, extern, unresolved, and ambiguous cases.
- [x] Multi-byte/UTF-16 source positions.
- [x] Empty/malformed files and stale snapshot suppression.
- [ ] Manual inspection with at least one light and dark standard theme in a semantic-token-capable client.

### Definition of Done

- [x] Semantic tokens are derived solely from compiler meaning and use valid UTF-16 delta encoding.
- [x] Semantic tokens enhance standard TextMate/Tree-sitter fallback scopes rather than replacing lexical highlighting.
