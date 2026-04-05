# Campaign Graph FSM

## Goal

`campaign_graph_fsm` is the first larger Pergyra scenario intended to feel like
a small game/system slice rather than a syntax sample.

It answers a specific question:

Can Pergyra still hold together when one program mixes:

- multi-file world construction
- graph costs
- subject-local FSM behavior
- vessel state
- role/ability declarations
- slot-based staging
- zone/world orchestration
- projection/report output

## Files

The scenario lives under `examples/campaign_graph_fsm/`.

Key files:

- `abilities.pgy`
- `roles.pgy`
- `vessels.pgy`
- `units.pgy`
- `views.pgy`
- `sectors.pgy`
- `world.pgy`
- `setup.pgy`
- `main.pgy`

## Features Exercised

- `subject`, `vessel`, `object`, `tobject`
- `ability` and `role`
- `zone` and `world`
- hosted `func` and `action`
- nested nominal dispatch
- graph-cost computation through world-owned state
- `Slot<T>` in real turn-flow code
- report generation through file I/O
- exact stdout/result golden coverage

## Design Notes

This scenario intentionally avoids giant inline constructor lists.

Instead it uses:

- `AdventurerSpec`
- small factory helpers such as `MakeIrisSpec()`
- `BuildSector(...)`

That keeps the example readable and better reflects how real code is likely to
be written.

## Problems Found While Building It

### 1. Nested hosted calls on nested nominal receivers

Paths such as:

- `watch.captain.Summary()`
- `watch.ProjectionLine()`

exposed incomplete nested nominal host lowering in both backends.

This led to fixes in:

- C transpiler nested host-call lowering
- LLVM nested nominal call lowering

### 2. Domain-returning helper signatures on LLVM

Free functions returning domain types such as `SectorZone` / `CampaignWorld`
were being forward-declared before domain type emission, causing bad LLVM
signatures and verifier failures.

This forced a compile-order fix in LLVM domain emission.

### 3. `ToString(Int)` buffer reuse on C

The biggest runtime parity bug came from `pgy_int_to_string`.

It reused a single static buffer, which meant large nested string-concat
reports collapsed values on the C backend. The campaign example exposed this
very clearly in world strategic lines and final reports.

This is now fixed by returning a fresh string for each integer conversion.

### 4. `SectorZone.Summary()` must lower cleanly

The right answer was not to avoid hosted summary calls. The summary/report path
was kept and the backend was fixed until the scenario worked on both C and
LLVM.

## Current Regression Status

This scenario is now treated as regression-grade.

It has:

- exact stdout golden: `expected_stdout.txt`
- exact report golden: `expected_results.txt`
- example smoke coverage in `tests/example_contract_smoke.sh`

Backends:

- C: exact
- LLVM: exact

## Remaining Sharp Edges

The scenario is stable now, but it still points toward future work:

- richer role/ability runtime usage beyond declaration-level presence
- larger framework-level projection/binding surfaces
- UI IR integration for visualizing world/zone state directly
