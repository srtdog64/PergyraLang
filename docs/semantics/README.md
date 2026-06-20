# Pergyra Proof Pack

Last updated: 2026-04-27

Status: `beta-proof-obligation`

This folder is the source of truth for Pergyra's mathematical proof obligations. The proof pack is organized by core language keyword and closure axis so each stable beta surface has a local theorem statement, assumptions, evidence, and remaining gap.

This is a proof-obligation pack, not a claim of completed mechanized proof. Regression tests, smoke tests, and backend compare runs are proof evidence, not proof itself.

## Folder Contract

Every stable beta feature must be represented in this folder before it can be called beta-complete.

Required shape for each proof document:

- Stable surface: the exact syntax/semantic subset being proven.
- Out-of-scope surface: syntax accepted experimentally, explicit rejects, or post-beta axes.
- Judgments: the typing, runtime, resource, or backend judgments used by the feature.
- Theorems: named preservation/progress/soundness/parity claims.
- Evidence: current tests, smoke gates, docs, and implementation paths.
- Remaining obligations: blocker items that still prevent the theorem from being considered closed.

## Documents

- [00_proof_contract.md](00_proof_contract.md): global proof vocabulary, semantic domains, judgment notation, and beta acceptance rule.
- [01_intent_world_zone.md](01_intent_world_zone.md): `intent`, `world`, `zone`, `subject`, `authority`, `handoff`, and observability proof obligations.
- [02_relation_effect_projection.md](02_relation_effect_projection.md): `relation`, `effect`, `projection`, `refresh`, `publish`, `bind`, and freshness/provenance proof obligations.
- [03_generics_modules_dag.md](03_generics_modules_dag.md): generic contracts, module visibility, and type-resolution DAG soundness.
- [04_ownership_abi.md](04_ownership_abi.md): anchored own/ref, slot handles, lifetime lanes, and ABI ownership proof obligations.
- [05_parallel_execution.md](05_parallel_execution.md): `parallel`, execution conflict policy, cancellation/failure baseline, and fairness boundary.
- [06_backend_parity.md](06_backend_parity.md): MIR, C, LLVM, declaration inventory, and observable backend parity.
- [07_air_abstraction_safety.md](07_air_abstraction_safety.md): AIR verification-only synthesis IR, intent/boundary coverage, and abstraction drift proof obligations.
- [08_slot_capability_calculus.md](08_slot_capability_calculus.md): Slot capability calculus, token invariants, generation checks, and Pin/Lease proof obligations. This document also records the negative claim that Slot is not a borrow checker by itself; borrow-checker-equivalent safety requires the ownership classifier plus CFG/body-dataflow bridge facts.
- [09_abstraction_loss_contracts.md](09_abstraction_loss_contracts.md): loss-contract rules for compiler and tooling abstraction boundaries: what may be lost, what must be preserved, who owns the original truth, which downstream reads are forbidden, and which evidence proves the loss budget.
- [10_behavior_contract_closure_gaps.md](10_behavior_contract_closure_gaps.md): anti-overclaim closure register for the remaining gap between compiler-enforced behavior evidence and a closed behavior-contract calculus.

Mechanized artifacts:

- [proofs/SlotCalculus.v](proofs/SlotCalculus.v): Coq proof sketch for the
  `stale_handle_read_impossible`, `handle_read_requires_issued_token`,
  `unissued_token_read_impossible`, `handle_pin_requires_issued_token`,
  `unissued_token_pin_impossible`, and `pin_non_eviction` invariants. This is
  not beta-closure evidence unless a CI gate type-checks it with Coq, and it
  does not prove the whole language.
- [proofs/AxisOwnership.v](proofs/AxisOwnership.v): Coq proof sketch for axis
  fact-ownership, no-silent-override, independent axis commutation, and
  projection non-writing invariants. The companion adequacy smoke binds the
  model to named compiler/source symbols, not to a full extracted verifier.
- [proofs/IRMinimality.v](proofs/IRMinimality.v): Coq proof sketch for the
  HIR/RIR/MIR codegen-layer lower bound under the live reads-from dependency
  model. `ir_minimality_adequacy_smoke.sh` binds that model to the current
  driver, RIR flow, MIR lowering, AIR, and backend dependency shape.

## Beta Proof Boundary

Stable proof scope:

- Core declarations: `intent`, `world`, `zone`, `subject`, `relation`, `effect`, `projection`, `authority`, `handoff`.
- Foundation expressions: primitive values, `let`, `func`, lambda baseline, control flow, `Option`, `Result`.
- Stable collections: `List<T>`, `Set<T>`, `HashMap<String, T>`, `HashMap<Int, T>`.
- Generic contracts: exact type arguments, ability bounds, multi-bound `where T: A + B`, default type argument actual resolution.
- Ownership: anchored slot-handle boundary subset only.
- Slot capability calculus: generation checks, secure token invariants, and
  Pin/Lease non-eviction for the runtime ABI subset.
- Borrow-checker-equivalent safety: only through the combined ownership
  classifier, CFG/body dataflow, task/channel boundary, token-transport reject,
  and Slot runtime layers. Slot alone is not advertised as a borrow checker.
- Runtime observability: `last`, `history`, `active`, `recent`.
- Execution: `parallel` conflict/failure baseline.
- Backends: MIR-equivalent C and LLVM behavior for the frozen subset.
- AIR abstraction safety: verification-only synthesis IR for stable intent/boundary drift checks.
- Abstraction loss contracts: stable compiler and tooling boundaries must name
  accepted loss, preserved facts, forbidden downstream reads, and evidence.
- Behavior-contract closure: stable behavior claims must not be described as a
  closed calculus until their judgment rules, typed evidence facts, strict
  proof path, pass/loss manifest, backend oracle class, and mechanized-proof
  boundary are named.

Out of beta proof scope:

- Full quantum resource model.
- Arbitrary/general ownership lattice.
- Higher-kinded types and full FP functor/applicative/monad laws.
- Arbitrary `HashMap<K, V>` key universes.
- Full fairness proof for fiber/coroutine scheduling.
- GPU/Spray, Skia/render graph, package manager, and advanced debugger semantics.

## Acceptance Rule

A stable surface is proof-aligned only when all four are true:

- It has a stable syntax/semantic/runtime/backend contract.
- It has a theorem or invariant statement in this proof pack.
- It has regression evidence that exercises success and failure paths.
- Its docs and diagnostics use the same vocabulary.

If any item is missing, the feature is either `IN PROGRESS`, `explicit reject`, or `OUT OF BETA`.
