# AP Function Generation

Scope: `transpilers/ap` (generic) + `transpilers/soh_ap` (SoH)

How RLS `define` functions become Python functions for the Archipelago
RuleBuilder target. The C++ `soh` target generates functions trivially; the AP
target cannot, because a `Bool` is not a uniform type there. This document
specifies why that is hard and how generation handles it. `SohApTranspiler::Transpile`
emits `functions.gen.py`; the section references below point at the code and
tests that realize each piece.

---

## 1. The problem in one sentence

In the C++ target every RLS `Bool` is a uniform C++ `bool`, so `&&`, `!`, and
the ternary operator just work. In the AP target a `Bool` is **either** a
runtime `Rule` object **or** a build-time Python `bool`, the two cannot be freely
combined, and the RLS surface syntax does not tell them apart. Function
generation is hard because it has to recover that distinction and bridge it.

---

## 2. The target model (RuleBuilder), and its hard constraints

Access logic in the AP world is built from `Rule` objects
(`rule_builder/rules.py`): `True_()`, `False_()`, `Has(...)`, `And`, `Or`,
`Filtered` (option filters), etc. A rule is bound to a region/location as a
callback `Callable[[bundle], Rule]` — `lambda bundle: <Rule>` — where
`bundle` is the `(parent_region, world)` tuple. The framework calls the lambda
with the bundle and `resolve()`s the resulting rule against collection state.

Three constraints make the model strict. All are load-bearing for this design:

1. **A `Rule` is not a Python boolean.** `Rule.__bool__` raises:
   ```python
   # rule_builder/rules.py:187
   raise TypeError("Use & or | to combine rules, or use `is not None` for boolean tests")
   ```
   ⇒ a `Rule` can never appear as the condition of a Python `if`, ternary,
   `and`, `or`, or `not`.

2. **`&` / `|` combine `Rule` with `Rule` only.** `Rule.__and__` /`__or__`
   accept `Rule | OptionFilter | Iterable[OptionFilter]`. A plain `bool` falls
   through to `self.options == other.options` and raises `AttributeError`
   (`bool` has no `.options`). ⇒ you cannot splice a Python `bool` into a rule
   expression with `&` / `|`.

3. **There is no runtime negation.** No `__invert__`, no `Not` rule class, so a
   *collection-state* rule (`has`, `can_use`, …) cannot be negated. A rule built
   only from `setting(...)` is the exception: it resolves at build time against
   `world.options`, so the transpiler negates it *structurally* via De Morgan —
   see §4.2. In the reference world `not` likewise appears only on `world.options`,
   never on a collection rule.

---

## 3. The central distinction: three value classes

A rule callback `lambda bundle: <expr>` runs **once** to build a `Rule` tree;
only the resulting `Rule` re-evaluates against collection state at solve time
(`Rule.resolve(world)` → `Resolved.__call__(state)`). So the question that
decides how an expression lowers is *when its value is known*, and there are
**three** answers, not two:

| Class | Lowers to | Known when | Examples |
|------|-----------|------------|----------|
| **R** (rule) | a `Rule` object | re-evaluated each solve step | `has(X)`, `can_use(X)`, `trick(X)`, `flag(X)`, `can_kill(...)`, **`setting(K)` / `setting(K) is V`** (→ OptionFilter rule), any user define that is R, `true`/`false`/`always`/`never` → `True_()`/`False_()` |
| **V** (build-time value) | a Python `int` / `bool` | frozen when the lambda runs | int/enum literals, arithmetic over V, **`Bool`/`Int` parameters** (bound to literals/config at the call that builds the rule), value-defines like `distance_to_int`, comparisons over V operands (`distance_to_int(d) <= N`) |
| **RV** (runtime non-rule value) | *nothing directly* | depends on collection state, but is **not** a `Rule` | `bottle_count()`, `collected_triforce_pieces()`, and any comparison over them (`bottle_count() >= 1`) |

The RLS *type* does not decide this — `has(X)`, `wall_or_floor` (a `Bool` param),
and `bottle_count() >= 1` are all `Bool`, yet they are R, V, and RV respectively.

> **Definition.** Classify bottom-up. A host call returning `Bool` is **R**; a
> host call returning anything else (e.g. `Int`) is **RV** — it reads collection
> state but is not a rule. Literals, enum values, and parameters are **V**. A
> user define takes its body's class (params treated as V). Operators fold their
> operands worse-of: **RV** dominates **R** dominates **V** (`and`/`or`,
> comparisons); a ternary/match is R if any branch/arm is R, else V if condition
> and branches are all V, else RV.
>
> **Why RV is its own class — the trap.** `bottle_count() >= 1` is *not*
> build-time: bottles are collected during the solve. Folding it as a ternary
> condition (`has(X) if bottle_count() >= 1 else False_()`) freezes it to the
> **initial, empty** collection state — a silent miscompile. It is also not a
> `Rule`, so it cannot combine with `&`/`|`. So the transpiler refuses to emit an
> RV value: it raises a diagnostic (§6.4) pointing the author at a host rule. The
> correct representation is a dedicated host rule — e.g. the reference world's
> `has_bottle_count(1)`, a custom `Rule` that counts at resolve time. The one RV
> define in the stdlib, `has_bottle`, is host-provided and skipped (§6.5); the
> transpiler does **not** synthesize host rules itself.
>
> **Note on settings.** Unlike the upstream reference world — where settings are
> build-time `world.options` and `not` applies to them directly — this transpiler
> emits `setting(K)` as `True_(options=[OptionFilter(K, …)])`, an atomic Rule. So
> settings are **R**, and `not setting(K)` is `OptionFilter(K, False)`, not a
> Python `not`.

Classification is the keystone the rest of the design rests on. It is
implemented as `ApTranspiler::ClassifyExpression` → `{Rule, BuildTime, Runtime}`
(`classify_expression.cpp`), pure analysis over the resolved AST, and pinned by
`ApClassify.*`.

---

## 4. Bridging the two worlds

Boolean operators in RLS are uniform; in AP their lowering depends on the
classes of their operands.

### 4.1 `and` / `or`

| Operands | Lowering | Why |
|----------|----------|-----|
| `R and R` | `R & R` | rule conjunction |
| `R or R`  | `R \| R` | rule disjunction |
| `V and V` | `V and V` (Python) | both build-time |
| `V or V`  | `V or V` (Python) | both build-time |
| `V and R` | `R if V else False_()` | build-time short-circuit |
| `V or R`  | `True_() if V else R`  | build-time short-circuit |
| `R and V` | `R if V else False_()` | (commute) |
| `R or V`  | `True_() if V else R`  | (commute) |
| `RV` involved | **unsupported → diagnostic** | the build-time condition would freeze a runtime value |

The mixed `V`/`R` cases are legal Python because the *condition* of the emitted
ternary is `V` — a value genuinely frozen when the lambda runs (a literal, a
parameter bound at the build call) — and only the branches are rules. This is the
only sound way to fold a build-time fact into a runtime rule.

The crucial guard is that the condition must be **V, not RV**. `wall_or_floor and
can_use(X)` is fine (`wall_or_floor` is a `Bool` parameter, frozen at build).
`bottle_count() >= 1 and has(X)` is **not** — `bottle_count() >= 1` is RV, and
freezing it as the condition is the miscompile of §3. Such expressions are
`Unrepresentable` in `ClassifyAndOr`: they raise a diagnostic (§6.4) and emit a
best-effort rule-op fallback. Lowering an RV to a host rule is left to the author
(the transpiler does not synthesize one). Pinned by `ApBridging.*`.

### 4.2 `not`

| Operand | Lowering |
|---------|----------|
| `not V` (build-time) | `not V` (Python) — a build-time bool, e.g. `not wall_or_floor` (a `Bool` param) |
| `not <pure option-filter rule>` | **De Morgan negation** — push `not` through `and`/`or` (swapping them), flip each `setting(K) is V` leaf to its `"ne"` form, and inline a no-arg pure-setting define's body. Covers `not setting(K)` (→ `OptionFilter(K, False)`), `not (setting(K) is V)` (→ `OptionFilter(K, V, "ne")`), negated membership (`not (A or B or C)` → AND of `"ne"` filters), and `not is_fire_loop_locked()`. Sound because settings resolve at build time. |
| `not R` (collection rule) | **unsupported → diagnostic** (no negation for a collection-state rule) |
| `not RV` | **unsupported → diagnostic** |

A "pure option-filter rule" is one built entirely from `setting(...)` comparisons,
bool literals, and `and`/`or`/`not` of those (or a no-arg define whose body is one).
The `not` over anything containing a collection rule (`has`, `can_use`, …) is impure
and stays a diagnostic. Implemented by `IsPureOptionFilterRule` +
`GenerateNegatedOptionFilterRule` (`generate_expression.cpp` / `classify_expression.cpp`),
pinned by `ApNegation.*` and `SohApRendering.NotFireLoopLockedNegatesKeysanityMembership`.

### 4.3 Ternary `cond ? a : b`

| Condition | Lowering |
|-----------|----------|
| `cond` is **V** | `a if cond else b` (legal; `a`/`b` may be R or V as long as they agree) |
| `cond` is **R**, rule branches | `(cond & a) \| b` — the then-branch is gated by the condition, the else-branch is unconditional. No rule negation is needed and no complement of `cond` is synthesized (the source never wrote one). More permissive than a strict ternary, but **monotonic**, which is what access logic wants: gaining `cond` never removes the else-branch's access. |
| `cond` is **R**, a value branch (or **RV** condition) | **unsupported → diagnostic** (a Rule cannot be a Python `if`, and `cond & <value>` is ill-typed) |

The rule-conditioned rule-branch case is selected by `isRuleConditionedRuleTernary`
(`generate_expression.cpp`), pinned by `ApTernary.*` (generic) and
`SohApHostRewrites.AgeConditional*` (SoH bundle/enum rendering).

---

## 5. Worked examples

### 5.1 Pure-R function

```
# RLS
define has_explosives():
    can_use(RG_BOMB_BAG) or can_use(RG_BOMBCHU_5)
```
```python
# AP (bundle-first; -> bool is the body's RLS type — see the §6.3 limitation)
def has_explosives(bundle) -> bool:
    return can_use(bundle, Items.RG_BOMB_BAG) | can_use(bundle, Items.RG_BOMBCHU_5)
```

### 5.2 Pure-V function — plain Python

```
# RLS
define distance_to_int(distance):
    match distance { ED_CLOSE: 0  ED_SHORT_JUMPSLASH: 1  ... }
```
```python
# AP — value match: returns an int, no rules involved. `rls_match_value`'s first
# arg is the type-appropriate default (0 here); each arm is (condition, body, fallthrough).
def distance_to_int(bundle, distance: EnemyDistance) -> int:
    return rls_match_value(0, (lambda distance=distance: distance == EnemyDistance.ED_CLOSE), (lambda: 0), False, ...)
```

### 5.3 Mixed V/R — the hard case

```
# RLS (from can_get_drop)
can_kill(e, distance) and
    (distance_to_int(distance) <= distance_to_int(ED_MASTER_SWORD_JUMPSLASH) or match e { ... })
```
- `can_kill(e, distance)` → **R**
- `distance_to_int(distance) <= …` → **V** (both sides build-time ints)
- `match e { … }` (rule bodies) → **R**

So the structure is `R and (V or R)`, lowering to (schematically):
```python
can_kill(...) & (True_() if distance_to_int(bundle, distance) <= distance_to_int(bundle, EnemyDistance.ED_MASTER_SWORD_JUMPSLASH) else rls_match_rule(...))
```

### 5.4 Unrepresentable — `Int` that depends on a runtime rule

```
# RLS
define wallet_capacity():
    has(RG_TYCOON_WALLET) ? 999 : has(RG_GIANT_WALLET) ? 500 : ... : 0
```
The ternary condition is **R** and the result is an `Int`. There is no way to
produce a state-dependent integer in this model, so `wallet_capacity` is **not
generated** — it is host-provided and skipped via `isHostProvidedDefine` (§6.5).
Its only consumer, `check_price(x) <= wallet_capacity()`, is special-cased at the
call site to `can_afford_slot(x)` (`soh_expression.cpp` `renderBinarySpecialCase`).
Same story for the triforce comparison → `CanWinTriforceHunt()`.

---

## 6. Cross-cutting behavior

### 6.1 `match` over rules

`match` with `or`-fallthrough accumulates bodies. Because `bool(Rule)` raises
(§2 constraint 1), the fallthrough path can never test a rule body with
`if bool(body())`. Two transpile-time-selected helpers in
`transpilers/soh_ap/src/rls_match.py` handle the two cases, chosen by the arm
bodies' value class (`generate_expression.cpp`, pinned by `ApMatch.*`):
- **value match** (build-time bodies) → `rls_match_value(default, …)` returns the
  selected value; the codegen passes `0` or `False` as the type-appropriate
  default;
- **rule match** (rule bodies) → `rls_match_rule(…)` `|`-combines the matched arm
  with the arms it falls through into, defaulting to `False_()`. It never calls
  `bool()` on a rule.

A match whose bodies are **runtime non-rule** values (RV) is diagnosed (§6.4).

### 6.2 Receiver position: bundle-first

`bundle` is the **first** parameter everywhere — both in the host-call rewrites
(`has_item(bundle, X)`) and in generated function signatures
(`is_child(bundle)`, `can_kill_enemy(bundle, …)`). The reference `LogicHelpers`
is itself inconsistent about where `bundle` goes (`can_use(item, bundle)` puts it
last, `can_kill_enemy(bundle, …)` puts it first), so there is no single upstream
convention to be drop-in compatible with; bundle-first is the self-consistent
choice and owns a divergent set of host helpers deliberately.
`GenerateFunctionDefinitionsSource` prepends the `ruleContextParam()` receiver.

### 6.3 Signatures and types

- `pythonTypeName` (`soh_functions.cpp`) is derived from the same `enumClassName`
  table that backs `renderEnumValue` (`soh_expression.cpp`), so a parameter
  holding `Items.RG_FOO` annotates as `Items` and the two cannot drift. Enum
  types with no dedicated reference class (Scene/Dungeon/Area, which
  `renderEnumValue` renders bare) fall to `unsupported_type`.
  `Condition`/`Callable` map to `Callable[[tuple[Regions, "SohWorld"]], Rule]` (a
  thunk taking the bundle and returning a Rule). Pinned by
  `SohApFunctionSignatures.*`.
- `Condition` parameters lower as a thunk on the way in and `cond(bundle)` on
  invoke (`ApCallables` / `SohApCallables`).
- Default-valued params land in the right class: the default binds like a call
  argument, so it is rendered via `GenerateCallArgument` — a value (Bool) default
  emits Python `True`/`False` (not the `True_()`/`False_()` rule literals), an enum
  default carries its `renderEnumValue` prefix, and a Condition default is thunked.
  Pinned by `SohApFunctionSignatures.{BoolParamDefaultUsesPythonLiteral,
  EnumParamDefaultIsPrefixed}`.
- **Limitation:** the return annotation is `pythonTypeName(bodyType)`, so a
  rule-valued body whose RLS type is `Bool` annotates as `-> bool` rather than
  `-> Rule`. Harmless at runtime (Python does not enforce annotations) but
  inaccurate; tightening it would mean annotating from the body's value class.

### 6.4 Diagnose, don't miscompile

Some RLS that *type-checks* cannot be expressed (`not R`, a rule-conditioned
*value* ternary/match, a state-dependent `Int`). The transpiler emits a precise
diagnostic at the offending node rather than generate code that throws
`TypeError` at world-load — an error, not silent wrong output. `Diagnose` +
`Diagnostics()` (`ap_transpiler.cpp`) accumulate these; `runTranspiler`
(`console/main.cpp`) prints them and aborts with a non-zero exit. Pinned by
`ApDiagnostics.*`.

### 6.5 Host-provided defines

A game can declare that certain `define`s are supplied natively by the world and
must **not** be generated, via the `isHostProvidedDefine` hook (base default:
generate everything). Generating them would either shadow a hand-written host
helper or emit an unrepresentable body. SoH names two:
- `has_bottle` — a hand-written host rule (RV; §3), referenced from the region rules;
- `wallet_capacity` — a state-dependent `Int` (§5.4), collapsed away at its only
  call site by `renderBinarySpecialCase`.

Pinned by `SohApFunctionSignatures.HostProvidedDefinesAreSkipped`.

---

## 7. Code and test map

| Concept | Code | Tests |
|---------|------|-------|
| Classification (R/V/RV) | `classify_expression.cpp` (`ClassifyExpression`) | `ApClassify.*` |
| `and`/`or` bridging (§4.1) | `generate_expression.cpp` (`ClassifyAndOr`) | `ApBridging.*` |
| `not` / De Morgan negation (§4.2) | `classify_expression.cpp` (`IsPureOptionFilterRule`) + `generate_expression.cpp` (`GenerateNegatedOptionFilterRule`) | `ApNegation.*`, `SohApRendering.NotFireLoopLockedNegatesKeysanityMembership` |
| Ternary (§4.3) | `generate_expression.cpp` (`isRuleConditionedRuleTernary`) | `ApTernary.*`, `SohApHostRewrites.AgeConditional*` |
| `match` (§6.1) | `rls_match.py` (`rls_match_value`/`rls_match_rule`) + `generate_expression.cpp` | `ApMatch.*`, `SohApHostRewrites.RuleMatch*` |
| Diagnostics (§6.4) | `ap_transpiler.cpp` (`Diagnose`/`Diagnostics`), `console/main.cpp` (`runTranspiler`) | `ApDiagnostics.*` |
| Signatures & types (§6.3) | `soh_functions.cpp` (`pythonTypeName`) + `soh_expression.cpp` (`enumClassName`/`renderEnumValue`) | `SohApFunctionSignatures.*` |
| Host-provided defines (§6.5) | `isHostProvidedDefine` hook + `soh_expression.cpp` | `SohApFunctionSignatures.HostProvidedDefinesAreSkipped` |
| Emission | `SohApTranspiler::Transpile` → `GenerateFunctionDefinitionsSource` (emits `functions.gen.py`) | `AcceptanceSoh` (byte-for-byte golden) |

The generated `functions.gen.py` preamble imports the host primitives
(`from .Rules import *` — the host rules, enum classes, and the `Callable`/`Rule`
names the annotations use) plus the match helpers; the regions file imports the
generated functions (`from .functions.gen import *`).

---

## 8. Why the C++ transpiler is not a reference here

`transpilers/soh/src/generate_functions.cpp` gives the *structure* (iterate
`DefineDecls`, build a signature, `return <expr>`), which is mirrored. But it
offers **no guidance on the V/R split**, because C++ has none: `has()` returns
`bool`, `&&`/`!`/ternary operate uniformly, and `wallet_capacity` is just a
function returning `int`. The entire difficulty in this document is specific to
the RuleBuilder target. Mirroring the C++ approach naively is exactly the trap.

---

## 9. Design decisions

1. **Receiver position** — **bundle-first** everywhere, owning a divergent set of
   host helpers (§6.2).
2. **Scope** — the **whole `stdlib`** transpiles, with host-provided defines
   skipped via `isHostProvidedDefine` (§6.5).
3. **Classification home** — kept **local to `ap`** (`classify_expression.cpp`),
   not promoted to sema; revisit only if a second target needs the R/V/RV split.
