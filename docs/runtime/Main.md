# Runtime And Materialization

This facade routes work on runtime behavior and the boundary between erased
evidence and retained machine state.

## Read First

1. [`../105_runtime_panic_contract.md`](../105_runtime_panic_contract.md) - fail-closed runtime behavior.
2. [`../113_memory_concurrency_model.md`](../113_memory_concurrency_model.md) - memory and concurrency contract.
3. [`../128_pointer_risk_register.md`](../128_pointer_risk_register.md) - live pointer and lifetime risks.
4. [`../94_arena_index_lifetime_plan.md`](../94_arena_index_lifetime_plan.md) - arena/index lifetime plan.

## Boundaries

| Concern | Canonical Document |
|---|---|
| Slot and pin behavior | [`../118_slot_model_rigor_audit.md`](../118_slot_model_rigor_audit.md), [`../74_slot_pinning_caching.md`](../74_slot_pinning_caching.md) |
| Runtime-none and backend boundary | [`../135_backend_wasm_pointer_closure.md`](../135_backend_wasm_pointer_closure.md) |
| Guard retention and amortization | [`../142_evidence_driven_guard_amortization.md`](../142_evidence_driven_guard_amortization.md) |
| ABI/layout materialization | [`../136_abi_niche_and_explicit_layout.md`](../136_abi_niche_and_explicit_layout.md), [`../145_bit_layout_boundary_matrix.md`](../145_bit_layout_boundary_matrix.md) |
| Wasm target work | [`../wasm_target_todo.md`](../wasm_target_todo.md) |

Whether a construct erases or materializes is an IR/evidence decision recorded
by its canonical owner. This facade does not define that decision.
