# Pergyra Formal Semantics and Proof Obligations

Last updated: 2026-10-08

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
- [docs/semantics/proofs/EvidenceLifecycleCore.v](semantics/proofs/EvidenceLifecycleCore.v)
- [docs/semantics/proofs/EvidenceLifecycleCore.md](semantics/proofs/EvidenceLifecycleCore.md)
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
- [docs/semantics/27_ownership_clean.md](semantics/27_ownership_clean.md)
- [docs/semantics/proofs/OwnershipCleanCore.v](semantics/proofs/OwnershipCleanCore.v)
- [docs/semantics/proofs/ProofSpine.v](semantics/proofs/ProofSpine.v)

Related rigor audits:

- [docs/118_slot_model_rigor_audit.md](118_slot_model_rigor_audit.md)
- [Proof-model red-team findings](audits/proof_model_redteam_2026-10-08.md)
- [Integrated repair receipt](audits/proof_model_redteam_remediation_2026-10-08.md):
  24 repaired bounded models/runtime paths, 9 corrected claims, 2 existing
  repairs reverified. The 63-owner fresh kernel run also includes six permanent
  regression consumers; physical allocator/refinement and installed CI remain
  separate obligations.

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
`EvidenceLifecycleCore.v` models the evidence-compression rule from
`09_abstraction_loss_contracts.md`: construction payload may disappear after
discharge and its last semantic consumer while the established authority
continues through a compact carrier. It is a bounded model, not implementation
adequacy or SoT/self-host progress.
The four direction cores (`AsyncScopeCore.v`, `CapabilityFlowCore.v`,
`SuspensionRevalidationCore.v`, `DeterministicSubsetCore.v`) model the
scope-tree, capability-flow, suspension-revalidation, and schedule-independence
disciplines docs/204 adopts, each with a machine-checked counterexample for the
unstructured alternative; `AsyncDirectionCores.md` fixes their claim boundary.

Run the proof-pack drift gate with:

```sh
make formal-semantics-test-smoke
```

2026-10-08 toolchain update: the project owner admits stable Rocq/rocqchk
9.3.0 with independently versioned Stdlib 9.2.0. Fresh corpus and extraction
commands activate the explicit switch through scripts/run_rocq_toolchain.sh;
neither system Coq nor cached proof objects are current proof evidence.
OwnershipCleanCore extraction/cost tests remain model-only. See
audits/ownership_clean_g_receipt_2026-10-08.md for observed hashes, commands,
negative controls and the still-open compiler/runtime boundary.

2026-10-08 sink core: OwnershipCleanCore now proves calls under every
parameter-mode table (borrow or sink) and inferred modes remove the GUI call
copies (2 to 0). The C2 MIR ownership contract that implementations consume
is section 5 of semantics/27_ownership_clean.md.

2026-10-08 places core: OwnershipCleanCore now gives every value one block per
node and proves a focus statement that moves one part of an owned value out
and back with exactly its blocks (member-path inout, update from self,
read-only part view; GUI state cases copy nothing). The read-only rewrite
refuses only writes to the alias or root, and drops of distinct variables
commute. The fresh corpus check covers 60 proofs.

2026-10-08 composition supplement: `OwnershipCleanComposition.v` imports that
same core and proves guarded branch/sequential composition and recursive Skip
normalization. Exact `texec` equivalence preserves final state, allocation
counter and trace; refusal and source/ownership soundness are inherited.
Syntax-size reduction is not a reduction in memory or observable effects.
The extraction controls consume both files; docs/207 section 9 bounds the
unit-valued bind analogy and the boundary of a general local-loan extension.

2026-10-08 read-only supplement: `OwnershipCleanReadOnly.v` proves a narrower
local alias rewrite without introducing loans into the canonical heap model.
Admission excludes writes, retention/consumption, live-out aliases and calls;
original canonical admission is preserved before rewriting. Valid source
executions preserve trace and inherit closed-program cleanup. A certificate
against `texec`'s allocation frontier establishes 3 -> 2 abstract allocations
on the fixed branch witness, with the same trace and two empty final heaps.
It is not a physical-memory benchmark or the member-projection/places core.
The exact propositions are extraction consumers; docs/207 section 10 and
the dated research audit record the current evidence and integration limits.

2026-10-08 automatic-memory naming and comparison: "ownership-based automatic
memory management" is the core ordinary-value lifecycle mechanism (27 §0).
`OwnershipCleanGCComparison.v` imports the existing ownership machine and
proves exact-retention bounds and explicitly conditional full-heap-sweep
operation-cost savings. Its canonical `elab`/`texec` workload and typed audit
also preserve counterexamples to universal strict superiority. A correct GC
can be equally memory-safe and can match retention. This is not wall-time,
arbitrary-graph collector correctness, or compiler/runtime refinement evidence.

2026-10-08 exits, unpack, regions and fail-closed summaries:
`OwnershipCleanExits.v` layers break/continue/return/throw/try over the core
and proves that every outcome runs with the source trace and ends with its
target live set, so an error before a pack releases the parts built so far.
`OwnershipCleanCore.v` adds unpack of a dead record (overlapping projections
with no copy) and one-value regions released at their end. Its mode table now
holds `option` summaries: a missing or wrong-length summary is refused at the
call and at the routine, never read as borrowed, and inference ascends from
no summaries. Member-path identity in MIR, the production call-graph
fixpoint, deep runtime glue, the physical allocator, panic/abort and
async/FFI remain implementation obligations (27 §4).

2026-10-09 bounded cutover prerequisites: `OwnershipCleanCallRecovery.v`
imports the value/exit machine and proves alias-refusing bundle call recovery,
one operational catch adapter and restore-before-dispatch. `OwnershipCleanViews.v`
keeps the same owning heap and proves scalar write-through INV/source CORR,
guarded backing drop and old-ticket refusal in linear schedules. Its currentness
checker is a dynamic ghost oracle, not the production static issuer. It reuses
teardown lease vocabulary without fabricating graph handles. The typed consumer is
`tests/coq/OwnershipCutoverPreflightAudit.v`; focused gate
`tests/ownership_cutover_preflight_smoke.sh` freshly kernel-checks it.
Production callee lowering/pointer ABI, complete/current caller-scope issuance,
source expression/place evaluation,
static final-MIR lifetime/effect facts, evidence linearity/snapshot binding,
whole-instruction view frame, general payload glue and native/self-host
consumers remain OPEN. These supplements are not whole P1 or compiler closure.

2026-10-09 Claude CL6/CL7: `OwnershipCleanCallLowering.v` lowers an exiting
callee into the core's ordinary procedure table by a status variable and
proves end-to-end recovery of every inout and the outcome packet for normal,
early-return and handled-error exits, with target soundness from
`elab_sound`; it does not depend on a new catch rule. `OwnershipCleanViewScope.v`
models a writable view as a whole-backing focus scoped to its last use and
proves the backing keeps its length; growth, transfer and release of the
suspended backing are refused statically by the core and the admission.
Both files are kernel-checked with no assumptions. Production direct-jump
epilogues, place disjointness, expression order, returned/stored views and
views passed to calls remain OPEN.

2026-10-08 graph links: `OwnershipGraphLinks.v` checks the proposal in
`docs/audits/ownership_graph_links_design_2026-10-08.md`. A store owns its
nodes; links are values (store id, slot, generation) that own nothing. Every
operation keeps the heap equal to the store footprints. A drop walks slots,
not edges. A stale link stays refused forever, and links are unique per
insert, because generations retire instead of wrapping. Borrows exclude
conflicting writes, deletion, drop and table growth. Append-only stores need
no generation check. Nodes no link reaches stay retained until their store
ends; that limit is proven, not hidden.

2026-10-08 teardown: `OwnershipTeardown.v` moves ownership from the store to
each node (one owner: a scoped root or a parent, as in a Qt object tree) and
keeps links non-owning. Node release and root drop always have a step. Each
clears every stored link into its unit through a reverse index, then frees
the unit and bumps generations; the unit comes from an owner-keyed children
index. Every live node belongs to a root still in scope, no step leaves a
dangling stored link, and no step acts through a released node's handle.
The ancestor check on reparenting stays mandatory: without it an ownership
cycle leaks permanently. Link fields can become empty, and owned nodes that
nothing links to stay until their owner releases them or their root ends.

2026-10-08 teardown red-team follow-up: retirement schedules require `NoDup`
without losing always-successful exact-unit existence. The admitted field and
parent transformations update only their actual old/new rows, with pointwise
refinement of the scan specifications and untouched-row sharing in fresh
OCaml execution. The permanent typed consumer joins the same fresh production
kernel snapshot. Bounded extraction checks 98,304 field updates and 16,384
teardowns; the cost observer preserves crowded old/new-row and quadratic
uniqueness limitations. At that checkpoint, caller authority, active loans/pins,
escaping root identity, indexed unit issuance and physical/concurrent
refinement remained OPEN.
Details: [teardown audit](audits/ownership_teardown_redteam_locality_2026-10-08.md).

2026-10-08 root/local repair: root-directed allocation, reparenting and drop
consume the same current root epoch; drop advances it and re-declaration
preserves it. Arbitrary continuation cannot revive the old root handle.
The executable `resolve_node` returns no node through a saved retired link;
`check_root` agrees with the admission predicate. Immutable source-slot
snapshots prevent recursive duplicate heap evaluation in repeated teardown.
These are single-forest identity/model-read results, not issued authority,
stable loans, cross-arena identity or physical allocator refinement. Those
obligations and compiler adoption remain OPEN. Details:
[root/local repair audit](audits/ownership_teardown_root_epoch_2026-10-08.md).

2026-10-08 teardown authority: `OwnershipTeardownAuthority.v` imports the
forest rather than introducing another heap. It executes root-right issuance,
transfer and consumption, owner-approved lifetime loans/pins, complete-unit
pre-destruction checks and explicit unchanged-state refusal. Whole admitted
runs preserve forest/rights/lease invariants; old consumed node/root identities
never regain admission. Right provenance, monotonic lease IDs and complete
bounded owner-path lookup support legitimate node/root cleanup. The permanent
redteam and fresh extraction consume this boundary, retaining the raw forest
permission counterexamples as specifications, not accepted cleanup APIs.
Physical protected state/caller binding, live recipient validation, CL2 access
and mutation, CL4 indexed walker, native cleanup and concurrency remain OPEN.

2026-10-08 memory-boundary composition: the user adopted the requirements in
[doc 28](semantics/28_memory_boundary_composition.md), joining one storage
owner, owner-bound access and one retirement edge. The typed
`tests/coq/MemoryBoundaryCompositionAudit.v` consumes the existing ownership,
graph and Slot proofs without a new machine. For an admitted same-root and
same-footprint correspondence, graph ODrop and ordinary TDrop agree on the
heap, preserve their invariants, and admit release of the same Slot. Positive
compatibility uses the existing cyclic graph; negative cases retain missing,
stale/released, token, pin and borrow checks. Root-binding issuance, source
semantics, atomicity and production refinement are OPEN. This audit is not a
proof that three physical operations may free the same payload in sequence.

2026-10-08 allocation/frame composition: graph-only `GInv` does not protect
another canonical owner's storage. The graph model's `gexec_framed` requires
an admitted external footprint and refuses missing evidence or allocation
overlapping that footprint before transition. The audit proves new-store, vacant-insert
and growth heap updates preserve `HeapSplit`, using canonical `free`. Own
retired-table reuse remains legal, and the growth-to-retirement consumer
executes graph drop, canonical `texec` and Slot release while another value
stays live. Frame/root issuance, physical placement and C/LLVM remain OPEN.

2026-10-08 reuse reinforcement: every model in the memory-boundary
composition now admits physical reuse. SlotCalculus releases to a tombstone
and reclaims at the next generation, matching the runtime; under the
earlier generation-1 claim a released handle read the next occupant
(`gen_one_reclaim_resurrects`). The graph model's allocator may reuse
just-freed blocks; the audit's address-based link admitted a stale node
after reuse and is replaced by generation links, rooted links that carry the
root's generation, a split-heap retirement theorem, and one canonical owner
per block. Store-id exhaustion, issuer correctness and production
refinement remain OPEN.
