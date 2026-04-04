# Simulator Pain Points

This file tracks pain points discovered while building larger scenario-style
examples such as the battle simulator and biome simulator.

## Current

- Projection currently reads only direct host fields. Large simulations want to
  project vessel-held state like `cycle.age` or `traits.metabolism`, but today
  that requires mirroring fields onto the `subject` or narrowing the
  `object`/`dto` surface.
- Zone ecology/state gets verbose quickly because large shared state must be
  written as many `shared Int` fields. A `zone vessel` or grouped passive state
  holder would make bigger worlds easier to model.
- `world` has been lowered to a contextual keyword for locals, but helper
  function parameter positions can still be fragile. In practice, naming a
  parameter `world` is best avoided until every parser path treats it as a
  plain identifier outside declarations.
- Mutating a zone after composing it into a world is still a sharp edge. Rich
  scenarios are more predictable when zones are fully configured first and only
  then embedded into the world value.
- World-level composed-state queries such as `HasZone(allProjectionReady)` still
  show backend parity gaps in larger scenarios. The biome simulator currently
  runs on both backends, but composed-state truth values are not yet fully
  aligned between C and LLVM.
- Scenario smoke tests are still substring-based. Rich simulators benefit from
  folder-level golden outputs so structural regressions are easier to detect.

## Recently Resolved

- Host-owned fields no longer require mandatory `self.` in hosted `func` /
  `action` bodies. Bare field access now works across subject/class/relation/
  effect/zone/world hosts, with `self.` still available as an explicit form.
- Hosted helper calls such as `PlantMass()`, `SeasonPulse()`, `battle.Tick()`,
  and `player.Hurt()` now lower correctly without mandatory `self.` on both C
  and LLVM paths.
