# Pergyra Proof Pack

Last updated: 2026-10-08

Status: `beta-proof-obligation`

This folder is the source of truth for Pergyra's mathematical proof obligations. The proof pack is organized by core language keyword and closure axis so each stable beta surface has a local theorem statement, assumptions, evidence, and remaining gap.

This is a proof-obligation pack, not a claim of completed mechanized proof. Regression tests, smoke tests, and backend compare runs are proof evidence, not proof itself.

The [2026-10-08 integrated red-team repair receipt](../audits/proof_model_redteam_remediation_2026-10-08.md)
distinguishes transition/runtime repairs from withdrawn or narrowed claims.
Its fresh kernel snapshot checks 63 owners and six permanent regression
consumers. It does not close physical allocation, compiler refinement, general
memory safety or GC performance superiority.

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
- [../192_protocol_abi_api_registry.md](../192_protocol_abi_api_registry.md):
  derived Protocol/ABI/API crosswalk. It joins protocol version, existing SoT
  owner, wire/layout, producer, final consumer, C/LLVM/self-host projection,
  missing-fact failure, compatibility policy, and gate without becoming a new
  fact authority.
- [boundary_migration_manifest.md](boundary_migration_manifest.md): executable
  ownership-movement ledger. Each row names the stable handle, old and new
  owners, complete consumer inventory, parity and negative evidence, and the
  retirement gate that prevents aliases or fallback authority from returning.
- [10_behavior_contract_closure_gaps.md](10_behavior_contract_closure_gaps.md): anti-overclaim closure register for the remaining gap between compiler-enforced behavior evidence and a closed behavior-contract calculus.
- [13_slot_abi_single_owner.md](13_slot_abi_single_owner.md): Slot ABI single-owner rule. `PgySlot_*` names always carry the checked `{ value, occupied }` layout; value-only storage must use a distinct explicit ABI owner instead of remapping the canonical Slot ABI.
- [16_language_contract_golden_spine.md](16_language_contract_golden_spine.md): golden-spine map for the language-design cleanup contracts: proof/refinement, semantic fallback, authority/effect, `inout`, logical Bool, value-collection mutation, proof-gated erasure, raw/FFI/layout, IR verifiers, machine-neutral compute, and self-hosted verifier/tool parity.
- [17_proof_carrying_pipeline.md](17_proof_carrying_pipeline.md): proof-carrying IR pipeline contract. Stage 1 wraps live AIR/MIR payloads in a `pgy.proof-carrying-ir.v1` certificate envelope with digest checks, required evidence/fact lists, and a negative deletion check; Stage 2 is the mechanized checker-core proof boundary.
- [18_machine_neutral_compute.md](18_machine_neutral_compute.md): machine-neutral compute contract. C and LLVM are the first CPU-family validation projections, while AIR/MIR/ABI owner facts preserve `intent`, `effect`, `authority`, `coordination`, `slot`, `world`, `zone`, layout/shape, loss-budget, materialization, and fallback facts for future dataflow, actor, tensor/NPU, capability, reconfigurable, and event-driven substrates. (Includes the 2026-06-22 capability-machine falsification: AIR now owns the measured effect/capability/slot/authority-contract projection fields, and `make machine-neutral-status` remains the executable regression marker.)
- [19_theoretical_foundations.md](19_theoretical_foundations.md): theory-lineage bibliography + synthesis boundary. Maps each Pergyra axis to established theory while explicitly stating that a citation is a lineage anchor, not a whole-language proof. The open work is the Pergyra abstract machine/core calculus.
- [21_basis_convergence_triangulation.md](21_basis_convergence_triangulation.md): M3 basis-selection argument. Records the five independent traditions (game, DDD, BDI/MAS, upper ontology, logic/PL) that converge on Pergyra's world/zone/intent/role vocabulary, while keeping the claim at thesis level.
- [22_axis_macro_expressibility.md](22_axis_macro_expressibility.md): M1 axis macro-expressibility argument. Records which axes have a strong static rejection or erasure observation, and which axes remain weak.
- [23_compiler_stable_identity.md](23_compiler_stable_identity.md): partial,
  gate-backed `SyntaxNodeId` contract for parser results and final imported
  programs, including post-merge reassignment, duplicate rejection, and
  overflow fail-close behavior.
- [24_semantic_declaration_identity.md](24_semantic_declaration_identity.md):
  partial, gate-backed semantic placeholder ownership. `Symbol` completes a
  forward declaration only when its kind and `SyntaxNodeId` match; line/column
  and same-name coalescing are forbidden identity sources.
- [25_hir_routine_identity.md](25_hir_routine_identity.md): partial,
  gate-backed HIR callgraph identity. Semantic declaration targets lower to
  `RoutineId` edges; routine names remain observability only and ambiguous
  name queries fail closed.
- [26_verified_projection_plan_intent_observability.md](26_verified_projection_plan_intent_observability.md):
  gate-backed first native projection-plan row. MIR inventory usage facts map
  intent observability to `OBS0/ERASE` or `OBS1/MATERIALIZE`; C and LLVM consume
  the same row and the same 51-row runtime-call ABI owner without AST/HIR or
  backend-local fallback tables.
- [27_ownership_clean.md](27_ownership_clean.md): formal model, implementation
  OPEN. Compiler-owned ownership cleanup: liveness decides move or copy, every
  dead value is released at its last use, and branches/loops need no drop
  flags. Calls lend borrowed arguments and move sink arguments; parameter
  modes are inferred, not written, and inout moves in and out. A focus moves
  one part of a value out and back, so member-path inout, updating a part
  from itself, and read-only part views copy nothing. Proven in
  `proofs/OwnershipCleanCore.v`; copy policy D1 is decided as (B). Section 5
  is the MIR ownership contract (C2) that the compiler pass, backends, and
  runtime implement.
- [28_memory_boundary_composition.md](28_memory_boundary_composition.md):
  adopted requirements joining one storage owner, owner-bound access and one
  retirement edge. Slot and ordinary cleanup retain their own authorities;
  graph links remain non-owning. The typed composition audit reuses the
  existing Core, graph design model and Slot guards, with an explicit
  root/footprint correspondence. Production issuance, atomicity, graph syntax
  and C/LLVM cleanup remain OPEN; no Slot per ordinary value or link. Design
  provenance credits the user's Qt-inspired graph proposal. Tradeoffs cover
  long-lived retention, nullable links, indexing/copy/analysis costs, cleanup
  bursts and the limits of unconditional release and GC comparisons.
- [../173_intent_axis_strengthening.md](../173_intent_axis_strengthening.md): intent-axis strengthening work order. Keeps source-level `intent` as the authoring binder, but splits AIR/MIR/Coq into purpose, participant, coordination, boundary, authority, effect, compensation, and trace fact families.
- [../207_compiler_owned_cleanup_algorithm.md](../207_compiler_owned_cleanup_algorithm.md):
  explanatory core-algorithm companion with move/copy/end/drop, branch/loop and
  inferred-sink diagrams, dated kernel evidence, and research applicability.
  It bounds the normal-exit whole-value model separately from production
  cleanup; document 27 and OwnershipCleanCore retain semantic authority.

Mechanized artifacts:

- [MemoryBoundaryCompositionAudit.v](../../tests/coq/MemoryBoundaryCompositionAudit.v):
  typed consumers of canonical ownership, graph and Slot proofs. Under the
  same-root/footprint admission, graph ODrop and ordinary TDrop agree on the
  resulting heap and preserve their invariants, while the same Slot admits
  release. Includes a concrete cyclic graph compatible with both invariants
  and missing/stale/released/token/pin/borrow refusal witnesses. Gate:
  `tests/memory_boundary_composition_smoke.sh`. No new heap interpreter,
  issuer proof, atomic runtime operation or production-support claim.
  Reuse (2026-10-08): access resolves a generation link, not an address
  (`address_link_admits_reused_node` is the counterexample); retirement
  splits the heap so other values stay live; rooted links survive store-id
  and root-slot reuse; one block has one canonical owner.
  Allocation/growth (2026-10-08): `gexec_framed` requires the admitted external
  footprint. New store, vacant insertion and growth preserve the canonical
  whole-heap split; overlap and missing-frame cases refuse without transition.
  Growth can reuse its own retired table and reach graph/canonical/Slot
  retirement with the other owner still live. Physical placement and the
  production frame/root issuer remain OPEN.

- [proofs/OwnershipCleanGCComparison.v](proofs/OwnershipCleanGCComparison.v):
  imports the canonical machine for ownership-based automatic memory
  management. Proves exact-retention bounds against live-covering collector
  heaps and conditional abstract cost savings against an ideal-root full-heap
  sweep on a canonically elaborated/executed workload. A correct GC can match
  exact retention; zero inspection weight and bulk reset refute universal
  strict speed claims. Typed audit: `tests/coq/OwnershipCleanGCComparisonAudit.v`.
  Gate: `tests/ownership_gc_comparison_smoke.sh`. No claim that correct GC is
  unsafe, all collectors are slower, or C/LLVM cleanup is implemented.

- [proofs/OwnershipGraphLinks.v](proofs/OwnershipGraphLinks.v): design check
  for graph stores (one owner, many non-owning links). Proves exact store
  footprints, release by slots, permanent staleness and unique links under
  generations that retire instead of wrapping, borrow exclusivity, and
  growth refusal under a borrow. Counterexamples cover dropping by edges,
  modular generations, growth under a borrow, and absolute internal edges in
  a snapshot. Unreachable nodes in a live store are retained. The allocator
  is adversarial and may reuse just-freed blocks; a stale link stays refused
  when its node's exact block is reused, while address identity and
  store-id reissue resurrect links (counterexamples). No async/FFI escape,
  finalizer, node move, or production claim.
- [proofs/OwnershipTeardown.v](proofs/OwnershipTeardown.v): design check
  for one owner per node (a Qt-style ownership tree) with non-owning links,
  scoped roots, a reverse link index and an owner-keyed children index.
  Proves that every live node belongs to a root still in scope, that no
  step leaves a dangling stored link, that node release and root drop
  always have a step and free exactly their unit, that both units equal
  their inductive children-index closures (not an implemented walker),
  and that no step ever acts
  through a released node's handle, even after slot reuse. Counterexamples
  drop one rule each: no link clearing, a stale index, attach without the
  ancestor check (a permanent ownership-cycle leak), link counts as
  lifetime (a reference-counting machine keeps an unreachable cycle
  forever), a relinking write during teardown, and a unit that misses a
  descendant. Owned-but-unlinked nodes stay until their owner releases them
  or their root is dropped. Handles are assumed opaque and roots lexically
  scoped. Retirement units now require `NoDup`; a unique exact unit still
  always exists. SetField/Attach use only the actual old/new index rows and
  refine the scan specifications under exact indexes; unrelated rows are
  returned without a copy. `tests/ownership_teardown_redteam_smoke.sh`
  kernel-checks the typed regression and freshly extracts bounded OCaml
  value/sharing/cost observations. Large old/new rows remain expensive;
  the list uniqueness observer is quadratic, not a production subtree walker.
  Root-directed Alloc/Attach/RootDrop now require a live current root epoch;
  RootDrop advances it and RootNew never resets it. Arbitrary-run proofs
  exclude the old root handle after redeclaration. `resolve_node` rejects a
  saved stale local link, into any member of a released or dropped unit, in
  every later state, and `check_root` implements the same root predicate;
  both are freshly extracted and tested through repeated reuse. Each node
  read/retirement projection snapshots its immutable source slot once, avoiding
  recursive repeated evaluation of earlier functional heaps.
  Raw forest caller authority/loans are handled by the importing model below;
  cross-arena root identity, concurrency, finite
  generation bounds, physical refinement and production cost remain OPEN.
  An immutable checked model read is not a stable physical loan. Scope/receipt:
  [teardown red-team audit](../audits/ownership_teardown_redteam_locality_2026-10-08.md).
  Successor: [root epochs and checked locals](../audits/ownership_teardown_root_epoch_2026-10-08.md).
- [proofs/OwnershipTeardownAuthority.v](proofs/OwnershipTeardownAuthority.v):
  executable sequential admission over the same forest. Root creation issues
  a holder-bound cleanup responsibility; transfer moves it; retirement consumes
  it and old node/root identities never regain admission in later runs.
  Whole unique units are checked before destruction and descendant lifetime
  loans/pins block retirement. Only the owner may issue a lease to a borrower;
  the borrower may return it, not free the owner. Whole-run invariants, right
  provenance, monotonic lease IDs and legitimate node/root cleanup are proved.
  The focused gate extracts these functions and checks positive/negative cases.
  Bounded full-bound unit verification is not the production indexed walker;
  trusted context/state, live recipient binding, physical access/mutation,
  concurrency, finalizers and native compiler synthesis remain OPEN.
- [proofs/OwnershipCleanExits.v](proofs/OwnershipCleanExits.v): imports the
  canonical machine; adds break, continue, return, throw and try with a
  target live set per exit, and proves that every outcome runs with the
  source trace and ends with exactly its target set bound. An error before a
  pack releases the parts built so far; an early return keeps only the
  result. No panic/abort, divergence or production compiler claim.
- [proofs/OwnershipCleanReadOnly.v](proofs/OwnershipCleanReadOnly.v): imports
  the canonical machine and composition supplement; checks a bounded local
  read-only alias region, substitutes its reads with the root, and proves
  trace preservation and inherited closed-program cleanup. A cost certificate
  measures the actual abstract allocation frontier; the branch witness has
  one fewer allocation, with both executions and empty heaps proved. Rejects
  writes, retention, consumption and calls; preserves original admission.
  Copied field reads remain copies. No general loan/place, projection-copy
  elision, early-exit, physical-runtime or production-compiler claim.
- [proofs/OwnershipCleanComposition.v](proofs/OwnershipCleanComposition.v):
  imports the canonical cleanup machine; proves sequential unit/associativity,
  guarded branch distribution, exact execution equivalence of Skip
  normalization, inherited INV/CORR and closed-program cleanup, and syntax
  size nonincrease without reducing resource sites. Refutes guard erasure,
  sequential double drop and idempotent emission. No new heap, full monad
  calculus, local-loan elision or production compiler refinement is claimed.
- [proofs/OwnershipCleanCore.v](proofs/OwnershipCleanCore.v): Rocq proof that
  the ownership-clean elaboration runs without a refused step, preserves the
  value-semantics trace, keeps the live heap equal to the live owners'
  disjoint footprints, and frees everything in a closed program, for every
  parameter-mode table. Inferred sink modes remove the GUI call copies. Refutes
  shallow alias copy (double free), early drop (use after free) and missing
  release (leak). Also proves unpack of a dead record (overlapping
  projections, no copy), one-value regions released at their end, fail-closed
  call summaries (missing or wrong-length is a refusal, never a borrow), and
  ascending mode inference from no summaries.
  Scope: [27_ownership_clean.md](27_ownership_clean.md).
- [proofs/SlotCalculus.v](proofs/SlotCalculus.v): Coq proof sketch for the
  `stale_handle_*_impossible`, `released_slot_*_impossible`,
  `handle_*_requires_issued_token`, `unissued_token_*_impossible`,
  `pinned_handle_release_impossible`, and `pin_non_eviction` invariants.
  Released slots are tombstones that keep their generation and are
  reclaimed at the next one; `stale_handle_never_admitted` is the ABA
  theorem, and `gen_one_reclaim_resurrects` refutes the earlier rule. CI
  type-checks this artifact under `formal-semantics-test-smoke`, so it is
  mechanized evidence for those modeled invariants only; it does not prove the
  whole language.
- [proofs/AxisOwnership.v](proofs/AxisOwnership.v): Coq proof sketch for axis
  fact-ownership, no-silent-override, independent axis commutation,
  idempotent same-axis update, and projection non-writing invariants. The
  same model now also covers the human-facing keyword-register composition
  rule from `docs/42`: programs activate bounded keyword subsets, subset
  unions preserve well-formedness, and same-fact keywords share one owner
  axis. The
  companion adequacy smoke binds the model to named compiler/source symbols,
  not to a full extracted verifier.
- [proofs/IntentStepSoundness.v](proofs/IntentStepSoundness.v): Coq proof
  sketch for the linear sequence of authority-guarded intent actions. Proves
  the progress (`intent_step_progress`) and preservation
  (`intent_step_preservation` / `intent_step_was_authorized`) theorems for the
  intent-step execution fragment, demonstrating a well-authorized program does
  not get stuck (`intent_no_stuck`).
- [proofs/IRMinimality.v](proofs/IRMinimality.v): Coq proof sketch for the
  three dependency levels under the fixed HIR -> RIR/DIR -> MIR reads graph.
  The AIR witness minimality claim is restricted to interface coverage, not
  architectural minimality or HKT/Functor expressiveness. Its restricted
  order-only witness omits required fields by definition.
  `ir_minimality_adequacy_smoke.sh` binds that model to the current driver, RIR
  flow, MIR lowering, AIR, backend dependency shape, and HKT/Functor soft-no
  documentation.
- [proofs/WitnessDataRace.v](proofs/WitnessDataRace.v): Coq proof sketch for
  the data-race-freedom invariant under the aliasing-xor-mutability (Witness)
  model. Proves that the Witness invariant rules out write-write and read-write
  data races by construction (`xor_mut_no_data_race`), that permitted boundary
  transitions preserve the invariant (`xor_mut_preserved`), and that the
  per-context release rule preserves other readers. The abstract xor invariant
  is an admission contract, not a proof of the concrete Pin implementation.
- [proofs/CheckedArith.v](proofs/CheckedArith.v): Coq proof sketch for
  fail-closed checked signed integer division and modulo (UB model). Proves
  that the checked helpers return `None` (panic) on exactly the two C undefined
  behavior inputs (`div_none_iff` for divide-by-zero/overflow), while returning
  correct and representable results for all other inputs.
- [proofs/PergyraMulCost.v](proofs/PergyraMulCost.v): Coq-checked scope
  boundary for the fixed-width `CheckedMul` contract. It proves exact result,
  representability, fail-closed behavior, and the absence of a variable
  bit-width parameter; it deliberately does not claim the tape transpose
  lower bound or a wall-clock speedup. See
  [proofs/PergyraMulCost.md](proofs/PergyraMulCost.md) for the benchmark and
  research references.
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
  soundness and the old-incarnation theorem `no_op_after_release`. Physical
  reclaim advances the generation; `retired_identity_preserved` extends the
  rejection over arbitrary runs. Storage reuse is not identity reuse.
  Complements `SlotCalculus.v`; allocator/runtime refinement remains separate.
- [proofs/MachineLayerCore.v](proofs/MachineLayerCore.v): Coq proof for the
  machine layer below the slot. `Grant`/`Region` own address,
  extent, mode, and declaration-rooted provenance; `TypeLayout` and
  `place_grounds_slot` are only the plain-data bridge up to `Slot`. The actual
  contact operation is the
  explicit `contact_step`, which requires declared hardware adequacy,
  grant-specific authority, a live lease, and a mode-compatible operation, then
  emits a target-identifying `ContactEvent`, and `contact_apply` owns the
  abstract memory/read observation transition. The core includes fail-closed
  no-capability, revoked-lease, and mode-mismatch theorems. 0 admits / 0 axioms.
  The abstract `DeviceSlot` compiler bridge now carries owner-directed contact
  facts through RIR, MIR, AIR, and fail-closed C/LLVM admission, including the
  manifest-owned contact-to-runtime-operation mapping; concrete board and
  device refinement remains open. See the design and implementation status in
  [proofs/MachineLayerCore.md](proofs/MachineLayerCore.md).
- [proofs/ResourceMachineBridge.v](proofs/ResourceMachineBridge.v): Coq proof
  for the explicit binding between logical resource authority and physical
  machine placement. It proves that grounded contact requires both owners and
  that resource identity does not determine address, nor address authority.
- [proofs/DelegationBoundaryCore.v](proofs/DelegationBoundaryCore.v): Coq proof
  for the narrow automation permit envelope. Capability possession,
  delegability, trusted authority evidence, complete mediation, and retained
  runtime guards remain separate obligations. Declared purpose is attribution,
  not proof of actual human purpose.
- [proofs/LossCompositionCore.v](proofs/LossCompositionCore.v): Coq proof for
  cumulative loss vectors and compiler-derived mechanism bounds. Local budgets
  do not imply a path budget; derivation requires observational equivalence and
  an observable-cost bound.
- [proofs/EvidenceLifecycleCore.v](proofs/EvidenceLifecycleCore.v): Rocq/Coq
  model of the evidence-lifecycle aesthetic owned by
  `09_abstraction_loss_contracts.md`. It separates rich-payload disposition from
  the compact carrier of established authority, proves construction evidence
  erases only after admission and its last consumer, rejects unjustified
  receipts, and makes semantic-interpretation count plus abstract
  representation size monotone non-increasing. The companion
  [scope note](proofs/EvidenceLifecycleCore.md) and adequacy gate forbid treating
  this bounded model as implementation conformance, SoT closure, or self-host
  substitution progress.
- [proofs/ArchitectureBoundaryCores.md](proofs/ArchitectureBoundaryCores.md):
  scope and ownership map connecting those three models to
  `MachineLayerCore.v` without claiming implementation adequacy.
- [proofs/AuthorityDelegationCore.v](proofs/AuthorityDelegationCore.v): Coq proof
  sketch for the FOURTH core-calculus corner -- the authority-check step
  (delegation, authorization-logic/ocap lineage). `delegation_requires_holding`
  (grant only what you hold) and `no_privilege_escalation` (delegation creates no
  new capability). With the prior three corners, all four base axes of the
  docs/19 abstract machine now have a mechanized soundness/fail-closed theorem;
  compensation + AIR binding are the open synthesis.
- [proofs/UnifiedCore.v](proofs/UnifiedCore.v): Coq proof sketch unifying the four
  corners and the compensation/rollback step into ONE abstract machine (single `config` + a `step` relation with all
  seven Step forms: Cross/Emit/Acquire/Use/Release/Delegate/Rollback). Proves the cross-cutting
  capstone `authority_conservation` -- no Step form anywhere creates a capability
  (delegation redistributes; the others do not touch holdings), the whole-machine
  no-ambient-authority theorem. Rollback now consumes snapshot-bearing effect
  log entries and multi-slot compensation targets (`comp_target : eff -> list slot`).
  Rollback leaves current lifecycle state intact: no released incarnation or
  consumed acquisition can be restored from a snapshot. Concrete value/effect
  compensation remains a separate refinement obligation.
  Proves the non-interference of delegation and rollback over actual `step` /
  `steps` edges (`delegate_then_rollback_sound`,
  `delegate_rollback_steps_sound`, `acquire_delegate_then_rollback_sound`, and
  `acquire_delegate_rollback_steps_sound`). Shows the capability, typestate,
  and rollback disciplines coexist on one state without interference. A full
  preservation/progress over a typing judgment remains the open synthesis.
- [proofs/CompensationCore.v](proofs/CompensationCore.v): Coq proof sketch for the
  compensation / rollback Step form (the intent-specific facet, Saga lineage). The
  effect->slots coupling `comp_target : eff -> list slot` and the logged
  pre-forward store snapshot define an ideal snapshot contract: `rollback_requires_log`
  (fail-closed), `rollback_restores_snapshot` (undo restores each coupled slot
  to the logged pre-forward state), `rollback_pops_log`, and the saga round-trip
  `do_then_rollback_restores`. These follow snapshot restoration, not execution
  of user-written compensate expressions. Do not use them as irreversible
  resource safety evidence; Core/Unified/WholeProgram preserve lifecycle instead.
- [proofs/CoordinationCore.v](proofs/CoordinationCore.v): Coq proof sketch for the
  coordination Step form (the step dependency graph; dataflow / Kahn Process Network
  lineage). `run_requires_deps` (fail-closed: a step runs only when every dependency
  is done) and `reachable_dep_closed` (any reachable schedule is dependency-closed --
  a completed step always has all its dependencies completed). Replaces the
  position-ordered "sequence" view of intent steps with an explicit readiness model.
  With the prior six files this mechanizes the full `intent` decomposition; the
  remaining work is preservation/progress over a typing judgment and binding the
  model's graphs, holdings, compensation targets, and snapshots to live AIR/MIR
  owner facts.
- [proofs/WholeProgramCore.v](proofs/WholeProgramCore.v): Coq proof sketch for the
  whole-program guard machine. It folds coordination into the shared config and
  proves `step_iff_guard`, `step_preserves_wf`, and `whole_program_safety` over
  the eight Step forms. WF is dependency closure only; separate run invariants
  preserve released lifecycle and prohibit repeated completed task identities.
- [proofs/AIRBinding.v](proofs/AIRBinding.v): Coq proof sketch for the
  calculus-to-AIR fact interface. It proves `guard_air_faithful` and
  `gate_locality` for the selected interface. Current config is also necessary;
  faithful equality is definitional, not producer adequacy or AIR minimality.
- [proofs/FormalKernel.v](proofs/FormalKernel.v): Coq proof sketch for
  source-vocabulary binding. It maps `world`, `zone`, `intent`, `effect`,
  `authority`, `slot`, participant terms, `projection`, `channel`, and
  `relation` to named kernel primitives and owner facts, and proves that this
  kernel meaning still does not permit a whole-language proof claim.
- [proofs/BasisCompleteness.v](proofs/BasisCompleteness.v): Coq proof sketch for
  the first basis-selection M2 fragment. It encodes a static bigraph place/link
  fragment into Pergyra axes (`zone`/`world` as place, `channel` as link),
  proves encode/decode conservativity, and proves `world_separation`: a
  channel-free path cannot cross world roots.
- [proofs/IntentObligations.v](proofs/IntentObligations.v): Coq proof sketch
  for the `intent` unit correction. It models source-level `intent` as a binder
  that elaborates into verifier fact families, keeps `purpose` and `trace`
  outside the non-library-expressibility claim, rejects an atomic `Intent` fact
  as a formal target, and records that WO-INT-0 fact-family naming precedes
  INT-1 participant declared-used checking. Finite emitted-family admission
  rejects missing families; ClaimClass labels remain a taxonomy, not a proof
  of library non-expressibility or compiler emission completeness.
- [proofs/IntentSpine.v](proofs/IntentSpine.v): Coq proof sketch for the
  operational intent fact kernel. It models participant, coordination, and
  compensation facts joined by one spine identity, proves
  `checked_intent_guard_free`, `no_dep_cycle`, fact-family reassembly, and the
  checked-intent erasure corollary. It still treats interprocedural participant
  used-set computation as an implementation/gate obligation. `no_dep_cycle`
  concerns intra-intent step dependencies, not runtime handle ancestry cycles.
- [proofs/IntentConflict.v](proofs/IntentConflict.v): Coq proof sketch for the
  cross-intent conflict kernel. It models the runtime admission guard, proves
  that statically separated co-active traces cannot fire that guard, and records
  that priority is a one-order tiebreak rather than separation evidence. The
  static co-activity computation remains implementation/gate work.
- [proofs/AuthorityIrreducibility.v](proofs/AuthorityIrreducibility.v): Coq
  proof sketch for unrestricted record separation. It gives two
  configurations with identical capability and zone projections but different
  delegation reachability. The ungranted pair is grant-inconsistent; on the
  explicit grant-consistent subset the cap projection already computes this
  verdict. It does not prove the language axis irreducible.
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
- [proofs/SoTAuthority.v](proofs/SoTAuthority.v): bounded-rung Coq model for
  single semantic authority. It proves required-owner existence and uniqueness,
  authority-only consumption, and rejection of missing facts, duplicate
  producers, and owner-plus-fallback bridges. The adequacy smoke binds the
  first concrete instances to semantic-owned array-literal body, try-let
  operand, collection-mutation statement, enum declaration, nominal/field,
  role declaration, expression/type runtime-usage, the first expression-shape
  consumer, canonical node-kind identity, entrypoint selection, function
  declaration identity, and three
  statement-routing facts and their
  codegen consumers; it is not a whole-compiler SoT proof.
- [proofs/AsyncLifecycleCore.v](proofs/AsyncLifecycleCore.v): bounded Rocq
  model of the named Future lifecycle. It proves suspend/Cancel
  non-retirement, single await/own-transfer consumption, trace-level
  structured containment, and fail-closed alternative CFG merge.
- [proofs/AsyncContextCore.v](proofs/AsyncContextCore.v): bounded Rocq model
  of task runtime-context carriage. It proves exact capability-mask, budget
  owner, and instance capture, lane/suspension preservation, and surrounding
  context restoration. [proofs/AsyncModelCores.md](proofs/AsyncModelCores.md)
  fixes the shared claim and implementation-adequacy boundary.
- [proofs/AsyncScopeCore.v](proofs/AsyncScopeCore.v): scope-tree containment.
  No orphan task under structured open/spawn/complete/cancel/close/detach,
  cancellation reaches every descendant scope, detach only through a
  capability; the pre-structured-lifecycle rule is refuted in three steps.
- [proofs/CapabilityFlowCore.v](proofs/CapabilityFlowCore.v): capability
  non-forgery across share/lend/move task creation, loan uniqueness and
  restoration on return; an executor reading the per-thread default instead
  of capturing the parent is refuted against a narrowed manifest.
- [proofs/SuspensionRevalidationCore.v](proofs/SuspensionRevalidationCore.v):
  slot temporal safety across a suspension. A stale generational reference
  never resolves after a despawn, a resolved one names the same incarnation;
  dereferencing by slot id alone is refuted against a respawned slot.
- [proofs/DeterministicSubsetCore.v](proofs/DeterministicSubsetCore.v):
  footprint-independent task bodies commute, so every schedule of an admitted
  family ends in the canonical index-order state; a write-conflicting pair is
  refuted. [proofs/AsyncDirectionCores.md](proofs/AsyncDirectionCores.md)
  fixes the four cores' claim boundary.
- [sot_owner_spine_registry.md](sot_owner_spine_registry.md): machine-gated
  28-row declaration of 15 architectural fact families plus thirteen bounded
  self-host closure facts, stable handles, unique owners,
  last legitimate consumers, forbidden fallbacks, enforcement gates, and
  honest `ACTIVE` / `BRIDGE` / `CLOSED` status.
- [proofs/ProofSpine.v](proofs/ProofSpine.v): top-level Coq proof spine that
  names every mechanized artifact as a proof-pack node and connects the runtime
  safety, axis ownership, intent core, unified machine, architecture boundary,
  formal-kernel,
  basis-selection,
  certificate pipeline, verification-methodology, SoT-authority, and bounded
  structured-async groups. Its negative
  theorem states that a complete spine is still not whole-language verification.

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
- Structured async: named Future containment and task-context carriage only;
  termination, scheduler fairness, detached capture, and a full memory model
  remain outside the mechanized claim.
- Backends: MIR-equivalent C and LLVM behavior for the frozen subset.
- AIR abstraction safety: verification-only synthesis IR for stable intent/boundary drift checks.
- Abstraction loss contracts: stable compiler and tooling boundaries must name
  accepted loss, preserved facts, forbidden downstream reads, compression
  evidence, and proof-gated erasure budget.
- Machine-neutral compute: stable source-level axes must be owned by AIR/MIR/ABI
  facts rather than C/LLVM physical artifacts, so CPU, self-hosted, tensor/NPU,
  dataflow, or capability-machine projections can consume the same evidence or
  fail closed with an explicit fallback/materialization reason.
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

## Stable proof toolchain and extracted cleanup cost (2026-10-08)

scripts/rocq_toolchain_owner.sh admits only Rocq/rocqchk 9.3.0 and Stdlib
9.2.0; scripts/run_rocq_toolchain.sh activates the explicit project switch.
System Coq is not a fallback. tests/coq_kernel_check.sh checks a fresh source
snapshot and the approved assumption bindings, never source-tree .vo files.

tests/ownership_cleanup_smoke.sh freshly extracts OwnershipCleanCore.elab and
callee elaboration. Its controls and hashed cost receipt are bounded model
evidence, not runtime drop glue, D1 policy, implementation refinement or beta
closure. Commands, exact hashes, limitations and recovery evidence are in
../audits/ownership_clean_g_receipt_2026-10-08.md.
