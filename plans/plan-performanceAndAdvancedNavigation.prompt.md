## Detailed Plan: Performance and Advanced Navigation

### Goal

Measure real editor workloads, improve responsiveness only where evidence requires it, and add advanced features that match RLS's actual semantic model.

### Dependencies and Boundary

This plan begins after the foundational LSP and core feature plans ship. It consumes their measurements and query APIs. It does not preemptively replace PEGTL, Tree-sitter, scheduling, or query models.

### 1. Instrumentation and Budgets

1. Record project discovery, source loading, overlay preparation, parsing, sema, index construction, snapshot publication, semantic-token generation, and request durations.
2. Record project file count, source bytes, memory use, cancellation count, debounce collapses, and stale-result discards.
3. Define measured responsiveness budgets for startup, first diagnostics, edit-to-diagnostics, and common query latency.
4. Log aggregate timings without source content by default.

### 2. Evidence-Driven Optimization

1. Profile representative small, typical, and large RLS projects before selecting changes.
2. If parsing dominates, consider per-file parse caches keyed by source content/version.
3. If sema dominates, map actual dependencies and add dependency-aware invalidation only when correctness rules are explicit.
4. If token payloads dominate, add semantic-token range/delta support.
5. If request latency dominates, optimize query indexes or scheduling before adding concurrency complexity.
6. Preserve immutable snapshot semantics and stale-result safety through every optimization.

### 3. Advanced Navigation

1. Add call hierarchy only from resolved callable edges:
   - Prepare hierarchy from concrete user/extern callable declarations.
   - Incoming calls from reference/call indexes.
   - Outgoing calls from resolved calls inside a callable body.
2. Consider code lenses only for meaningful counts such as reference count; make them opt-in if visual density is undesirable.
3. Do not implement type hierarchy because RLS has no inheritance/subtyping relationship to expose.
4. Evaluate inlay hints only after authoring feedback identifies a concrete need, such as named-argument or inferred-enum clarification.

### 4. Project Configuration Evolution

1. Evolve `rls.json` only from demonstrated requirements: external host libraries, target-specific configuration, source generators, or advanced root layout.
2. Version schema changes and preserve migration/compatibility behavior.
3. Avoid adding a manifest field merely to mirror internal implementation details.

### Tests and Release Gates

- Benchmark/trace fixtures for representative project sizes.
- Regression budgets for edit-to-diagnostic and query latency.
- Cancellation and stale-snapshot correctness under load.
- Call-hierarchy correctness for recursion, externs, unresolved calls, and cross-file calls.
- Cross-platform profiling smoke tests.

### Definition of Done

Every performance change is justified by measurements, preserves snapshot/query correctness, and advanced navigation reflects actual RLS semantics rather than generic protocol checkboxes.
