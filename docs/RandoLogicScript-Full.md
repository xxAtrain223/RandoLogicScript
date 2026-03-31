# Rando Logic Script (RLS) - Design Document

## 1. Motivation & Problem Statement

The current randomizer logic is written directly in C++ using a macro-and-lambda pattern. While powerful, this approach has significant ergonomic problems:

### Current Pain Points

**Verbosity & Noise** - A simple reachability check like "can the player reach this chest?" requires C++ lambda syntax, macro wrappers, and explicit `logic->` prefixes on every call:

```cpp
LOCATION(RC_SPIRIT_TEMPLE_CHILD_BRIDGE_CHEST, logic->HasItem(RG_OPEN_CHEST)),
```

**Deeply Nested Booleans** - Conditions become walls of `&&` and `||` that are difficult to read, review, or modify:

```cpp
ENTRANCE(RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_NORTH,
    (logic->Get(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) &&
     logic->CanPassEnemy(RE_GREEN_BUBBLE, ED_CLOSE, false)) ||
    logic->CanUse(RG_HOVER_BOOTS) ||
    logic->CanUse(RG_LONGSHOT)),
```

**SpiritShared Complexity** - The parallel-universe key logic for Spirit Temple requires a bespoke `SpiritShared()` helper that takes up to 3 region/condition pairs and is easy to get wrong:

```cpp
LOCATION(RC_SPIRIT_TEMPLE_GS_LOBBY,
    SpiritShared(RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD,
        []{return logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA, ED_LONGSHOT);}, false,
        RR_SPIRIT_TEMPLE_INNER_WEST_HAND,
        []{return logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA,
            ctx->GetTrickOption(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT);},
        RR_SPIRIT_TEMPLE_GS_LEDGE,
        []{return logic->CanKillEnemy(RE_GOLD_SKULLTULA);})),
```

**Archipelago Logic Drift** - The [Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH) project maintains a separate, hand-written Python port of the randomizer logic. This port uses a different region decomposition (fewer, more collapsed regions), duplicates/inlines conditions that the C++ code expresses via region graph traversal, and lacks constructs like `SpiritShared` or `AnyAgeTime`. For example, the C++ Spirit Temple uses ~15 fine-grained regions with `SpiritShared()` for multi-path access, while the Python port uses ~10 flatter regions with manually expanded conditions. Every time the C++ logic is updated, the Python port must be manually synchronized - a process that is error-prone and frequently falls out of date.

**Implicit Conventions** - Key counting, age gating, enemy distance enums, and trick flags are all ad-hoc patterns learned by reading hundreds of lines of existing code.

**Logic Tracker Debugging** - The logic tracker window needs to show *why* a check is or is not accessible by evaluating sub-expressions against live game state. Currently this is done by a `LogicExpression` system that parses stringified C++ conditions at runtime. Since we're generating C++ from RLS, the transpiler can instead generate pre-built expression tree structures - display strings, lambdas, and child nodes - eliminating runtime parsing entirely and enabling a cleaner, purpose-built logic tracker.

---

## 2. Goals

| Goal                      | Description                                                                                                                                                                                                                      |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Readability**           | Logic should read like a description of game knowledge, not like C++ plumbing.                                                                                                                                                   |
| **Archipelago Parity**    | The Python transpilation target must produce valid Archipelago `World` integration code - regions, locations, items, access rules, and completion conditions - so that SoH and Archipelago share the same logic source of truth. |
| **C++ Transpilation**     | Scripts transpile to C++ for the main Shipwright build, generating region definitions, and condition lambdas.                                                                                                                    |
| **Logic Tracker Support** | The C++ transpiler generates structured expression trees (display text + evaluation lambdas + children) so the logic tracker can show sub-expression results without any runtime parsing.                                        |
| **Type Safety**           | Catch errors (misspelled item names, wrong enemy types) at parse/transpile time, not at compile time or runtime.                                                                                                                 |
| **Parity**                | Every construct expressible in the current C++ logic must be expressible in RLS.                                                                                                                                                |
| **Single Migration**      | The full migration from hand-written C++ to RLS will be done as one large PR rather than incrementally. Existing C++ logic and generated C++ will not coexist long-term.                                                        |
| **File Modularity**       | Logic for different shuffle features (pots, crates, grass, freestanding, etc.) can be split into separate files and composed, so contributors can work on features independently.                                                |

### Non-Goals

- **Game modding / actor system** - RLS does not cover adding new actors, enemies, or rendering code to the game. That is the domain of the existing C/C++ mod system.
- **General-purpose scripting** - RLS intentionally limits control flow to keep logic auditable. It is not a replacement for Lua, Python, or C++.
- **Runtime parsing** - RLS is not interpreted at runtime. The C++ transpiler generates all structures needed by the logic tracker at compile time. The existing `LogicExpression` runtime parser can be removed once migration is complete.

---

## 3. Language Overview

RLS is a **declarative, domain-specific language** for defining randomizer regions, locations, exits, events, and their access conditions. It is *not* a general-purpose language - it intentionally limits control flow to keep logic auditable.

Since the language is transpiled (never interpreted at runtime), the syntax is free to optimize for **human readability** without concern for runtime parsing compatibility.

### 3.1 File Structure

Each `.rls` file contains any combination of top-level declarations - `region`, `extend region`, `define`, and `enemy`. A file typically corresponds to one dungeon or overworld area, but can also be a pure library (e.g. `stdlib/enemies.rls` containing only `enemy` declarations).

The transpiler processes all `.rls` files in the project together. All top-level declarations are globally visible - there is no `import` mechanism. The transpiler derives dependencies from usage during semantic analysis.

```rls
# shuffle_pots/spirit_temple_pots.rls
#
# Shuffle feature files add locations to regions defined elsewhere.
# This file can be developed and reviewed in isolation without
# touching the core dungeon logic.

extend region RR_SPIRIT_TEMPLE_FOYER {
    locations {
        RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()
        RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()
    }
}
```

### 3.2 Core Types

| Type         | Examples                                                                            | Notes                                                 |
| ------------ | ----------------------------------------------------------------------------------- | ----------------------------------------------------- |
| `Item`       | `RG_HOOKSHOT`, `RG_FAIRY_BOW`, `RG_BOMB_BAG`                                        | Maps directly to `RG_*` enum values.                  |
| `Enemy`      | `RE_ARMOS`, `RE_GOLD_SKULLTULA`, `RE_IRON_KNUCKLE`                                  | Maps to `RE_*` enum.                                  |
| `Distance`   | `ED_CLOSE`, `ED_BOMB_THROW`, `ED_BOOMERANG`, `ED_HOOKSHOT`, `ED_LONGSHOT`, `ED_FAR` | Maps to `ED_*` enum.                                  |
| `Trick`      | `RT_SPIRIT_CHILD_CHU`, `RT_SPIRIT_WEST_LEDGE`                                       | Maps to `RT_*` enum.                                  |
| `Setting`    | `RSK_SUNLIGHT_ARROWS`, `RSK_SHUFFLE_DUNGEON_ENTRANCES`                              | Maps to `RSK_*` / `RO_*`.                             |
| `Region`     | `RR_SPIRIT_TEMPLE_FOYER`, `RR_SPIRIT_TEMPLE_STATUE_ROOM`                            | Maps to `RR_*` enum.                                  |
| `Check`      | `RC_SPIRIT_TEMPLE_CHILD_BRIDGE_CHEST`                                               | Maps to `RC_*` enum.                                  |
| `Logic`      | `LOGIC_FORWARDS_SPIRIT_CHILD`, `LOGIC_SPIRIT_PLATFORM_LOWERED`                      | Maps to `LOGIC_*` flags.                              |
| `Scene`      | `SCENE_SPIRIT_TEMPLE`                                                               | Maps to `SceneID` enum.                               |
| `Dungeon`    | `SPIRIT_TEMPLE`                                                                     | Maps to `DungeonKey` enum.                            |
| `Area`       | `RA_CASTLE_GROUNDS`, `RA_HYRULE_FIELD`                                              | Maps to `RA_*` enum. Used in region `areas` property. |
| `Trial`      | `TK_LIGHT_TRIAL`, `TK_FOREST_TRIAL`                                                 | Maps to `TK_*` enum. Used with `trial_skipped()`.     |
| `WaterLevel` | `WL_HIGH`, `WL_LOW`, `WL_MID`                                                       | Maps to `WL_*` enum. Used with `water_level()`.       |

All names use the **same identifiers as the C++ enums**. This eliminates a mapping layer, makes cross-referencing trivial, and allows the transpiler to emit enum values directly. Names are validated at transpile time against the enum registry generated from `randomizerEnums.h`.

### 3.3 Type Inference

RLS does not require type annotations in most cases - the transpiler infers types at transpile time from context:

1. **Enum identifiers are self-typing.** Every enum prefix maps to a unique type: `RG_*` → `Item`, `RE_*` → `Enemy`, `ED_*` → `Distance`, `RT_*` → `Trick`, `RSK_*`/`RO_*` → `Setting`, etc. The transpiler resolves the type from the enum registry. Passing `has(RE_ARMOS)` is a type error - `RE_ARMOS` is an `Enemy`, not an `Item`.

2. **Host functions have known signatures.** `has()` takes an `Item`, `keys()` takes a `Scene` and an `int`, `trick()` takes a `Trick`. The transpiler validates arguments against these signatures.

3. **`match` arms provide type context.** `match distance { ED_CLOSE: ... }` tells the transpiler the discriminant is `Distance`. If a call site passes a non-`Distance` value, it's a type error.

4. **`define` parameters are inferred from usage.** If you write `define foo(d): can_hit_switch(d)` and `can_hit_switch` expects a `Distance` first argument, the transpiler infers `d: Distance`. If a call site passes `foo(RG_HOOKSHOT)`, that's a type error.

5. **Literals and booleans.** Number literals are `int`. `true`/`false` (and their aliases `always`/`never`) are `bool`. `and`/`or`/`not` produce `bool`. Condition expressions in `locations`/`exits`/`events` must be `bool`. Integers have an implicit conversion to `bool` - zero is `false`, non-zero is `true` - so functions returning a count can be used directly in conditions.

Optional type annotations are available for documentation or when inference is ambiguous (e.g. a parameter only used in arithmetic):

```rls
define foo(d: Distance): can_hit_switch(d)
```

---

## 4. Syntax

### 4.1 Conditions - The Core Abstraction

The fundamental unit is a **condition expression**. Conditions are boolean expressions built from a small set of primitives:

```rls
# Item checks
has(RG_HOOKSHOT)                              # logic->HasItem(RG_HOOKSHOT)
can_use(RG_HOOKSHOT)                          # logic->CanUse(RG_HOOKSHOT)

# Enemy interactions
can_kill(RE_ARMOS)                            # logic->CanKillEnemy(RE_ARMOS)
can_kill(RE_GOLD_SKULLTULA, ED_HOOKSHOT)      # logic->CanKillEnemy(RE_GOLD_SKULLTULA, ED_HOOKSHOT)
can_pass(RE_GREEN_BUBBLE)                     # logic->CanPassEnemy(RE_GREEN_BUBBLE)
can_get_drop(RE_GOLD_SKULLTULA, ED_BOOMERANG) # logic->CanGetEnemyDrop(RE_GOLD_SKULLTULA, ED_BOOMERANG)

# Environment
can_hit_switch()                              # logic->CanHitSwitch()
can_hit_switch(ED_BOOMERANG)                  # logic->CanHitSwitch(ED_BOOMERANG)
keys(SCENE_SPIRIT_TEMPLE, 3)                  # logic->SmallKeys(SCENE_SPIRIT_TEMPLE, 3)

# State queries
flag(LOGIC_SPIRIT_PLATFORM_LOWERED)           # logic->Get(LOGIC_SPIRIT_PLATFORM_LOWERED)
setting(RSK_SUNLIGHT_ARROWS)                  # ctx->GetOption(RSK_SUNLIGHT_ARROWS)
trick(RT_SPIRIT_CHILD_CHU)                    # ctx->GetTrickOption(RT_SPIRIT_CHILD_CHU)

# Composite helpers (defined in stdlib, overridable)
has_explosives()                              # logic->HasExplosives()
has_fire_source()                             # logic->HasFireSource()
blast_or_smash()                              # logic->BlastOrSmash()
can_jumpslash()                               # logic->CanJumpslash()
```

**Operators:** `and`, `or`, `not`, parentheses. Comparisons: `==`, `!=`, `>=`, `<=`, `>`, `<` (with word aliases `is` for `==` and `is not` for `!=`). Arithmetic: `+`, `-`, `*`, `/`. Ternary: `? :`.

The word operators `and`/`or`/`not`/`is`/`is not` are used instead of `&&`/`||`/`!`/`==`/`!=` for readability. The symbolic forms remain valid for arithmetic comparisons where word operators would feel unnatural (e.g. `fire_timer() >= 48`).

```rls
can_use(RG_HOOKSHOT) or can_use(RG_BOOMERANG)
has_explosives() or can_use(RG_MEGATON_HAMMER)
```

### 4.2 Age & Time

Age and time-of-day are first-class keywords:

```rls
is_child                           # logic->IsChild
is_adult                           # logic->IsAdult
at_day                             # logic->AtDay
at_night                           # logic->AtNight
```

`any_age { ... }` evaluates its body across all age/time combinations that have access to the current region. The transpiler generates the appropriate `Region::AnyAgeTime()` call:

```rls
any_age { can_kill(RE_ARMOS) }     # region->AnyAgeTime([]{return logic->CanKillEnemy(RE_ARMOS);})
```

Since `any_age { ... }` is a first-class expression, it can appear anywhere a value is expected — including composed with `and`/`or` alongside other conditions:

```rlsl
can_hit_switch() and any_age { can_use(RG_DINS_FIRE) } and has(RG_OPEN_CHEST)
```

The transpiler generates:
```cpp
logic->CanHitSwitch() && AnyAgeTime([]{return logic->CanUse(RG_DINS_FIRE);})
    && logic->HasItem(RG_OPEN_CHEST)
```

### 4.3 Region Definitions

Events, locations, and exits are each `Name: condition` entries. The section header (`events`, `locations`, `exits`) determines the semantic meaning.

```rls
region RR_SPIRIT_TEMPLE_FOYER {
    scene: SCENE_SPIRIT_TEMPLE

    events {
        LOGIC_FORWARDS_SPIRIT_CHILD: is_child
        LOGIC_FORWARDS_SPIRIT_ADULT: is_adult
    }

    locations {
        RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()
        RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()
    }

    exits {
        RR_SPIRIT_TEMPLE_ENTRYWAY:       always
        RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB: (is_adult or has(RG_SPEAK_GERUDO) or flag(LOGIC_SPIRIT_NABOORU_KIDNAPPED))
                                         and can_use(RG_CRAWL)
        RR_SPIRIT_TEMPLE_ADULT_SIDE_HUB: can_use(RG_SILVER_GAUNTLETS)
    }
}
```

Key design choices:
- `always` / `never` as aliases for `true` / `false` - more readable for access conditions. Both forms are valid.
- Enum identifiers used for names, so cross-referencing with the C++ codebase is trivial.
- Shorter function names (`has`, `can_use`, `flag`, `keys`) for ergonomics - the transpiler maps them to the full C++ method names.

#### Region Properties

Regions support optional properties declared before the section blocks:

| Property                         | Syntax                  | Default                 | Notes                                                  |
| -------------------------------- | ----------------------- | ----------------------- | ------------------------------------------------------ |
| `scene`                          | `scene: SCENE_ID`       | *(required)*            | The scene ID for key counting, variant detection, etc. |
| `time_passes` / `no_time_passes` | keyword                 | Auto-derived from scene | Whether the game clock advances in this region.        |
| `areas`                          | `areas: [AREA_ID, ...]` | Auto-derived from scene | The set of hint areas this region belongs to.          |

Defaults are derived automatically from the `scene` value - most regions don't need to specify `time_passes` or `areas` explicitly. Use explicit values only when the auto-derived defaults are incorrect (e.g. Hyrule Castle Grounds doesn't have time pass despite being an overworld scene):

```rls
region RR_HC_GARDEN {
    scene: SCENE_CASTLE_COURTYARD_GUARDS_DAY
    no_time_passes
    areas: [RA_CASTLE_GROUNDS]

    exits {
        RR_HC_GARDEN_GATE: always
    }
}
```

The C++ transpiler generates the appropriate constructor form: if only `scene` is specified, the 5-argument auto-deriving constructor is used; if `time_passes`/`no_time_passes` or `areas` are specified, the 7-argument explicit constructor is generated.

#### Duplicate Locations Across Regions

The same location identifier can appear in multiple regions with different conditions. This is needed when a check is reachable from several independent regions - for example, the freed carpenter checks in Thieves' Hideout appear in four separate fight regions, each with their own condition:

```rls
# Each carpenter fight is in its own region, but shares one freed-carpenters location
region RR_GERUDO_FORTRESS_FIGHT_1 {
    scene: SCENE_THIEVES_HIDEOUT

    locations {
        RC_TH_FREED_CARPENTERS:
            can_kill(RE_GERUDO_GUARD) or has(RG_GERUDO_MEMBERSHIP_CARD)
    }
}

region RR_GERUDO_FORTRESS_FIGHT_2 {
    scene: SCENE_THIEVES_HIDEOUT

    locations {
        RC_TH_FREED_CARPENTERS:
            can_kill(RE_GERUDO_GUARD) or has(RG_GERUDO_MEMBERSHIP_CARD)
    }
}
```

The transpiler treats this as a union - the location is reachable if *any* region containing it satisfies its condition. A duplicate location within the *same* region remains an error (see §8).

### 4.4 Shared / Multi-Region Checks (Replacing SpiritShared)

The `SpiritShared` pattern is one of the most complex constructs. RLS replaces it with a `shared` block that makes the multi-region logic visually clear:

```rls
region RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD {
    scene: SCENE_SPIRIT_TEMPLE

    locations {
        RC_SPIRIT_TEMPLE_MAP_CHEST: has(RG_OPEN_CHEST) and shared {
            from here:
                has_fire_source_with_torch()
                or (trick(RT_SPIRIT_MAP_CHEST) and can_use(RG_FAIRY_BOW))
            from RR_SPIRIT_TEMPLE_STATUE_ROOM:
                has_fire_source()
        }

        RC_SPIRIT_TEMPLE_GS_LOBBY: shared {
            from here:
                can_get_drop(RE_GOLD_SKULLTULA, ED_LONGSHOT)
            from RR_SPIRIT_TEMPLE_INNER_WEST_HAND:
                can_get_drop(RE_GOLD_SKULLTULA,
                    trick(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT)
            from RR_SPIRIT_TEMPLE_GS_LEDGE:
                can_kill(RE_GOLD_SKULLTULA)
        }
    }
}
```

#### `shared any_age` - AnyAge Evaluation within SpiritShared

The C++ `SpiritShared()` function has a boolean `anyAge` parameter (default `false`). When `true`, it evaluates the condition with both Child and Adult access flags set based on which ages have CertainAccess to the region. This is used for events and checks where the result should reflect what *any* reachable age can accomplish - analogous to `any_age { ... }` but within the shared key-logic framework.

In RLS, this is expressed by adding the `any_age` modifier after `shared`:

```rls
region RR_SPIRIT_TEMPLE_SUN_BLOCK_CHEST_LEDGE {
    scene: SCENE_SPIRIT_TEMPLE

    events {
        # anyAge=true: evaluates with all ages that have CertainAccess
        LOGIC_SPIRIT_SUN_BLOCK_TORCH: shared any_age {
            from here: always
        }
    }

    locations {
        # anyAge=false (default): normal per-age evaluation
        RC_SPIRIT_TEMPLE_SUN_BLOCK_ROOM_CHEST:
            has(RG_OPEN_CHEST) and shared {
                from here:
                    has_fire_source()
                    or (flag(LOGIC_SPIRIT_SUN_BLOCK_TORCH)
                        and (can_use(RG_STICKS)
                             or (trick(RT_SPIRIT_SUN_CHEST) and can_use(RG_FAIRY_BOW))))
            }
    }
}
```

The C++ transpiler generates:
- `shared { ... }` → `SpiritShared(region, condition, false, ...)`
- `shared any_age { ... }` → `SpiritShared(region, condition, true, ...)`

The `shared` block:
1. Lists each region that could contribute access (`from <region>` or `from here`).
2. Each branch has its own condition.
3. The optional `any_age` modifier sets the `anyAge` parameter - when present, the generated code evaluates with both ages' CertainAccess flags.
4. The C++ transpiler generates the appropriate `SpiritShared()` call with lambdas.
5. The expression tree generated for the logic tracker treats each `from` branch as a named child node, so the tracker can show which region's path is satisfied.

### 4.5 Dungeon Variant Selection

```rls
region RR_SPIRIT_TEMPLE_ENTRYWAY {
    scene: SCENE_SPIRIT_TEMPLE

    exits {
        RR_SPIRIT_TEMPLE_FOYER:              is_vanilla
        RR_SPIRIT_TEMPLE_MQ_FOYER:           is_mq
        RR_DESERT_COLOSSUS_OUTSIDE_TEMPLE:   always
    }
}
```

`is_vanilla` / `is_mq` transpiles to `ctx->GetDungeon(SPIRIT_TEMPLE)->IsVanilla()` / `->IsMQ()`. The dungeon is inferred from the region's scene.

### 4.6 Functions

Reusable logic can be defined at file scope:

```rls
define spirit_explosive_key_logic():
    keys(SCENE_SPIRIT_TEMPLE, has_explosives() ? 1 : 2)

define spirit_east_to_switch():
    (is_adult and trick(RT_SPIRIT_STATUE_JUMP))
    or can_use(RG_HOVER_BOOTS)
    or (can_use(RG_ZELDAS_LULLABY) and can_use(RG_HOOKSHOT))
```

These are **pure** - no side effects, no mutation. The transpiler always generates them as callable functions (never inlined). For the C++ target, each `define` generates both a function (for the solver) and a named expression tree node (for the logic tracker, so users can see `spirit_explosive_key_logic()` as a collapsible node and drill into its children). For Python/Archipelago, they become Python functions.

### 4.7 Ternary / Conditional Expressions

```rls
can_hit_switch(is_adult and trick(RT_SPIRIT_LOWER_ADULT_SWITCH) ? ED_BOMB_THROW : ED_BOOMERANG)
```

This directly mirrors the current pattern:
```cpp
logic->CanHitSwitch(logic->IsAdult && ctx->GetTrickOption(RT_SPIRIT_LOWER_ADULT_SWITCH)
    ? ED_BOMB_THROW : ED_BOOMERANG)
```

### 4.8 Match Expressions

Many combat and environment functions dispatch on a distance or enemy enum and need **fallthrough accumulation** - anything that works at close range also works at longer range. C++ uses `switch` with explicit fallthrough for this, but that pattern is error-prone and hard to read.

RLS provides a `match` expression with an explicit, per-arm opt-in to fallthrough using a trailing `or`.

#### Basic Syntax

```rls
match <value> {
    <pattern>: <body>
    <pattern>: <body>
}
```

Each arm is `Pattern: body`. The `match` expression evaluates the first arm whose pattern equals `<value>` and returns its body. If no arm matches, the `match` expression evaluates to `false`.

#### Fallthrough with Trailing `or`

A trailing `or` **after** an arm's body causes that arm's result to be **OR-accumulated** with the next arm. This is how distance-based dispatch works - weapons effective at close range are also effective at longer range:

```rls
define can_hit_switch(distance = ED_CLOSE, inWater = false):
    match distance {
        ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or can_use(RG_MEGATON_HAMMER) or
        ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or
        ED_LONG_JUMPSLASH: can_use(RG_BIGGORON_SWORD) or can_use(RG_STICKS) or
        ED_BOMB_THROW: not inWater and can_use(RG_BOMB_BAG) or
        ED_BOOMERANG: can_use(RG_BOOMERANG) or
        ED_HOOKSHOT: can_use(RG_HOOKSHOT) or can_use(RG_BOMBCHU_5) or
        ED_LONGSHOT: can_use(RG_LONGSHOT) or
        ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)
    }
```

Calling `can_hit_switch(ED_BOOMERANG)` evaluates the `ED_BOOMERANG` arm and, because it ends with `or`, accumulates downward through `ED_HOOKSHOT`, `ED_LONGSHOT`, and `ED_FAR`. The result is `can_use(RG_BOOMERANG) or can_use(RG_HOOKSHOT) or can_use(RG_BOMBCHU_5) or can_use(RG_LONGSHOT) or can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)`.

The last arm (`ED_FAR`) has **no** trailing `or`, so accumulation stops there.

#### Multi-Value Arms

`or` **before** the colon creates a multi-value arm that matches any of the listed patterns:

```rls
match distance {
    ED_CLOSE or
    ED_SHORT_JUMPSLASH:
        can_use(RG_MEGATON_HAMMER) or can_use(RG_KOKIRI_SWORD) or
    ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or
    ED_LONG_JUMPSLASH: can_use(RG_BIGGORON_SWORD) or
    ...
}
```

Here `ED_CLOSE or ED_SHORT_JUMPSLASH:` means "this arm matches either value." Both `ED_CLOSE` and `ED_SHORT_JUMPSLASH` evaluate to the same body.

#### Three Positional Uses of `or`

The keyword `or` appears in three positions within a `match`, each with a distinct meaning:

| Position            | Meaning                                       | Example                                         |
| ------------------- | --------------------------------------------- | ----------------------------------------------- |
| Before `:`          | Multi-value arm (matches any listed pattern)  | `ED_CLOSE or ED_SHORT_JUMPSLASH:`               |
| Within body         | Normal boolean OR                             | `can_use(RG_HOOKSHOT) or can_use(RG_BOMBCHU_5)` |
| Trailing after body | Fallthrough - OR-accumulate with the next arm | `can_use(RG_BOOMERANG) or` (at end of arm)      |

These are unambiguous from context: `or` before a colon is multi-value, `or` followed by more expressions in the same arm is boolean, and `or` at the very end of an arm (followed by the next arm or `}`) is fallthrough.

#### Selective Fallthrough

Not every arm needs to fall through. Arms without a trailing `or` terminate normally. This allows mixing fallthrough and non-fallthrough arms in the same `match`:

```rls
# RE_STALFOS: all arms fall through except the last (ED_FAR),
# which terminates normally and stops accumulation.
match distance {
    ED_CLOSE or
    ED_SHORT_JUMPSLASH:
        can_use(RG_MEGATON_HAMMER) or can_use(RG_KOKIRI_SWORD) or
    ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or
    ED_LONG_JUMPSLASH:
        can_use(RG_BIGGORON_SWORD)
        or (quantity <= 1 and can_use(RG_STICKS)) or
    ED_BOMB_THROW:
        quantity <= 2 and not timer and not inWater
        and (can_use(RG_NUTS) or hookshot_or_boomerang())
        and can_use(RG_BOMB_BAG) or
    ED_BOOMERANG or
    ED_HOOKSHOT: can_use(RG_BOMBCHU_5) or
    ED_LONGSHOT or
    ED_FAR: can_use(RG_FAIRY_BOW)
}
```

### 4.9 Enemy Declarations

#### Motivation

The current C++ code scatters knowledge about each enemy across four separate functions - `CanKillEnemy`, `CanPassEnemy`, `CanAvoidEnemy`, and `CanGetEnemyDrop` - each containing a large `switch` statement with 40+ cases.

RLS provides an `enemy` declaration that puts **all knowledge about one enemy in one place**, like a bestiary entry.

#### Syntax

```rls
enemy <ENEMY_ID> {
    kill(<params>): <condition>
    pass(<params>): <condition>
    drop(<params>): <condition>
    avoid: <condition>
}
```

**Fields:**

| Field   | Meaning                                                   | Default if omitted                                  |
| ------- | --------------------------------------------------------- | --------------------------------------------------- |
| `kill`  | Can the player kill this enemy?                           | *(required)*                                        |
| `pass`  | Can the player get past this enemy without killing it?    | `always` (most enemies can be walked past)          |
| `drop`  | Can the player collect the enemy's drop after killing it? | Same as `kill` (trivially collected at close range) |
| `avoid` | Can the player avoid taking damage from this enemy?       | `always` (most enemies are avoidable)               |

The defaults are chosen to minimize boilerplate - most enemies only need a `kill` field. The `pass` default of `always` reflects that most enemies can simply be walked around. The `drop` default matching `kill` reflects that most drops are collected at the same range the enemy was killed.

#### Parameters

Fields can take parameters for situational modifiers:

```rls
enemy RE_GOLD_SKULLTULA {
    kill(wallOrFloor = true):
        match distance {
            ED_CLOSE: can_use(RG_MEGATON_HAMMER) or
            ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or
            ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or
            ED_LONG_JUMPSLASH: can_use(RG_BIGGORON_SWORD) or can_use(RG_STICKS) or
            ED_BOMB_THROW: can_use(RG_BOMB_BAG) or
            ED_BOOMERANG: can_use(RG_BOOMERANG) or can_use(RG_DINS_FIRE) or
            ED_HOOKSHOT: can_use(RG_HOOKSHOT) or
            ED_LONGSHOT: can_use(RG_LONGSHOT)
                or (wallOrFloor and can_use(RG_BOMBCHU_5)) or
            ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)
        }
    drop:
        kill and match distance {
            ED_BOOMERANG: can_use(RG_BOOMERANG) or
            ED_HOOKSHOT: can_use(RG_HOOKSHOT) or
            ED_LONGSHOT: can_use(RG_LONGSHOT)
        }
}
```

The `kill(wallOrFloor = true)` field has a boolean parameter with a default. Call sites like `can_kill(RE_GOLD_SKULLTULA)` use the default; `can_kill(RE_GOLD_SKULLTULA, wallOrFloor: false)` overrides it.

In the `drop` field, `kill` refers to the result of the `kill` field for the same enemy - "the player can kill it, and also...".

#### Wiring to Built-In Functions

The transpiler generates the `can_kill`, `can_pass`, `can_get_drop`, and `can_avoid` built-in functions from `enemy` declarations. These functions are callable anywhere in conditions:

```rls
# Call sites - these look up the corresponding enemy declaration
can_kill(RE_ARMOS)                                         # → RE_ARMOS.kill()
can_kill(RE_GOLD_SKULLTULA, ED_HOOKSHOT)                   # → with distance=ED_HOOKSHOT
can_kill(RE_STALFOS, ED_CLOSE, quantity: 2, timer: true)   # → with named overrides
can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false)                 # → with distance + wallOrFloor
can_get_drop(RE_GOLD_SKULLTULA, ED_LONGSHOT)               # → with distance=ED_LONGSHOT
```

Call sites pass arguments positionally (matching the field's parameter list) or by name. The first argument after the enemy identifier is always `distance` (from the `match distance` in the field body). Additional parameters declared in the `kill`/`pass`/`drop` fields (e.g. `quantity`, `timer`, `inWater`, `wallOrFloor`) can be passed positionally or with named syntax (`param: value`). Parameters not supplied at the call site use their declared defaults.

The transpiler:
1. Collects all `enemy` declarations.
2. For each call to `can_kill(RE_X, ...)`, resolves it to the `kill` field of `enemy RE_X`.
3. Passes the `distance` argument to the `match` expression inside the field (if present).
4. Binds any additional arguments (positional or named) to the field's parameters.
5. Applies field defaults for omitted fields (`pass` → `always`, `drop` → `kill`, `avoid` → `always`).
6. Generates the appropriate C++ lambda / Python function for each concrete usage.

For the C++ expression tree, `can_kill(RE_ARMOS)` generates a node with `DisplayText = "can_kill(RE_ARMOS)"` whose children are the expanded `kill` body of `enemy RE_ARMOS`.

#### Examples

**Simple enemy - only kill field needed:**

```rls
enemy RE_IRON_KNUCKLE {
    kill: can_use_sword() or can_use(RG_MEGATON_HAMMER) or has_explosives()
    pass: never
}
```

Iron Knuckle cannot be passed (`never`) - it blocks the room. No `drop` field, so drop defaults to `kill`. No `avoid` field, so avoid defaults to `always`.

**Enemy with custom pass logic:**

```rls
enemy RE_GERUDO_GUARD {
    kill: never
    pass:
        trick(RT_PASS_GUARDS_WITH_NOTHING)
        or has(RG_GERUDO_MEMBERSHIP_CARD)
        or can_use(RG_FAIRY_BOW) or can_use(RG_HOOKSHOT)
}
```

Gerudo Guards can't be killed, only passed. `drop` defaults to `kill` which is `never` - correct, since they don't drop anything.

**Enemy with all fields (including distance dispatch):**

```rls
enemy RE_ARMOS {
    kill:
        blast_or_smash() or can_use(RG_MASTER_SWORD)
        or can_use(RG_BIGGORON_SWORD) or can_use(RG_STICKS)
        or can_use(RG_FAIRY_BOW)
        or ((can_use(RG_NUTS) or can_use(RG_HOOKSHOT) or can_use(RG_BOOMERANG))
            and (can_use(RG_KOKIRI_SWORD) or can_use(RG_FAIRY_SLINGSHOT)))
}
```

Armos only needs `kill` - pass, drop, and avoid all use the defaults.

**Complex enemy with multi-value arms and selective fallthrough:**

```rls
enemy RE_STALFOS {
    kill(quantity = 1, timer = false, inWater = false):
        match distance {
            ED_CLOSE or
            ED_SHORT_JUMPSLASH:
                can_use(RG_MEGATON_HAMMER) or can_use(RG_KOKIRI_SWORD) or
            ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or
            ED_LONG_JUMPSLASH:
                can_use(RG_BIGGORON_SWORD)
                or (quantity <= 1 and can_use(RG_STICKS)) or
            ED_BOMB_THROW:
                quantity <= 2 and not timer and not inWater
                and (can_use(RG_NUTS) or hookshot_or_boomerang())
                and can_use(RG_BOMB_BAG) or
            ED_BOOMERANG or
            ED_HOOKSHOT: can_use(RG_BOMBCHU_5) or
            ED_LONGSHOT or
            ED_FAR: can_use(RG_FAIRY_BOW)
        }
}
```

This shows multi-value arms (`ED_CLOSE or ED_SHORT_JUMPSLASH:`), fallthrough accumulation (all arms end with trailing `or` except the final arm `ED_FAR` which terminates normally), and field parameters (`quantity`, `timer`, `inWater`) that call sites can override.

---

## 5. Standard Library

The RLS standard library is split into two categories: **host functions** that require interaction with the SoH engine, SaveContext, or other C++ internals, and **RLS-definable functions** that are pure logic expressible entirely within the RLS language.

### 5.1 Host Functions

These functions map to C++ `Logic` / `Context` methods and cannot be expressed in RLS alone. The transpiler knows the mapping and generates the correct C++ calls. For the Python target, these map to the corresponding `LogicHelpers` functions.

#### Core Functions

| RLS function    | C++ method                   | Notes                                 |
| ---------------- | ---------------------------- | ------------------------------------- |
| `has(item)`      | `logic->HasItem(item)`       | Queries SaveContext inventory         |
| `can_use(item)`  | `logic->CanUse(item)`        | Includes age + prerequisite checks    |
| `keys(scene, n)` | `logic->SmallKeys(scene, n)` | Queries SaveContext key counts        |
| `flag(logicVal)` | `logic->Get(logicVal)`       | Reads logic state flags               |
| `setting(key)`   | `ctx->GetOption(key)`        | Reads randomizer settings (see below) |
| `trick(key)`     | `ctx->GetTrickOption(key)`   | Reads trick toggle state (see below)  |

#### Runtime State Queries

| RLS function                  | C++ method                                            | Notes                                                                              |
| ------------------------------ | ----------------------------------------------------- | ---------------------------------------------------------------------------------- |
| `fire_timer()`                 | `logic->FireTimer()`                                  | Frames of fire resistance available (int)                                          |
| `water_timer()`                | `logic->WaterTimer()`                                 | Frames of water breathing available (int)                                          |
| `water_level(level)`           | `logic->WaterLevel(level)`                            | Whether Water Temple water is at `level` (e.g. `WL_HIGH`)                          |
| `effective_health()`           | `logic->EffectiveHealth()`                            | Effective health in half-heart hits accounting for defense/damage multiplier (int) |
| `hearts()`                     | `logic->Hearts()`                                     | Current heart container count (int)                                                |
| `stone_count()`                | `logic->StoneCount()`                                 | Number of spiritual stones obtained (int)                                          |
| `ocarina_buttons()`            | `logic->OcarinaButtons()`                             | Number of ocarina buttons available (int)                                          |
| `check_price()`                | `GetCheckPrice()`                                     | Price of current shop/scrub/merchant slot (int)                                    |
| `check_price(location)`        | `GetCheckPrice(location)`                             | Price of a specific shop location (int)                                            |
| `wallet_capacity()`            | `GetWalletCapacity()`                                 | Max wallet rupee capacity (int)                                                    |
| `can_plant_bean(region, bean)` | `CanPlantBean(region, bean)`                          | Whether a magic bean can be planted at `region` with `bean` soul                   |
| `bean_planted(bean)`           | `BeanPlanted(bean)`                                   | Whether the bean soul's corresponding bean is planted                              |
| `trial_skipped(trial)`         | `ctx->GetTrial(trial)->IsSkipped()`                   | Whether a Ganon's Castle trial is skipped                                          |
| `triforce_pieces()`            | `logic->GetSaveContext()->...triforcePiecesCollected` | Number of Triforce pieces collected (int)                                          |
| `big_poes()`                   | `logic->BigPoes`                                      | Number of Big Poes collected (int)                                                 |

Functions returning `int` have implicit `int → bool` conversion (zero is `false`, non-zero is `true`), so they can be used directly in conditions: `stone_count() >= 3`, `fire_timer() >= 48`, `effective_health() > 2`, etc.

#### Setting & Trick Semantics

`setting()` and `trick()` return option values that support comparison operators and boolean truthiness:

| Usage                                            | C++ equivalent                                          | Meaning                                    |
| ------------------------------------------------ | ------------------------------------------------------- | ------------------------------------------ |
| `setting(RSK_SUNLIGHT_ARROWS)`                   | `(bool)ctx->GetOption(RSK_SUNLIGHT_ARROWS)`             | Truthy if the option is enabled (non-zero) |
| `setting(RSK_FOREST) is RO_CLOSED_FOREST_ON`     | `ctx->GetOption(RSK_FOREST).Is(RO_CLOSED_FOREST_ON)`    | Exact value comparison                     |
| `setting(RSK_FOREST) is not RO_CLOSED_FOREST_ON` | `ctx->GetOption(RSK_FOREST).IsNot(RO_CLOSED_FOREST_ON)` | Value inequality                           |
| `not setting(RSK_SUNLIGHT_ARROWS)`               | `!ctx->GetOption(RSK_SUNLIGHT_ARROWS)`                  | Negated boolean truthiness                 |
| `trick(RT_UNINTUITIVE_JUMPS)`                    | `ctx->GetTrickOption(RT_UNINTUITIVE_JUMPS).Get() != 0`  | Truthy if trick is enabled                 |

`is` / `is not` are word-operator aliases for `==` / `!=`. Both forms are valid everywhere, but `is` / `is not` are preferred for setting comparisons because they read naturally: `setting(RSK_FOREST) is RO_CLOSED_FOREST_ON` mirrors the C++ `.Is()` method and reads as a plain-English assertion.

This covers all option access patterns found in the current C++ codebase: `.Is()`, `.IsNot()`, boolean cast, and `!` negation.

### 5.2 RLS-Definable Functions

These functions are composite helpers that can be expressed purely in terms of other RLS functions and operators. They are provided as `define` functions in standard library `.rls` files, making them transparent, overridable, and visible to the logic tracker as expandable nodes.

#### Simple Helpers

```rls
# stdlib/helpers.rls

define has_explosives():
    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)

define has_fire_source():
    can_use(RG_DINS_FIRE) or can_use(RG_FIRE_ARROWS)

define has_fire_source_with_torch():
    has_fire_source() or can_use(RG_STICKS)

define blast_or_smash():
    has_explosives() or can_use(RG_MEGATON_HAMMER)

define hookshot_or_boomerang():
    can_use(RG_HOOKSHOT) or can_use(RG_BOOMERANG)

define sunlight_arrows():
    setting(RSK_SUNLIGHT_ARROWS) and can_use(RG_LIGHT_ARROWS)

define can_jumpslash():
    can_use(RG_STICKS) or can_use_sword() or can_use(RG_MEGATON_HAMMER)

define reach_scarecrow():
    scarecrows_song() and can_use(RG_HOOKSHOT)
```

#### Enemy & Distance Dispatch Functions

These functions are generated by the transpiler from `enemy` declarations (§4.9) and `match` expressions (§4.8). They are pure logic - they only call `can_use`, `has`, `trick`, and other RLS-definable helpers - and are now fully expressible in RLS.

| Function                   | Generated From                 | C++ Equivalent                                                                |
| -------------------------- | ------------------------------ | ----------------------------------------------------------------------------- |
| `can_kill(enemy, ...)`     | `enemy` block `kill` field     | `Logic::CanKillEnemy(enemy, distance, wallOrFloor, quantity, timer, inWater)` |
| `can_pass(enemy, ...)`     | `enemy` block `pass` field     | `Logic::CanPassEnemy(enemy, distance, wallOrFloor)`                           |
| `can_get_drop(enemy, ...)` | `enemy` block `drop` field     | `Logic::CanGetEnemyDrop(enemy, distance, aboveLink)`                          |
| `can_avoid(enemy)`         | `enemy` block `avoid` field    | `Logic::CanAvoidEnemy(enemy)`                                                 |
| `can_hit_switch(...)`      | `define` with `match distance` | `Logic::CanHitSwitch(distance, inWater)`                                      |
| `can_break_pots(...)`      | `define` with `match distance` | `Logic::CanBreakPots(distance, wallOrFloor, inWater)`                         |

The per-enemy functions (`can_kill`, `can_pass`, `can_get_drop`, `can_avoid`) are **built-in** - the transpiler wires each call to the corresponding field in the named `enemy` declaration (see §4.9 for the wiring rules and default behavior).

The distance-only functions (`can_hit_switch`, `can_break_pots`) are standard `define` functions that use `match distance` for fallthrough accumulation (see §4.8):

```rls
# stdlib/combat.rls

define can_hit_switch(distance = ED_CLOSE, inWater = false):
    match distance {
        ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or can_use(RG_MEGATON_HAMMER) or
        ED_MASTER_SWORD_JUMPSLASH: can_use(RG_MASTER_SWORD) or
        ED_LONG_JUMPSLASH: can_use(RG_BIGGORON_SWORD) or can_use(RG_STICKS) or
        ED_BOMB_THROW: not inWater and can_use(RG_BOMB_BAG) or
        ED_BOOMERANG: can_use(RG_BOOMERANG) or
        ED_HOOKSHOT: can_use(RG_HOOKSHOT) or can_use(RG_BOMBCHU_5) or
        ED_LONGSHOT: can_use(RG_LONGSHOT) or
        ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)
    }

define can_break_pots(distance = ED_CLOSE, wallOrFloor = true, inWater = false):
    match distance {
        ED_CLOSE: can_use(RG_MEGATON_HAMMER) or
        ED_SHORT_JUMPSLASH: can_jumpslash() or
        ED_BOMB_THROW: not inWater and can_use(RG_BOMB_BAG) or
        ED_BOOMERANG: can_use(RG_BOOMERANG) or
        ED_HOOKSHOT:
            can_use(RG_HOOKSHOT)
            or (not inWater and can_use(RG_BOMBCHU_5)) or
        ED_LONGSHOT: can_use(RG_LONGSHOT) or
        ED_FAR: can_use(RG_FAIRY_SLINGSHOT) or can_use(RG_FAIRY_BOW)
    }
```

The `enemy` declarations live in stdlib files (e.g. `stdlib/enemies.rls`) alongside these helpers, making the full combat knowledge base browsable and auditable.

#### Dungeon-Specific Helpers

Dungeon-specific helpers live in their own files:

```rls
# stdlib/spirit_helpers.rls

define spirit_explosive_key_logic():
    keys(SCENE_SPIRIT_TEMPLE, has_explosives() ? 1 : 2)

define spirit_east_to_switch():
    (is_adult and trick(RT_SPIRIT_STATUE_JUMP))
    or can_use(RG_HOVER_BOOTS)
    or (can_use(RG_ZELDAS_LULLABY) and can_use(RG_HOOKSHOT))

define spirit_sun_block_south_ledge():
    has(RG_POWER_BRACELET) or is_adult or can_kill(RE_BEAMOS)
    or (can_use(RG_HOOKSHOT) and
        (has_fire_source()
         or (flag(LOGIC_SPIRIT_SUN_BLOCK_TORCH)
             and (can_use(RG_STICKS)
                  or (trick(RT_SPIRIT_SUN_CHEST) and can_use(RG_FAIRY_BOW))))))
```

#### Transpiler Behavior

The distinction matters for the transpiler:
- **Host functions** require platform-specific code generation (C++ method calls vs. Python `LogicHelpers` calls).
- **RLS-definable functions** are transpiled identically on both targets - the transpiler expands or generates them from the RLS source.
- **Enemy/distance dispatch functions** are generated from `enemy` declarations and `define` functions using `match` expressions - fully RLS-defined.

For the C++ target, `define` functions generate both a callable function (for the solver) and a named expression tree node (for the logic tracker). For Python/Archipelago, they become Python functions.

---

## 6. Transpilation Targets

### 6.1 C++ Target (Build-Time)

Each `.rls` file transpiles to a `.cpp` + `.h` pair. The transpiler generates **two representations** for each condition:

1. **A lambda** for the solver / access checking (same role as today's macros).
2. **An expression tree structure** for the logic tracker (replaces runtime parsing).

#### Solver Output (Lambdas + Region Definitions)

The lambda output is structurally similar to the current hand-written code:

```rls
# Input (spirit_temple.rls)
exits {
    RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_NORTH:
        (flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) and can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false))
        or can_use(RG_HOVER_BOOTS)
        or can_use(RG_LONGSHOT)
}
```
```cpp
// Output (spirit_temple.gen.cpp) - solver lambda
Entrance(RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_NORTH, [] {
    return (logic->Get(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) &&
            logic->CanPassEnemy(RE_GREEN_BUBBLE, ED_CLOSE, false)) ||
           logic->CanUse(RG_HOVER_BOOTS) ||
           logic->CanUse(RG_LONGSHOT);
})
```

#### Logic Tracker Output (Expression Trees)

For each condition, the transpiler also generates a static `ExpressionNode` tree. This is what the logic tracker uses to show sub-expression results - no runtime parsing required.

```cpp
// Generated expression tree structure
struct ExpressionNode {
    std::string DisplayText;             // RLS source text for this node
    std::function<ValueVariant()> Eval;  // Evaluates this node against live game state
    std::vector<ExpressionNode> Children;
};
```

For the exit above, the generated tree looks like:

```cpp
// Output (spirit_temple.gen.cpp) - expression tree for logic tracker
ExpressionNode{
    .DisplayText = "(flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) and can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false))"
                   " or can_use(RG_HOVER_BOOTS) or can_use(RG_LONGSHOT)",
    .Eval = [] { return /* full condition */; },
    .Children = {
        ExpressionNode{
            .DisplayText = "flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) and can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false)",
            .Eval = [] { return logic->Get(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE)
                             && logic->CanPassEnemy(RE_GREEN_BUBBLE, ED_CLOSE, false); },
            .Children = {
                ExpressionNode{
                    .DisplayText = "flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE)",
                    .Eval = [] { return logic->Get(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE); },
                    .Children = {}
                },
                ExpressionNode{
                    .DisplayText = "can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false)",
                    .Eval = [] { return logic->CanPassEnemy(RE_GREEN_BUBBLE, ED_CLOSE, false); },
                    .Children = {}
                },
            }
        },
        ExpressionNode{
            .DisplayText = "can_use(RG_HOVER_BOOTS)",
            .Eval = [] { return logic->CanUse(RG_HOVER_BOOTS); },
            .Children = {}
        },
        ExpressionNode{
            .DisplayText = "can_use(RG_LONGSHOT)",
            .Eval = [] { return logic->CanUse(RG_LONGSHOT); },
            .Children = {}
        },
    }
}
```

The logic tracker walks this tree, calls `Eval()` on each node for each age/time combination, and displays the results - exactly the same UX as today, but with the RLS source as display text and no runtime parsing step.

For `define` functions, the tree contains a node whose `DisplayText` is the function name (e.g. `"spirit_explosive_key_logic()"`) and whose `Children` contain the expanded body. The tracker shows the function call at the top level and lets the user expand it to see internals.

For `shared` blocks, each `from` branch becomes a named child node (e.g. `"from RR_SPIRIT_TEMPLE_GS_LEDGE"`), making it clear which region's path is contributing access.

For `any_age { ... }` blocks, the tree node shows `"any_age { ... }"` and the tracker evaluates the child with all age/time flags set from the region.

### 6.2 Python / Archipelago Target

The Python target generates code for the [Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH) project under `worlds/oot_soh/`. This is the primary reason for the Python target - so that SoH and Archipelago share the same logic source of truth, eliminating the manual porting that currently causes logic drift.

**Archipelago requirements** (from [adding games.md](https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/adding%20games.md)):
- A `World` subclass with `item_name_to_id`, `location_name_to_id`.
- At least one origin `Region` (default name: `"Menu"`).
- Locations added to regions with access rules.
- Items added to the multiworld itempool.
- A completion condition.
- `create_item` implementation.
- Game info and setup docs.

#### Existing Archipelago-SoH Pattern

The current hand-written Python code uses helper functions from a shared `LogicHelpers` module and a `bundle` parameter convention:

```python
# Current hand-written code (worlds/oot_soh/location_access/dungeons/spirit_temple.py)
from ...LogicHelpers import *

def set_region_rules(world: "SohWorld") -> None:
    # Connections
    connect_regions(Regions.SPIRIT_TEMPLE_LOBBY, world, [
        (Regions.SPIRIT_TEMPLE_ENTRYWAY, lambda bundle: True),
        (Regions.SPIRIT_TEMPLE_CHILD,    lambda bundle: is_child(bundle)),
        (Regions.SPIRIT_TEMPLE_EARLY_ADULT,
            lambda bundle: can_use(Items.SILVER_GAUNTLETS, bundle))
    ])

    # Locations
    add_locations(Regions.SPIRIT_TEMPLE_LOBBY, world, [
        (Locations.SPIRIT_TEMPLE_LOBBY_POT1,
            lambda bundle: can_break_pots(bundle)),
        (Locations.SPIRIT_TEMPLE_LOBBY_POT2,
            lambda bundle: can_break_pots(bundle))
    ])

    # Events
    add_events(Regions.SPIRIT_TEMPLE_BOSS_ROOM, world, [
        (EventLocations.SPIRIT_TEMPLE_TWINROVA,
            Events.SPIRIT_TEMPLE_COMPLETED,
            lambda bundle: can_kill_enemy(bundle, Enemies.TWINROVA))
    ])
```

Key characteristics of this pattern:
- **`lambda bundle:`** - Every condition takes a `bundle` parameter (contains state, player, world info).
- **`connect_regions()`** - Exits are (target_region, condition) tuples.
- **`add_locations()`** - Locations are (location_id, condition) tuples.
- **`add_events()`** - Events are (event_location, event_flag, condition) tuples.
- **Enum classes** - `Regions.*`, `Locations.*`, `Items.*`, `Tricks.*`, `Events.*`, `Enemies.*`.
- **Helper functions** - `can_use()`, `has_item()`, `has_explosives()`, `small_keys()`, `can_do_trick()`, `is_child()`, `is_adult()`, `can_kill_enemy()`, etc. from `LogicHelpers`.
- **Flattened logic** - Conditions that the C++ code expresses through region graph traversal (e.g. `SpiritShared`, `AnyAgeTime`) are manually expanded and duplicated in the Python code.

#### Generated Output

The RLS transpiler generates Python code that matches this existing pattern, so it integrates seamlessly with the Archipelago-SoH codebase:

```python
# Auto-generated from spirit_temple.rls
# → worlds/oot_soh/location_access/dungeons/spirit_temple.py

from ...LogicHelpers import *

if TYPE_CHECKING:
    from ... import SohWorld

def set_region_rules(world: "SohWorld") -> None:
    # Region: RR_SPIRIT_TEMPLE_FOYER
    # Locations
    add_locations(Regions.SPIRIT_TEMPLE_FOYER, world, [
        (Locations.SPIRIT_TEMPLE_LOBBY_POT_1,
            lambda bundle: can_break_pots(bundle)),
        (Locations.SPIRIT_TEMPLE_LOBBY_POT_2,
            lambda bundle: can_break_pots(bundle)),
    ])

    # Connections
    connect_regions(Regions.SPIRIT_TEMPLE_FOYER, world, [
        (Regions.SPIRIT_TEMPLE_ENTRYWAY, lambda bundle: True),
        (Regions.SPIRIT_TEMPLE_CHILD_SIDE_HUB,
            lambda bundle:
                (is_adult(bundle) or has_item(Items.SPEAK_GERUDO, bundle)
                 or has_item(Events.SPIRIT_NABOORU_KIDNAPPED, bundle))
                and can_use(Items.CRAWL, bundle)),
        (Regions.SPIRIT_TEMPLE_ADULT_SIDE_HUB,
            lambda bundle: can_use(Items.SILVER_GAUNTLETS, bundle)),
    ])

    # Region: RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB
    # Events
    add_events(Regions.SPIRIT_TEMPLE_CHILD_SIDE_HUB, world, [
        (EventLocations.SPIRIT_TEMPLE_NUT_ACCESS,
            Events.NUT_ACCESS,
            lambda bundle: can_break_small_crates(bundle)),
    ])

    # Connections
    connect_regions(Regions.SPIRIT_TEMPLE_CHILD_SIDE_HUB, world, [
        (Regions.SPIRIT_TEMPLE_FOYER,
            lambda bundle: can_use(Items.CRAWL, bundle)),
        (Regions.SPIRIT_TEMPLE_SWITCH_BRIDGE_SOUTH,
            lambda bundle: can_kill_enemy(bundle, Enemies.ARMOS)),
    ])
```

The transpiler also generates:
- `Regions`, `Locations`, `Items`, `Tricks`, `Events`, `Enemies` enum entries for any new values introduced by RLS files.
- `item_name_to_id` and `location_name_to_id` mappings.
- Flattened logic where needed - `shared` blocks are expanded into the appropriate per-path conditions, and `any_age { ... }` blocks are translated to the Archipelago model (which handles age/time differently than the C++ solver).

#### Handling C++ ↔ Python Model Differences

The C++ and Archipelago models differ in important ways:

| Aspect              | C++ (SoH)                                           | Python (Archipelago-SoH)                                 |
| ------------------- | --------------------------------------------------- | -------------------------------------------------------- |
| Region granularity  | Fine-grained (~15 regions for Spirit Temple)        | Coarser (~10 regions)                                    |
| Age/time evaluation | Per-region, per age/time combination                | `bundle` parameter, evaluated differently                |
| `SpiritShared`      | Multi-region lambda callbacks                       | Manually expanded per-path conditions                    |
| `AnyAgeTime`        | Evaluates across age/time combos with region access | Not directly available; conditions written for both ages |
| Event propagation   | Fixed-point iteration over region graph             | Archipelago's own reachability analysis                  |

The transpiler bridges these differences:
- **`shared` blocks** → expanded into the union of per-path conditions in Python.
- **`any_age { ... }`** → transpiled to conditions that check both ages where the Archipelago model requires it.
- **Region mapping** → RLS regions map 1:1 to C++ regions. The Python transpiler can optionally merge regions that don't need to be separate in the Archipelago model (e.g. regions connected by `always`).
- **`define` functions** → become Python helper functions at the top of the generated file.

---

## 7. Grammar (EBNF Sketch)

```ebnf
file          = (region | extend | define | enemy)* ;

region        = "region" IDENT "{" region_body "}" ;
extend        = "extend" "region" IDENT "{" region_body "}" ;
region_body   = region_props section* ;
region_props  = ("scene:" IDENT)?
                ("time_passes" | "no_time_passes")?
                ("areas:" "[" ident_list "]")? ;

section       = section_kind "{" entry* "}" ;
section_kind  = "events" | "locations" | "exits" ;
entry         = IDENT ":" expr ;

define        = "define" IDENT "(" params? ")" ":" expr ;

enemy         = "enemy" IDENT "{" enemy_field+ "}" ;
enemy_field   = ("kill" | "pass" | "drop" | "avoid") ("(" params? ")")? ":" expr ;

params        = param ("," param)* ;
param         = IDENT (":" type)? ("=" expr)? ;

expr          = ternary ;
ternary       = or_expr ("?" ternary ":" ternary)? ;
or_expr       = and_expr ("or" and_expr)* ;
and_expr      = comparison ("and" comparison)* ;
comparison    = add_sub (comp_op add_sub)? ;
comp_op       = "==" | "is" | "!=" | "is" "not" | ">=" | "<=" | ">" | "<" ;
add_sub       = mul_div (("+" | "-") mul_div)* ;
mul_div       = unary (("*" | "/") unary)* ;
unary         = "not" unary | primary ;
primary       = call | shared_block | any_age_block | match_expr | atom | "(" expr ")" ;

match_expr    = "match" IDENT "{" match_arm+ "}" ;
match_arm     = match_pattern ":" expr trailing_or? ;
match_pattern = IDENT ("or" IDENT)* ;
trailing_or   = "or" ;  /* fallthrough: OR-accumulate with next arm */

call          = IDENT "(" (arg ("," arg)*)? ")" ;
arg           = (IDENT ":" expr) | expr ;  /* named or positional */

shared_block  = "shared" "any_age"? "{" shared_branch+ "}" ;
shared_branch = "from" (IDENT | "here") ":" expr ;

any_age_block = "any_age" "{" expr "}" ;

atom          = "always" | "never" | "is_child" | "is_adult"
              | "at_day" | "at_night" | "is_vanilla" | "is_mq"
              | "true" | "false"
              | IDENT | NUMBER ;

IDENT         = [A-Za-z_] [A-Za-z0-9_]* ;
NUMBER        = [0-9]+ ;
ident_list    = IDENT ("," IDENT)* ;
type          = IDENT ;
```

Key differences from the existing `LogicExpression` parser:
- `and`/`or`/`not` keywords instead of `&&`/`||`/`!`. `is`/`is not` as aliases for `==`/`!=`.
- `always`/`never`/`is_child`/`is_adult`/`at_day`/`at_night`/`is_vanilla`/`is_mq` as keywords.
- `shared { from ... }` and `any_age { ... }` as first-class expression blocks.
- `match <value> { ... }` expressions with trailing `or` for fallthrough accumulation.
- `enemy` declarations as top-level bestiary entries.
- File-level structure (`region`, `extend`, `define`, `enemy`).

---

## 8. Error Reporting

Since logic errors can cause softlocks in generated seeds, RLS prioritizes error quality:

| Error Class              | Example                              | Message                                                                                                                                                                |
| ------------------------ | ------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Unknown symbol**       | `can_use(RG_HOOKSHAT)`               | `error: unknown enum 'RG_HOOKSHAT'. Did you mean 'RG_HOOKSHOT'?`                                                                                                       |
| **Age impossibility**    | `is_child and can_use(RG_HOOKSHOT)`  | `warning: RG_HOOKSHOT is adult-only; condition is always false.`                                                                                                       |
| **Unreachable region**   | Region with no incoming exits        | `error: region 'RR_SPIRIT_TEMPLE_ORPHAN' has no incoming exits.`                                                                                                       |
| **Key over-requirement** | `keys(SCENE_SPIRIT_TEMPLE, 12)`      | `error: SPIRIT_TEMPLE has at most 5/7 small keys (Vanilla/MQ).`                                                                                                        |
| **Unused define**        | `define foo(): ...` never referenced | `warning: 'foo' is defined but never used.`                                                                                                                            |
| **Unknown function**     | `can_fly()`                          | `error: unknown function 'can_fly'. Available: can_use, can_kill, ...`                                                                                                 |
| **Duplicate entry**      | Same check in same region twice      | `error: duplicate location 'RC_SPIRIT_TEMPLE_LOBBY_POT_1' in region 'RR_SPIRIT_TEMPLE_FOYER'.` (Note: the same location in *different* regions is allowed - see §4.3.) |

---

## 9. File Organization

Logic files mirror the existing `location_access/` directory structure:

```
logic/
├── stdlib/
│   ├── helpers.rls                  # has_explosives, blast_or_smash, etc.
│   ├── combat.rls                   # can_hit_switch, can_break_pots (define + match)
│   ├── enemies.rls                  # enemy declarations (bestiary)
│   └── spirit_helpers.rls           # define functions for Spirit Temple
├── overworld/
│   ├── kokiri_forest.rls
│   ├── hyrule_field.rls
│   ├── kakariko.rls
│   └── ...
├── dungeons/
│   ├── spirit_temple.rls
│   ├── spirit_temple_mq.rls
│   ├── forest_temple.rls
│   └── ...
└── shuffle_features/
    ├── pots/
    │   ├── spirit_temple_pots.rls   # extend regions with pot locations
    │   ├── forest_temple_pots.rls
    │   └── ...
    ├── crates/
    │   ├── spirit_temple_crates.rls
    │   └── ...
    ├── grass/
    │   └── ...
    └── freestanding/
        └── ...
```

The `extend region` mechanism allows shuffle features to add locations to regions defined in the core dungeon files without modifying those files. This maps cleanly to how the C++ codebase already has `LOCATION()` entries for pots/crates/grass alongside chest locations.

---

## 10. Migration Strategy

The migration will be done as **one large PR** rather than incrementally. This avoids the complexity of maintaining two parallel systems (hand-written and generated C++) and ensures the full logic graph is consistent when it lands.

### Phase 1 - Parser & C++ Solver Transpiler
- Implement the RLS parser (lexer, AST, semantic analysis against enum registry).
- Implement the C++ transpiler generating solver lambdas and region definitions.
- Validate that transpiled output is structurally equivalent to the current hand-written code.

### Phase 2 - Expression Tree Generation
- Define the `ExpressionNode` C++ struct for the logic tracker.
- Extend the transpiler to generate expression tree structures alongside solver lambdas.
- Each condition produces both a lambda (for the solver) and an `ExpressionNode` tree (for the logic tracker).
- Build a new logic tracker that consumes the generated `ExpressionNode` trees.
- This is a separate phase because expression tree generation has different design constraints (display text formatting, tree depth, collapsible `define` nodes) than solver lambda generation.

### Phase 3 - Archipelago Python Target
- Implement the Python transpiler producing Archipelago `World` code.
- Generate Python helper functions mirroring the C++ `Logic` class methods, integrating with the existing `LogicHelpers` module pattern (see §6.2).
- Generate `item_name_to_id` and `location_name_to_id` from the enum registry.
- Validate against existing Archipelago OoT integration tests.

### Phase 4 - Full Conversion
- Convert all dungeons (Spirit Temple first as a proving ground, then the rest) and overworld areas to `.rls` files.
- Split pot, crate, grass, and freestanding locations into `shuffle_features/` files using `extend region`.
- Validate by running the existing seed generation tests to confirm parity across the entire logic graph.
- Remove the hand-written C++ `location_access/` files.
- Remove the `LogicExpression` runtime parser (`logic_expression_parse.cpp`, `logic_expression_eval.cpp`, `logic_expression_registry.cpp`) - its role is now filled entirely by transpiler-generated expression trees.
- The generated `.gen.cpp` files become build artifacts, not hand-edited source.

---

## 11. Full Example - Spirit Temple Foyer

```rls
# spirit_temple.rls

# ── Vanilla Regions ─────────────────────────────────────

region RR_SPIRIT_TEMPLE_ENTRYWAY {
    scene: SCENE_SPIRIT_TEMPLE

    exits {
        RR_SPIRIT_TEMPLE_FOYER:              is_vanilla
        RR_SPIRIT_TEMPLE_MQ_FOYER:           is_mq
        RR_DESERT_COLOSSUS_OUTSIDE_TEMPLE:   always
    }
}

region RR_SPIRIT_TEMPLE_FOYER {
    scene: SCENE_SPIRIT_TEMPLE

    events {
        LOGIC_FORWARDS_SPIRIT_CHILD: is_child
        LOGIC_FORWARDS_SPIRIT_ADULT: is_adult
    }

    locations {
        RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()
        RC_SPIRIT_TEMPLE_LOBBY_POT_2: can_break_pots()
    }

    exits {
        RR_SPIRIT_TEMPLE_ENTRYWAY:        always
        RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB:
            (is_adult or has(RG_SPEAK_GERUDO) or flag(LOGIC_SPIRIT_NABOORU_KIDNAPPED))
            and can_use(RG_CRAWL)
        RR_SPIRIT_TEMPLE_ADULT_SIDE_HUB:  can_use(RG_SILVER_GAUNTLETS)
    }
}

region RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB {
    scene: SCENE_SPIRIT_TEMPLE

    events {
        LOGIC_NUT_ACCESS: can_break_small_crates()
        LOGIC_SPIRIT_SILVER_RUPEE_BRIDGE_TORCHES:
            any_age { can_kill(RE_ARMOS) } and can_use(RG_STICKS)
    }

    exits {
        RR_SPIRIT_TEMPLE_FOYER:               can_use(RG_CRAWL)
        RR_SPIRIT_TEMPLE_CHILD_BOXES:         can_use(RG_CRAWL)
        RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_SOUTH: any_age { can_kill(RE_ARMOS) }
        RR_SPIRIT_TEMPLE_RUPEE_BRIDGE_SOUTH:  any_age { can_kill(RE_ARMOS) }
    }
}

region RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_SOUTH {
    scene: SCENE_SPIRIT_TEMPLE

    events {
        LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE:
            can_use(RG_BOOMERANG) or can_use(RG_FAIRY_SLINGSHOT)
            or can_use(RG_FAIRY_BOW)
            or (can_use(RG_BOMBCHU_5) and trick(RT_SPIRIT_CHILD_CHU))
    }

    exits {
        RR_SPIRIT_TEMPLE_CHILD_SIDE_HUB:      always
        RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_NORTH:
            (flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE)
             and can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false))
            or can_use(RG_HOVER_BOOTS)
            or can_use(RG_LONGSHOT)
    }
}

region RR_SPIRIT_TEMPLE_STATUE_ROOM_CHILD {
    scene: SCENE_SPIRIT_TEMPLE

    locations {
        RC_SPIRIT_TEMPLE_MAP_CHEST: has(RG_OPEN_CHEST) and shared {
            from here:
                has_fire_source_with_torch()
                or (trick(RT_SPIRIT_MAP_CHEST) and can_use(RG_FAIRY_BOW))
            from RR_SPIRIT_TEMPLE_STATUE_ROOM:
                has_fire_source()
        }

        RC_SPIRIT_TEMPLE_GS_LOBBY: shared {
            from here:
                can_get_drop(RE_GOLD_SKULLTULA, ED_LONGSHOT)
            from RR_SPIRIT_TEMPLE_INNER_WEST_HAND:
                can_get_drop(RE_GOLD_SKULLTULA,
                    trick(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT)
            from RR_SPIRIT_TEMPLE_GS_LEDGE:
                can_kill(RE_GOLD_SKULLTULA)
        }
    }

    exits {
        RR_SPIRIT_TEMPLE_SUN_ON_FLOOR_2F:   always
        RR_SPIRIT_TEMPLE_INNER_WEST_HAND:   always
        RR_SPIRIT_TEMPLE_GS_LEDGE:
            can_use(RG_HOVER_BOOTS) or reach_scarecrow()
        RR_SPIRIT_TEMPLE_PLATFORM:
            flag(LOGIC_SPIRIT_PLATFORM_LOWERED)
            and (can_use(RG_LONGSHOT)
                 or (trick(RT_SPIRIT_PLATFORM_HOOKSHOT) and can_use(RG_HOOKSHOT)))
        RR_SPIRIT_TEMPLE_EMPTY_STAIRS: has(RG_POWER_BRACELET)

        # Child reverse entry with 4 keys cannot lock out colossus
        RR_DESERT_COLOSSUS:
            setting(RSK_SHUFFLE_DUNGEON_ENTRANCES) is RO_DUNGEON_ENTRANCE_SHUFFLE_OFF
            and has(RG_POWER_BRACELET)
            and can_use(RG_CRAWL)
            and keys(SCENE_SPIRIT_TEMPLE, 4)
            and can_kill(RE_IRON_KNUCKLE)
    }
}
```

---

## 12. Comparison Summary

| Aspect                           | Current C++                                                                                | RLS                                                                                    |
| -------------------------------- | ------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------- |
| Lines for Spirit Temple Vanilla  | ~520                                                                                       | ~280 (est.)                                                                             |
| `logic->` / `ctx->` prefix noise | Every call                                                                                 | None - short function names                                                             |
| Lambda boilerplate               | `[]{return ...;}`                                                                          | None - bare expressions                                                                 |
| Macro wrappers                   | `ENTRANCE()`, `LOCATION()`, `EVENT_ACCESS()`                                               | Uniform `Name: condition` in typed sections                                             |
| Boolean operators                | `&&` / `||` / `!`                                                                          | `and` / `or` / `not`                                                                    |
| Setting comparisons              | `.Is(RO_*)` / `.IsNot(RO_*)` / `(bool)` cast                                               | `setting() is RO_*` / `is not RO_*` / bare truthiness                                   |
| SpiritShared calls               | 3-region callback soup                                                                     | `shared { from ...: ... }` blocks                                                       |
| Age/time helpers                 | `AnyAgeTime([]{...})`                                                                      | `any_age { ... }` blocks                                                                |
| Variant selection                | `ctx->GetDungeon(...)->IsVanilla()`                                                        | `is_vanilla` / `is_mq` keywords                                                         |
| Logic tracker data               | Runtime parsing of stringified C++                                                         | Transpiler-generated `ExpressionNode` trees                                             |
| Logic tracker display text       | `CleanConditionString(#condition)`                                                         | RLS source text embedded in generated structs                                          |
| Enemy combat knowledge           | Scattered across 3 `switch` statements (`CanKillEnemy`, `CanPassEnemy`, `CanGetEnemyDrop`) | `enemy` declarations - all knowledge in one place per enemy                             |
| Distance fallthrough             | C++ `switch` fallthrough (error-prone, implicit)                                           | `match` with trailing `or` (explicit, per-arm opt-in)                                   |
| Runtime parser dependency        | `LogicExpression` parser required                                                          | None - can be removed after full migration                                              |
| Archipelago integration          | Manual Python port, frequent drift                                                         | Auto-generated, matches existing `connect_regions`/`add_locations`/`add_events` pattern |
| Shuffle feature isolation        | All in one file per dungeon                                                                | `extend region` in dedicated feature files                                              |
| Error checking                   | Compile-time C++ errors (often cryptic)                                                    | Domain-specific error messages with suggestions                                         |
| Tooling targets                  | C++ only                                                                                   | C++ (lambdas + expression trees), Python/Archipelago                                    |
