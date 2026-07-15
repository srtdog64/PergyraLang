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

## Supporting Evidence

| Concern | Canonical Document |
|---|---|
| Fortran-derived vector and alias facts | [`../168_fortran_parallel_evidence.md`](../168_fortran_parallel_evidence.md) |
| Audit findings | [`../177_parallelism_audit_findings.md`](../177_parallelism_audit_findings.md) |
| Closure capture | [`../141_closure_capture_design.md`](../141_closure_capture_design.md) |
| Implementation board | [`../54_parallel_execution_relayout_board.md`](../54_parallel_execution_relayout_board.md) |
| Async positioning | [`../114_async_model_positioning.md`](../114_async_model_positioning.md) |

Implementation readiness must be read from the work-order documents and their
gates. The presence of a design link is not an implementation claim.
