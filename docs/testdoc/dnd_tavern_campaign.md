# DND Tavern Campaign

## Goal

This scenario is the first intentionally larger DND-style campaign slice for
Pergyra.

It exists to answer a harder question than the earlier graph/FSM samples:

Can Pergyra still stay readable when one program combines:

- tavern-phase dialogue and recruitment
- four persistent party members
- role/ability declarations
- multi-floor dungeon traversal
- graph-like route costs
- traps, monsters, and layered status effects
- affinity-driven companion events
- a final boss phase
- report generation with exact regression coverage

## Scenario Outline

The scenario models this flow:

1. The protagonist enters a tavern.
2. The protagonist talks to companions and forms a four-person party.
3. The party enters a three-floor dungeon.
4. Each floor has traps, monsters, and affinity-sensitive companion events.
5. The party reaches the dragon lair and defeats the final boss.

The runtime is deterministic. The “random dungeon” is represented as a seeded,
scripted route/threat pattern so exact goldens remain stable on both backends.

## Intended Language Surface

This scenario should exercise:

- `subject`
- `vessel`
- `object` / `dto`
- `ability`
- `role`
- `relation`
- `effect`
- `zone`
- `world`
- `Slot<T>`
- hosted `func`
- hosted `action`
- file output and exact report snapshots

## File Layout

The example lives under `examples/dnd_tavern_campaign/`.

Current source/golden layout:

- `common.pgy`
- `vessels.pgy`
- `abilities.pgy`
- `roles.pgy`
- `layers.pgy`
- `views.pgy`
- `units.pgy`
- `tables.pgy`
- `journey.pgy`
- `world.pgy`
- `setup.pgy`
- `report.pgy`
- `main.pgy`
- `expected_stdout.txt`
- `expected_results.txt`

Generated at runtime:

- `results.txt`

## Systems

### Party Persistence

The same four adventurers must stay legible across:

- tavern
- floor 1
- floor 2
- floor 3
- dragon lair

### State Layers

Each adventurer carries multiple state groups:

- battle stats
- bonds / affinity
- pack / loot
- effect state
- quest/FSM state

This is specifically meant to stress the `subject + vessel` model.

### Dungeon Graph

The dungeon is linear in floor count but still graph-shaped in traversal cost.
The world owns route costs and adjusts them based on zone state:

- town -> gate
- floor 1 -> floor 2
- floor 2 -> floor 3
- floor 3 -> lair

### Affinity Events

Companion affinity is not just flavor text. It should influence:

- buffs
- recovery
- route stability
- boss preparation quality

## Regression Goal

This scenario is intended to become a regression-grade example like
`campaign_graph_fsm`, with:

- exact stdout golden
- exact `results.txt` golden
- example smoke coverage

## Expected Pain Points

This scenario is likely to expose:

- large hosted string/report paths
- nested vessel access under world/zone orchestration
- action-heavy subject code readability limits
- role/ability surface that exists semantically but still wants better runtime ergonomics
- the point where grouped passive state may want more framework help

## Current Status

- Multi-file implementation is running end to end on both C and LLVM
- The protagonist and companions are built through deterministic DND-style
  sheet/spec factories rather than raw constructor spam
- Character creation now carries explicit `background`, `intro`, `crest`, and
  `signature` flavor through factory-built specs into the live `subject`
  instances, so tavern recruitment reads like campaign setup rather than
  constructor noise
- The zone/world runtime now tracks more campaign-like narrative state:
  `morale`, floor camp reports, tavern hooks, and dragon epilogue text all flow
  into stdout and `results.txt`
- The campaign now also exercises live `relation` / `effect` runtime wiring via
  party-bond links and battle-blessing layers, with `HasZoneLayer(...)` /
  `HasZoneState(...)` showing up in the final report path
- Those layers now affect actual campaign math: tavern morale, route pressure,
  trap pressure, and dragon preparation all change based on bond/blessing state
- `results.txt` is transcript-first and now captures the entire scenario flow,
  not just a final summary
- Exact stdout and exact `results.txt` goldens are now part of the regression shape

## Discovered Pain Points

- Hosted helper forward declarations in the C transpiler were too weak for
  larger zone/world hosts. `JourneyZone.Snapshot()` exposed this when it called
  helpers defined later in the same host. The C backend now emits hosted
  forward declarations for class/party/systemic/relation/effect/zone/world
  methods before bodies.
- Standalone helper predeclarations also needed filtering. The DND campaign's
  factory refactor introduced helpers like `BuildJourneyZone() -> JourneyZone`,
  and blindly predeclaring every free function too early broke the C backend
  before domain types were emitted. The transpiler now only early-predeclares
  standalone helpers whose signatures use already-known types.
- LLVM `ToString(Bool)` was incorrectly lowering `i1` directly into the
  `pgy_int_to_string(i32)` runtime path. The coercion path now widens bools
  before string conversion, so the DND campaign runs on LLVM as well as C.
- The default example smoke path could pick a stale repo-local `bin/pgy` while
  a newer `/tmp/pgy-PergyraLang-bin/pgy` existed from test builds. Example
  smoke now prefers the newer tmp binary automatically unless `PGY_BIN` is set.
- World-level layer/state queries must use the cross-layer surface
  `HasZoneLayer(zoneSlot, layerSlot)` / `HasZoneState(zoneSlot, stateName)`
  rather than zone-local `HasLayer(...)` / `HasState(...)`. The DND campaign
  hit this immediately when layer visibility was added to world transcript logs.
- Layer shared defaults were not something this scenario could safely treat as
  “set and forget”. The campaign now explicitly initializes layer shared values
  in `TavernScene`, which makes the runtime behavior legible and stable on both
  backends.

## Remaining Follow-up

- The scenario is now regression-grade, but it is still deterministic. If the
  campaign grows further, a seeded random surface may want a first-class helper
  instead of ad hoc deterministic-roll funcs in `setup.pgy`.
- The campaign currently stresses `subject/vessel/role/ability/zone/world`
  heavily, and now includes relation/effect runtime values that directly modify
  campaign flow, but the combat engine still stops short of a full tactical
  rules engine with initiative stacks, per-target targeting rules, and deeper
  status-resolution ordering.
