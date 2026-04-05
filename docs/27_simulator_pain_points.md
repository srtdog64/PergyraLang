# Simulator Pain Points

This file tracks pain points discovered while building larger scenario-style
examples such as the battle simulator and biome simulator.

## Current

- Projection sync is lighter now, but larger framework work still wants a
  richer declarative surface beyond one-slot-at-a-time `bind`. Grouped
  bindings and deeper auto-propagation are the next pressure point.
- Large story scenarios now support scripted, seeded-random, and player-input
  modes, but the encounter engine is still world-hosted orchestration rather
  than a first-class encounter/turn DSL with richer AI policy authoring and
  deeper tactical resolution ordering.
- Intent declarations now execute, but the runtime is still minimal. Large
  story scenarios can now call `Intent(args...)` and move subjects through
  repeated `on:` clauses or zero-arg same-name action dispatch, and
  `exclusive` / `concurrent` / `priority` now have a concrete runtime conflict
  registry on both C and LLVM. Step-level `guard` / `invariant` now run too,
  reverse-order `compensate:` rollback works, and `IntentLastTrace()` /
  `IntentLastFailure()` plus `IntentLastName()` / `IntentLastHandle()` /
  `IntentLastStepCount()` / `IntentLastFailed()` expose minimal history.
  `IntentHistoryCount()` / `IntentHistoryStep*()` also expose typed
  step-level history for the last completed intent.
  `using: zoneAlias;` now gives intent steps a live concrete zone instance and
  now also materializes bound `who` actors into matching zone subject slots
  before sync. `transfer: source -> target;` now also performs v1 cross-world
  handoff materialization and leaves explicit transfer trace lines on both C
  and LLVM. The remaining gaps are structured trace/history data instead of
  richer trace ids beyond the current last-intent surface and richer rollback
  policy.

## Recently Resolved

- LLVM nominal dispatch no longer lets stale local class-tracking shadow the
  current host field. Larger scenarios such as `dnd_tavern_campaign` exposed
  this sharply once `subject` started owning `class` tools directly
  (`Adventurer.weapon: WeaponCard`): a previous function's `weapon` local could
  leak into later host methods and collapse `weapon.Summary()` to `0` on the
  LLVM path. Current lookup now prefers in-scope locals and then current host
  fields, so subject-owned class tools dispatch consistently on both C and LLVM.
- LLVM nominal inference now follows unqualified host methods that return class
  values. Paths such as `let weapon = MemberWeapon(); weapon.Strain(...)` are
  now regression-covered and backend-equal instead of losing the returned class
  identity on the LLVM side.
- `bind <slot> from <source>` now exists for `relation` / `effect` / `zone`.
  It keeps the explicit projection contract but removes the extra `refresh` vs
  `publish` split from many scenario declarations by inferring `object` vs
  `dto` from the target slot kind.

- `fsm_factory` no longer needs the old `ResetFactory()` workaround to seed
  `world shared` defaults before simulation. Current C and LLVM runs now agree
  on constructor/default-state behavior for that scenario, so the example can
  construct `FactoryWorld(alpha, beta)` and run immediately.
- C domain/world constructor lowering now preserves omitted `shared = default`
  initializers instead of silently zeroing them. This closes the `gridNoise`
  parity bug that `fsm_factory` exposed between C and LLVM.
- Stable slot built-ins and slot method sugar now agree for local slots on both
  backends, including non-primitive payloads like `Slot<Vec2>`. C type
  inference now treats `Read(slot)` / `slot.Read()` as the inner payload type,
  and LLVM now preserves the same nominal value path so `let v = Read(slot);
  v.x` and `let v = slot.Read(); v.x` both produce the correct runtime value.

- Early standalone helper predeclarations on the C backend are no longer
  blindly emitted before domain types exist. Larger scenario factories can now
  use helpers like `BuildJourneyZone() -> JourneyZone` without introducing
  `unknown type name` failures in generated C.
- Example smoke no longer prefers a stale repo-local `bin/pgy` over the newer
  tmp build artifact produced by test targets. It now auto-picks the newer
  `/tmp/pgy-PergyraLang-bin/pgy` unless `PGY_BIN` is explicitly set.
- Hosted helper forward declarations are no longer order-sensitive only because
  of source layout. Large class/party/systemic/relation/effect/zone/world hosts
  now emit method prototypes before bodies on the C backend, so scenario code
  such as `JourneyZone.Snapshot()` can call helpers declared later in the host.
- LLVM `ToString(Bool)` no longer sends raw `i1` values into the integer-string
  runtime function. Bool string conversion now widens through the same coercion
  path used by mixed string concatenation.
- `ToString(Int)` no longer reuses a single static buffer across nested string
  concatenations. The campaign graph/FSM scenario exposed this sharply on the C
  backend, where strategic/world report lines collapsed many numbers to the
  same value. `pgy_int_to_string` now returns a fresh string, so C and LLVM
  agree on large report-style outputs.
- Projection no longer stops at direct host fields. `ToObject`/`ToDto` and
  domain-local `refresh`/`publish` now resolve nested vessel-backed fields such
  as `cycle.age` or `traits.metabolism` automatically.
- Zone ecology/state no longer has to explode into many `shared Int` fields.
  `zone vessel` now gives larger scenarios a grouped passive state holder.
- `world`/`zone`/`effect`/`relation`/`actor`-style names now work in many more
  plain identifier positions, including local bindings and function parameters.
- Scenario result files are no longer substring-only. Folder-level
  `expected_results.txt` goldens now allow exact comparison for simulator
  outputs such as `battle_simulator/results.txt` and `biome_simulator/results.txt`.
- Scenario stdout is no longer substring-only. Example smoke now supports
  backend-aware exact normalized stdout goldens, so richer simulators can pin
  full console transcripts separately for C and LLVM.
- Example smoke no longer hard-depends on external `diff` for exact golden
  checks. CI paths such as GitHub Actions on Windows can now fall back through
  `cmp`, `git diff --no-index`, or Python unified diff output instead of
  failing before the actual example comparison runs.
- Host-owned fields no longer require mandatory `self.` in hosted `func` /
  `action` bodies. Bare field access now works across subject/class/relation/
  effect/zone/world hosts, with `self.` still available as an explicit form.
- Hosted helper calls such as `PlantMass()`, `SeasonPulse()`, `battle.Tick()`,
  and `player.Hurt()` now lower correctly without mandatory `self.` on both C
  and LLVM paths.
- World-level layer/state reporting is now clearer in practice: large scenarios
  should use `HasZoneLayer(worldZoneSlot, layerSlot)` / `HasZoneState(...)`
  from world code rather than zone-local `HasLayer(...)` / `HasState(...)`.
- In larger relation/effect scenarios, layer shared defaults are clearer and
  more stable when the scenario explicitly seeds them during setup instead of
  assuming declaration-time defaults will be the only source of truth.
- Zones embedded into a world value are now frozen outside that world root.
  Direct assignment or hosted `func` / `action` calls through the old zone
  binding now produce semantic errors instead of soft warnings.
- Complex simulator scenarios now have a dedicated folder shape with exact
  stdout and `results.txt` goldens. `examples/fsm_factory/` joins battle and
  biome as a regression-grade multi-file scenario rather than an ad hoc sample.
- The new `raid_graph_fsm` scenario now exercises graph costs, world-owned room
  topology, slot-based staging, and role-differentiated subject FSM behavior on
  both C and LLVM backends with matching stdout and report output.
- LLVM world `all/any` composed-state queries now match the C backend in
  simulator paths such as `battle_simulator`.
- DND transcript helpers no longer rely on C-unsafe `String == "literal"`
  comparisons for class-sensitive flavor text. Campaign dialogue/combat text
  now keys off stable role/class codes, so dense story transcripts run without
  backend-specific warnings leaking into stdout.
- DND campaign execution is no longer regression-only. The same world state
  machine now runs in scripted, seeded-random, and player-input mode, with
  prompts and choices recorded directly into the transcript.
- DND combat narration is no longer only a world-hosted string table. Weapon
  factories and strategy cards now drive seat-by-seat combat text, so class
  flavor, posture, guard pressure, and effect hints come from game-layer
  factories rather than one-off combat strings.
- Intent step contracts no longer stop at name lookup. The semantic checker now
  validates that `who` / `authorized by` actors have matching subject slots in
  the referenced zone, and that `requires` abilities are actually implemented
  by the declared actor subject type.
- Runtime hashmap helpers no longer leak raw `strdup(...)` warnings into large
  example builds. They now route string duplication through the local
  `pgy_runtime_strdup(...)` wrapper.
- LLVM `zone effect pool` lowering no longer stops at semantic validation.
  Pooled effect layers now have concrete runtime storage and `HasLayer(pool)`
  smoke coverage on the LLVM path without compiler crashes.
- Intent runtime no longer stops at a declaration-shaped sequential function.
  Both backends now register active intent instances, reject conflicting
  same-subject `exclusive` entries, allow `concurrent`/`concurrent`
  coexistence, and allow higher-priority nested intents to override lower
  priority active entries. This also fixed LLVM nested intent calls that were
  incorrectly forwarding subject-pointer allocas instead of the subject
  pointer value itself.
- Intent steps with explicit `on:` clauses no longer emit the misleading
  warning that they are "declarative only" just because no same-name subject
  action exists. If a step has concrete `on:` expressions, the compiler now
  treats that as a real executable lowering path.
- The new shopping-mall checkout/refund scenario exposed a practical boundary
  in `using:` + actor materialization: trace and slot binding are strong, but
  richer nested actor state is still more predictable when a canonical actor
  is updated explicitly and zones mirror the shared execution result. The
  example now follows that pattern and documents it as the safer v1 shape.
