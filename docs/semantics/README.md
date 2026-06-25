# Pergyra Proof Pack

Last updated: 2026-06-22

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
- [09_abstraction_loss_contracts.md](09_abstraction_loss_contracts.md): loss/compression-contract rules for compiler and tooling abstraction boundaries: what may be lost, what must be preserved, who owns the original truth, which downstream reads are forbidden, which evidence proves the loss budget, and when a source-level domain axis may be retained, summarized, erased, or forbidden to erase.
- [pass_contract_manifest.md](pass_contract_manifest.md): pass-level fact
  contract manifest for CFG/MIR, AIR, DAG/type-resolution, MIR/LLVM declaration
  parity, and ABI/Slot/Pin layout closure.
- [10_behavior_contract_closure_gaps.md](10_behavior_contract_closure_gaps.md): anti-overclaim closure register for the remaining gap between compiler-enforced behavior evidence and a closed behavior-contract calculus.
- [13_slot_abi_single_owner.md](13_slot_abi_single_owner.md): Slot ABI single-owner rule. `PgySlot_*` names always carry the checked `{ value, occupied }` layout; value-only storage must use a distinct explicit ABI owner instead of remapping the canonical Slot ABI.
- [16_language_contract_golden_spine.md](16_language_contract_golden_spine.md): golden-spine map for the language-design cleanup contracts: proof/refinement, semantic fallback, authority/effect, `inout`, logical Bool, value-collection mutation, proof-gated erasure, raw/FFI/layout, IR verifiers, machine-neutral compute, and self-hosted verifier/tool parity.
- [17_proof_carrying_pipeline.md](17_proof_carrying_pipeline.md): proof-carrying IR pipeline contract. Stage 1 wraps live AIR/MIR payloads in a `pgy.proof-carrying-ir.v1` certificate envelope with digest checks, required evidence/fact lists, and a negative deletion check; Stage 2 is the mechanized checker-core proof boundary.
- [18_machine_neutral_compute.md](18_machine_neutral_compute.md): machine-neutral compute contract. C and LLVM are the first validation projections, while AIR/MIR/ABI owner facts preserve `intent`, `effect`, `authority`, `coordination`, `slot`, `world`, and `zone` for future dataflow, actor, tensor, capability, reconfigurable, and event-driven substrates. (Includes the 2026-06-22 capability-machine falsification: AIR now owns the measured effect/capability/slot/authority-contract projection fields, and `make machine-neutral-status` remains the executable regression marker.)
- [19_theoretical_foundations.md](19_theoretical_foundations.md): theory-lineage bibliography + synthesis boundary. Maps each Pergyra axis to established theory while explicitly stating that a citation is a lineage anchor, not a whole-language proof. The open work is the Pergyra abstract machine/core calculus.

Mechanized artifacts:

- [proofs/SlotCalculus.v](proofs/SlotCalculus.v): Coq proof sketch for the
  `stale_handle_*_impossible`, `released_slot_*_impossible`,
  `handle_*_requires_issued_token`, `unissued_token_*_impossible`,
  `pinned_handle_release_impossible`, and `pin_non_eviction` invariants. CI
  type-checks this artifact under `formal-semantics-test-smoke`, so it is
  mechanized evidence for those modeled invariants only; it does not prove the
  whole language.
- [proofs/AxisOwnership.v](proofs/AxisOwnership.v): Coq proof sketch for axis
  fact-ownership, no-silent-override, independent axis commutation,
  idempotent same-axis update, and projection non-writing invariants. The
  companion adequacy smoke binds the model to named compiler/source symbols,
  not to a full extracted verifier.
- [proofs/IntentStepSoundness.v](proofs/IntentStepSoundness.v): Coq proof
  sketch for the linear sequence of authority-guarded intent actions. Proves
  the progress (`intent_step_progress`) and preservation
  (`intent_step_preservation` / `intent_step_was_authorized`) theorems for the
  intent-step execution fragment, demonstrating a well-authorized program does
  not get stuck (`intent_no_stuck`).
- [proofs/IRMinimality.v](proofs/IRMinimality.v): Coq proof sketch for the
  HIR/RIR/MIR codegen-layer lower bound under the live reads-from dependency
  model, plus the AIR witness minimality claim for
  intent/effect/authority/coordination verification. The latter proves that
  HKT/Functor evidence is not adequate for this axis because it does not witness
  authority, effect, boundary, coordination, or provenance facts.
  `ir_minimality_adequacy_smoke.sh` binds that model to the current driver, RIR
  flow, MIR lowering, AIR, backend dependency shape, and HKT/Functor soft-no
  documentation.
- [proofs/WitnessDataRace.v](proofs/WitnessDataRace.v): Coq proof sketch for
  the data-race-freedom invariant under the aliasing-xor-mutability (Witness)
  model. Proves that the Witness invariant rules out write-write and read-write
  data races by construction (`xor_mut_no_data_race`), that permitted boundary
  transitions preserve the invariant (`xor_mut_preserved`), and that the
  pin-exclusivity discipline entails data-race-free safety.
- [proofs/CheckedArith.v](proofs/CheckedArith.v): Coq proof sketch for
  fail-closed checked signed integer division and modulo (UB model). Proves
  that the checked helpers return `None` (panic) on exactly the two C undefined
  behavior inputs (`div_none_iff` for divide-by-zero/overflow), while returning
  correct and representable results for all other inputs.
- [proofs/ZoneCrossingCore.v](proofs/ZoneCrossingCore.v): Coq proof sketch for the
  FIRST fragment of the Pergyra abstract machine / core calculus (docs/semantics/19):
  the capability-gated boundary-transfer step (zone crossing, ambient-calculus
  lineage). Mechanizes capability soundness (`crossing_capability_sound`),
  progress/fail-closed (`fail_closed_crossing`), and no-ambient-authority
  (`no_ambient_authority`, `reaches_authority_stable`) for the world/zone facet
  only. The other Step forms (effect, slot lifecycle, authority delegation) and the
  binding onto live AIR/MIR owner facts are the open synthesis.
- [proofs/EffectAuthorityCore.v](proofs/EffectAuthorityCore.v): Coq proof sketch for
  the SECOND core-calculus corner -- the capability-gated effect-emit step composed
  with the zone-crossing step over one shared state (`held` authority + `here` zone +
  `elog` effect log). Mechanizes effect isolation (`step_effect_authorized`),
  crossing soundness, progress/fail-closed for emission (`fail_closed_emit`), and
  no-ambient-authority under either step. Shows two capability disciplines compose on
  one authority evidence; slot/typestate and authority-delegation steps remain open.
- [proofs/SlotLifecycleCore.v](proofs/SlotLifecycleCore.v): Coq proof sketch for the
  THIRD core-calculus corner -- the resource-operation step (slot lifecycle,
  affine/typestate lineage). Typestate-gated acquire/use/release with precondition
  soundness and the affine-safety theorem `no_op_after_release` (use-after-release
  and double-release are not derivable). Complements the runtime-invariant
  `SlotCalculus.v` by modeling the slot as a composing Step form.
- [proofs/AuthorityDelegationCore.v](proofs/AuthorityDelegationCore.v): Coq proof
  sketch for the FOURTH core-calculus corner -- the authority-check step
  (delegation, authorization-logic/ocap lineage). `delegation_requires_holding`
  (grant only what you hold) and `no_privilege_escalation` (delegation creates no
  new capability). With the prior three corners, all four base axes of the
  docs/19 abstract machine now have a mechanized soundness/fail-closed theorem;
  compensation + AIR binding are the open synthesis.
- [proofs/UnifiedCore.v](proofs/UnifiedCore.v): Coq proof sketch unifying the four
  corners into ONE abstract machine (single `config` + a `step` relation with all
  six Step forms: Cross/Emit/Acquire/Use/Release/Delegate). Proves the cross-cutting
  capstone `authority_conservation` -- no Step form anywhere creates a capability
  (delegation redistributes; the others do not touch holdings), the whole-machine
  no-ambient-authority theorem. Shows the four disciplines coexist on one state
  without interference. Compensation and a full preservation/progress over a typing
  judgment are the remaining synthesis.
- [proofs/CompensationCore.v](proofs/CompensationCore.v): Coq proof sketch for the
  compensation / rollback Step form (the intent-specific facet, Saga lineage). The
  effect->slot coupling `comp_target` makes rollback sound: `rollback_requires_log`
  (fail-closed), `rollback_restores` (undo restores the coupled slot to Empty),
  `rollback_pops_log`, and the saga round-trip `do_then_rollback_restores`. This is
  the point where the effect facet and the slot/lifecycle facet are shown to agree.
- [proofs/CoordinationCore.v](proofs/CoordinationCore.v): Coq proof sketch for the
  coordination Step form (the step dependency graph; dataflow / Kahn Process Network
  lineage). `run_requires_deps` (fail-closed: a step runs only when every dependency
  is done) and `reachable_dep_closed` (any reachable schedule is dependency-closed --
  a completed step always has all its dependencies completed). Replaces the
  position-ordered "sequence" view of intent steps with an explicit readiness model.
  With the prior six files this mechanizes the full `intent` decomposition; the
  remaining work is preservation/progress over a typing judgment and binding the
  model's graphs to live AIR/MIR owner facts.
- [proofs/ProofCarryingIR.v](proofs/ProofCarryingIR.v): Coq proof sketch for
  the Stage 2 checker-core rule behind `pgy.proof-carrying-ir.v1`: a valid
  certificate permits downstream fact consumption, while missing AIR/MIR facts
  or compatibility-success backend policy force fail-closed. The adequacy smoke
  binds this model to the live Stage 1 certificate envelope gate.
- [proofs/VerificationMethodology.v](proofs/VerificationMethodology.v): Coq
  proof sketch for the evidence-ladder discipline behind
  `docs/139_golden_adt_verification_methodology.md`: golden fixtures,
  differential oracles, verifier gates, ADT owners, and mechanized models are
  separate evidence forms and cannot be substituted for each other. The smoke
  gate binds this model to the methodology document and the proof-pack index.
- [proofs/ProofSpine.v](proofs/ProofSpine.v): top-level Coq proof spine that
  names every mechanized artifact as a proof-pack node and connects the runtime
  safety, axis ownership, intent core, unified machine, certificate pipeline,
  and verification-methodology groups. Its negative theorem states that a
  complete spine is still not whole-language verification.

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
  accepted loss, preserved facts, forbidden downstream reads, compression
  evidence, and proof-gated erasure budget.
- Machine-neutral compute: stable source-level axes must be owned by AIR/MIR/ABI
  facts rather than C/LLVM physical artifacts, so future backend projections can
  consume the same evidence or fail closed.
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
