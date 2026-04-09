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

The scenario now supports three execution modes:

- scripted regression mode
- seeded random mode
- player-input mode

Scripted mode remains exact-golden-friendly. Random and player mode run the
same world state machine, but swap the choice/combat roll source.

## Intended Language Surface

This scenario should exercise:

- `subject`
- `vessel`
- `object` / `tobject`
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
- `subjects/vessels.pgy`
- `subjects/abilities.pgy`
- `subjects/roles.pgy`
- `subjects/views.pgy`
- `subjects/units.pgy`
- `zones/layers.pgy`
- `zones/journey.pgy`
- `intents/campaign_intents.pgy`
- `tables.pgy`
- `dialogue.pgy`
- `combat_text.pgy`
- `combat_cards.pgy`
- `story_cards.pgy`
- `events.pgy`
- `world.pgy`
- `setup.pgy`
- `report.pgy`
- `main.pgy`
- `random_main.pgy`
- `player_main.pgy`
- `expected_stdout.txt`
- `expected_results.txt`

Generated at runtime:

- `results.txt`
- `results.random.txt`
- `results.player.txt`

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
It also stresses subject-owned `class` tools: `Adventurer.weapon: WeaponCard` is embedded as a passive value object and used through hosted `func` calls inside the campaign flow.

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
- The campaign now runs as an explicit world-owned game state machine:
  tavern -> floor 1 -> floor 2 -> floor 3 -> dragon -> epilogue
- The same state machine now runs in three public entry modes:
  `RunCampaignScripted()`, `RunCampaignRandom(seed)`, and
  `RunCampaignPlayer()`
- Each phase now exposes four numbered options and logs the rolled
  deterministic choice, so the transcript reads like a GM-driven scenario
  instead of a straight-line function dump
- Player mode now records prompts and chosen inputs directly into the
  transcript, so the same report can be read either as a regression transcript
  or as an interactive text-campaign log
- The transcript is now substantially denser: each major phase emits explicit
  state-entry/state-exit markers, six scene beats, five node/event beats,
  and more detailed combat rounds before the final report snapshot
- The protagonist and companions are built through deterministic DND-style
  sheet/spec factories rather than raw constructor spam
- Character creation now carries explicit `background`, `intro`, `crest`, and
  `signature` flavor through factory-built specs into the live `subject`
  instances, so tavern recruitment reads like campaign setup rather than
  constructor noise
- The world entry path is now also factory-shaped. `main.pgy` opens the
  scenario through `OpenTavernCampaign()` instead of spelling a long
  `TavernCampaignWorld(...)` constructor call inline, so the example reads like
  "enter the tavern campaign" rather than "manually wire every shared field"
- The zone/world runtime now tracks more campaign-like narrative state:
  `morale`, floor camp reports, tavern hooks, and dragon epilogue text all flow
  into stdout and `results.txt`
- The campaign now also exercises live `relation` / `effect` runtime wiring via
  party-bond links and battle-blessing layers, with `HasZoneLayer(...)` /
  `HasZoneState(...)` showing up in the final report path
- The campaign now also carries first-class `intent` declarations in
  `intents/campaign_intents.pgy`, and those intents are now actually invoked from the live
  world state machine. Tavern recruitment, floor delving, and dragon-slaying
  are no longer just static contracts; they execute as generated intent
  functions on top of the same live `JourneyZone` / `Adventurer` runtime model
- Those DND intents now bind a concrete live zone instance through
  `using: journey;`. The runtime no longer treats `where: JourneyZone` as
  static metadata only; each step re-syncs the actual bound `JourneyZone`
  instance while it orchestrates the party subjects
- DND intent steps now use repeated `on:` clauses plus `guard` / `invariant`,
  so a single declarative
  step can orchestrate multiple subject actions in order without dropping to a
  free-function wrapper
- Those layers now affect actual campaign math: tavern morale, route pressure,
  trap pressure, and dragon preparation all change based on bond/blessing state
- Combat resolution is now driven by game-layer factories and strategy cards.
  Each seat resolves through weapon loadout factories plus per-round strategy
  selection, so class flavor, posture, aggression, guard pressure, and effect
  text come from data-shaped helpers instead of one-off hard-coded lines
- Scene/event pacing is now also card-driven. GM prompts, stakes, rewards,
  companion reactions, and boss phase intent all come from story-card
  factories instead of only the core world loop
- Journey projection wiring now uses `bind ... from ...` so object/tobject target
  kind comes from slot declarations instead of forcing repeated `refresh` /
  `publish` choices in the scenario body
- `results.txt` is transcript-first and now captures the entire scenario flow,
  not just a final summary
- The current transcript/report size is now in the low-thousands of lines,
  and currently sits around one thousand lines per exact golden, which is
  enough to read like a substantial GM transcript instead of a short demo
- Exact stdout and exact `results.txt` goldens are now part of the regression shape

## Discovered Pain Points

- Hosted helper forward declarations in the C transpiler were too weak for
  larger zone/world hosts. `JourneyZone.Snapshot()` exposed this when it called
  helpers defined later in the same host. The C backend now emits hosted
  forward declarations for class/party/roster/relation/effect/zone/world
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
- Rich constructor calls that omit newer trailing `world shared` fields can
  still create backend-sensitive behavior. The campaign hit this when the new
  FSM choice fields were added; the fix was to make the root world constructor
  explicit about every trailing shared field used by the state machine.
- Intent is no longer declaration-only. The campaign now calls
  `TavernRecruitment(...)`, `DelveThreeFloors(...)`, and `SlayDragon(...)`
  directly from the world state machine, and those calls lower to generated
  runtime functions on both C and LLVM.
- The remaining intent gap is now narrower: basic runtime conflict
  arbitration now exists for `exclusive` / `concurrent` / `priority`,
  reverse-order `compensate:` rollback is present, and
  `IntentLastTrace()` / `IntentLastFailure()` /
  `IntentLastName()` / `IntentLastHandle()` /
  `IntentLastStepCount()` / `IntentLastFailed()` now expose minimal execution
  history, and `IntentHistoryCount()` / `IntentHistoryStep*()` expose the last
  completed intent's step-level typed history. `using: journey;` now provides
  live concrete zone-instance binding and actor-to-zone-slot materialization
  for scenario intents. The remaining gaps are richer trace ids, richer
  rollback policy, and true cross-world transfer semantics.
- Runtime hashmap helpers used raw `strdup(...)`, which leaked portability
  warnings into larger example builds. The runtime now routes those copies
  through `pgy_runtime_strdup(...)` instead.
- Hosted slot built-ins and method sugar used to diverge for non-primitive
  payloads. While tightening this scenario, `let v = Read(slot)` and
  `let v = slot.Read()` on `Slot<Vec2>` exposed separate C and LLVM failures.
  Those are now fixed, and both forms behave the same in the campaign runtime.

## Remaining Follow-up

- Scripted, random, and player mode now all exist, but only scripted mode is
  exact-golden-covered in the default smoke path. If player mode becomes part
  of normal regression, the smoke harness will need a first-class stdin script
  layer.
- The campaign currently stresses `subject/vessel/role/ability/zone/world`
  heavily, and now includes relation/effect runtime values that directly modify
  campaign flow, but the combat engine still stops short of a full tactical
  rules engine with initiative stacks, per-target targeting rules, and deeper
  status-resolution ordering.
- The campaign now looks like a game state machine in the transcript, but the
  encounter engine is still world-hosted orchestration rather than a
  standalone encounter/turn DSL with richer AI policy authoring.
