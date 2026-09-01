# 16. Language Contract Golden Spine

Last updated: 2026-06-22

Status: `beta-golden-spine`

Executable gate: `make language-contract-golden-test-smoke`.

This document ties the language-design cleanup decisions to golden or
golden-adjacent gates. It does not claim that every implementation unit is
closed. It says which gate must move when a contract moves, so source, proof,
backend, self-hosted tooling, and docs cannot drift silently.

## Contract

Pergyra keeps the broad semantic vocabulary (`world`, `zone`, `roster`,
`role`, `intent`, `slot`) because it models actor/world behavior. The required
surface must still stay small: unused axes must either have no runtime artifact
or be retained only with named evidence.

The beta rule is:

```text
source concept -> owner fact -> verifier/evidence -> backend consumption -> golden
```

If a stable concept lacks any link in that chain, it is not beta-closed.

## Golden Map

| Axis | Contract | Golden owner |
| --- | --- | --- |
| Proof/refinement | Coq is proof-guidance over small models, not whole-language proof. Every model must name its implementation refinement gap. | `make formal-semantics-test-smoke`; `tests/slot_calculus_adequacy_smoke.sh`; `tests/axis_keyword_adequacy_smoke.sh`; `tests/ir_minimality_adequacy_smoke.sh` |
| Backend roles | Pergyra semantic, ownership, ABI, and canonical MIR facts own language meaning. LLVM/native may remain the performance-primary path and C the reference/bootstrap/debug path; both are projections over the same facts. Self-hosted artifacts must be compared against their declared C/LLVM oracle class, and no backend may reconstruct or override semantics. | `docs/51_c_backend_reference_policy.md`; `docs/52_llvm_native_first_roadmap.md`; `make llvm-test-backend-compare`; `make backend-fail-closed-test-smoke` |
| Semantic fallback | semantic fallback is not a compatibility feature. Backend consumers must fail closed or read MIR/ABI/verified-projection facts; AIR remains verifier-only and DAG facts must be lowered before emission. | `make backend-fail-closed-test-smoke`; `make mir-declaration-inventory-test-smoke` |
| Authority/effect | authority evidence discharges an effect-derived obligation. It must not be inferred from unrelated `who` defaults. | `make intent-compression-contract-test-smoke`; AIR authority/effect evidence cases |
| Mutability spelling | `inout` is value-result mutation. `&mut` remains rejected so the surface does not imply Rust borrow semantics. | parser diagnostics in `parser_decl.c`, `parser_async.c`, `parser_type.c`; `tests/cases/backend_compare/inout_caller_mutation/` |
| Logical operators | logical operators produce Bool and require Bool operands. C-style Int logical values are not the stable language contract. | `tests/self_hosted/parity/semantic_parity.sh`; `src/self_hosted/semantic/expected/bad_logical_*.diag` |
| Value collection mutation | Caller-visible collection mutation must be explicit: `inout` or a named owned sink/handle. Plain value-parameter mutation is not a caller-effect contract, including collection builtins and direct array index assignment. | positive golden: `inout_caller_mutation`; negative goldens: `bad_value_param_arraypush`, `bad_value_param_array_index_assign`; compiler owner: `reject_default_param_collection_mutator_receiver` |
| Abstraction compression | proof-gated erasure is the rule: source-level axes may be retained, summarized, erased, or forbidden to erase only by evidence. | `tests/air_erasure/baseline.json`; `tests/air_erasure/gate.ps1`; `docs/semantics/14_air_erasure_measurement.md` |
| Raw/FFI/layout | raw/FFI/explicit layout stays boundary-scoped. General structs do not inherit packed/union/address semantics. | `make raw-escape-contract-test-smoke`; `make abi-ownership-shape-test-smoke`; `docs/136_abi_niche_and_explicit_layout.md` |
| IR verifiers | Each IR contract must be verified at its owner layer before backend use. | `make air-drift-test-smoke`; `make cfg-body-dataflow-test-smoke`; `make type-resolution-dag-test-smoke`; `make abi-ownership-shape-test-smoke` |
| Proof-carrying IR | Proof-carrying compiler work starts with a `pgy.proof-carrying-ir.v1` envelope over live owner facts, not a whole-compiler proof claim. | `make proof-carrying-pipeline-test-smoke`; `docs/semantics/17_proof_carrying_pipeline.md` |
| Verification methodology | Golden fixtures, differential oracles, verifier gates, ADT owners, and mechanized models are separate evidence forms. None may be substituted for another when claiming hard self-hosting, layout/niche soundness, or runtime materialization. | `make verification-methodology-test-smoke`; `docs/139_golden_adt_verification_methodology.md`; `docs/semantics/proofs/VerificationMethodology.v` |
| Architecture boundary | Declared purpose is attribution, authorization is not delegability, missing evidence never becomes guessed static success, local loss composes into a path budget, and logical resource authority meets physical machine contact only through an explicit projection binding. | `docs/semantics/proofs/DelegationBoundaryCore.v`; `docs/semantics/proofs/LossCompositionCore.v`; `docs/semantics/proofs/ResourceMachineBridge.v`; `docs/semantics/proofs/MachineLayerCore.v`; `docs/semantics/proofs/ArchitectureBoundaryCores.md` |
| Proof spine | The proof pack is connected through named proof nodes. Runtime safety, axis ownership, intent core, unified machine, architecture boundary, formal kernel, basis selection, certificate pipeline, and verification methodology can be cited as connected groups only through the spine; even a complete spine is not whole-language verification. Intent core includes the unit-correction model that treats source `intent` as a binder over verifier fact families, not one atomic formal primitive, plus the operational spine model for checked-intent guard freedom, fact-family reassembly, and cross-intent conflict separation. Axis ownership includes the authority irreducibility model: delegation history is not an alias for cap-by-zone facts. | `make proof-spine-test-smoke`; `docs/semantics/proofs/ProofSpine.v`; `docs/semantics/proofs/ProofSpine.md`; `docs/semantics/proofs/FormalKernel.v`; `docs/semantics/proofs/BasisCompleteness.v`; `docs/semantics/proofs/IntentObligations.v`; `docs/semantics/proofs/IntentSpine.v`; `docs/semantics/proofs/IntentConflict.v`; `docs/semantics/proofs/AuthorityIrreducibility.v`; `docs/semantics/proofs/DelegationBoundaryCore.v`; `docs/semantics/proofs/LossCompositionCore.v`; `docs/semantics/proofs/ResourceMachineBridge.v`; `docs/semantics/proofs/MachineLayerCore.v` |
| Machine-neutral compute | C and LLVM are first CPU-family validation projections, not the language ontology. Future dataflow, actor, tensor/NPU, capability, reconfigurable, and event-driven Projection Planners must consume the same owner facts, target-capability envelope, loss budget, and AIR evidence certificate; the Projection Plan Gate validates the candidate and backends consume the same `VerifiedProjectionPlan` contract or fail closed. | `docs/semantics/18_machine_neutral_compute.md`; `make language-contract-golden-test-smoke` |
| Theoretical foundations | Theory citations are lineage anchors, not implementation theorems. The proof target is a Pergyra abstract machine/core calculus that composes world/zone, effect/capability, authority, slot/lifecycle, channel, and intent facts. | `docs/semantics/19_theoretical_foundations.md`; `make language-contract-golden-test-smoke` |
| Self-hosting | self-hosted work starts with verifier/tool parity, not a second compiler claim. | `make self-host-preparation-test-smoke`; self-host diagnostic, semantic, MIR JSON, linter, fuzz-generator parity gates |

## Promotion Rule

A TODO on this page becomes beta-stable only when:

- the positive and negative fixtures exist;
- C and LLVM consume the same owner fact;
- self-hosted tooling either matches the C oracle or explicitly stays out of
  scope;
- documentation names the same contract vocabulary as the diagnostic/golden;
- the corresponding smoke refuses the old path.

## Value-Param Mutator Golden

The negative value-collection mutator goldens cover both source shapes:

```text
ArrayPush(xs, item);
xs[index] = item;
```

When `xs` is a default value parameter and the caller-visible mutation is not
spelled `inout` or routed through an owned sink/handle, the compiler rejects.
The positive `inout_caller_mutation` fixture pins the supported path, and
`src/self_hosted/semantic/fixture/bad_value_param_arraypush.pgy` and
`src/self_hosted/semantic/fixture/bad_value_param_array_index_assign.pgy` pin
the same self-hosted semantic code with distinct `ArrayPush` and `ArraySet`
facts.

The compiler-side owner records parameter mode in the symbol table and rejects
default-parameter collection mutation from the stdlib mutator path and from
direct array index assignment. This keeps the contract at the function boundary
instead of rediscovering intent from the call site.
