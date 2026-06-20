# Abstraction Loss Contract Manifest

A machine-readable index of the abstraction-loss boundaries defined in prose in
[`09_abstraction_loss_contracts.md`](09_abstraction_loss_contracts.md), bound to
the compiler stages they connect and to the gate that *enforces* the boundary's
forbidden-loss clauses (where one exists).

This closes one specific gap: `09_abstraction_loss_contracts.md` is
developer-facing prose with no machine-readable form a verifier can read. The
manifest below is verified by
[`../../tests/loss_contract_adequacy_smoke.sh`](../../tests/loss_contract_adequacy_smoke.sh):
every boundary's stage artifact must exist, and every boundary that *claims*
enforcement must name a gate script that exists. Move a stage or delete a gate
and the test fails.

## What this is NOT

It is **not** a proof that loss is bounded, and it does **not** make the prose
contracts executable. It is an *index*: it pins each boundary to a real stage and
to its enforcing gate, and it reports honestly which boundaries are gate-enforced
versus documentation-only. The `status` column is the measured truth, not a
claim that every loss rule is mechanically checked.

## Manifest

Columns: `boundary  stage-artifact  enforcement-gate  status`.
`status`: `enforced` (a gate checks the boundary's forbidden-loss clauses) |
`documented` (prose contract only, no dedicated gate yet).

<!-- BEGIN loss-contract-manifest -->
```
parser_to_ast       src/parser/ast.c             documented                                     documented
ast_to_mir          src/compiler/mir.c           documented                                     documented
mir_to_air          src/compiler/air.c           tests/air_drift_smoke.sh                       enforced
mir_to_backends     src/codegen/llvm_backend.c   tests/backend_compare_llvm_coverage_smoke.sh   enforced
selfhost_to_oracle  src/self_hosted              tests/self_hosted_component_contract_smoke.sh  enforced
```
<!-- END loss-contract-manifest -->

## Coverage (honest)

5 canonical boundaries, **3 gate-enforced** (MIR->AIR by the AIR drift gate;
MIR->C/LLVM by the backend-compare gate; self-hosted->C oracle by the component
contract gate) and **2 documentation-only** (`parser_to_ast` and `ast_to_mir`
are internal lowering steps with no dedicated loss gate yet). So the loss
contracts are **3/5 enforced**. The two early-lowering boundaries are the
concrete remaining work to make every loss contract machine-enforced rather than
prose.
