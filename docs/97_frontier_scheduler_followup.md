# Frontier Scheduler Follow-up

Date: 2026-04-24

## Closed In This Slice

- `GameWorld_sync` now emits a bounded outer frontier loop in the C transpiler.
- `GameWorld_sync` now emits the same bounded outer frontier loop shape in the LLVM backend.
- `BattleZone_sync` now emits a bounded outer frontier loop in both the C transpiler and the LLVM backend.
- LLVM world active-state change detection now updates `__world_derived_dirty` storage instead of keeping the dirty bit as a purely local staging value.
- `world_fixpoint_abi` is green again on both backends after the LLVM frontier-loop parity work.
- `world_embedded_projection_abi` is green on both backends after embedded zone source assignment started forcing same-turn zone recompute instead of leaving projection readiness stale.
- `world_embedded_method_projection_abi` is green on both backends after embedded zone subject method calls started forcing the same recompute path.
- `world_embedded_branch_projection_abi` is green on both backends after the same-turn recompute path stayed fresh across a simple branch-join.

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
- The strict beta blocker that remains in this area is broader frontier scheduling across handoff and the rest of the world-zone propagation family, not just the world-derived, zone-lifecycle, and embedded projection slices.
- In other words, the `world_fixpoint_abi` slice, the zone lifecycle frontier loop, and the embedded zone projection read-after-mutate slice are closed, but the entire zone/world propagation surface is not yet at 100%.
- `authority_failure_abi` is now green on both backends, so runtime authority rejection has a non-aborting queryable baseline in the same ABI smoke matrix.
- `authority_failure_surface` is now green in backend-compare too, so extern `Bool` runtime exports no longer drift as `1/0` on C and `true/false` on LLVM.
- `world_embedded_branch_projection_visibility` is now green in backend-compare too, so embedded projection freshness across a simple branch-join is locked by direct stdout parity and not only the ABI pipeline.
- On the runtime-failure side, the missing baseline queryable rejection surface is now closed; the remaining work there is richer policy depth.

## Verification

- `make test-transpile`
  - result: `670 passed, 0 failed`
- `make test-all`
- `make llvm-test-backend-compare`
  - result: `37/37 passed, 0 failed`

## Practical Status

- `projection_chain_abi`: closed on C and LLVM
- `world_fixpoint_abi`: closed on C and LLVM
- `zone_frontier_abi`: closed on C and LLVM
- `world_embedded_projection_abi`: closed on C and LLVM
- `world_embedded_method_projection_abi`: closed on C and LLVM
- `world_embedded_branch_projection_abi`: closed on C and LLVM
- remaining propagation debt: generalize the bounded frontier scheduler so handoff and the rest of the world-zone propagation paths are driven by the same source of truth
