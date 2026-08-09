## Detailed Plan: Semantic Highlighting

### Goal

Overlay semantic distinctions unavailable to TextMate or Tree-sitter while preserving those lexical grammars as immediate fallbacks.

### Dependencies and Boundary

Consume compiler occurrence/symbol records from [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md) and server capability/routing services from [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md). This plan does not own lexical syntax highlighting or re-tokenize documents.

### Token Design

1. Define a small standard LSP semantic token legend:
   - Function for defines/extern defines where appropriate.
   - Parameter for parameters.
   - Enum and enumMember for enum types/members.
   - Property/variable only where an RLS source category maps honestly.
2. Define modifiers only when semantically true: declaration, definition, readonly, defaultLibrary, deprecated.
3. Map every semantic token to existing TextMate fallback behavior and avoid custom token types that common clients/themes will ignore.
4. Explicitly decide treatment for regions, extension targets, entries, region data keys, and unresolved identifiers. Prefer omitting uncertain tokens over misleading classification.

### Implementation

1. Implement `textDocument/semanticTokens/full` from current-snapshot occurrence records and `Name` spans.
2. Classify declarations and references consistently, including parameters, calls, enum/member expressions, extern/default-library symbols, and source-level entries where the model supports them.
3. Sort, validate non-overlap, and delta-encode tokens centrally. Convert source ranges using the shared position converter.
4. Respect client legend/capabilities and snapshot/document generations.
5. Do not scan document text or use identifier-prefix rules in the endpoint.
6. Start with full-document results. Defer range and delta requests until profiling demonstrates a need.

### Tests

- Encoded stream snapshots for representative files.
- Declaration/reference modifier correctness.
- Enum/member, parameter, call, extern, unresolved, and ambiguous cases.
- Multi-byte/UTF-16 source positions.
- Empty/malformed files and stale snapshot suppression.
- Manual inspection with at least one light and dark standard theme in a semantic-token-capable client.

### Definition of Done

Semantic tokens are derived solely from compiler meaning, are valid for the negotiated encoding, and enhance rather than replace lexical highlighting.
