## Detailed Plan: Tolerant Editor Parser

### Goal

Produce trustworthy partial syntax indexes for incomplete editor text from the compiler parser itself, then remove the grammar-like recovery scanner from `SourceIndex`.

### Boundary

- The strict parser remains the compiler, CLI, and transpiler contract.
- Editor parsing may preserve partial syntax structure and diagnostics, but it must not invent semantic declarations or resolutions.
- `SourceIndex` stores parser-produced complete and recovered value contexts. It must not independently reconstruct RLS grammar.
- Sema analyzes complete AST nodes only and returns unknown when syntax is not trustworthy.

### 1. Parse Mode Contract

- [x] Add explicit `ParseMode::Strict` and `ParseMode::Editor` APIs.
- [x] Keep strict parsing as the default for compiler-facing entry points.
- [x] Route `AnalysisSnapshot` editor overlays through editor mode.
- [x] Prove strict/editor AST, diagnostics, spans, and source-index parity for valid source.

### 2. Recovery Representation

- [x] Use one mode-aware grammar for strict parsing and editor recovery.
- [x] Define parser-owned missing/error syntax records with spans and recovery status.
- [x] Distinguish complete AST declarations from recovered syntax contexts.
- [ ] Preserve comments, strings, and delimiters sufficiently to synchronize without lexical false positives.
- [ ] Define synchronization points for declarations, regions, sections, parameter lists, calls, and expressions.

### 3. Region And Section Recovery

- [ ] Recover incomplete base/extension region boundaries and names.
- [ ] Recover section boundaries, section kinds, entry labels, and region data keys.
- [ ] Preserve active-section and existing-entry queries used by completion.
- [ ] Move `regionContextAt`, `sectionEntryAt`, `sectionEntryNames`, and `regionNames` construction out of the recovery scanner.

### 4. Expression Recovery

- [x] Recover incomplete member access qualifiers and member spans.
- [x] Recover call boundaries, nested argument slots, labels, and active value spans.
- [x] Recover parameter and extern return type positions.
- [x] Recover enum declaration names needed by incomplete same-file type completion.
- [x] Move member/call/type contexts out of the recovery scanner.

### 5. Semantic Degradation

- [ ] Analyze unaffected complete declarations when neighboring syntax is malformed.
- [ ] Exclude recovered declarations from public semantic symbols until complete.
- [ ] Resolve recovered calls only when callee and argument structure are trustworthy.
- [ ] Never reuse stale semantic meaning or derive candidate identity from partial text.

### 6. Scanner Removal

- [ ] Delete grammar reconstruction from `source_index.cpp`.
- [ ] Retain only genuinely lexical helpers such as active-token replacement ranges.
- [ ] Verify every editor recovery query is parser-produced.

### Tests

- [ ] Valid-source strict/editor parity across representative syntax and all examples.
- [ ] Recovery tests at every synchronization boundary and nested malformed construct.
- [ ] Comment/string false-positive tests.
- [ ] Existing completion, navigation, diagnostics, and stale-generation tests remain green during migration.
- [ ] Full build and cross-platform process smoke tests.

### Definition Of Done

- [ ] The compiler owns strict and tolerant syntax parsing through one grammar.
- [ ] `SourceIndex` contains no second parser or grammar-shaped token scanner.
- [ ] Editor features remain responsive under incomplete source without fabricated or stale semantics.