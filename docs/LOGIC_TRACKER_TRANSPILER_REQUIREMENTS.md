# RandoLogicScript logic-tracker transpiler requirements

## 1. Purpose and scope

Implement RLS generation of the structured C++ expression graph consumed by Shipwright's logic tracker. The tracker must explain **why** a location, exit, or event condition succeeds or fails, including arguments, user-defined helper bodies, skipped branches, and invoked conditions. Generating only a Boolean callback or a display string is insufficient.

This document is an implementation handoff for an agent working in the **RandoLogicScript repository**. It specifies tracker output only; it does not request a new gameplay transpiler, Python/AP changes, a new runtime parser, or a tracker UI rewrite.

**MUST** indicates a required behavior. **SHOULD** indicates a preferred implementation with room for an equivalent solution. Names marked **proposed** are new interfaces or packaging suggestions, not interfaces already available in Shipwright.

### Baseline and portability

The compatibility baseline is the Shipwright workspace on branch `RandoLogicScript-LogicTracker` from which this document was prepared. Its expression table is currently hand-written demonstration code. Do not assume an upstream Shipwright checkout has the same runtime.

In this document:

- `SW/` means the Shipwright repository root.
- `RLS/` means the RandoLogicScript repository root. In this workspace it is nested at `SW/soh/soh/Enhancements/randomizer/logic/RandoLogicScript/`.
- `LOGIC/` means `SW/soh/soh/Enhancements/randomizer/logic/`.
- `RUNTIME/` means `SW/soh/soh/Enhancements/randomizer/`.

The receiving agent starts at the **RandoLogicScript repository root** and has this document only. `RLS/` paths are relative to that root; do not search for a nested RLS checkout or require access to Shipwright or ShipOfHarkinian-RandoLogic. `SW/`, `LOGIC/`, and `RUNTIME/` paths below identify optional downstream references, not local files or implementation prerequisites. Section 16 embeds the required consumer contract and standalone input corpus. Compile-only fixtures derived from that contract verify API shape, not consumer runtime semantics.

## 2. Local compiler entry points and optional downstream references

### Optional downstream references (not available to the RLS-only agent)

| File | Why it matters |
| --- | --- |
| `RUNTIME/logic_expression.h` | Authoritative expression constructors, value types, scope, callback, and table ABI. |
| `RUNTIME/logic_expression.cpp` | Evaluation order, lazy capture/invocation, arithmetic, comparison, match, and display behavior. |
| `RUNTIME/randomizer_logic_tracker.cpp` | Prototype adapters/table; expression-row construction; check/exit/event consumers; age/time evaluation. |
| `RUNTIME/randomizer_logic_tracker.h` | Public tracker entry points. |
| `RUNTIME/rls_host.h` | Actual host function signatures and implementations. |
| `RUNTIME/location_access.h`, `RUNTIME/entrance.h` | Macros storing callbacks and stringified conditions. |
| `RUNTIME/location_access.cpp` | `CleanConditionString`, `GetCheckPrice`, and context-dependent evaluation. |
| `LOGIC/src/host.rls` | Production extern declarations, default arguments, local enums. This is more extensive than the RLS example host file. |
| `LOGIC/src/shared/functions.rls`, `LOGIC/src/shared/enemies.rls` | Real helper bodies, arithmetic, enums, matches, and defaults. |
| `LOGIC/src/dungeons/`, `LOGIC/src/overworld/`, `LOGIC/src/shuffles/` | Complete generation corpus, including extensions and variants. |
| `SW/tests/logic_expression/logic_expression_tests.cpp` | Executable runtime contract, especially closures, trace ownership, scope cleanup, parameters, and matches. |
| `LOGIC/CMakeLists.txt`, `LOGIC/cmake/RandoLogic.cmake` | Existing generation/build integration. |

### RLS compiler extension points

| File | Relevant facilities |
| --- | --- |
| `RLS/ast/include/ast.h` | Typed AST, spans, `Project`, `HereRef`, `InvokeExpr`, `MatchExpr`, parameter and enum metadata. |
| `RLS/sema/src/resolve_types.cpp` | Types, function references, callable behavior. |
| `RLS/sema/src/validate_declarations.cpp` | Entry compatibility and declaration validation. |
| `RLS/transpilers/soh/include/soh.h` | Existing SoH backend surface. |
| `RLS/transpilers/soh/src/generate_expression.cpp` | Host enum qualification, resolved arguments, coercions, lexical `here`. |
| `RLS/transpilers/soh/src/generate_functions.cpp` | Local enums and C++ host type mappings. |
| `RLS/transpilers/soh/src/generate_regions.cpp` | Region/extension composition and ordinary condition emission. |
| `RLS/transpilers/soh/src/enum_mappings.h` | Existing host enum mapping table to reuse. |
| `RLS/transpilers/soh/include/rls_match.h` | Existing ordinary-C++ match behavior, including fallthrough. |
| `RLS/transpilers/soh/src/soh.cpp` | Backend orchestration and validation-before-emission. |
| `RLS/console/main.cpp`, `RLS/project/rls.schema.json` | Target dispatch and project/CLI integration. |
| `RLS/transpilers/soh/tests/`, `RLS/console/tests/` | Existing backend and end-to-end test conventions. |

Read the local compiler extension points above and the embedded contract in section 16 first. Paths reflect the authoring snapshot; locate renamed files by symbol in the local checkout rather than treating a stale path as a blocker. The design reference `RLS/docs/RandoLogicScript-Full.md` describes intent; current local AST/sema and this embedded consumer contract take precedence over historical examples.

## 3. Existing behavior versus new work

### Already available in the runtime

- Indexed expression factories, sparse owning `LogicExpressionCache`, and on-access child resolution without a startup graph walk.
- Logical, arithmetic, comparison, ternary, function-call, extern-call, parameter, value, condition, invocation, and match nodes.
- `VariableScope` with stacked bindings, capture, and exception-safe runtime binding cleanup.
- Lexically captured condition values evaluated later against **live game state**.
- Per-evaluation child traces and a separate condition invocation trace.
- Tracker evaluation across child/adult and day/night states.
- `ConversionExpression<T>` with compile-time integral targets, flattened operand traces containing the converted result, and transparent tracker text/rows.
- `MatchExpression<T>` results, including typed zero/false when no arm matches; `MatchExpression<>` retains legacy untyped behavior.
- Stable `LogicRootKey`/`LogicRootMap` metadata and validated `LogicRootRegistry` lookup; generated root data and access-object wiring are still required.

### Not already implemented

1. The hand-written prototype expression factory, count metadata, resolver, and adapters need generated replacements.
2. `PopulateConnectionExpression(connection, key)` now resolves through `LogicRootRegistry`. The prototype registers only `RR_KF_KOKIRI_SHOP` / `RC_KF_SHOP_ITEM_1` through `RC_KF_SHOP_ITEM_8`, all at expression index 0. Missing registrations report an error; there is no demonstration-root fallback.
3. The root registry contract is implemented in `logic_expression.h`, but generated `GetLogicRoots()` records covering the complete project are not implemented. The prototype uses enum-to-symbolic-name adapters for location/exit keys and the supplied event name for event keys; full event identity and duplicate-occurrence integration still require validation.
4. Generated output alone will not wire every existing `LocationAccess`, `EventAccess`, and `Entrance` object to its correct root. A small Shipwright integration change is required.
5. Some typed-language semantics need explicit lowering or runtime coordination; see section 12.

The RLS agent should deliver generator changes, tests, generated sample artifacts, and a precise integration note. Necessary Shipwright changes must be listed separately, not silently represented as existing facilities.

## 4. Output artifacts and compiler integration

### Required artifacts

File names below are **proposed**; functionality and ABI compatibility are mandatory.

| Artifact | Required content |
| --- | --- |
| `logic_tracker.gen.h` | Include `logic_expression.h` using a documented consumer-provided include path; reuse its root/value contracts rather than redeclaring incompatible structs. |
| `logic_tracker.gen.cpp` | Exactly one definition each of `GetExpression(index_t)`, `GetExpressionCount()`, and `GetLogicRoots()`, plus per-ID factories, host adapters, root records, and metadata storage. Use the runtime's owning lazy cache; splitting implementation across TUs is allowed while preserving one global ID space and resolver. |
| Optional debug manifest | Human-readable node/root/source listing. If emitted, it must be deterministic and must not be required at runtime. |

Output must remain C++20-compatible and support downstream C++23/MSVC/Ninja builds. Use the RLS checkout's existing build/test tooling; do not inspect or modify unavailable downstream CMake files. Do not require C++23-only features, change compiler versions, or add dependencies merely to implement this backend.

### Packaging recommendation, not an existing CLI

A separate opt-in target named `soh_tracker` is a reasonable packaging choice:

```text
RandoLogicScript -p rls.json -t soh_tracker -o generated/soh_tracker
```

This target name does **not** exist at the baseline. An opt-in extension of `soh` is also acceptable if it preserves existing outputs and supplies equivalent tests/documentation. Choose one supported route; do not implement both without need. Update dispatch/help, manifest validation/schema where applicable, build targets, and CLI tests consistently.

The backend MUST consume the same semantically analyzed `Project` as normal generation. Reuse:

- `Project::getType`, `getEnumType`, and enum metadata;
- `Project::getResolvedCallArgs` for positional/named/defaulted arguments;
- resolved identifier kinds and `HereRef::resolvedRegion`;
- all region declarations and their `ExtendRegionDecls`.

Do not parse emitted C++, infer types from names/prefixes, invent a second RLS parser, or modify the AST in a way that changes a subsequent `soh`/`soh_ap` emission in the same invocation.

## 5. Expression ABI and graph invariants

The current public contract uses `uint32_t` for node indices. An older consumer with `uint16_t` must be updated before using this contract:

```cpp
using index_t = uint32_t;
using condition_t = std::shared_ptr<const ConditionValue>;
using value_t = std::variant<bool, int8_t, int16_t, int32_t,
							 uint8_t, uint16_t, uint32_t, condition_t>;
using function_t = value_t (*)(VariableScope&);
using extern_function_t = value_t (*)(std::span<EvalResult>);

LogicExpression* GetExpression(index_t index);
uint64_t GetExpressionCount();
using expression_factory_t = std::unique_ptr<LogicExpression> (*)(index_t);
using expression_resolver_t = LogicExpression* (*)(index_t);
```

`EvalResult` contains `Expression`, `Result`, `Children`, `ChildIndex`, and `Invocation`. `Invocation` is a shared pointer to a const evaluation result. `ConditionValue` contains a body pointer and captured bindings. Generate objects compatible with these types; do not invent alternate evaluator signatures.

### Factory construction and cache lifetime

- Generate deterministic metadata for the total descriptor count and an indexed factory (a switch or equivalent descriptor dispatch). IDs are contiguous from zero to `GetExpressionCount() - 1`; every ID must have a valid factory case. Metadata may be available immediately, but it MUST NOT allocate expression objects.
- Construct `LogicExpressionCache(GetExpressionCount(), factory)` as a function-local static in `GetExpression(index)`. The cache stores owning `std::unique_ptr<LogicExpression>` instances sparsely by ID; it does not allocate a pointer slot for every possible node. `LogicExpression` has a virtual destructor.
- A factory returns exactly one node with child IDs, never a preconstructed descendant graph. Repeated requests return the same cached object with a stable address, including after the cache grows. Shared helper definitions use the same IDs/cache entries across roots.
- Every child/reference MUST use the same global resolver/ID space. Forward references are permitted. `GetChildren()` is a lazy view: inspecting its size does not construct children; accessing/iterating a child resolves that ID. There is no `ResolveChildren()` initialization pass or `GetExpressions()` pointer-table API in this contract.
- Factories MUST NOT evaluate game logic or recursively resolve dependencies. The cache detects reentrant construction of an in-progress ID and throws. Out-of-range requests and null factory results throw; failed construction removes its placeholder so a subsequent request can retry. Successfully constructed dependencies remain valid if an enclosing custom factory fails.
- Cached structure and its string backing storage must outlive all rows, evaluation traces, and captured condition body pointers. Do not evict or rebuild nodes during the play session. `GetCachedExpressionCount()` reports completed cached nodes for tests/debugging, not total descriptors.
- Cache only immutable expression structure, never game-state results. Reevaluate conditions for each check refresh and age/time scenario. A captured condition may retain its body node pointer without constructing its descendants until invocation.
- Tracker rows are shallow: immediate child rows are materialized for evaluation trace mapping or visible expansion, not by recursively walking every descendant. Rendering text may resolve nodes needed to show that expression; it must not evaluate them, and ordinary helper-call text must not force its definition body. Skipped subtree roots may be needed for display, but unvisited descendants should remain unconstructed.
- Construction errors during evaluation, rendering, or expansion must be reported in the tracker rather than escaping the UI. A failed visible row retains its error without automatic frame-by-frame retries; a new investigation/refresh can retry through the cache's failure-safe resolver.
- The current cache/resolver is for the tracker/game thread and is not thread-safe. Concurrent construction or asynchronous retained callbacks require a separate coordinated ownership/synchronization design.
- Every referenced descriptor MUST exist. Validate the complete generated ID graph before writing output; runtime demand lookup does not replace compiler validation of unreachable or unexpanded branches.
- With `index_t = uint32_t`, representable indices are 0 through 4,294,967,295: at most 4,294,967,296 nodes, absent any separately reserved sentinel. This is a representational limit, not a promise that such a graph fits in memory. Use a wider count type and checked conversions so the node count itself does not overflow at this boundary. Reject a larger graph; never wrap indices or silently split it into incompatible registries.
- `ParameterExpression` has a `uint8_t` parameter index: indices 0 through 255 are representable. Reject a call needing index 256 unless the consumer ABI is explicitly widened.
- Runtime names/operator/display text are stored as `std::string_view`. Emit string literals or other program-lifetime storage, never temporary `std::string` backing storage.
- Deterministic input must produce byte-identical artifacts. Define normalized project-relative source ordering and declaration/extension traversal. Do not rely on pointer values, absolute checkout paths, or filesystem enumeration order.
- Node IDs need only be deterministic for identical input; they are not persistent public identities across edits. Root identities must not depend on node numbers.
- Sharing immutable definition bodies is allowed. Sharing a node MUST NOT cache its result or merge separate call/argument trace occurrences. Preserve duplicate child positions such as `f(x, x)`.
- Ordinary structural graph cycles are unsafe: the tracker recursively builds rows. Detect recursive definition dependencies and reject with a source diagnostic unless a coordinated runtime solution exists. Do not infinitely inline recursion.
- Empty projects must report a zero descriptor count, construct no nodes, and reject indexed requests; their root span may be empty. A clear documented compiler diagnostic is also acceptable. Avoid deduced empty-array constructs that fail to compile.

### Runtime node lowering table

The constructor signatures below are the existing ABI; `ids` means ordered `std::vector<index_t>` children.

| RLS construct | Existing node constructor | Important behavior |
| --- | --- | --- |
| Boolean/integer/enum/parameter read | `ValueExpression(function_t, text, isEvent)` | Callback reads live values or scope; no evaluated result in generated storage. |
| `a and b` | `AndExpression(ids)` | At least two children, left-to-right, short-circuits. |
| `a or b` | `OrExpression(ids)` | At least two children, left-to-right, short-circuits. |
| `not a` | `NotExpression(id)` | Boolean conversion of operand, not callable invocation. |
| `+`, `-`, `*`, `/` | `ArithmeticExpression(op, ids)` | At least two children; left fold, not algebraic reassociation. |
| Equality/order comparison | `ComparisonExpression(op, left, right)` | Supports `==`, `!=`, `<`, `<=`, `>`, `>=`, `is`, `is not`. |
| `test ? yes : no` | `TernaryExpression(test, yes, no)` | Evaluate test and only selected branch. |
| RLS helper call | `FunctionCallExpression(name, argumentIds, definitionId)` | All argument IDs must point to `ParameterExpression` wrappers. |
| Host call | `ExternFunctionCallExpression(adapter, name, argumentIds)` | Use parameter wrappers here too; UI reads names through that concrete type. |
| One supplied/resolved argument | `ParameterExpression(position, name, valueId)` | Records argument value and supplies binding/display metadata. |
| Integral/Boolean coercion | `ConversionExpression<T>(operandId)` | Evaluates once and returns the operand trace with its result replaced by the converted value; transparent in tracker text and rows. |
| Bool-compatible expression converted to `Condition` | `ConditionExpression(bodyId)` | Captures scope without evaluating body. |
| Invocation of condition | `InvokeExpression(calleeId)` | Evaluates condition value, then invokes it and records invocation trace. |
| `match` | `MatchExpression<T>(discriminantId, armIds)` | Ordered arm evaluation and short-circuit fallthrough; selected results and unmatched zero are normalized to `T`. `MatchExpression<>` retains legacy untyped behavior. |
| Match arm | `MatchArm(patternsId, bodyId, fallThrough)` | `fallThrough` comes from AST, not a trailing-text heuristic. |
| Arm patterns/default | `MatchArmPatterns(patternIds)` | Multiple patterns mean alternative equalities; empty list means `_`. |

Keep decomposable RLS operations as structural nodes. Do not turn a complete `and`, helper body, ternary, or match into an opaque `ValueExpression` merely because ordinary C++ generation can emit its text.

## 6. Values, types, and conversions

### Leaves

Emit explicitly typed results. Examples:

```cpp
new ValueExpression(
	[](VariableScope&) -> value_t { return true; }, "always", false);
new ValueExpression(
	[](VariableScope&) -> value_t { return int32_t{99}; }, "99", false);
new ValueExpression(
	[](VariableScope&) -> value_t {
		return static_cast<int32_t>(RandomizerGet::RG_HOOKSHOT);
	}, "RG_HOOKSHOT", false);
new ValueExpression(
	[](VariableScope& scope) -> value_t { return scope.get("item"); },
	"item", false);
```

`value_t` has no enum, string, list, floating-point, or arbitrary function-pointer alternative. Normalize enums to a documented integral representation and cast back only at host boundaries. Preserve representable values; diagnose values outside the selected representation. Do not emit an enum-class directly into the variant.

Reuse `enum_mappings.h`, including value namespaces. `Setting` values intentionally have no C++ value namespace in that table. Local enums such as `WaterLevel` and `HasProjectileAge` must retain declared order/explicit values and be declared in a shared, non-duplicated place. Including ordinary `functions.gen.h` is acceptable if that dependency is explicit; a standalone target must otherwise supply the needed enum declarations without redefinition.

Region, event, and location identifiers used as values are **keys**, not automatically reachability/event tests. `flag(LOGIC_X)` is an event read; `LOGIC_X` passed to `flag` is its key.

### Coercions are semantic operations

The generator MUST preserve the analyzed RLS type at function boundaries, conditional branches, and roots, rather than relying on accidental variant alternatives:

- Int-to-Bool is `value != 0`, not truncation or reinterpretation.
- Enum-to-Int and Int-to-enum must follow existing sema/host mapping rules.
- Passing an Int to a Bool parameter must normalize the value **before** binding it in a user-defined helper. Example: `define echo(flag: Bool): flag`; `echo(2)` must yield Boolean true, not the integer 2.
- A condition value must not be passed through `ToBool`/`ToInt`; these conversions throw for callables. Invoke only where the language requires invocation.
- Mixed Bool/Int ternaries and matches need the coercions selected by RLS type inference. The ordinary SoH emitter currently Boolean-normalizes mixed Bool/Int branches/arms; merely returning a selected integral node can differ in later arithmetic/comparison.
- Entry conditions and invoked condition bodies must return Boolean-compatible results with the expected Boolean normalization.

Use `ConversionExpression<T>(operandId)` for required coercions. The `LogicTarget` concept accepts exactly `bool`, `int8_t`, `int16_t`, `int32_t`, `uint8_t`, `uint16_t`, and `uint32_t`; map RLS Int and enum representations to the chosen integral target (normally `int32_t`). `ConvertValue<T>(value)` is the shared conversion operation. Boolean conversion uses nonzero truthiness; integral conversion follows C++ `static_cast` rules. All conversions reject condition values at runtime; unsupported target types are rejected at compile time. There is no runtime target enum or per-node target field. `ConversionExpression<T>` derives directly from `LogicExpression`.

Conversion wrappers MUST NOT add visible tracker rows, cast labels, or synthetic ternaries. `ToString()` and colored drawing delegate to the operand, including its original grouping. `GetDisplayExpression()` projects through nested conversion nodes for row construction. Evaluation returns the operand's `EvalResult`, replacing only `Result` with the converted value and preserving `Expression`, `Children`, `ChildIndex`, and `Invocation`. There is no extra conversion trace, separately retained raw operand result, or display-result unwrapping API. The tracker consumes this trace directly and uses its converted value for availability (e.g. 256 converted to `uint8_t` is zero). Ordinary descendant traces remain intact. Do not hide a compound operand inside a leaf to implement its conversion.

Arithmetic must preserve AST grouping and C++-compatible integral promotion behavior for supported values. `/` is integer division; runtime division by zero throws. Do not silently change overflow behavior, add floating-point arithmetic, or add unsupported operators.

Strings/lists used as region metadata (e.g. `name`, `areas`) do not require value nodes. If a reachable executable expression needs an unrepresentable value, emit a source diagnostic; do not silently drop it, substitute false, or require support for metadata as runtime values.

### Lexical `here`, not the demo implementation

`HereRef::resolvedRegion` is filled in by semantic analysis. The ordinary SoH backend emits `RandomizerRegion::<resolvedRegion>`.

The tracker prototype contains a leaf reading `logic->CurrentRegionKey` with display text `here`. This is **not** the generator contract. Emit the semantically resolved constant while retaining `here` as its node display text. An extension's `here` resolves to the region being extended. A delayed callback or helper that changes current runtime region must not change that lexical value.

Runtime-dependent calls such as `is_vanilla()` and `is_mq()` still legitimately use the current evaluation context through the host API.

## 7. Function calls, defaults, and readable expansion

### Required call algorithm

1. Resolve the declaration and get `Project::getResolvedCallArgs(call)`.
2. Use declaration order, not the order of named arguments in source.
3. Materialize **all** resolved arguments, including defaults, and perform required conversions.
4. Wrap every argument in `ParameterExpression` with its declaration name and zero-based index.
5. For an RLS `define`, reference its generated body through `FunctionCallExpression`.
6. For an `extern define`, reference a correctly typed adapter through `ExternFunctionCallExpression`.

Arguments evaluate in caller scope, left-to-right, before new callee bindings are installed. Do not eagerly evaluate bodies of `Condition` arguments. Runtime scope machinery handles shadowing and cleanup; do not generate global mutable parameter variables or capture C++ references to temporary bindings.

Use RLS names for display, even when C++ helper symbols are mangled. Definition bodies must remain expandable in the tracker. Do not implement `wallet_capacity()` as a leaf calling the already generated gameplay function; that loses its ternary/`has` explanation.

### Example A: complete small expression graph

RLS:

```text
define owns(item: Item): has(item)

# This condition can occur in a region entry:
owns(RG_HOOKSHOT) and not is_child()
```

Illustrative generated C++ (includes/adapters omitted here; cases identify node IDs):

```cpp
static std::unique_ptr<LogicExpression> CreateExpression(index_t index) {
	switch (index) {
		case 0: return std::make_unique<AndExpression>(std::vector<index_t>{1, 6});
		case 1: return std::make_unique<FunctionCallExpression>("owns", std::vector<index_t>{2}, 4);
		case 2: return std::make_unique<ParameterExpression>(0, "item", 3);
		case 3: return std::make_unique<ValueExpression>([](VariableScope&) -> value_t {
			return static_cast<int32_t>(RandomizerGet::RG_HOOKSHOT);
		}, "RG_HOOKSHOT", false);
		case 4: return std::make_unique<ExternFunctionCallExpression>(has_adapter, "has", std::vector<index_t>{5});
		case 5: return std::make_unique<ParameterExpression>(0, "item", 8);
		case 6: return std::make_unique<NotExpression>(7);
		case 7: return std::make_unique<ExternFunctionCallExpression>(is_child_adapter, "is_child", std::vector<index_t>{});
		case 8: return std::make_unique<ValueExpression>([](VariableScope& scope) -> value_t {
			return scope.get("item");
		}, "item", false);
		default: throw std::out_of_range("Unknown expression ID");
	}
}

uint64_t GetExpressionCount() {
	return 9;
}

LogicExpression* GetExpression(index_t index) {
	static LogicExpressionCache cache(GetExpressionCount(), CreateExpression);
	return cache.GetExpression(index);
}
```

Root index is 0 **only in this example**, not a required numbering scheme. Both call nodes use parameter wrappers. The helper body reads `item` from scope rather than baking in `RG_HOOKSHOT`. Zero-argument host calls are shown as extern nodes for consistency; a leaf optimization must preserve meaningful call display text.

### Example B: named arguments and defaults

Actual signature:

```text
extern define spirit_shared(
	first_region: Region, first_condition: Condition, any_age: Bool = false,
	second_region: Region = RR_NONE, second_condition: Condition = false,
	third_region: Region = RR_NONE, third_condition: Condition = false) -> Bool
```

Call inside a region:

```text
spirit_shared(here, can_use(RG_HOOKSHOT), any_age: true)
```

The generated call must have these seven wrappers in this order:

| Position | Name | Value graph |
| --- | --- | --- |
| 0 | `first_region` | Integral leaf for lexical `here`. |
| 1 | `first_condition` | `ConditionExpression` wrapping the `can_use` call graph. |
| 2 | `any_age` | Boolean true. |
| 3 | `second_region` | Integral `RR_NONE` leaf. |
| 4 | `second_condition` | `ConditionExpression` wrapping Boolean false. |
| 5 | `third_region` | Integral `RR_NONE` leaf. |
| 6 | `third_condition` | `ConditionExpression` wrapping Boolean false. |

The false defaults must be callable conditions, **not plain false values handed to `MakeConditionCallback`**. Also generate the optional `check_price` argument: its resolved default is `RC_UNKNOWN_CHECK`; the prototype's no-argument adapter is not a complete template for this declaration.

## 8. Extern adapters and evaluation context

Generate one adapter per distinct supported extern declaration/signature (or equivalently deduplicated typed binding). The compiler must cover **all reachable production externs**, not just the five demonstration adapters.

Basic adapters:

```cpp
static value_t has_adapter(std::span<EvalResult> arguments) {
	const auto item = static_cast<RandomizerGet>(ToInt<int32_t>(arguments[0].Result));
	return has(item);
}

static value_t check_price_adapter(std::span<EvalResult> arguments) {
	const auto check = static_cast<RandomizerCheck>(ToInt<int32_t>(arguments[0].Result));
	return int32_t{check_price(check)};
}

static value_t is_child_adapter(std::span<EvalResult>) {
	return is_child();
}
```

Conversion policy:

| Host parameter/return | Generated conversion |
| --- | --- |
| Bool parameter | `ToBool(arguments[i].Result)` |
| Int parameter | `ToInt<int32_t>(arguments[i].Result)` or equivalent documented host-compatible integral conversion |
| Enum/key parameter | Integral extraction followed by `static_cast<HostEnum>(...)` |
| Condition parameter | `MakeConditionCallback(arguments[i])` |
| Bool return | Store Boolean alternative. |
| Int/enum return | Explicitly normalize to the selected supported integral alternative. |

Use the embedded host signatures in section 16, not return-width guesses from RLS names. For example, `setting` is RLS Int but the C++ host returns `uint8_t`. The backend needs a consistent RLS numeric policy rather than copying host widths accidentally.

Adapters operate on the **original** `EvalResult` arguments passed by the extern node. Do not copy an argument into a temporary and attach callback traces there; the UI will not see them.

### Callback adapters

```cpp
static value_t any_age_adapter(std::span<EvalResult> arguments) {
	TrackerHostStateGuard stateGuard;
	return any_age(MakeConditionCallback(arguments[0]));
}

static value_t spirit_shared_adapter(std::span<EvalResult> arguments) {
	TrackerHostStateGuard stateGuard;
	return spirit_shared(
		static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[0].Result)),
		MakeConditionCallback(arguments[1]),
		ToBool(arguments[2].Result),
		static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[3].Result)),
		MakeConditionCallback(arguments[4]),
		static_cast<RandomizerRegion>(ToInt<int32_t>(arguments[5].Result)),
		MakeConditionCallback(arguments[6]));
}
```

`TrackerHostStateGuard` above is a proposed consumer-integration type, not an existing runtime API. Require it through a documented consumer-provided integration header (section 16), with a test implementation in the RLS-local fixture. It saves/restores `IsChild`, `IsAdult`, `AtDay`, `AtNight`, and `CurrentRegionKey`. The downstream prototype has a private `LogicStateGuard` that generated code cannot directly use. Do not infer private game layouts from unavailable headers.

Restore state on success and exceptions, including nested callbacks. Audit each context-changing host binding rather than assuming every extern is pure. Callback capture is lexical scope only; age/time/current-region must be read when the callback runs, not frozen at graph construction or closure creation.

The outer tracker remains responsible for establishing region, check, and age/time context. In particular, `check_price(RC_UNKNOWN_CHECK)` resolves through `logic->CurrentCheckKey`; the generator must not replace the sentinel with a guessed location. The existing guard does not save that field, so outer integration must also preserve any check context it changes.

Generated `std::function` callbacks reference evaluation argument storage. The current host bridge is synchronous: callbacks must not escape the extern invocation. Supporting deferred/asynchronous callback retention would require a different ownership contract and is out of scope.

For other hosts/custom extern enums, use an explicit mapping/extension point or a diagnostic identifying the missing host binding. Wildcard enum declarations are not an instruction to invent numeric enum values.

## 9. Lazy conditions, references, and invocation

### Core requirements

- Passing a Bool-compatible expression to a `Condition` parameter wraps its graph in `ConditionExpression`.
- Passing an existing Condition value forwards that value without evaluating or wrapping it into a condition whose body incorrectly treats it as Bool.
- `ConditionExpression` captures the current lexical scope by value. The body is not evaluated until invocation.
- `InvokeExpression` represents `condition()` or invocation of a returned callable. It is not the same thing as an ordinary named RLS helper call.
- A function reference, e.g. a zero-argument Boolean helper used without `()`, becomes a lazy condition whose invocation still exposes a helper call/body. Baseline sema permits only declarations with zero parameters and a Bool result, for both RLS helpers and externs. Even a parameter with a default disqualifies a bare function reference. Do not broaden this rule in tracker generation; use the analyzed declaration and distinguish an explicit call with omitted default arguments from a bare reference.
- Do not emit a C++ function address into `value_t`.
- Do not invent support for general parameterized first-class functions. The AST currently marks that syntax as TODO; the consumer condition type is `() -> Bool`.

### Example C: lazy argument and pass-through

```text
define relay(condition: Condition): any_age(condition)

relay(is_adult() and has(RG_HOOKSHOT))
```

Expected graph shape:

```text
FunctionCall relay
  Parameter condition
	ConditionExpression
	  And
		ExternCall is_adult
		ExternCall has
		  Parameter item -> RG_HOOKSHOT
  definition: ExternCall any_age
	Parameter condition
	  Value: scope.get("condition")
```

Creating the argument closure must call neither `is_adult` nor `has`. The `any_age` adapter creates a callback from its actual argument result. Each callback invocation evaluates the captured body in a fresh invocation scope against the then-current host age/time state.

An expression returning a Condition must remain usable after its original helper scope has unwound. A useful regression fixture is a helper that returns its Condition parameter, called from a helper with an `item` parameter; the delayed body must retain that original item while reading current game state. Use AST-level fixtures if a source-level returned-condition form is not yet supported; do not present an invented RLS lambda syntax as implemented grammar.

### Trace fidelity

The baseline stores only the **last** invocation in an argument's `EvalResult::Invocation`; repeated callback calls overwrite that pointer. This is intentional current compatibility behavior, not a full call-history feature. A callback never invoked leaves `Invocation` empty. Different parameters sharing the same condition value must retain separate argument traces. Do not evaluate a callback again merely to populate the UI or fabricate values for skipped operations.

## 10. Operators, grouping, ternaries, and matches

### Example D: grouping and left-fold arithmetic

| RLS expression | Required arithmetic structure | Result |
| --- | --- | --- |
| `2 + 3 + 4` | `+(2, 3, 4)` or equivalent nested left tree | 9 |
| `20 - 3 - 2` | `-(20, 3, 2)` or `-(-(20, 3), 2)` | 15 |
| `24 / 3 / 4` | `/(24, 3, 4)` or `/(/(24, 3), 4)` | 2 |
| `2 + 3 * 4` | `+(2, *(3, 4))` | 14 |
| `(2 + 3) * 4` | `*(+(2, 3), 4)` | 20 |
| `20 - (3 - 2)` | `-(20, -(3, 2))` | 19 |

Preserving the binary AST is always acceptable. Flattening is optional and must preserve order, grouping, intermediate type promotions, and exception behavior. Do not flatten right-nested subtraction/division or blindly reassociate addition/multiplication.

`wallet_capacity()` in production RLS is a nested ternary over wallet `has` calls, yielding 999/500/200/99/0. Generate the full helper definition graph and evaluate only the selected path. A ternary's static children are condition/true/false at positions 0/1/2; runtime traces contain only the selected branch with its correct `ChildIndex`.

### Example E: multi-pattern match, default, and fallthrough

```text
define sample(distance: Distance):
	match distance {
		ED_CLOSE or ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or
		ED_HOOKSHOT: can_use(RG_HOOKSHOT)
		_: never
	}
```

Graph recipe (symbolic IDs, not literal generated C++):

```text
MatchExpression<bool>(distanceRead, [arm0, arm1, arm2])
arm0 = MatchArm(patterns0, swordCall, true)
patterns0 = MatchArmPatterns([ED_CLOSE, ED_SHORT_JUMPSLASH])
arm1 = MatchArm(patterns1, hookshotCall, false)
patterns1 = MatchArmPatterns([ED_HOOKSHOT])
arm2 = MatchArm(defaultPatterns, falseLeaf, false)
defaultPatterns = MatchArmPatterns([])
```

The `or` **between patterns** is a pattern list. The trailing `or` **after an arm body** is the AST's fallthrough flag. Neither should be confused with an ordinary Boolean `OrExpression` in a body.

Required behavior:

1. Evaluate discriminant once; visit arms and patterns in source order.
2. Stop checking an arm's alternative patterns after the first equality match.
3. For the first matching arm, evaluate its body.
4. Non-fallthrough returns that body even if false.
5. Fallthrough returns immediately if the body is truthy; otherwise evaluate the next arm body **without checking its patterns**, continuing by its own fallthrough flag.
6. `_` matches unconditionally when reached normally.
7. Nested matches must restore the runtime's private match binding. Do not emit user variables using its reserved internal name `$match`.
8. Callable values are not legal comparison patterns/discriminants under the current evaluator.

For example, with distance `ED_CLOSE`, sword false, and hookshot true, result is true even though `ED_HOOKSHOT` did not equal the discriminant. With distance `ED_HOOKSHOT`, skip the sword body.

**Typed result contract:** construct generated matches with the analyzed result representation, e.g. `MatchExpression<int32_t>(discriminantId, armIds)`. The template parameter defaults to `value_t` for existing untyped callers (`MatchExpression<>`), preserving pass-through results and an unmatched Boolean false. A supported integral/Boolean type supplies the corresponding zero/false on no match and converts every selected arm result before testing fallthrough truthiness. Thus an Int32 match returns `int32_t{0}` when unmatched, not a Boolean variant. Boolean-normalized mixed Bool/Int matches use `MatchExpression<bool>`. Enums use their selected integral representation; general callable-valued typed matches are unsupported. `MatchExpression<T>` derives directly from `LogicExpression` and stores no runtime target type. Do not add a synthetic visible fallback arm merely to obtain a typed default.

## 11. Root registry, display reconstruction, and entry coverage

A graph is unusable if the tracker cannot find the right root. The backend MUST generate a registry covering every event, location, and exit **occurrence** from the composed project, including unconditional `always`/`never` entries.

### Stable identity and implemented record contract

A root identity should include:

```text
(kind, owning region key, entry key, occurrence discriminator)
```

- Location key: `RC_*`, qualified by owning region.
- Event key: `LOGIC_*`, qualified by owning region; many regions can set the same event.
- Exit key: original declared destination, qualified by source region. Do not derive identity from the destination currently attached after entrance shuffle.
- Occurrence discriminator: deterministic ordinal or equivalent stable source discriminator if duplicate keys within one owner are permitted by sema. If forbidden, surface that diagnostic rather than silently overwrite.

Reuse these contracts from `logic_expression.h`:

```cpp
enum class LogicRootKind { Location, Event, Exit };

struct LogicRootKey {
	LogicRootKind Kind;
	std::string_view OwnerRegion;
	std::string_view Entry;
	uint32_t Occurrence = 0;
	bool operator==(const LogicRootKey&) const = default;
};

struct LogicRootKeyHash {
	size_t operator()(const LogicRootKey& key) const;
};

using LogicRootMap = std::unordered_map<LogicRootKey, index_t, LogicRootKeyHash>;

const LogicRootMap& GetLogicRoots();
```

`OwnerRegion` and `Entry` are literal symbolic keys (`"RR_..."`, `"RC_..."`, etc.), not display names or numeric enum guesses. This keeps the registry independent of game headers. Store original declared exit destinations even after shuffle. Root records contain only identity and expression ID, not source locations or stored condition strings. Reconstruct expression text and rendering through `ToString()`, `GetDrawingSegments()`, and `Draw()`.

The transpiler defines `GetLogicRoots()` with a static const `LogicRootMap` containing full root keys mapped to expression IDs. Construct `LogicRootRegistry(GetLogicRoots(), GetExpressionCount())`; its optional third argument is an `expression_resolver_t`, defaulting to `::GetExpression`. The registry owns its map, accepting it by value and moving it into storage. String backing storage, the resolver, and session-long cached nodes must outlive registry consumers. Map construction and `Find` MUST NOT invoke the resolver or instantiate nodes; lazy expression creation is separate from root storage. Construction rejects empty owner/entry keys, unknown kinds, out-of-range expression IDs, invalid descriptor counts, and a null resolver with `std::invalid_argument`. There is no separate runtime duplicate-key detection pass. Only `registry.GetExpression(key)` resolves the requested root. Resolver/factory failures propagate and null resolver results throw when requested; an unconstructed root is not invalid merely because it has no cached pointer yet. Registry validation does not replace complete compiler graph validation. Distinct keys may share an expression ID.

`Find(key)` uses hashed lookup and returns `std::optional<index_t>` (`std::nullopt` when missing); an engaged value of zero is a valid expression ID. `GetExpression(key)` returns its `const LogicExpression*`, or `nullptr` for a missing key, never a fallback expression. The registry exposes no iteration or grouped-lookup API, and map iteration order is not a contract. No string-only legacy lookup is implicitly provided; any such bridge must follow the compatibility rules below.

### Region/extension composition

For every region, collect its core sections **and every** `extend region` block targeting it. Vanilla/MQ region declarations and shuffle files are part of the same project. Preserve declaration ownership; retain source provenance inside the compiler for diagnostics, not in emitted runtime root records.

Real examples:

- `src/dungeons/deku_tree.rls` contains entryway/boss regions, not the entire dungeon.
- `src/dungeons/deku_tree_vanilla.rls` and `deku_tree_mq.rls` supply separate region sets.
- `src/shuffles/hints.rls` extends both `RR_DODONGOS_CAVERN_LOBBY` and `RR_DODONGOS_CAVERN_MQ_GOSSIP_STONE` with the same `RC_DODONGOS_CAVERN_GOSSIP_STONE` check but different conditions.

Both hint roots must exist, with different owning regions and appropriate conditions. Do not deduplicate them merely because their location key matches. Do not treat the currently active editor file as the entire translation or pre-prune logic based on the generator machine's current randomizer settings.

### Example F: event versus location versus exit

Actual source from `src/dungeons/deku_tree.rls`:

```text
region RR_DEKU_TREE_BOSS_ROOM {
	name: "Deku Tree Boss Room"
	scene: SCENE_DEKU_TREE_BOSS
	events {
		LOGIC_DEKU_TREE_CLEAR: can_kill(RE_GOHMA)
	}
	locations {
		RC_QUEEN_GOHMA: flag(LOGIC_DEKU_TREE_CLEAR)
		RC_DEKU_TREE_QUEEN_GOHMA_HEART: flag(LOGIC_DEKU_TREE_CLEAR)
	}
	exits {
		RR_DEKU_TREE_BOSS_EXIT: always
		RR_KF_OUTSIDE_DEKU_TREE: flag(LOGIC_DEKU_TREE_CLEAR)
	}
}
```

Generate five root records: one event, two locations, two exits. Reusing the same immutable `flag(...)` graph is allowed, but root identities remain distinct. The event root is its **setter condition** (`can_kill`); the location/exit roots **read** the event. Do not replace either with the other or mutate event state while explaining a condition.

### Structural display data

- Keep human-facing RLS names and leaf labels in expression nodes, not only C++ implementation names. Reconstruct complete expressions from graph structure with the existing display APIs.
- Do not emit per-root source paths/spans or duplicate RLS/legacy condition strings. Keep source provenance in the compiler for generation diagnostics, including helper definition versus call site.
- Preserve parameter names even for defaults and named arguments. Identifying synthesized defaults in metadata is recommended.
- Escape quotes, backslashes, newlines, and control characters correctly in C++ strings. Preserve Unicode text. Do not embed absolute user directories or timestamps.
- Runtime `ToString()` may normalize formatting. Do not store exact source slices or require byte-for-byte reproduction from structural nodes where the runtime cannot preserve syntax aliases.
- The existing `ValueExpression` `isEvent` member is stored but not currently used by its renderer. Generate explicit root/event dependency metadata if navigation needs it. Do not claim that setting this flag on an enum key implements event navigation or event evaluation.

### Compatibility with current string-based access objects

The baseline `EVENT_ACCESS`, `LOCATION`, and `ENTRANCE` macros store `CleanConditionString(#condition)`. Those strings are **generated C++**, not the original RLS text and not necessarily `LogicExpression::ToString()` output.

Preferred integration: attach a generated stable root identifier to each access/entrance object, or maintain an explicit side table that associates those objects with root identities. This requires a coordinated, limited change to ordinary SoH emission/Shipwright consumption, not replacement of its gameplay predicates.

If initial integration must use `GetConditionStr()`, emit an explicit compatibility lookup using exactly the ordinary emitter's condition representation and the same C++ stringification/cleaning rules. Test actual compiled macro strings. Whitespace, enum qualification, Boolean conversions, defaults, and lambda text all matter; reconstructing C++ text from tracker `ToString()` is not valid. String collisions may share a root only when their evaluated structure/context semantics are equivalent; otherwise require owner/entry identity and report ambiguity.

The RLS deliverable must state which integration route it supports. The Shipwright consumer must replace the hard-coded lookup before end-to-end tracker completion can be claimed.

## 12. Runtime coordination checklist

These issues must be resolved or explicitly delivered as blocked compatibility work; they are not permission to silently reduce semantics:

| Issue | Required handling |
| --- | --- |
| Prototype factory conflicts with generated resolver/count | Remove/exclude prototype definitions in the Shipwright integration build; exactly one definition each of `GetExpression(index_t)` and `GetExpressionCount()`. |
| Private prototype state guard | Proposed `TrackerHostStateGuard` from a documented consumer integration header, with a local test implementation and downstream restoration tests. |
| Prototype root coverage limited to eight Kokiri Shop checks | Generate complete root records and validate actual access-object association, including events and duplicate occurrences. |
| Typed coercions | Use the implemented transparent `ConversionExpression<T>`; retain operands and original display text without extra UI rows. |
| Numeric/enum match result types | Supply the analyzed template target in `MatchExpression<T>`; `MatchExpression<>` deliberately retains legacy behavior. |
| Captured/returned conditions versus ordinary backend limitations | Treat semantically analyzed language intent and runtime closure tests as contract; flag ordinary emitter bugs rather than copy them or silently alter that backend. |
| Ordinary C++ match re-emits its discriminant in pattern predicates | Tracker node evaluates once. Hosts should be query-like; test/document behavior for stateful externs rather than assuming call counts match ordinary code in this case. |
| Recursive helper graph | Diagnose until row construction/evaluation can handle cycles. |
| Host bridge retains callback beyond call | Unsupported by current borrowed argument tracing; diagnose/configure binding restriction. |
| Root registry data | Generate `GetLogicRoots()` using the implemented record schema; actual access-object association remains integration work. |

No arbitrary runtime source changes should be bundled into the RLS repo under the guise of generated output. If additional runtime functionality is essential, describe its exact API and add compatibility fixtures, with a clearly separated Shipwright follow-up.

## 13. Diagnostics and failure behavior

Use the normal compiler diagnostic path, with source span and a useful symbol/type description. Generation MUST fail for a required condition that cannot be represented. Validate before committing successful artifacts; avoid partial output that looks usable after failure.

Cover at least:

- Missing analyzed type, unresolved call/declaration/`here`, or absent resolved argument metadata.
- Unsupported host type/mapping or reachable runtime String/List/general callable value.
- Unknown operator or invalid node arity.
- Nonrepresentable numeric/enum value, node index overflow, or parameter index overflow.
- Unsupported recursive graph, with dependency chain/call site.
- Ambiguous root registration or unsupported duplicate ownership under the chosen key scheme.
- Required coercion or callback/return type not representable by the baseline ABI.

Do not emit empty callbacks, empty C++ expressions, default false roots, or a fallback numeric index to hide errors. Unreachable unused declarations may be omitted if reachability is deterministic and all entry roots and their dependencies remain included; test that policy.

## 14. Acceptance tests

Snapshot tests alone are insufficient. In the RLS-only checkout, test analyzed-IR structure, diagnostics, deterministic output, and C++ syntax/API compatibility using declarations derived from section 16 and controlled host fixtures. Do not call such fixtures real-runtime compatibility tests or invent a substitute evaluator and claim consumer parity. Executable semantic/lifecycle/UI tests against the real consumer are a separate downstream acceptance gate; report them as pending when that runtime is unavailable. The matrix below specifies both the RLS-local generation assertions and downstream behavioral obligations.

### Required test matrix

| Area | Required checks |
| --- | --- |
| Minimal output | Empty/minimal projects compile; metadata queries allocate no nodes; repeated `GetExpression(id)` returns stable pointers; each descriptor resolves when requested; two TUs can include generated header without ODR violations. |
| Lazy lifecycle | Factories construct only requested nodes; skipped evaluation branches and unexpanded helper bodies remain unconstructed until needed; shared helpers are cached once; failures/null returns/recursive construction clean up and allow retry; virtual destruction releases cached nodes; cache growth preserves captured pointers; no stale result caching. |
| Structural lowering | All node kinds above; helper call retains expandable body; extern arguments are real `ParameterExpression` nodes, including zero/defaulted/named-argument cases. |
| Values | Bool, supported integral boundaries, local/host enums, `Enum.Member`, declared region/location/event keys, and lexical `here` in core/extension/delayed bodies. |
| Numeric semantics | Arithmetic table in example D; integer division; division-by-zero exception; numeric comparison and `is`/`is not`; enum/Int and Bool/Int conversions permitted by sema. |
| Transparent conversions | All target variants, invalid/callable rejection, single evaluation, nested conversions, converted result replacing the operand result without an extra trace, hidden rows/text, preserved parentheses/colors, and unchanged child/invocation positions. |
| Lazy branches | `never and fail()` and `always or fail()` skip `fail`; unselected ternary branch never executes; trace omits skipped children rather than records false. |
| Defaults | `check_price()` passes `RC_UNKNOWN_CHECK`; explicit check honored; `spirit_shared` supplies all seven arguments with false Condition defaults wrapped lazily. |
| Parameters | Declaration-order named arguments; caller-scope evaluation; nested shadowing; no binding leakage on success or exceptions; duplicate arguments traced separately; 256 positions allowed, 257th rejected. |
| Closures | No eager body evaluation; captured item survives helper return; live state is not captured; Condition pass-through; function reference; explicit returned-condition invocation; non-callable invocation rejection. |
| Callback traces | Never-called callback has no invocation; repeated calls retain final trace; independently shared callable arguments have separate traces; nested callbacks retain correct ownership. |
| State/context | `any_age`/`spirit_shared` restore all changed state even on throw; all four age/time scenarios; correct owning-region/check context; lexical `here` does not change with host region switches. |
| Matches | Single/multiple patterns, `_`, no match, nested match, false non-fallthrough stop, true fallthrough stop, false fallthrough ignores next patterns, final fallthrough; typed numeric/enum fallback and mixed Bool/Int arms. |
| Root coverage | Locations/events/exits, always/never, multiple setters, same check in vanilla/MQ regions, all shuffle extensions, duplicate occurrence policy, shuffled exit retains original root identity. |
| Lookup | Actual access object/string bridge selects correct graph; missing/ambiguous query reports unavailable/error, never selects unrelated demo root. |
| Display reconstruction | RLS display names, parameter labels, structural `ToString()`/drawing output, quotes/backslashes/newlines/Unicode; root records contain no source spans or stored condition strings. |
| Limits/reproducibility | Index 4,294,967,295 and node count 4,294,967,296 are representable, next index/count rejected; exercise boundary checks with synthetic counts rather than allocating billions of nodes. Reordered filesystem enumeration and different checkout directories yield identical output; recursion gets a diagnostic. |
| Tooling | CLI/manifest dispatch and help for chosen integration, output writing failures, source diagnostics, multiple backends in same invocation do not alter each other's results. |

The downstream runtime has tests named `LazyCaptureAndLiveState`, `ReturnedConditionAndInvoke`, `ConditionPassThrough`, `RepeatedAndSkippedCallbacks`, `FinalInvocationShortCircuit`, `NestedCallbackTraces`, `SharedCallableArgumentsRecordSeparately`, `ExternArgumentEvaluationOrder`, `FunctionArgumentBindings`, `ExceptionCleanup`, `FalseDefaultAndFunctionReference`, `ParameterEvaluationTrace`, `LargeParameterCount`, `MatchPatternsAndDefault`, `MatchFallThrough`, and `NestedMatchAndExceptionCleanup`. These names are informational, not files/tests the receiving agent must locate or run. Required scenarios are specified in the matrix and embedded contracts.

The implemented prerequisites also have regression groups `ValueConversions`, `ConversionTraces`, `ConversionPresentation`, `TypedMatchResults`, `TypedMatchFallThrough`, `RootRegistryLookup`, and `RootRegistryValidation` in the same harness.

Lazy construction is covered by `LazyConstructionAndLookup`, `LazyConditionConstruction`, `LazyCacheFailureRecovery`, and `LazyCacheBoundsAndStability`, including full-width descriptor counts without allocating billions of slots.

For RLS-local testing, combine the embedded examples and synthetic local fixtures with available `RLS/examples/soh/src` inputs. Include helper-heavy logic, arithmetic/ternaries, Spirit Shared, variant regions, and separate extension files; report fixture node/root counts and verify every composed entry has a map key. Exercise IDs above 65,535 without pruning roots. A downstream maintainer must separately run the complete production corpus (the unavailable `LOGIC/src`) and actual consumer integration, report production counts, and confirm the 32-bit ABI. Do not fabricate production counts or make an external checkout a prerequisite for delivering the RLS backend.

For ordinary-C++ versus tracker parity, evaluate both from identical reset host snapshots and context. Compare results, appropriate types/conversions, observable query behavior, and skip behavior. Do not evaluate one after the other against already mutated state. Where the ordinary emitter has a known mismatch (e.g. discriminant evaluation or capture generation), isolate and document it rather than weakening tracker correctness.

## 15. Suggested implementation sequence and delivery checklist

1. Add a typed tracker IR carrying node kind, children, value/adapter payload, inferred type, and provenance; lower from analyzed AST without emitting strings first.
2. Resolve all roots and helper dependencies, normalize arguments/conditions/coercions, and validate graph limits/cycles.
3. Emit count metadata, per-ID factories, the cached indexed resolver, and typed extern adapters; compile a small fixture against the baseline evaluator.
4. Emit deterministic root identity/index records and implement the selected access-object compatibility contract.
5. Integrate the chosen backend invocation, build outputs, schema/help, and tests without changing existing target behavior.
6. Run the RLS-local structural/diagnostic/compiled-output tests and embedded corpus; deliver results and explicit pending downstream runtime/production-corpus validation.

Final handoff should contain:

- Generator implementation and invocation documentation.
- Generated examples covering sections 7–11, with node/root listings.
- Unit tests, compiled/runtime compatibility tests, and commands/results.
- Exact generated files and required include/link dependencies; any dependence on `functions.gen.h`/`rls_host.h` is explicit.
- Root registry map schema, lookup failure behavior, and deterministic generated-source emission rules (runtime map order is unspecified).
- A short Shipwright integration checklist: exclude prototype factory/resolver/count/root records/adapters; compile generated TU(s); provide guard/includes; keep construction demand-driven with no startup resolution walk; associate access objects with roots; preserve context; replace the shop-only prototype registry with complete generated coverage; run tracker check/exit/event smoke tests.
- Any unavoidable runtime ABI extension described separately with before/after signatures and regression tests.

## 16. Embedded consumer contract for an RLS-only checkout

This section and the semantic rules above are the handoff contract. No Shipwright or ShipOfHarkinian-RandoLogic checkout, private header, game binary, or companion attachment is required to implement the RLS backend. Do not implement a replacement game runtime in RLS. Generated artifacts target the consumer APIs below; downstream integration supplies their implementations.

### Consumer declarations

The following is a declaration contract, not a standalone header to paste verbatim: include standard C++20 headers, order forward declarations appropriately, and derive each concrete expression from `LogicExpression`. The consuming header is named `logic_expression.h`. Its implementations and explicit template instantiations are linked by the consumer.

```cpp
struct VariableScope;
struct ConditionValue;
struct EvalResult;
struct ImVec4;
class LogicExpression;
using index_t = uint32_t;
using condition_t = std::shared_ptr<const ConditionValue>;
using value_t = std::variant<bool, int8_t, int16_t, int32_t,
						   uint8_t, uint16_t, uint32_t, condition_t>;
using bindings_t = std::unordered_map<std::string, value_t>;
using function_t = value_t (*)(VariableScope&);
using extern_function_t = value_t (*)(std::span<EvalResult>);
using expression_factory_t = std::unique_ptr<LogicExpression> (*)(index_t);
using expression_resolver_t = LogicExpression* (*)(index_t);

struct ConditionValue {
	const LogicExpression* Body;
	bindings_t Bindings;
};
struct EvalResult {
	const LogicExpression* Expression = nullptr;
	value_t Result;
	std::vector<EvalResult> Children;
	size_t ChildIndex = 0;
	std::shared_ptr<const EvalResult> Invocation;
};
struct VariableScope {
	VariableScope();
	VariableScope(const VariableScope&) = delete;
	VariableScope& operator=(const VariableScope&) = delete;
	void push(std::string_view name, value_t value);
	value_t get(std::string_view name) const;
	void pop(std::string_view name);
	bindings_t Capture() const;
	~VariableScope();
};

class LogicExpression {
  public:
	virtual ~LogicExpression();
	virtual EvalResult Evaluate(VariableScope&) const = 0;
	virtual std::string ToString() const = 0;
	virtual std::vector<std::pair<ImVec4, std::string>>
		GetDrawingSegments(size_t startColorIndex = 0) const;
	virtual size_t GetChildColorIndex(size_t childIndex, size_t startColorIndex) const;
	virtual const LogicExpression* GetDisplayExpression() const;
	void Draw(size_t startColorIndex = 0) const;
	// GetChildren() returns a sized, random-access lazy view of LogicExpression*.
};
class LogicExpressionCache {
  public:
	LogicExpressionCache(uint64_t count, expression_factory_t factory);
	LogicExpression* GetExpression(index_t);
	uint64_t GetExpressionCount() const;
	size_t GetCachedExpressionCount() const;
	// Noncopyable; owns all constructed nodes until destruction.
};

bool ToBool(const std::optional<value_t>&);
template <typename T> T ToInt(const std::optional<value_t>&);
// LogicTarget accepts exactly the seven bool/integer alternatives of value_t.
template <LogicTarget T> value_t ConvertValue(const value_t&);
EvalResult Invoke(const value_t&);
std::function<bool()> MakeConditionCallback(EvalResult&);
LogicExpression* GetExpression(index_t); // generated definition
```

`ToBool`/`ToInt` accept a `value_t` through optional conversion. Missing values become false/zero; condition values throw rather than invoke. Scope lookup is lexical and throws for an absent name. `Capture()` copies the currently visible bindings, not game state. `Invoke` rejects nonconditions, creates a fresh scope from captured bindings, evaluates `Body`, and returns its trace. `MakeConditionCallback(argument)` invokes `argument.Result` synchronously, replaces `argument.Invocation` with the latest invocation trace, and returns its Boolean truthiness; a skipped callback leaves no invocation. It does not retain a history of repeated calls.

Concrete constructor signatures (all classes have the evaluation/display overrides required by their bases):

```cpp
AndExpression(std::vector<index_t> children);
OrExpression(std::vector<index_t> children);
NotExpression(index_t child);
ArithmeticExpression(std::string_view operation, std::vector<index_t> children);
ComparisonExpression(std::string_view operation, index_t left, index_t right);
TernaryExpression(index_t condition, index_t trueBranch, index_t falseBranch);
FunctionCallExpression(std::string_view name, std::vector<index_t> parameters, index_t definition);
ExternFunctionCallExpression(extern_function_t adapter, std::string_view name,
							 std::vector<index_t> parameters);
ParameterExpression(uint8_t position, std::string_view name, index_t child);
ValueExpression(function_t evaluator, std::string_view displayText, bool isEvent);
ConditionExpression(index_t body);
InvokeExpression(index_t callable);
ConversionExpression<T>(index_t operand); // T satisfies LogicTarget
MatchExpression<T>(index_t discriminant, std::vector<index_t> arms);
MatchArm(index_t patterns, index_t body, bool fallThrough);
MatchArmPatterns(std::vector<index_t> patterns); // empty means wildcard
```

`AndExpression`, `OrExpression`, and `ArithmeticExpression` derive from `BinaryExpression`; `ExternFunctionCallExpression` derives from `FunctionCallExpression`; the other listed nodes derive directly from `LogicExpression`. `MatchExpression` defaults `T` to `value_t`; typed matches accept the seven `LogicTarget` types. Conversion and match implementations are supplied by the consumer with explicit instantiations for those types (and `value_t` for matches). Noncapturing value lambdas must explicitly return a supported `value_t` alternative. All string views require session-long backing storage.

Use section 11's exact root key/map declarations and generated `const LogicRootMap& GetLogicRoots()`. The consumer provides `LogicRootKeyHash` and `LogicRootRegistry(LogicRootMap, uint64_t, expression_resolver_t = ::GetExpression)`, `Find(key) -> std::optional<index_t>`, and `GetExpression(key) -> const LogicExpression*`. A standalone API-check fixture may supply a hash with equivalent equality consistency; hash bit patterns are not an ABI requirement. `GetExpressionCount()` is a generated helper, not a required declaration in the consumer header; define it before generated use or declare it in the generated header if needed across translation units.

### Host binding signatures

These are consumer-provided global C++ functions. Enum **types and symbolic constants** are supplied by the game or test host; reuse the RLS checkout's SoH enum mappings. Never copy guessed production numeric values into generated output. Test-only enum values may be synthetic if assertions compare symbolic identities, not production ordinals.

```cpp
using ConditionFnCap = std::function<bool()>;
uint8_t setting(RandomizerSettingKey);
bool has(RandomizerGet);
bool flag(LogicVal);
bool trick(RandomizerTrick);
bool is_child(); bool is_adult(); bool at_day(); bool at_night();
bool is_vanilla(); bool is_mq();
bool any_age(ConditionFnCap);
bool spirit_shared(RandomizerRegion, ConditionFnCap, bool,
				   RandomizerRegion, ConditionFnCap, RandomizerRegion, ConditionFnCap);
bool spirit_certain_access(RandomizerRegion);
bool can_use(RandomizerGet);
bool can_plant_bean(RandomizerRegion, RandomizerGet);
int bottle_count(); int ocarina_buttons(); bool bombchus_enabled();
int effective_health(); int hearts(); int dungeon_count();
int stone_count(); int medallion_count(); int fire_timer(); int water_timer();
int get_gs_count(); int get_big_poe_count();
int check_price(RandomizerCheck); // RLS default is RC_UNKNOWN_CHECK
bool trial_skipped(TrialKey);
bool can_build_rainbow_bridge(); bool can_trigger_lacs();
bool small_keys(SceneID, int);
int required_triforce_pieces(); int collected_triforce_pieces();
```

For default arguments, use the analyzed RLS declaration, not the absence of C++ defaults in this signature list. The seven-argument `spirit_shared` RLS signature/defaults are embedded in section 7. Select reachable bindings from the actual analyzed project, including custom externs; this list describes the baseline SoH host, not a closed list of legal RLS externs.

Generated adapters may include consumer-supplied `rls_host.h` and a documented generated-integration header. That header must provide the game declarations above and the agreed state-guard type `TrackerHostStateGuard`; this is a **proposed downstream integration requirement**, not an existing public class. For an RLS-only compiled fixture, provide those declarations and a test implementation of the guard. It must restore `IsChild`, `IsAdult`, `AtDay`, `AtNight`, and `CurrentRegionKey` on normal/exceptional exit; the outer consumer establishes and restores check context. Do not generate references to the private prototype `LogicStateGuard`. Do not duplicate the game's mutable global structures inside generated code.

### Embedded standalone corpus

Use the worked inputs in sections 7–11 plus this core/extension fixture as test input; no external production files are required. Add matching extern/key declarations using the syntax and host mappings already present in the local RLS checkout. Symbols such as `SCENE_KOKIRI_SHOP` are fixture labels, not assertions that the production game defines those exact constants; declare synthetic host symbols locally or substitute locally supported names consistently.

```text
// core.rls
region RR_KF_KOKIRI_SHOP {
	name: "Kokiri Shop"
	scene: SCENE_KOKIRI_SHOP
}
// shops.rls, a separate file in the same project
extend region RR_KF_KOKIRI_SHOP {
	locations {
		RC_KF_SHOP_ITEM_1: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_2: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_3: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_4: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_5: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_6: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_7: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
		RC_KF_SHOP_ITEM_8: has(RG_SPEAK_KOKIRI) and check_price() <= wallet_capacity()
	}
}
```

`wallet_capacity` is an ordinary RLS helper, not a new C++ host binding. For this standalone fixture define it as a typed integer helper using the repository's normal syntax (a constant return of 99 is sufficient). Expect eight distinct root keys with the same owner and entries numbered 1–8. Sharing their identical graph is permitted, but no numeric ID, including the prototype's zero, is prescribed. Add local synthetic variant/extension files to exercise distinct owners, shared check keys, and deterministic composition without needing the production corpus.

**RLS-only definition of done:** generator, local fixtures, structural/diagnostic tests, generated C++ API checks, and documented consumer requirements are complete. Runtime/game integration and full production-corpus validation must be explicitly listed as pending until executed against the actual consumer; do not claim those results from declaration-only fixtures.
