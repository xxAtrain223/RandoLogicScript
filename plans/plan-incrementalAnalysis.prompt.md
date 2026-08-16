## Detailed Plan: Incremental Analysis

### Goal

Reduce edit-to-diagnostics and interactive query latency by reusing verified work from unchanged source files while preserving immutable, exact-generation `AnalysisSnapshot` semantics.

### Dependencies and Boundary

This plan builds on [plan-compilerQueryModelAndDiagnosticLsp.prompt.md](plan-compilerQueryModelAndDiagnosticLsp.prompt.md), [plan-explicitFeatureOrientedLsp.prompt.md](plan-explicitFeatureOrientedLsp.prompt.md), and [plan-tolerantEditorParser.prompt.md](plan-tolerantEditorParser.prompt.md). Measurement and prioritization remain owned by [plan-performanceAndAdvancedNavigation.prompt.md](plan-performanceAndAdvancedNavigation.prompt.md).

- Do not answer semantic requests from mixed source generations.
- Do not reuse AST pointers, snapshot-local `SymbolId` values, diagnostics, or resolved meaning across snapshots without an explicit stable value representation.
- Preserve cancellation, stale-result rejection, overlay precedence, and immutable published snapshots.
- Start conservatively: project-wide semantic invalidation is acceptable until dependency correctness is proven.

### 1. Measurement And Cache Contract

- [ ] Measure per-file parse/index time, project semantic time, cache lookup time, hit rate, invalidation breadth, and snapshot assembly time.
- [ ] Define cache keys from canonical file identity, exact source content/version, parser mode, and relevant compiler/configuration version.
- [ ] Define ownership and memory limits for cached source text, ASTs, parser indexes, diagnostics, and exported declaration summaries.
- [ ] Make cache eviction unable to invalidate an already published snapshot.

### 2. Per-File Syntax Reuse

- [ ] Extract a reusable immutable parsed-file product containing source text, complete AST declarations, parser diagnostics, and `SourceIndex` data.
- [ ] Reuse parsed-file products only for byte-identical source and matching parse/configuration inputs.
- [ ] Reparse only changed files while retaining unchanged parsed products.
- [ ] Assemble a new project AST and document indexes without mutating cached products.
- [ ] Preserve strict/editor parsing behavior and tolerant recovery records exactly.

### 3. Semantic Dependency Model

- [ ] Define stable value summaries for exported regions, extensions, defines, extern defines, enums, members, patterns, and callable signatures.
- [ ] Record dependencies from type references, identifier/member resolution, calls, region extensions, section entries, extern wildcard observations, and validation rules.
- [ ] Distinguish local-body changes from exported declaration/signature changes.
- [ ] Begin with conservative project-wide semantic invalidation when any exported summary changes.
- [ ] Narrow invalidation only after tests prove transitive dependency closure and diagnostic equivalence.

### 4. Incremental Semantic Products

- [ ] Separate reusable semantic facts from snapshot-local pointer and `SymbolId` identity.
- [ ] Recompute affected type resolution, call binding, validation, occurrences, expected types, and semantic tokens from dependency-aware inputs.
- [ ] Rebuild snapshot-local IDs deterministically for every published snapshot.
- [ ] Preserve diagnostics and related locations for unchanged files without retaining stale cross-file meaning.
- [ ] Produce output equivalent to a clean whole-project analysis for the same source set.

### 5. Scheduling And Interactive Queries

- [ ] Keep one monotonic project generation and exact source-set capture per request.
- [ ] Let `awaitSnapshot` expedite pending work without publishing partial or mixed-generation snapshots.
- [ ] Cancel superseded incremental work and discard results whose dependency inputs changed.
- [ ] Measure completion, signature help, hover, navigation, diagnostics, and semantic-token latency separately.
- [ ] Consider document-local syntax-only fast paths only for queries that require no semantic identity.

### First Implementation Slice

- [ ] Add instrumentation that separates parsing, sema, indexing, and publication time.
- [ ] Introduce a bounded per-file parse cache keyed by exact source content and parser mode.
- [ ] Reuse unchanged parsed files but continue running whole-project sema.
- [ ] Prove byte-for-byte diagnostic and query equivalence against cache-disabled analysis.
- [ ] Measure comma-triggered signature-help and completion latency before and after the cache.

### Tests And Release Gates

- [ ] Cache hit/miss, eviction, cancellation, and concurrent project tests.
- [ ] Changed-file, added-file, removed-file, renamed-file, overlay-open/close, and manifest-change tests.
- [ ] Cross-file dependency tests for calls, enum types/members, wildcard observations, regions/extensions, and section entries.
- [ ] Malformed editor source and recovery equivalence tests.
- [ ] Cached versus clean-analysis differential tests over all repository examples.
- [ ] Stale-generation suppression under rapid edits and worker contention.
- [ ] Memory and latency budgets on representative small, typical, and large projects.
- [ ] Windows, Linux, and macOS process smoke coverage.

### Definition Of Done

- [ ] Incremental and clean whole-project analysis produce equivalent diagnostics and query results.
- [ ] Interactive requests never observe mixed generations or stale semantic identities.
- [ ] Measured edit-to-query latency improves for unchanged-heavy project edits without unacceptable memory growth.
- [ ] The implementation can fall back to clean analysis when cache or dependency invariants are uncertain.
