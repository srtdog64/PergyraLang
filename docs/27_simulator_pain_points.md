# Simulator Pain Points

This file tracks pain points discovered while building larger scenario-style
examples such as the battle simulator and biome simulator.

## Current

- Projection sync is still host-field centric. Rich scenarios are usable now,
  but larger framework work will eventually want higher-level projection/binding
  surfaces over repeated `refresh` / `publish` wiring.
- Rich constructor/default-state paths still have backend-sensitive edges in
  large `world` + embedded `zone` scenarios. The new FSM example stabilizes its
  runtime with an explicit `ResetFactory`/`ResetBaseline` pass after world
  construction, but deeper constructor parity is still worth tightening.
- LLVM still has a gap between stable slot built-ins and slot method sugar.
  `Write(slot, value)` / `Read(slot)` are stable and now back the raid graph
  example, but direct `slot.Write(...)` / `slot.Read()` on local slots remain a
  separate LLVM bug to close.

## Recently Resolved

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
