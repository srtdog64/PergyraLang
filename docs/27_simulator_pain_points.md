# Simulator Pain Points

This file tracks pain points discovered while building larger scenario-style
examples such as the battle simulator and biome simulator.

## Current

- Projection sync is still host-field centric. Rich scenarios are usable now,
  but larger framework work will eventually want higher-level projection/binding
  surfaces over repeated `refresh` / `publish` wiring.
- Rich constructor/default-state paths still have a backend-sensitive edge in
  large `world` + embedded `zone` scenarios. `fsm_factory` now runs without the
  old reset workaround, but C still misses at least one `world shared` default
  initializer (`gridNoise`) that LLVM applies correctly. Deeper constructor
  parity is still worth tightening.
- Stable slot built-ins and slot method sugar still diverge. `Write(slot, value)`
  / `Read(slot)` remain the reliable path, but direct local member-call sugar
  (`slot.Write(...)` / `slot.Read()` / `slot.Release()`) still has a split bug:
  C type inference can still treat `slot.Read()` results too narrowly, while
  LLVM can still lower the same path to the wrong runtime value. This now looks
  broader than an LLVM-only issue.

## Recently Resolved

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
