# Rando Logic Script (RLS) - Overview

> This is a condensed overview of RLS. For the full specification - grammar, type inference, transpiler output details, error reporting, migration strategy, and more - see [RandoLogicScript-Full.md](RandoLogicScript-Full.md).

## What & Why

RLS is a **declarative, domain-specific language** for defining randomizer logic - regions, locations, exits, events, and their access conditions. It transpiles to **C++** (for [Shipwright](https://github.com/HarbourMasters/Shipwright)) and **Python** (for [Archipelago-SoH](https://github.com/HarbourMasters/Archipelago-SoH), possibly rulebuilder json), so both projects share a single source of truth.

**Current C++:**
```cpp
ENTRANCE(RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_NORTH,
    (logic->Get(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE) &&
     logic->CanPassEnemy(RE_GREEN_BUBBLE, ED_CLOSE, false)) ||
    logic->CanUse(RG_HOVER_BOOTS) ||
    logic->CanUse(RG_LONGSHOT)),
```

**RLS equivalent:**
```RLS
RR_SPIRIT_TEMPLE_SWITCH_BRIDGE_NORTH:
    (flag(LOGIC_SPIRIT_CHILD_SWITCH_BRIDGE)
     and can_pass(RE_GREEN_BUBBLE, ED_CLOSE, false))
    or can_use(RG_HOVER_BOOTS)
    or can_use(RG_LONGSHOT)
```

No lambdas, no macros, no `logic->` prefixes. `and`/`or`/`not` instead of `&&`/`||`/`!`.

---

## Regions

A region defines a scene, events, locations, and exits. Each entry is `Name: condition`.

```RLS
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

- `always` / `never` are aliases for `true` / `false`.
- `is_vanilla` / `is_mq` replace `ctx->GetDungeon(...)->IsVanilla()`.
- All names are the same `RR_*`, `RC_*`, `RG_*`, etc. enum identifiers used in C++.

Regions can also be extended from separate files (e.g. shuffle features adding pot locations without touching core dungeon logic):

```RLS
# shuffle_pots/spirit_temple_pots.rls
extend region RR_SPIRIT_TEMPLE_FOYER {
    locations {
        RC_SPIRIT_TEMPLE_LOBBY_POT_1: can_break_pots()
    }
}
```

---

## Conditions & Built-In Functions

Conditions are boolean expressions using short function names that the transpiler maps to C++ methods:

| RLS                                         | C++                                                  |
| -------------------------------------------- | ---------------------------------------------------- |
| `has(RG_HOOKSHOT)`                           | `logic->HasItem(RG_HOOKSHOT)`                        |
| `can_use(RG_HOOKSHOT)`                       | `logic->CanUse(RG_HOOKSHOT)`                         |
| `keys(SCENE_SPIRIT_TEMPLE, 3)`               | `logic->SmallKeys(SCENE_SPIRIT_TEMPLE, 3)`           |
| `flag(LOGIC_SPIRIT_PLATFORM_LOWERED)`        | `logic->Get(LOGIC_SPIRIT_PLATFORM_LOWERED)`          |
| `setting(RSK_FOREST) is RO_CLOSED_FOREST_ON` | `ctx->GetOption(RSK_FOREST).Is(RO_CLOSED_FOREST_ON)` |
| `trick(RT_SPIRIT_CHILD_CHU)`                 | `ctx->GetTrickOption(RT_SPIRIT_CHILD_CHU)`           |
| `is_child`, `is_adult`, `at_day`, `at_night` | `logic->IsChild`, etc.                               |

`any_age { ... }` evaluates across all age/time combinations with access to the current region:

```RLS
any_age { can_kill(RE_ARMOS) }     # → region->AnyAgeTime(...)
```

---

## Functions (`define`)

Reusable logic is defined at file scope. Functions are pure (no side effects).

```RLS
define has_explosives():
    has(RG_BOMB_BAG) or has(RG_BOMBCHU_5)

define spirit_explosive_key_logic():
    keys(SCENE_SPIRIT_TEMPLE, has_explosives() ? 1 : 2)
```

---

## Match Expressions

`match` dispatches on an enum value. A trailing `or` at the end of an arm means **fallthrough** - OR-accumulate with the next arm. This replaces C++ `switch` fallthrough for distance-based dispatch:

```RLS
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

`can_hit_switch(ED_BOOMERANG)` returns `can_use(RG_BOOMERANG) or can_use(RG_HOOKSHOT) or ... or can_use(RG_FAIRY_BOW)` - everything from that arm downward. The last arm (`ED_FAR`) has no trailing `or`, so accumulation stops.

`or` before a colon is a multi-value pattern: `ED_CLOSE or ED_SHORT_JUMPSLASH:` matches either.

---

## Enemy Declarations

All knowledge about one enemy in one place - replaces scattered `CanKillEnemy`/`CanPassEnemy`/`CanGetEnemyDrop` switches:

```RLS
enemy RE_GOLD_SKULLTULA {
    kill(wallOrFloor = true):
        match distance {
            ED_CLOSE: can_use(RG_MEGATON_HAMMER) or
            ED_SHORT_JUMPSLASH: can_use(RG_KOKIRI_SWORD) or
            # ... arms with fallthrough ...
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

The transpiler wires `can_kill(RE_GOLD_SKULLTULA, ED_HOOKSHOT)` to the `kill` field above. Omitted fields get defaults: `pass` → `always`, `drop` → same as `kill`, `avoid` → `always`.

---

## Shared Blocks (Replacing SpiritShared)

`shared` blocks replace the `SpiritShared()` callback soup with explicit multi-region access paths:

```RLS
RC_SPIRIT_TEMPLE_GS_LOBBY: shared {
    from here:
        can_get_drop(RE_GOLD_SKULLTULA, ED_LONGSHOT)
    from RR_SPIRIT_TEMPLE_INNER_WEST_HAND:
        can_get_drop(RE_GOLD_SKULLTULA,
            trick(RT_SPIRIT_WEST_LEDGE) ? ED_BOOMERANG : ED_HOOKSHOT)
    from RR_SPIRIT_TEMPLE_GS_LEDGE:
        can_kill(RE_GOLD_SKULLTULA)
}
```

`shared any_age { ... }` sets the `anyAge` flag for evaluating with all ages that have CertainAccess.

---

## Transpilation

The transpiler produces two targets from the same `.rls` source:

**C++** - Solver lambdas (same structure as today's macros) plus `ExpressionNode` trees for the logic tracker, eliminating the `LogicExpression` runtime parser.

**Python** - Archipelago-compatible `connect_regions()` / `add_locations()` / `add_events()` code matching the existing `LogicHelpers` pattern.

---

## File Organization

```
logic/
├── stdlib/
│   ├── helpers.rls              # has_explosives, blast_or_smash, etc.
│   ├── combat.rls               # can_hit_switch, can_break_pots
│   └── enemies.rls              # enemy declarations (bestiary)
├── overworld/                    # kokiri_forest.rls, hyrule_field.rls, ...
├── dungeons/                     # spirit_temple.rls, forest_temple.rls, ...
└── shuffle_features/
    ├── pots/                     # extend regions with pot locations
    ├── crates/
    └── ...
```

---

## At a Glance

| Aspect               | Current C++                          | RLS                                       |
| -------------------- | ------------------------------------ | ------------------------------------------ |
| Boilerplate          | Lambdas, macros, `logic->` prefixes  | Bare `Name: condition` entries             |
| Boolean operators    | `&&` / `\|\|` / `!`                  | `and` / `or` / `not`                       |
| SpiritShared         | 3-region callback soup               | `shared { from ...: ... }`                 |
| Enemy knowledge      | Scattered across 4 switch statements | `enemy` declarations - one block per enemy |
| Distance fallthrough | C++ `switch` fallthrough (implicit)  | `match` with trailing `or` (explicit)      |
| Logic tracker        | Runtime parsing of stringified C++   | Transpiler-generated expression trees      |
| Archipelago sync     | Manual Python port, frequent drift   | Auto-generated from same source            |
| Shuffle features     | Mixed into dungeon files             | `extend region` in dedicated files         |
