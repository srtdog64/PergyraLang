# Security And Unsafe Boundaries

This facade complements [`README.md`](README.md) with the current top-level
contracts for authority, raw escape, runtime checks, and sandboxing.

## Reading Order

1. [`../03_security_mode_design.md`](../03_security_mode_design.md) - security mode design.
2. [`../15_compiler_security_modifications.md`](../15_compiler_security_modifications.md) - compiler enforcement changes.
3. [`../16_security_implementation_report.md`](../16_security_implementation_report.md) - implementation evidence.
4. [`../132_unsafe_capability_scope.md`](../132_unsafe_capability_scope.md) - unsafe capability boundary.
5. [`../128_pointer_risk_register.md`](../128_pointer_risk_register.md) - pointer risk inventory.

## Related Contracts

| Concern | Canonical Document |
|---|---|
| Secure slots | [`../secure_slot_v2.md`](../secure_slot_v2.md) |
| Runtime panic | [`../105_runtime_panic_contract.md`](../105_runtime_panic_contract.md) |
| Backend/Wasm lifetime claims | [`../135_backend_wasm_pointer_closure.md`](../135_backend_wasm_pointer_closure.md) |
| Bit reinterpretation boundaries | [`../145_bit_layout_boundary_matrix.md`](../145_bit_layout_boundary_matrix.md) |

Security claims require the linked implementation gates. This page is not a
security-status ledger.
