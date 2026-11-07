# Tracker generation and integration

`soh_tracker` is opt-in: configure it in `rls.json` or supply explicit inputs and an output directory.

Run from the repository/project root:

```powershell
RandoLogicScript -t soh -o out\soh -t soh_tracker -o out\tracker src
RandoLogicScript -p examples\soh\rls.json -t soh_tracker
```

The first command also generates matching SoH enum declarations. The second uses the sample's configured tracker output; to refresh all sample goldens, omit `-t soh_tracker`. See [project invocation rules](../../README.md#project-files) for overrides.

## Generated ABI

- Outputs: `logic_tracker.gen.h` and `logic_tracker.gen.cpp` only; no debug manifest. Compile the generated `.cpp` once as C++20 and link the consumer expression runtime. Include paths must provide the tracker output, consumer `logic_expression.h` / `rls_host.h`, and **SoH `functions.gen.h` generated from the same analyzed project**, even when no local enum is used.
- Exclude prototype factory/resolver/count/root-map definitions and adapters; generated code supplies the replacements. Keep ordinary SoH gameplay generation separate.

```cpp
uint64_t GetExpressionCount();
LogicExpression* GetExpression(index_t index); // consumer index_t is uint32_t
const LogicRootMap& GetLogicRoots();
```

Construct `LogicRootRegistry(GetLogicRoots(), GetExpressionCount())`. Root storage/lookup does not resolve nodes; the generated resolver uses a lazy `LogicExpressionCache`. IDs are deterministic for unchanged input, not stable across edits; distinct roots may share an ID. Use symbolic root identity, never cached IDs, display text, or map iteration order:

| Access object    | Root tuple `(kind, owner, entry, occurrence)`                    |
| ---------------- | ---------------------------------------------------------------- |
| `LocationAccess` | `(Location, owning RR key, RC key, 0)`                           |
| `EventAccess`    | `(Event, owning RR key, symbolic event key, 0)`                  |
| `Entrance`       | `(Exit, source RR key, original declared destination RR key, 0)` |

Include base regions and all extensions, retaining vanilla/MQ owners and pre-shuffle exit identities. Sema rejects duplicate keys within an owner/kind, hence occurrence zero. `Find` returns an optional ID; zero is valid. Missing keys are unavailable/errors (`GetExpression(key)` returns `nullptr`), never a demonstration fallback. See the [consumer contract](../../docs/LOGIC_TRACKER_TRANSPILER_REQUIREMENTS.md#11-root-registry-display-reconstruction-and-entry-coverage).

## Semantics and boundaries

- The private tracker IR lowers existing parser/AST/sema output; it is not another frontend. Homogeneous `and`/`or` flatten both sides; `+`, `-`, `*`, `/` flatten only left-associated chains into shallower rows, preserving operand order and conversion/call boundaries. Thus `a / b / c` is n-ary, but `a / (b / c)` and mixed operators remain grouped. Runtime left-fold evaluation must throw on an early zero divisor before evaluating later operands.
- Emission preflights all graph occurrences before sharing nodes, then reuses the inspected edges and root paths for deterministic numbering. Host adapter definitions are rendered from the same validated declarations used by adapter tests.
- Host signatures/defaults come from analyzed `extern define` declarations. Reuse SoH enum mappings; unmapped external enums require the library API's `HostEnumBinding` (no CLI override). Numeric values use checked `int32_t`, not guessed host return widths.
- The **host enforces nonmutation**; generated adapters neither guard nor restore game state. Outer integration establishes owning region, check key, age, and time. Preserve the declared `check_price(RC_UNKNOWN_CHECK)` default rather than substituting the location. Condition arguments borrow `EvalResult` storage via synchronous callbacks: finish before the host returns, including exception paths; do not retain them. Returned `condition_t` must own valid body/capture lifetime. Lexical captures still observe live game state.
- Executable strings, lists, general callables, missing analyzed/binding metadata, and ABI overflows produce compiler diagnostics before artifacts open; region metadata is not executable. I/O exceptions propagate and the CLI reports failure, but writes are not transactional and may leave partial files.
- Graph walks are cycle-safe and recursion-ready; sema still rejects recursive source definitions. Recursive evaluation, trace/UI behavior, and depth policy remain deferred. Ordinary SoH emission still has repeated match-discriminant evaluation and noncapturing Condition thunks; isolate these in parity tests rather than importing or fixing them here.

**Pending in the actual SoH project:** compile/link against real C++20 headers (including two-TU header inclusion and single generated entry points); confirm host widths/ranges and the uint32 index ABI; run the full production corpus and report counts; verify cache failure recovery/pointer stability, exact root wiring/missing-key behavior, evaluation/trace order and early exceptions, typed match cleanup, callback/capture lifetimes, nonmutation/context, rendering/gameplay parity, and tracker smoke tests. Local IR/unit/golden tests are not consumer certification. Phase 8's additional local acceptance expansion was intentionally skipped.
