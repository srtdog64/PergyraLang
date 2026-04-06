# Testdoc

`testdoc` is the working style for larger Pergyra examples.

Each scenario should be treated as a single bundle:

- multi-file example under `examples/<scenario>/`
- exact stdout golden: `expected_stdout.txt`
- exact report/result golden: `expected_results.txt`
- one scenario note under `docs/testdoc/<scenario>.md`

The goal is not only to show syntax. A `testdoc` scenario should:

- exercise multiple language layers together
- expose real pain points in semantic/codegen/runtime behavior
- record what broke
- record what was fixed
- leave behind exact regression coverage

## Recommended Structure

For a regression-grade scenario:

1. Add or extend `examples/<scenario>/main.pgy`
2. Split the scenario across multiple files when the structure matters
3. Run both backends
4. Save exact stdout and exact `results.txt`
5. Write `docs/testdoc/<scenario>.md`
6. Register the scenario in `tests/example_contract_smoke.sh`

## What A Good Testdoc Should Record

- scenario goal
- language features exercised
- failure points discovered while building it
- compiler/runtime fixes made because of it
- remaining sharp edges

## Current Testdoc-Grade Scenarios

- `campaign_graph_fsm`
- `dnd_tavern_campaign`
- `shopping_mall_checkout_refund`
- `logistics_intent_probe`
- `resource_scheduler_async_probe`
- `spray_device_probe`
