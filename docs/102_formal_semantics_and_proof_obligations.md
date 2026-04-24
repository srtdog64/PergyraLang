# Pergyra Formal Semantics and Proof Obligations

Last updated: 2026-04-25

Status: `index`

The mathematical proof source of truth has moved into the proof pack folder:

- [docs/semantics/README.md](semantics/README.md)
- [docs/semantics/00_proof_contract.md](semantics/00_proof_contract.md)
- [docs/semantics/01_intent_world_zone.md](semantics/01_intent_world_zone.md)
- [docs/semantics/02_relation_effect_projection.md](semantics/02_relation_effect_projection.md)
- [docs/semantics/03_generics_modules_dag.md](semantics/03_generics_modules_dag.md)
- [docs/semantics/04_ownership_abi.md](semantics/04_ownership_abi.md)
- [docs/semantics/05_parallel_execution.md](semantics/05_parallel_execution.md)
- [docs/semantics/06_backend_parity.md](semantics/06_backend_parity.md)

This file remains as a stable English index for older references from the beta board and TODO.

`docs/45_math_layer_design.md` covers the math library layer. `docs/semantics/` covers the mathematical semantics of the language itself.

Regression tests, smoke tests, and backend compare runs are evidence. They are not mathematical proof by themselves.

Run the proof-pack drift gate with:

```sh
make formal-semantics-test-smoke
```
