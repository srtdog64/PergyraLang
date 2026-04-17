# Testdoc: logistics_intent_probe

`logistics_intent_probe` is the first example deliberately built to cross all four compiler layers together:

- `DIR`: role/ability completeness, zone/world membership, intent step dependency
- `RIR`: relation/effect/zone/world handle facts, `using:`/`transfer:` resource ops, branch/join flow blocks
- `MIR`: phi placement over a local merge function, intent cleanup/rollback/invalidation CFG
- runtime: actual `intent` execution, typed history, transfer trace, final report output

## Scenario Goal

The scenario is intentionally small. A courier and dispatcher move cargo from `LoadingZone` to `DeliveryZone` through `RouteCargo(...)`. The point is not game depth. The point is to make one example that:

- runs end-to-end
- dumps `--dir`, `--rir`, `--mir`
- exercises `role`, `ability`, `relation`, `effect`, `zone`, `world`, `intent`
- forces a real CFG merge through `MergeRouteScore(...)`

## Files

Read it in this order:

1. `examples/logistics_intent_probe/intents/logistics_intents.pgy`
2. `examples/logistics_intent_probe/world.pgy`
3. `examples/logistics_intent_probe/zones/*.pgy`
4. `examples/logistics_intent_probe/subjects/*.pgy`
5. `examples/logistics_intent_probe/setup.pgy`
6. `examples/logistics_intent_probe/main.pgy`

- `examples/logistics_intent_probe/main.pgy`
- `examples/logistics_intent_probe/common.pgy`
- `examples/logistics_intent_probe/intents/logistics_intents.pgy`
- `examples/logistics_intent_probe/world.pgy`
- `examples/logistics_intent_probe/zones/*.pgy`
- `examples/logistics_intent_probe/subjects/*.pgy`
- `examples/logistics_intent_probe/setup.pgy`

## What It Exercises

- role/ability completeness edge
- zone authority and layer declarations
- effect/relation handles as explicit RIR facts
- `using:` plus `transfer:` on a real intent
- intent compensation and cleanup graph
- MIR phi nodes from `MergeRouteScore(...)`
- report persistence via `WriteFile(...)`

## Pain Points Found

### 1. `relation/effect slot` bare access was still brittle

During the first build, `seal` and `route` inside zone methods failed as undefined symbols unless accessed as `self.seal` / `self.route`.

This example now uses the explicit path consistently.

### 2. Returning `subject` by value is still blocked

The first setup path tried to return `Courier` / `Dispatcher` directly from helper functions. That still hits the current subject-by-value limitation.

The probe now constructs subjects in zone builders instead.

### 3. Intent failure returned the wrong boolean

The biggest real bug this probe found was in intent lowering. On a failed step, the generated C/LLVM path returned the `failure:` predicate value instead of returning `false`.

That meant a failed intent could still surface as `ok=true` if the failure predicate evaluated to `true`.

This was fixed in both backends. The probe keeps the runtime output shape simple enough that this regression is easy to spot.

## Regression Coverage

- exact stdout: `examples/logistics_intent_probe/expected_stdout.txt`
- exact report: `examples/logistics_intent_probe/expected_results.txt`
- IR dump smoke: `tests/ir_pipeline_probe.sh`
- example smoke registration: `tests/example_contract_smoke.sh`

## Remaining Sharp Edge

`transfer:` is still fairly strict about `where` zone and target binding shape. The example is stable, but the surface may still need one more design pass if intent transfer should become less rigid in large programs.
