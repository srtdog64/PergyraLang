# Frontier Scheduler Follow-up

Date: 2026-04-24

## Closed In This Slice

- `GameWorld_sync` now emits a bounded outer frontier loop in the C transpiler.
- `GameWorld_sync` now emits the same bounded outer frontier loop shape in the LLVM backend.
- `BattleZone_sync` now emits a bounded outer frontier loop in both the C transpiler and the LLVM backend.
- LLVM world active-state change detection now updates `__world_derived_dirty` storage instead of keeping the dirty bit as a purely local staging value.
- `world_fixpoint_abi` is green again on both backends after the LLVM frontier-loop parity work.

## What This Actually Closed

- The earlier closure work already covered:
  - hidden provenance cells: `dirty/ready + epoch/cause`
- bounded world-derived recompute
- bounded zone lifecycle frontier recompute
- bounded projection-chain recompute
- This follow-up closed the next gap above that layer:
  - world-derived recompute is no longer driven by a single one-shot sync shape on LLVM
  - C and LLVM now both run a bounded outer frontier loop before finalizing world-derived state

## What Is Still Open

- This is not yet the full zone/world transitive scheduler.
- The strict beta blocker that remains in this area is broader frontier scheduling across branch/join/handoff/embedded zone-world paths, not just the world-derived and zone-lifecycle slices.
- In other words, the `world_fixpoint_abi` slice and the zone lifecycle frontier loop are closed, but the entire zone/world propagation surface is not yet at 100%.
- `authority_failure_abi` is now green on both backends, so runtime authority rejection has a non-aborting queryable baseline in the same ABI smoke matrix.
- `authority_failure_surface` is now green in backend-compare too, so extern `Bool` runtime exports no longer drift as `1/0` on C and `true/false` on LLVM.
- On the runtime-failure side, the missing baseline queryable rejection surface is now closed; the remaining work there is richer policy depth.

## Verification

- `make test-transpile`
  - result: `670 passed, 0 failed`
- `make test-all`
- `make llvm-test-backend-compare`
  - result: `36/36 passed, 0 failed`

## Practical Status

- `projection_chain_abi`: closed on C and LLVM
- `world_fixpoint_abi`: closed on C and LLVM
- `zone_frontier_abi`: closed on C and LLVM
- remaining propagation debt: generalize the bounded frontier scheduler so branch/join/handoff/embedded zone-world paths are driven by the same source of truth
