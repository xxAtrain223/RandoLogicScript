# Logic-tracker transpiler implementation plan

## Problem and approach

Implement the full [logic-tracker requirements](LOGIC_TRACKER_TRANSPILER_REQUIREMENTS.md) as a separate, opt-in `soh_tracker` backend. Lower the existing semantically analyzed RLS project into a tracker-specific descriptor graph, validate that lowered graph against the consumer ABI, then emit deterministic C++20 factories, lazy lookup, typed host adapters, and the exact consumer root map. The internal representation (IR) is only for backend lowering, not another parser, AST, or semantic-analysis layer.

Use the existing [SoH backend](../transpilers/soh) as prior art for analyzed arguments, host enum mappings, types, lexical `here`, and region composition. Do not generate tracker structure by parsing ordinary generated C++.

This plan covers the full document in implementation phases and has been approved. Implementation proceeds only in user-requested, review-sized checkpoints, not automatically through the full plan.

## Working and handoff instructions

### Review-sized execution

- Keep one central plan. A fresh chat per phase is appropriate, but a phase may contain several smaller checkpoints.
- At the start of a chat, read this plan and its current handoff, then inspect the relevant code and worktree changes. Use the requirements for the specific checkpoint, including section 16 when dealing with the consumer ABI. Do not assume previous chat context or session-local SQL todos are available.
- Explain the checkpoint's purpose, implement only its focused scope, run the relevant checks, and summarize the result. Stop for user review before starting another checkpoint, even within the same phase.
- Distinguish implemented, verified, and user-reviewed work. Do not infer review approval from passing tests or from this plan's approval.
- Update the current handoff before ending a checkpoint or switching chats. Record where work actually stopped, including unfinished edits or failures. Read-only discussion does not require a new handoff entry.

### Handoff hygiene

- Maintain exactly one **Current handoff** below as a replace-in-place snapshot, not an append-only diary. Keep it to roughly 40 lines or fewer; compress stale details before adding new ones.
- Retain only information needed to resume safely: completed phases/checkpoints, current scope, relevant files/worktree state, review status, validation evidence, blockers, and the exact next step.
- Keep architectural decisions in the relevant plan section. Reference them from the handoff rather than duplicating them; update their source section when the user approves a change.
- Record exact focused validation commands and concise outcomes, including failures and checks not run. Do not paste full logs, diffs, generated code, tool transcripts, or chat summaries.
- Use repository-relative links. Avoid machine-specific absolute paths, timestamps, environment inventories, exhaustive file lists, and copied session todo databases.
- Link to existing code, tests, issues, or commits for detail. Do not create per-chat/per-checkpoint reports or new tracking files unless explicitly requested or required as a planned deliverable.
- Remove resolved blockers and superseded next steps. Compress completed work to a short phase/checkpoint summary; use version control for history rather than preserving old handoffs.
- Preserve unresolved decisions, unreviewed changes, and failing checks until resolved. Brevity must not hide correctness risks or imply that pending downstream tests passed.
- Do not expand this document with speculative features or repeat the full requirements matrix. Add durable detail only when it changes implementation, verification, or integration.

### Current handoff

- **Review baseline:** Phases 1-7a were user-approved; checkpoint 7b added independent SoH/AP/tracker golden acceptance checks and explicitly configured tracker in the sample manifest. Its cross-backend ordering/nonmutation test was removed at user request.
- **Current checkpoint:** Structural expression sharing and emitted-C++ cleanup. [NumberDistinctGraph](../transpilers/soh_tracker/src/tracker_graph.cpp) canonicalizes immutable descriptors after occurrence-specific validation, without mutating the graph or caching results. [Emission](../transpilers/soh_tracker/src/tracker_emission.cpp) remaps every child/root ID and checks paths for all reachable occurrences, including aliases. Emitted helpers remain `static` at global scope; generated host calls are unqualified, and the detail namespace is removed. Integer/index/parameter literals use plain representable values, and zero-argument adapters omit the unused span name. Parameter positions remain checked against 0–255.
- **Acceptance artifacts:** The [sample manifest](../examples/soh/rls.json) configures `soh_tracker` alongside SoH/AP. Its two [golden files](../examples/soh/out_soh_tracker) now contain **574 nodes / 304 roots**, down from 1,927 nodes, with no repeated factory constructors. Default invocation runs all three configured targets; the CLI never enables tracker implicitly.
- **Generation/documentation:** `.\build\console\Debug\RandoLogicScript.exe --project .\examples\soh\rls.json -t soh_tracker` regenerates the baselines. Existing sample unresolved-exit warnings and one unused-helper info diagnostic are not errors. Detailed tracker usage/regeneration prose remains intentionally absent from the README/building guide.
- **Validation:** The tracker test project and console generator built successfully with the installed Visual Studio MSBuild; `build\transpilers\soh_tracker\Debug\soh_tracker_tests.exe` passed **151/151**. The sample tracker output was regenerated; `git diff --check` passed, and the generated source contains no detail namespace or qualified host calls. The earlier **177/177** focused tracker/console/acceptance CTest run also passed. Full repository/sema suites, other platforms, and real-consumer checks remain unrun.
- **Worktree/review:** Changes are uncommitted and await review. The worktree includes tracker numbering/emission, related tests, the tracker C++ golden, and this plan. Console dispatch/writer, sema, schema, user-provided requirements, and CI workflows are unchanged.
- **Publication boundary:** Phase 7a's checked console writer remains caller-owned; `OutputWriter` stays open-only. Tracker compiler diagnostics precede artifact opens, but generation/output exceptions can leave partial files. No transactional publication or crash durability is claimed. Unit-injected write/flush/close failures do not certify real disk exhaustion; cross-filesystem-root rejection and real disk flush/close failure CLI status remain untested end-to-end on this Windows checkout.
- **Remaining local work:** Phase 8's broader acceptance matrix/corpus, root-set accounting, and numeric/descriptor limits remain separate. The 7b corpus counts and goldens are not exhaustive semantic coverage.
- **Pending consumer gates:** Generated source still needs real C++20 consumer headers and matching SoH `functions.gen.h`. Compilation/API compatibility, host ranges/return widths, ODR, lazy cache/runtime behavior, callback lifetimes, and host-enforced nonmutation remain downstream. Recursive runtime execution remains deferred; synthetic emission does not enable recursive RLS in sema.
- **Next action:** Stop for sharing review. After approval, choose the next review-sized Phase 8 checkpoint; do not jump to consumer certification.

## Confirmed decisions and boundaries

- Add only the `soh_tracker` CLI target, not a second tracker mode on `soh`.
- Reuse the parser, existing AST, and sema as the sole source-language frontend. The tracker IR only records target-node choices, ordered child references, adapters, representations/conversions, root records, and provenance needed for emission; it must not duplicate parsing, symbol resolution, type inference, or source argument binding.
- Use section 11's exact consumer-provided `LogicRootKind`, `LogicRootKey`, `LogicRootKeyHash`, and `LogicRootMap` declarations, with generated `const LogicRootMap& GetLogicRoots()`. Do not introduce a competing registry, generated access-object side table, or string-based lookup.
- Keep existing SoH gameplay emission unchanged. Actual access-object association with these root keys is a separate, documented Shipwright task.
- Emit only `logic_tracker.gen.h` and `logic_tracker.gen.cpp`; do not emit a debug manifest/listing. The requirements' optional debug manifest is intentionally not implemented. Provenance remains internal for diagnostics and deterministic ordering.
- Reuse the matching SoH-generated `functions.gen.h` for local RLS enum declarations rather than redeclaring them. Document generation from the same project and the required include paths. Tracker graphs must not call ordinary generated helper functions as opaque leaves.
- Flatten homogeneous `and` and `or` chains, and homogeneous left-associated `+`, `*`, `-`, and `/` chains into n-ary nodes while respecting semantic boundaries. Dynamic division does not require a constant or provably nonthrowing prefix: the runtime performs each prefix operation before evaluating the next child.
- Keep the tracker architecture recursion-ready without introducing a tracker-specific recursion ban or requiring an acyclic helper graph. Enabling recursive RLS programs in sema and supporting their execution/row expansion in the consumer are deferred, coordinated work.
- Deliver entirely within the RLS checkout. No external checkout, production game binary, new evaluator, tracker UI rewrite, Python/AP changes, language expansion, new dependency, or compiler-version change is required.
- Keep structural, diagnostic, mapping, and generated-artifact tests in RLS. Do not duplicate consumer declarations or add local generated-output compile checks. Compilation/API compatibility, runtime semantics, full production-corpus validation, and game integration are downstream gates against the actual consumer.
- Leave the user-provided requirements document and any unrelated user-owned changes intact.

## Findings from the current checkout

- [SohTranspiler](../transpilers/soh/include/soh.h) accepts a const analyzed project; [its orchestration](../transpilers/soh/src/soh.cpp) validates before opening outputs. Follow that pattern.
- [Project metadata](../ast/include/ast.h) already supplies typed expressions/parameters, enum identity, resolved call arguments, declaration maps, extensions, function references, and resolved `here`.
- [Declaration validation](../sema/src/validate_declarations.cpp) rejects duplicate entry keys within the same owner and section kind across the base region and all extensions. Valid generated roots therefore use `Occurrence = 0`; retain diagnostics instead of overwriting duplicates or inventing ordinals.
- [Type resolution](../sema/src/resolve_types.cpp) distinguishes callable-parameter calls, allows bare references only to zero-parameter Bool functions, Boolean-normalizes mixed Bool/Int conditional results, and currently rejects ordinary define-call cycles. That existing sema limitation remains unchanged in this work; it is not a new tracker policy. Tracker graph walks must terminate on cyclic references and validate every descriptor edge without assuming a DAG.
- Enum-valued matches retain their result enum identity in sema, including nested results, and reject conflicting result enum identities. Tracker match targets and enum facts come from that metadata, not a backend arm-inspection inference pass.
- [The shared host enum mapping table](../transpilers/shared/include/soh/enum_mappings.h) includes distinct C++ type/value namespaces; `Setting` deliberately has no value namespace. Both SoH backends use the same header through a private interface-target dependency.
- [Ordinary region emission](../transpilers/soh/src/generate_regions.cpp) composes core sections with all extensions, but also validates gameplay-specific region metadata. Tracker root collection must not unnecessarily require metadata such as `name` or `scene`.
- [CLI dispatch/help](../console/main.cpp) currently recognizes only `soh` and `soh_ap`. [The project schema](../project/rls.schema.json) intentionally permits arbitrary transpiler names; installed target validation belongs to the CLI. Preserve that extensibility.
- Existing infrastructure provides C++20, GoogleTest, CTest, in-memory output writers, CLI tests, golden-output acceptance tests, and cross-platform CI. No new test framework is needed.
- The local [SoH examples](../examples/soh/src) already contain helpers, wallet ternaries, Spirit Shared, vanilla/MQ regions, and separate shuffle extensions.

## Architecture boundary: lowering only

```text
Existing parser -> existing AST/Project -> existing sema
    -> tracker lowering descriptors -> target-graph/ABI validation -> C++ emission
```

| Layer | Responsibility |
| --- | --- |
| Existing parser and AST | Source grammar, syntax nodes, declarations, and source locations. No new parser or parallel source AST is introduced. |
| Existing sema | Identifier/callable classification, declaration resolution, inferred types/enum identity, argument binding/defaults, lexical `here`, and language diagnostics. These remain authoritative. |
| Tracker lowering IR | A compact emission-oriented descriptor graph: consumer node kinds/payloads, child IDs, shared definition references, typed host-adapter references, target value representations/conversion nodes, roots, and provenance. |
| Tracker validation/emission | Descriptor completeness, target ABI representability, n-ary node invariants, semantic-boundary preservation, ID/parameter limits, output consistency, and generation. Not a second language type checker or evaluator. |

- Consume the const analyzed `Project` and its `getType`, `getEnumType`, `getResolvedCallArgs`, declaration/enum metadata, resolved identifier kinds, and `HereRef::resolvedRegion` directly.
- Carry references to existing declarations/source nodes and sema facts where useful; do not clone the language AST or create a second source-level symbol/type system. Internal maps from existing declarations to emitted IDs are lowering bookkeeping, not name resolution.
- Reuse resolved argument order/default expressions exactly. Do not rebind positional/named arguments or independently infer omitted defaults. Sema resolves defaults globally; lower them without the caller's parameter scope. Explicit arguments retain caller scope even when the callee reuses the same parameter names.
- Translate analyzed types to consumer-supported value representations and insert required conversion nodes. This is target lowering of established semantics, not type inference. Missing required semantic metadata produces a diagnostic rather than guessed replacement facts.
- Keep this graph private to `soh_tracker`, scoped to generation, and no more general than required for node sharing, flattening, validation, deterministic IDs, and C++ serialization. Do not mutate the input AST or add a cross-backend IR framework.
- Backend tests normally parse/analyze using the existing frontend. Synthetic graph tests exercise only lowering/representation invariants and do not establish a separate semantic pipeline.

### Deterministic source and graph ordering

- Supply an explicit absolute project source root to the private ordering/numbering utilities. Normalize paths lexically to project-relative UTF-8 names with generic separators; do not consult the filesystem or process working directory. Sources outside the root retain relative `..` components; paths on incompatible roots produce diagnostics instead of absolute-path fallbacks.
- Traverse the sorted source-file view by normalized path, retaining each file's declaration order and each declaration's section, entry, and argument order. Extensions follow that same file/declaration order. Do not sort or mutate the analyzed project's containers; duplicate normalized file paths are errors.
- Start final numbering from roots sorted by normalized source path and source positions, with symbolic root identity as the deterministic tie-breaker. Traverse descriptor children in payload order, call arguments before a helper body, match discriminants before arms, and arm patterns before bodies. Assign contiguous IDs on first visit using an iterative preorder walk; handles and input storage order are not emitted identities.
- Validate reference integrity across all stored descriptors, then number only root-reachable nodes. Preserve repeated child positions even when discovery visits a shared target once. Unreachable definitions receive no final IDs; cycles do not cause recursive expansion or a tracker-specific recursion diagnostic.
- These utilities operate on lowering descriptors, not source expressions. Phase 2 will connect the ordered source view to root collection/lowering; later emission must reuse this ordering and path normalization.

### Structural expression sharing

- Validate the original occurrence-specific graph before sharing. Retain its source spans, analyzed declarations, argument provenance, and root records unchanged; sharing must not hide an invalid occurrence or skip its source-path preflight.
- `NumberGraph` retains occurrence numbering for graph/lowering tests. Emission uses `NumberDistinctGraph`: build bottom-up structural equivalence classes, then assign contiguous IDs in the original deterministic root/child preorder. All reachable slots, including aliases, map to an emitted ID; only one representative per class receives a factory case.
- Keys include node/value kind, source type and enum identity, target representation, values/symbols, operators, ordered canonical children, argument positions/names, conversions, and match fall-through. Helper/adapter identity remains significant. Parameter reads use their runtime binding name and type, not the source parameter declaration address; identical typed names and argument wrappers may share across helpers because evaluation supplies the current scope.
- Definition identity already determines a helper body, so that implicit edge is excluded from structural keys. This keeps synthetic self/mutual helper references finite; it does not enable recursive RLS in sema or certify recursive consumer execution. Ordinary structural cycles produce a preflight diagnostic.
- Share immutable structure only. Preserve every root identity and every child position, including repeated IDs; do not merge evaluation traces, memoize game-state results, reorder operands, reassociate arithmetic, or equate merely logically equivalent expressions. Condition nodes remain factories for runtime captures, not stored captured values.
- Internal declaration identities and hash-table order never determine emitted numbering. Source storage/enumeration changes and checkout relocation must still yield byte-identical artifacts.

## Required flattening policy

Flattening is part of normal tracker lowering, not an optional optimization or a CLI flag. Use ordered n-ary child lists supported by the consumer's existing logical/arithmetic constructors.

- **`and` / `or`:** combine directly nested instances of the same operator on either side into one ordered node. Preserve left-to-right leaf evaluation, short-circuiting, and Boolean results. Do not cross a different operator, helper-call boundary, condition/invocation boundary, or conversion wrapper.
- **`+` / `*`:** collect homogeneous left-associated chains into a single left-fold node. This removes nested operator nodes without changing arithmetic grouping. Retain right-nested arithmetic subtrees: integer promotion, intermediate results, and overflow behavior make blind reassociation unsafe even for addition and multiplication.
- **`-` / `/`:** apply the same left-spine collection, including dynamic operands. The runtime's left-fold evaluator performs the prefix subtraction/division before evaluating the next child, matching the nested graph's evaluation and exception timing. No constant-prefix or nonthrowing-prefix restriction is needed. Never rewrite right-nested subtraction/division.
- **General caveat:** do not flatten across explicit conversion nodes, different operators, or other semantic boundaries. Arithmetic flattening preserves left association; it does not assume mathematical associativity.
- **Safety barriers:** retain any subtree whose removal would change intermediate type promotion/normalization, move a conversion, reorder or duplicate a host query, or cross helper-call, condition, or invocation boundaries. Preserve the runtime's interleaved operand-evaluation/operation sequence; an early division by zero must still throw before any later child is evaluated.
- **Representation:** collect legal operands during typed lowering, before final ID assignment, so eliminated intermediate operator nodes do not leave orphan descriptors. Keep source provenance internally; do not add runtime source fields.
- **Tracker presentation:** flattened chains intentionally expose their ordered operands as immediate children rather than retaining redundant same-operator rows. Preserve repeated operand occurrences and test the resulting child positions and skipped-child mapping. Arithmetic groups that remain nested must remain distinguishable in structural display.

Required structural examples:

| Input | Required graph shape |
| --- | --- |
| `(a and b) and (c and d)` | `and(a, b, c, d)` |
| `a or (b or c)` | `or(a, b, c)` |
| `a and (b or c)` | `and(a, or(b, c))` |
| `2 + 3 + 4` | `+(2, 3, 4)` |
| `2 * 3 * 4` | `*(2, 3, 4)` |
| `20 - 3 - 2` | `-(20, 3, 2)` |
| `24 / 3 / 4` | `/(24, 3, 4)` |
| `a / b / c` | `/(a, b, c)`; perform `a / b` before evaluating `c`, including when `b` is zero |
| `20 - (3 - 2)` | `-(20, -(3, 2))` |
| `24 / (3 / 4)` | `/(24, /(3, 4))`; preserve the zero-divisor outcome |
| `2 + (3 + 4)` | `+(2, +(3, 4))`; retain right-nested arithmetic grouping |
| `2 * (3 * 4)` | `*(2, *(3, 4))`; retain right-nested arithmetic grouping |
| `2 + 3 * 4` | `+(2, *(3, 4))` |
| `(2 + 3) * 4` | `*(+(2, 3), 4)` |

## Recursion readiness and deferred execution support

- This decision supersedes the requirements document's proposed tracker-level rejection of recursive dependencies. Do not add a blanket recursive-call diagnostic, a DAG-only IR invariant, or recursive inlining to this backend.
- Represent helper bodies once and refer to them by stable internal handles/IDs. Register a helper's handle before lowering its body, so self/mutual references can reuse it rather than expand indefinitely.
- Make reachability, dependency discovery, validation, numbering, emission, and any graph walk cycle-safe using visited/in-progress bookkeeping. A dependency cycle is not itself a tracker validation error; dangling references, incomplete definitions, and invalid types/arities remain errors.
- Retain dependency/provenance information for future diagnostics and support work, without imposing a recursion-depth cap or inventing termination behavior.
- Current source-level recursion still receives the existing sema diagnostic before tracker generation. Do not remove that diagnostic or claim recursive programs are accepted by the CLI in this implementation.
- The baseline consumer's recursively expanded rows and recursive evaluation are not certified for cyclic call graphs. Future recursion enablement must coordinate sema/type inference with consumer call-frame, trace ownership, evaluation, and expansion behavior before recursive graphs are used in the tracker.
- Add small synthetic IR self-reference/mutual-reference tests proving finite construction, shared definition handles, terminating graph walks, deterministic numbering/emission, and absence of a tracker-specific cycle rejection. These are architecture tests, not claims of recursive runtime execution.

## Proposed deliverables and code layout

### Backend

Add `transpilers/soh_tracker/` following the current library/test conventions:

- `include/soh_tracker.h`: project-based transpiler entry point and diagnostics.
- Internal tracker-lowering descriptors, graph/ABI validation, adapter generation, and C++ emission files under `src/`; keep responsibilities separate without a second frontend or general-purpose compiler framework.
- `tests/`: IR/diagnostic/output tests and standalone RLS source fixtures; no copied consumer interface or standalone enum mapping tests.
- `CMakeLists.txt`: backend library and compiler tests, without consumer compile-check targets.

Expose or extract the existing host enum mapping header to a shared include location. Keep its values and ordinary SoH behavior unchanged.

### Generated artifacts

| Artifact | Contract |
| --- | --- |
| `logic_tracker.gen.h` | Includes consumer `logic_expression.h`; declares generated entry points without redefining consumer types or defining globals in headers. |
| `logic_tracker.gen.cpp` | Exactly one global definition each of `GetExpression(index_t)`, `uint64_t GetExpressionCount()`, and `GetLogicRoots()`; includes per-ID factories, adapters, and literal-backed metadata. |

The generated implementation includes consumer `rls_host.h` and matching SoH-generated `functions.gen.h` unconditionally. State clearly which includes and link implementations the consumer must supply. Host code must enforce the nonmutating-extern contract; generated adapters do not manage host state.

### Integration, samples, and documentation

- Register the library in [transpilers/CMakeLists.txt](../transpilers/CMakeLists.txt).
- Wire the target through [console/main.cpp](../console/main.cpp), [console/CMakeLists.txt](../console/CMakeLists.txt), and the CLI/acceptance tests.
- Add checked-in sample tracker artifacts under `examples/soh/out_soh_tracker/` and small focused source fixtures for worked examples A-F and the embedded core/shops corpus.
- Update [README.md](../README.md) and add focused invocation/consumer-integration documentation under `docs/`.
- Keep the existing example manifest's default targets unchanged; document an explicit tracker invocation rather than silently adding tracker generation to existing workflows.
- Preserve the generic project schema; add tracker manifest coverage in project tests where necessary rather than introducing a closed list of target names.

## Implementation phases and todos

Tests accompany each phase. The acceptance phase consolidates the complete requirement matrix; it is not the first point at which testing occurs.

### 1. Establishing the backend contract and lowering IR

Todo ID: `tracker-foundation`

- Add the backend library skeleton and test conventions, without CLI exposure until generation works.
- Define typed node IDs/counts, node-kind-specific payloads, ordered n-ary logical/arithmetic child lists, references to sema-established RLS type/enum facts, helper/adapter references, roots, and internal source provenance. Keep C++ string rendering out of lowering; do not introduce a source type-inference pass.
- Represent graph references independently of traversal order, with one registered handle per helper definition and no DAG-only type/invariant. Use cycle-safe discovery and deterministic final ID assignment rather than recursive inlining.
- Share the existing enum mappings through a single header.
- Define deterministic traversal: normalized project-relative source paths, stable declaration/extension ordering, and source-order section/entry/argument traversal. Pass minimal source-root context to the backend if needed; do not mutate the analyzed AST or global project ordering.
- Use section 16 as the consumer contract reference without copying its declarations into local fixtures. Compile generated output against the real interface in the downstream project.
- Establish the target representation policy for already-analyzed types: ordinary RLS Int, enum, and key values use `int32_t`; Bool remains Bool. Support all seven consumer integral/Boolean conversion targets where applicable. This does not redefine or reinfer source-language types.

Exit criteria: the library and compiler tests compile; the descriptor graph stores target-lowering facts rather than a duplicate source AST/type system or untyped string-emission placeholders; original backend tests still build.

### 2. Lowering composed roots and ordinary expressions

Todo ID: `tracker-lowering`

Depends on: `tracker-foundation`.

- Collect every event/location/exit entry from every base region and extension, including unconditional entries and all variant/shuffle files. Do not filter by current randomizer settings.
- Build exact root identities from kind, symbolic owner key, symbolic entry key, and occurrence zero. Exit entries retain their original declared destination; events use their setter condition.
- Lower Boolean/integer leaves, enum members and declared keys, parameter reads, lexical `here`, logical operations, comparisons, arithmetic, ternaries, and matches to structural IR.
- Read expression/parameter types, enum identities, resolved identifiers, and lexical regions from existing sema metadata. Do not infer them again from expressions, names, prefixes, or emitted code.
- Apply the required flattening policy during lowering: flatten same-operator logical chains and homogeneous left-associated addition, multiplication, subtraction, and division, including dynamic operands. Preserve ordered short-circuit structure, mixed-operator and explicit-conversion boundaries, right-nested arithmetic, intermediate promotions, and the runtime's interleaved left-fold evaluation/exception timing.
- Use typed match targets with explicit pattern-list/default nodes and AST fallthrough flags. Do not add visible synthetic fallback arms.
- Insert required transparent conversions at roots, helper boundaries, conditional branches, and arguments; preserve numeric promotions and Boolean normalization.
- Omit deterministically unreachable helper declarations, but include every root and all dependencies. Ignore metadata-only String/List values; diagnose executable unsupported values.

Exit criteria: IR assertions cover all ordinary node shapes, the required flattened and deliberately unflattened shapes above, the eight shop roots, the five boss-room roots, shared check keys in distinct owners, extension `here`, and numeric examples D/E.

### 3. Lowering helpers, conditions, and invocation

Todo ID: `tracker-callables`

Depends on: `tracker-lowering`.

- Retrieve helper/extern declarations from the analyzed project's existing metadata and use `Project::getResolvedCallArgs` in declaration order, including every default. Do not implement a second resolver or argument binder.
- Give every call argument its own `ParameterExpression` wrapper, declaration name, and checked zero-based position. Preserve separate call/argument occurrences; share only immutable helper definitions initially.
- Lower helper calls to expandable body references; register each definition before lowering its body and reuse that handle for repeat/self/mutual references. Do not specialize away scope reads, infinitely inline definitions, or call ordinary gameplay helpers.
- Distinguish a callable-parameter `condition()` represented by `CallExpr` from a named helper call, and handle `InvokeExpr` for returned callable invocation.
- Wrap Bool-compatible expressions passed as Condition in lazy `ConditionExpression`; forward existing Condition values unchanged.
- Lower valid bare function references as lazy zero-argument call graphs, without broadening sema's restrictions or placing C++ function pointers into `value_t`.
- Preserve returned conditions and captured lexical parameters without capturing live game state. Use AST-level fixtures only for forms not expressible by the existing grammar.
- Reserve the runtime's `$match` binding and avoid generating global mutable scope variables.

Exit criteria: structural tests cover complete named/default arguments, Spirit Shared's seven wrappers and callable false defaults, pass-through, references, caller-scope arguments, distinct traces by construction, returned-condition invocation, and finite synthetic self/mutual helper-reference graphs.

### 4. Generating typed host adapters

Todo ID: `tracker-adapters`

Depends on: `tracker-foundation`, `tracker-callables`.

- Generate a typed adapter for each reachable supported extern declaration/signature, not a hard-coded demonstration subset.
- Convert Bool arguments with `ToBool`, Int arguments with `ToInt<int32_t>`, enum/key arguments with mapped host casts, and Condition arguments with `MakeConditionCallback` on the original argument result.
- Normalize host return values to the analyzed RLS representation. In particular, `setting` returns host `uint8_t` but represents RLS Int.
- Preserve `check_price`'s analyzed `RC_UNKNOWN_CHECK` default and leave check-context interpretation to the host.
- Require host externs to be nonmutating and enforce that contract in the host program. Keep Condition callbacks synchronous and document that borrowed callbacks must not escape the extern invocation.
- Permit extern declarations whose analyzed types have supported host C++ types. Diagnose unsupported/missing type mappings; do not infer types from symbol prefixes or introduce a separate host-function binding registry.
- Require synchronous Condition callback use by host functions; deferred/asynchronous callback ownership is out of scope.
- Document host-enforced extern nonmutation; verify it in the actual consumer project.

Exit criteria: local structural/output assertions cover enum namespaces, return widths, defaults, and callback argument references. Adapter compilation and host-enforced extern nonmutation remain downstream checks against real bindings.

#### Adapter contracts and integration (checkpoint 4a)

- [ValidateHostAdapters](../transpilers/soh_tracker/src/tracker_adapters.h) checks adapter contracts and returns diagnostics without generating C++; `ValidateGraph` uses this diagnostic-only pass. After successful preflight, `GenerateHostAdapter` streams a definition without diagnostic checks. The independent `RenderHostAdapters` convenience API validates first and captures definitions for focused adapter tests using the same generator. Calling `LowerRoots` alone does not certify host bindings.
- Adapter names, argument conversions, and result representations come from analyzed `extern define` descriptors, not a built-in function registry or symbol prefixes. By default, adapters call the declared host function directly with typed converted arguments, without a leading global `::`; the actual host header supplies its C++ signature and return width. Bool returns directly preserve the host Bool result; Int and enum/key returns use the generated emitter's `CheckedHostInt` helper before narrowing to `int32_t`, including a host `uint8_t` result for an RLS Int declaration such as `setting`. Adapter definitions are fragments rendered with that helper, not standalone translation units. No return-width guess or exact C++ function-pointer signature is needed for this default path.
- Generated implementation helpers, including `CheckedHostInt`, host adapters, and `CreateExpression`, have `static` internal linkage at global scope; do not wrap them in a named detail namespace. Emit plain decimal switch labels, child indices, representable `int32_t` literals, and validated parameter positions. Omit unused names on zero-argument `std::span<EvalResult>` adapter parameters.
- Enum/key arguments use the shared SoH C++ **type** mappings, never their constant namespaces or invented ordinals. Unmapped normal enums use the declarations from matching SoH-generated `functions.gen.h`; generate it from the same analyzed project and add its output directory to consumer include paths. Unmapped external enums require an explicit `HostEnumBinding`, otherwise a source diagnostic identifies the missing mapping.
- Every emitted host-function name, arity, argument type, and RLS result type comes from the analyzed `extern define`; there is no separate function binding or signature override API. `HostEnumBinding` supplies only a C++ type name for an unmapped external enum, not a host function or signature.
- Every host receiving a Condition argument must accept a synchronous callback such as `std::function<bool()>` (including an equivalent `ConditionFnCap` alias). Adapters call `MakeConditionCallback(arguments[i])` on original `EvalResult` storage, without copying it or freezing game state. Callbacks must finish before the host invocation returns, including on exception; retaining borrowed callback storage is unsupported. A Condition **return** is normalized to consumer `condition_t`, preserving the returned condition rather than wrapping a callback; the host must supply an owning condition with valid body/capture lifetime, never borrowed argument storage.
- Final generated implementation includes consumer `logic_expression.h`, `rls_host.h`, plus matching `functions.gen.h` unconditionally. The consumer supplies the declared host functions and runtime operations. Phase 6 is responsible for emitting these includes. Generated adapters do not include a state-management guard or mutate/restore host state.
- Tracker host externs must not mutate game state. The consumer/host program is responsible for enforcing and validating that contract; RLS-generated adapters do not attempt to detect or prevent mutation. Outer tracker integration establishes the appropriate region, check key, age, and time context before evaluation. Adapters preserve sema's `check_price(RC_UNKNOWN_CHECK)` default as an ordinary argument and do not substitute a location.

### 5. Validating the complete graph and failure behavior

Todo ID: `tracker-validation`

Depends on: `tracker-lowering`, `tracker-callables`, `tracker-adapters`.

- Validate all lowered descriptor references, node arities, target payload representations, adapters, helper edges, required semantic metadata, root records, and emitted coercions before emission, including unexpanded/skipped branches. Reuse sema's authoritative facts; do not repeat language-level declaration checking, argument binding, or type inference.
- Validate n-ary nodes have at least two ordered children, compatible arithmetic representations, and preserved semantic barriers. No flattening rule may cross a conversion/call boundary or remove a potentially throwing operation without preserving its timing.
- Diagnose missing analyzed type/call/`here` metadata, consumer-unsupported values/operators/callable representations, unavailable host mappings, target numeric/enum representation overflow, and duplicate emitted root records. These are lowering/ABI integrity checks, not an alternate implementation of sema's language rules.
- Walk every dependency, including function-reference/default/match/invocation edges, with visited/in-progress bookkeeping so cycles cannot cause infinite validation. Preserve call/definition provenance; do not reject a graph solely because helper references are recursive. Leave existing sema cycle diagnostics unchanged and document the separate deferred runtime-support boundary.
- Check IDs with wide counts: maximum index 4,294,967,295 and maximum descriptor count 4,294,967,296 are representable; reject the next values without wrapping or using an in-range sentinel.
- Allow parameter positions 0-255; reject a 257th argument.
- Support a zero-node/empty-root backend result, with valid generated C++ and no deduced empty arrays.
- Lower and finish all diagnostic-producing preflight work before opening final artifacts, then generate directly into streams as SoH does. Preflight includes graph/adapter validation, checked numbering, and normalized source paths. Complete-artifact buffering is not required. Source/graph/ABI/path failures must produce no successful-looking partial output. Test failing writers and ensure open/write/flush/publish errors are surfaced with an unsuccessful CLI result; keep any required writer changes narrowly scoped.

Exit criteria: diagnostic fixtures verify useful symbols/spans, boundary helpers are tested with synthetic counts without huge allocations, cyclic-reference architecture tests terminate without a tracker-specific cycle error, and invalid references/types/arities never reach output publication.

#### Graph preflight integration (checkpoint 5a)

- [ValidateGraph](../transpilers/soh_tracker/src/tracker_validation.h) is a private, pure pass over the const analyzed project and lowered graph. It checks every stored descriptor, including helper edges and skipped/unreachable nodes, using shared `ValidateGraphStructure` checks and diagnostic-only `ValidateHostAdapters`. Validation does not assign IDs or generate C++.
- The return value is only `std::vector<ast::Diagnostic>`, empty on success, matching SoH's separation of validation and generation. `NumberGraph` independently produces deterministic, cycle-safe numbering using the same structural checks; `RenderHostAdapters` independently produces adapter definitions. Validation opens no artifacts and mutates neither AST nor graph. Call it again after graph mutation; success does not certify consumer runtime behavior.
- Root coverage is checked against `CollectRoots`, including region extensions and the current zero-occurrence policy. Call wrappers are checked against declarations and sema-resolved argument pointers rather than rebound. Pattern lists/arms remain structural, and required integral conversions and lazy Condition boundaries remain explicit.
- Checkpoints 5b/5c below complete local chain and known-numeric/source integrity checks. Checkpoints 6a/6b implement the output-dependent empty-artifact and no-publication-before-success checks, along with writer exception propagation. Caller-side disk flush/close failure reporting and tracker CLI failure status remain Phase 7 gates, not something inferred from private validation tests.

#### Chain integrity (checkpoint 5b)

- [CollectChainSources](../transpilers/soh_tracker/src/tracker_ir.h) centralizes the existing lowering policy using analyzed representations: flatten both sides of homogeneous logical chains, but only the left side of homogeneous arithmetic chains. Retain mixed operators, right-nested arithmetic, and intermediate normalization boundaries. Collection is iterative, preserves ordered source-expression pointers, and does not resolve calls, evaluate values, or infer types.
- Lowering uses those ordered operands and eliminated-operator occurrences. Validation checks each chain's operator and full provenance against that policy, then verifies child count/order and required operand normalization against the original sema facts. It never searches through a call or nested operator for a replacement child; only the immediate required conversion may wrap the original source operand.
- These are compiler descriptor-integrity assertions, not a proof of runtime arithmetic or exception behavior. Existing pinned lowering shapes and synthetic malformed-graph tests verify the intended structure; real-consumer left-fold timing, traces, and exception behavior remain downstream gates.

#### Numeric and source integrity (checkpoint 5c)

- [CheckInt32Value](../transpilers/soh_tracker/src/tracker_ir.h) checks a wide signed value before narrowing, accepting both int32 extrema and diagnosing the next values with source provenance. Literal lowering uses it before constructing an IR value; validation also checks known enum member values. Parser rejection of oversized source literals remains authoritative.
- Validate enum definitions used by nodes or adapters once, requiring supported enum metadata and normalized local values. Do not assign new ordinals, validate deterministically unused declarations, or fabricate values for external wildcard keys. Normal member identities and source value/`here`/root-condition associations are descriptor integrity checks, not a second language resolver.
- The frontend normal-enum counter now explicitly represents an unavailable next value instead of overflowing. A member at `INT_MAX` is valid; a following implicit member gets `RLS-V021`, remains unnormalized, and cannot pass semantic analysis. An explicit reset restores valid numbering. This fixes overflow before any backend sees the project and leaves other declaration/cycle rules unchanged.
- External enum ordinals, game key ordinals, and actual host C++ return widths are not present in the analyzed project. Preserve symbolic identities locally; Phase 6 must emit range enforcement against actual host constants/results before narrowing, rather than silently truncating or guessing ordinals. Consumer compilation/execution remains a separate verification gate.
- Validation does not evaluate arithmetic, reject intentional runtime conversions based on constant folding, sample host state, or generate C++. Empty graphs, wide count/index bounds, 256 parameters, and finite recursive-reference architecture remain covered by earlier fixtures.

### 6. Emitting factories, lazy lookup, and the exact root map

Todo ID: `tracker-emission`

Depends on: `tracker-validation`.

- Assign contiguous deterministic IDs and emit a valid per-ID factory case for every descriptor. Each case constructs one node from child IDs, never resolves descendants, evaluates game logic, or caches results.
- Assign IDs and emit references from the finite descriptor set, not by recursively expanding call edges or requiring a topological order. Synthetic cyclic-reference output checks are structural/API-only; actual recursive consumer execution remains deferred.
- Emit `GetExpressionCount()` without allocation and `GetExpression(index_t)` using the consumer's function-local static `LogicExpressionCache`.
- Maintain one global ID space/resolver and program-lifetime string backing. Rely on the consumer cache for ownership, stable addresses, sparse demand construction, bounds checks, failure recovery, and thread restrictions.
- Define `GetLogicRoots()` using a function-local static const `LogicRootMap` with exact symbolic literals and expression IDs. Map access must not resolve nodes. Root records contain no source spans or condition strings.
- Generate explicit typed value lambdas and structural conversion/match constructors compatible with the embedded ABI. Keep meaningful RLS names and correct escaping, including Unicode.
- Enforce int32 representability of host enum/key constants and actual host integral/enum results before narrowing, using the real C++ declarations. Local preflight checks only known source values; unknown host ordinals must not silently truncate or become guessed numeric literals.
- Emit each flattened chain directly as one `AndExpression`, `OrExpression`, or `ArithmeticExpression` with its ordered child-ID vector. Retain nested node references wherever the flattening policy requires a grouping/safety boundary.
- Emit only the header and C++ implementation; retain provenance internally and omit timestamps and absolute checkout paths from generated artifacts.
- Check generated empty, minimal, and representative artifacts structurally in RLS; defer compilation, public-signature compatibility, and two-translation-unit ODR checks to the real consumer project.
- Close the emission-dependent Phase 5 gates: reject invalid graphs and all other compiler diagnostics before opening final artifacts, then generate directly into output streams. Exercise failing open/write/flush/publish paths. Carry unsuccessful CLI status checks into Phase 7 dispatch integration. Do not claim these gates passed based solely on diagnostic-only preflight tests.

Exit criteria: local artifact checks pass, every emitted ID/root is accounted for, and byte-identical output is demonstrated for identical logical inputs under reordered enumeration and relocated directories. Real-interface compilation remains a downstream gate.

#### Artifact generation (checkpoint 6a, revised with 6b)

- [EmitArtifacts](../transpilers/soh_tracker/src/tracker_emission.h) is private and takes the const analyzed project, lowered graph, output writer, absolute source root, and optional external enum type bindings. Its preflight runs `ValidateGraph`, `NumberDistinctGraph`, and source-path normalization checks for all reachable occurrences before opening any output. Generation retains only checked numbering, not normalized node-path storage or whole-artifact strings.
- The two outputs are `logic_tracker.gen.h` and `logic_tracker.gen.cpp`. The header reuses `logic_expression.h`; the implementation always includes `rls_host.h` and matching SoH `functions.gen.h`, even for empty graphs or projects without local enums. Generate that shared header from the same analyzed project and supply its include directory downstream.
- Factories construct one node per case using child IDs, with explicit vectors even when empty. Value lambdas defer scope reads and symbolic host-value checks until evaluation; no factory/registry initialization resolves children or evaluates host logic. String literals use fixed-width octal escapes for non-ASCII UTF-8 bytes and control characters, preserving byte identity without temporary backing storage.
- The generated private `CheckedHostInt` handles actual enum underlying types and signed/unsigned integral widths, including Bool/character types, checks int32 representability, and throws `std::out_of_range` before narrowing. `GetHostEnumType` is shared by adapter, constant validation, and emission; unmapped external enums require explicit host type bindings even when used only as standalone constants. Consumer compilation/execution of these checks remains downstream.
- Adapter definitions are emitted directly through `GenerateHostAdapter` in stable symbol order after adapter preflight; descriptor/root ordering comes from `NumberDistinctGraph`. Runtime root records retain only the exact consumer key fields and expression ID.

#### Checked publication (checkpoint 6b)

- [SohTrackerTranspiler::Transpile](../transpilers/soh_tracker/include/soh_tracker.h) and the namespace-level `Transpile` accept `(OutputWriter&, absoluteSourceRoot, span<const HostEnumBinding> = {})` after the project supplied to the class/free function. Public enum bindings carry only RLS enum and host C++ type names; the graph remains private.
- Orchestration is `LowerRoots` -> `EmitArtifacts` preflight (`ValidateGraph`, `NumberDistinctGraph`, source-path checks) -> non-diagnostic `GenerateHeader` and `GenerateSource`. The two fixed outputs are `logic_tracker.gen.h` and `logic_tracker.gen.cpp`. This user-approved validate-then-generate flow replaces the earlier render-everything-before-opening guarantee.
- Writing follows the existing SoH pattern: `auto& stream = out.open(filename); stream << content;`. There is no whole-artifact buffer, tracker-specific write helper, stream-state checking, explicit flush, or exception-to-diagnostic conversion. Compiler errors return diagnostics exclusively from preflight; generation and writer exceptions propagate to the caller. The writer/caller is responsible for I/O checking and disk flushing/closing, so a returned diagnostic vector does not certify publication.
- The shared `OutputWriter` interface remains open-only, as requested by the user; no backend completion hook is added. Callers own streams and must check disk flushing/closing separately. At checkpoint 6b the console writer and tests were unchanged; console-owned completion/error reporting is implemented in checkpoint 7a below.
- Publication remains nontransactional. Source/graph/ABI/path diagnostics preserve existing files; allocation/programming and writer exceptions propagate but can leave partial files or previously completed artifacts once generation starts. Console completion/error reporting and tracker CLI status are covered by checkpoint 7a, not by backend preflight alone. Atomic multi-file replacement, rollback, and crash durability are outside this guarantee.

### 7. Wiring CLI, project workflows, and acceptance outputs

Todo ID: `tracker-cli`

Depends on: `tracker-emission`.

- Register `soh_tracker` in dispatch, target linkage, and help with the normal source diagnostic path and explicit failure reporting.
- Cover explicit input paths, manifest-selected targets, manifest defaults when explicitly configured, output overrides, invalid target/configuration cases, and output failures.
- Keep the project manifest/schema extension model intact; add tests proving `soh_tracker` is accepted structurally and dispatched by the installed CLI.
- Keep independent SoH/AP/tracker golden acceptance checks. Cross-backend ordering/nonmutation testing was explicitly removed from this phase at user request.
- Add tracker golden acceptance output to existing CMake/CTest infrastructure so current CI discovers it without unnecessary workflow changes; do not add local consumer compile-check targets.

Exit criteria: documented CLI invocation generates the required artifacts, and existing SoH/AP golden files remain unchanged.

#### CLI/workflow integration (checkpoint 7a)

- Register and link `soh_tracker` without enabling it implicitly. Preserve manifest defaults, configured-only selections, and per-target output overrides; retain the open-ended manifest/schema target model.
- Use the manifest's canonical root for provenance. For explicit inputs, compute the common parent of collected canonical source files independently of the current directory and input enumeration; reject tracker inputs without a shared absolute root.
- The console writer owns checked write/flush/close completion. Catch filesystem and stream failures at dispatch, print target/output context, and return unsuccessful status. Other exceptions continue to propagate. No backend completion hook or transactional publication is introduced.
- Tests cover CLI workflows, source/backend diagnostics, directory/open failures, and unit-injected write/flush/close errors. Existing SoH/AP acceptance outputs must remain unchanged.
- Stop for review before checkpoint 7b adds tracker golden acceptance.

#### Golden acceptance (checkpoint 7b)

- Explicitly configure tracker output in the existing SoH sample manifest and retain only its header and implementation under `examples/soh/out_soh_tracker/`. Keep existing SoH/AP baselines unchanged.
- Register tracker tests through `console_acceptance_tests` and `rls_add_gtest`, without CI workflow changes or local consumer compilation targets.
- Compare tracker output against its golden directory independently, alongside the existing SoH/AP golden tests. Do not retain the cross-backend ordering test or AST snapshot helper; the user removed that coverage during review.
- These checks establish local golden emission stability for the sample, not cross-backend nonmutation, exhaustive source semantics, consumer API compatibility, or runtime parity.

### 8. Completing the local acceptance matrix and sample corpus

Todo ID: `tracker-acceptance`

Depends on: `tracker-cli`.

- Exercise worked examples A-F, the separate core/shops files, and the available SoH source corpus. Include helper-heavy logic, wallet ternaries, Spirit Shared, vanilla/MQ ownership, and shuffle extensions.
- Report actual fixture node/root counts and compare the root set against every composed entry. Verify eight distinct shop roots and five boss-room roots without assuming numeric IDs.
- Exercise real emitted IDs above 65,535 without dropping roots; test the full 32-bit representational limits through synthetic arithmetic checks.
- Test all lowered node kinds, boundary conversions, parameter order/defaults, Condition forwarding/references/invocation, typed matches, reserved names, escaping, and deterministic output. Use analyzed-IR/API assertions, not snapshots alone.
- Assert flattened node/child counts and operand order for all six operators. Cover long chains, both-sided logical nesting, mixed operators, duplicate operands, explicit right-nested arithmetic, and explicit conversion/other semantic boundaries. Require n-ary IR for dynamic left-associated division, including a potentially zero divisor before a later host query; do not gate flattening on constant/nonthrowing prefixes. Local tests verify the required graph structure; downstream runtime regression tests verify early division exceptions skip the later query and preserve trace/evaluation order.
- Cover unknown/missing type and binding metadata, unsupported executable strings/lists/callables, duplicate ownership, invalid arities/operators, numeric/count/parameter overflows, and write errors.
- Add finite self/mutual-reference IR tests for definition sharing, cycle-safe reachability/validation, deterministic emission, and no tracker-specific recursion ban. Keep a source-level regression documenting the current sema cycle rejection separately; do not present it as a new backend restriction or as recursive execution coverage.
- Record downstream checks for representative generated C++ against the real C++20 consumer interface, including two-translation-unit header inclusion and no duplicate generated entry points.
- Run the smallest existing backend/CLI/project suites covering the changes, plus directly affected SoH/AP regression suites. Escalate only if targeted results reveal broader risk.
- Do not claim compilation/API compatibility, runtime evaluation, trace, lifetime, rendering, or gameplay parity without checks against the actual consumer.

Exit criteria: local compiler tests and generated-artifact assertions pass, sample C++ artifacts are reproducible, and the handoff records exact commands, counts, results, and remaining downstream gates.

### 9. Documenting invocation and the downstream integration contract

Todo ID: `tracker-handoff`

Depends on: `tracker-acceptance`.

- Document explicit/project invocations, all generated artifacts, include/link requirements, the matching `functions.gen.h` dependency, deterministic IDs versus stable root identity, and unsupported input diagnostics.
- Explain that the tracker IR is a private lowering/serialization model consuming existing parser/AST/sema output, with no duplicated frontend or source-language type analysis.
- Document the default flattening policy and its intended shallower tracker rows, including examples of safe chains and arithmetic groups that remain nested.
- Document the recursion-ready graph design, unchanged current sema limitation, and deferred coordinated recursion support. Explicitly distinguish cycle-safe compiler architecture from recursive evaluation/trace/UI support; no runtime recursion-depth policy is selected by this work.
- Specify `LogicRootRegistry(GetLogicRoots(), GetExpressionCount())` construction and lookup behavior: missing keys are unavailable/error, ID zero is valid, there is no demonstration fallback, and roots/cache lookup stay separate.
- Document how Shipwright must associate every LocationAccess, EventAccess, and Entrance with the exact root tuple. Use symbolic event identity, owner regions, occurrence zero, and original exit destination before shuffling. Do not reconstruct keys from tracker display strings or rely on registry iteration.
- List prototype definitions/adapters/root records that the downstream build must remove or exclude, and generated translation units it must compile instead.
- Specify and enforce in the consumer host program that tracker externs are nonmutating. Outer integration establishes owning region, check key, age, and time; retain the check-price sentinel behavior.
- Record known ordinary-emitter limitations, such as repeated match discriminant evaluation and noncapturing Condition thunks. Do not copy their bugs or silently fix the ordinary backend as part of tracker work.
- Enumerate pending real-runtime tests and production-corpus validation, including reporting production counts and confirming the uint32_t ABI. Any essential runtime API change must be described separately with before/after signatures, not committed as a replacement game runtime.

Exit criteria: a downstream maintainer can integrate the emitted ABI without guessing, and the handoff clearly separates verified RLS work from unexecuted consumer work.

## Validation commands and acceptance gates

Use the repository's existing configured build and toolchain. The eventual implementation should provide focused targets equivalent to:

```powershell
cmake --build build --config Debug --target soh_tracker_tests soh_tracker_api_checks console_project_tests console_acceptance_tests soh_tests soh_ap_tests project_tests
ctest --test-dir build --build-config Debug --output-on-failure -R "SohTracker|ConsoleProject|ConsoleAcceptance|SohExpressions|SohFunctions|SohRegions|SohAp|Project"
.\build\console\RandoLogicScript.exe -p .\examples\soh\rls.json -t soh_tracker -o .\examples\soh\out_soh_tracker
```

The target and test names above are proposed for new tests; select the exact discovered existing GoogleTest names during implementation. Use the active configuration/executable layout for the selected generator rather than assuming Ninja or Visual Studio subdirectories.

### Locally verifiable

- Correct analyzed IR, required flattened/grouped structures, and complete roots/dependencies.
- Finite cyclic-reference IR, cycle-safe compiler graph walks, and deterministic definition/reference emission without a new recursion ban.
- Source diagnostics and checked graph/ABI limits.
- Deterministic generated C++.
- C++20 constructor/signature/template compatibility against section 16.
- CLI/manifest workflows, output errors, and unchanged ordinary backend artifacts.

### Explicitly pending downstream

- Actual cache demand construction, pointer stability, failure recovery, root validation/lookup, and lazy row expansion.
- Evaluation order, flattened operand child indices/short-circuit traces, arithmetic intermediate conversions and early division exceptions, transparent conversion presentation, typed match fallback/fallthrough, and nested-match binding cleanup.
- Captured lexical scope versus live game state, returned conditions, callback invocation ownership/final-trace behavior, shadowing, and exception cleanup.
- Host-program verification that tracker externs are nonmutating, plus check-price behavior.
- Actual location/event/exit access-object association, missing-root errors, shuffled-exit identity, and tracker display/smoke tests.
- Ordinary-C++/tracker parity from independently reset identical host snapshots, with known ordinary-emitter mismatches isolated rather than masked.
- Complete unavailable production corpus generation, reported counts, consumer build/linking, and 32-bit ABI confirmation.

### Deferred recursion feature work

- Recursive source/type-inference support in sema and any necessary cross-backend coordination.
- Consumer evaluation, call-frame/binding cleanup, trace ownership, lazy construction, and bounded/on-demand row expansion for recursive calls.
- Recursive behavioral tests, including terminating self/mutual calls and user-visible handling of nontermination/expansion. These are not required to deliver the current nonrecursive corpus and must not be claimed by the synthetic IR architecture tests.

## Completion standard

The RLS implementation is complete only after the backend, local fixtures/tests, generated sample C++ artifacts, CLI workflow, and integration documentation are persistent and verified. End-to-end Shipwright tracker completion must not be claimed until its separate consumer gates pass.
