# Async And Parallel Execution

This facade separates user-facing syntax, static evidence, lane selection, and
runtime execution.

## Reading Order

1. [`../05_async_concurrency.md`](../05_async_concurrency.md) - user-facing model.
2. [`../113_memory_concurrency_model.md`](../113_memory_concurrency_model.md) - frozen memory contract.
3. [`../53_parallel_core_policy.md`](../53_parallel_core_policy.md) - parallel core policy.
4. [`../146_sea_execution_lanes.md`](../146_sea_execution_lanes.md) - execution lane classification.
5. [`../178_parallel_boundary_evidence.md`](../178_parallel_boundary_evidence.md) - boundary evidence.
6. [`../181_parallel_surface_full_design.md`](../181_parallel_surface_full_design.md) - full surface design.
7. [`../182_parallel_remaining_bones_work_orders.md`](../182_parallel_remaining_bones_work_orders.md) - remaining implementation work.
8. [`../186_parallel_full_implementation_plan.md`](../186_parallel_full_implementation_plan.md) - landed runtime/optimizer rungs and measured closure order.
9. [`../204_concurrency_direction_pscc_review.md`](../204_concurrency_direction_pscc_review.md) - direction decision: the PSCC proposal reviewed against canon. Records the corrections (async stays a suspension marker, scope owns lifetime; ConcurrencyPlan is a sealed MIR fact and AIR never lowers) and the adopted rungs (context propagation across spawn, structured spawn scope, dynamic disjointness evidence, checked suspension contract).

## Supporting Evidence

| Concern | Canonical Document |
|---|---|
| Fortran-derived vector and alias facts | [`../168_fortran_parallel_evidence.md`](../168_fortran_parallel_evidence.md) |
| Audit findings | [`../177_parallelism_audit_findings.md`](../177_parallelism_audit_findings.md) |
| Closure capture | [`../141_closure_capture_design.md`](../141_closure_capture_design.md) |
| Implementation board | [`../54_parallel_execution_relayout_board.md`](../54_parallel_execution_relayout_board.md) |
| Async positioning | [`../114_async_model_positioning.md`](../114_async_model_positioning.md) |
| Direction decision and proposal review | [`../204_concurrency_direction_pscc_review.md`](../204_concurrency_direction_pscc_review.md) |
| Spawn runtime authority carriage | [`../113_memory_concurrency_model.md`](../113_memory_concurrency_model.md) — Spawn Runtime Authority Contract; executable gate `runtime-spawn-context-propagation-test-smoke` |
| Structured async Rocq models | [`../semantics/proofs/AsyncModelCores.md`](../semantics/proofs/AsyncModelCores.md) — lifecycle containment and task-context carriage; adequacy gate `async-model-adequacy-test-smoke` |

Implementation readiness must be read from the work-order documents and their
gates. The presence of a design link is not an implementation claim.
