# Pergyra Formal Semantics and Proof Obligations

Last updated: 2026-04-27

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
- [docs/semantics/07_air_abstraction_safety.md](semantics/07_air_abstraction_safety.md)
- [docs/semantics/08_slot_capability_calculus.md](semantics/08_slot_capability_calculus.md)
- [docs/semantics/09_abstraction_loss_contracts.md](semantics/09_abstraction_loss_contracts.md)
- [docs/semantics/10_behavior_contract_closure_gaps.md](semantics/10_behavior_contract_closure_gaps.md)
- [docs/semantics/proofs/SlotCalculus.v](semantics/proofs/SlotCalculus.v)
- [docs/semantics/proofs/MachineLayerCore.v](semantics/proofs/MachineLayerCore.v)
- [docs/semantics/proofs/DelegationBoundaryCore.v](semantics/proofs/DelegationBoundaryCore.v)
- [docs/semantics/proofs/LossCompositionCore.v](semantics/proofs/LossCompositionCore.v)
- [docs/semantics/proofs/ResourceMachineBridge.v](semantics/proofs/ResourceMachineBridge.v)
- [docs/semantics/proofs/PergyraMulCost.v](semantics/proofs/PergyraMulCost.v)
- [docs/semantics/proofs/PergyraMulCost.md](semantics/proofs/PergyraMulCost.md)
- [docs/semantics/proofs/ArchitectureBoundaryCores.md](semantics/proofs/ArchitectureBoundaryCores.md)
- [docs/semantics/proofs/AsyncLifecycleCore.v](semantics/proofs/AsyncLifecycleCore.v)
- [docs/semantics/proofs/AsyncContextCore.v](semantics/proofs/AsyncContextCore.v)
- [docs/semantics/proofs/AsyncModelCores.md](semantics/proofs/AsyncModelCores.md)
- [docs/semantics/proofs/AsyncScopeCore.v](semantics/proofs/AsyncScopeCore.v)
- [docs/semantics/proofs/CapabilityFlowCore.v](semantics/proofs/CapabilityFlowCore.v)
- [docs/semantics/proofs/SuspensionRevalidationCore.v](semantics/proofs/SuspensionRevalidationCore.v)
- [docs/semantics/proofs/DeterministicSubsetCore.v](semantics/proofs/DeterministicSubsetCore.v)
- [docs/semantics/proofs/AsyncDirectionCores.md](semantics/proofs/AsyncDirectionCores.md)
- [docs/semantics/proofs/ProofSpine.v](semantics/proofs/ProofSpine.v)

Related rigor audits:

- [docs/118_slot_model_rigor_audit.md](118_slot_model_rigor_audit.md)

This file remains as a stable English index for older references from the beta board and TODO.

`docs/45_math_layer_design.md` covers the math library layer. `docs/semantics/` covers the mathematical semantics of the language itself.

Regression tests, smoke tests, and backend compare runs are evidence. They are not mathematical proof by themselves.

Each Coq/Rocq file owns a bounded model. `SlotCalculus.v` covers handle/token
and pin invariants; the architecture-boundary cores cover delegation,
cumulative loss, and the logical-resource/physical-machine bridge.
`ModuleAuthority.v` covers the planned `use MODULE;` surface before any
compiler code exists: stratified links, unique export resolution, and
authority provenance, with size-quantified load theorems (docs/202). Do not
describe model theorems as implementation adequacy, completed beta proof, or a
whole-language proof. The proof spine makes that negative boundary explicit.
`AsyncLifecycleCore.v` and `AsyncContextCore.v` separately model the current
named-Future lifecycle and task-context carriage contracts; neither assigns
lifetime or authority ownership to the `async` marker itself.
The four direction cores (`AsyncScopeCore.v`, `CapabilityFlowCore.v`,
`SuspensionRevalidationCore.v`, `DeterministicSubsetCore.v`) model the
scope-tree, capability-flow, suspension-revalidation, and schedule-independence
disciplines docs/204 adopts, each with a machine-checked counterexample for the
unstructured alternative; `AsyncDirectionCores.md` fixes their claim boundary.

Run the proof-pack drift gate with:

```sh
make formal-semantics-test-smoke
```
