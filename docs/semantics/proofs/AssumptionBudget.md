# Approved assumption contracts

`AssumptionBudget.v` is the kernel-checked approval owner. The corpus admits
only these two opaque SlotCalculus API declarations:

| Qualified declaration | Approved type | Abstract contract |
| --- | --- | --- |
| `SlotCalculus.MaxSlotId` | `nat` | Reserved slot-id sentinel; no positivity or fixed-width bound is assumed. |
| `SlotCalculus.verify_token` | `nat -> nat -> nat -> SlotCalculus.AccessMode -> bool` | Token, slot id, generation, access mode, then a boolean decision. No cryptographic or implementation correctness law is assumed. |

The access-mode domain is Read, Write, Release, Pin and Claim. The approval
module imports the actual parameter declarations, pins the expanded scalar
types and exhaustively matches that domain. The kernel gate checks both the
qualified-name budget and this module. Its last consumer,
`tests/coq/AssumptionBudgetAudit.v`, reaches the exported bindings and checks
their definitional identity with the actual API. Removing or emptying the
approval module is a failure, not permission to fall back to a names-only check.
Type drift under an unchanged name is tested with benign abstract API fixtures
by `coq_kernel_check_selftest.sh`.

These are deliberate assumptions, not proved native runtime implementations.
Type equality does not establish intended token semantics, cryptography,
argument-role correctness or source-to-model refinement. Changing this approval
owner together with the API is a visible contract change requiring review.

## Admitted prover theory (G0, 2026-10-08)

The toolchain owner pins stable Rocq 9.3.0 and its checker, with independently
versioned Stdlib 9.2.0. The gate requires predicative Set, no rewrite rules,
and empty unsafe-fixpoint, type-in-type and assumed-positivity reports.
Rocq 9.3 also reports inductives relying on indices not mattering. The default
profile includes standard equality dependencies; the complete report remains
visible. These are theory-profile dependencies, not extra named axioms, and a
zero-name isolated-core budget is not a claim of independence from the kernel
theory. See the [official 9.3 changes](https://rocq-prover.org/doc/V9.3.0/refman/changes.html).

Fresh isolated sources and dependency-sorted compilation are kernel-checked
before extraction outputs are exported. Old/orphan .vo files are neither
consumed nor deleted. The compiler's source-to-model refinement remains a
separate obligation.
