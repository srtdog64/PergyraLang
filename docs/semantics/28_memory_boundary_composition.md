# Memory Boundary Composition

Status: **ADOPTED COMPOSITION REQUIREMENTS; production implementation OPEN**.
Selected by the user on 2026-10-08: Slot, ownership-based automatic memory
management and graph links must obey one coherent lifetime contract.

**One storage owner, access rights tied to that owner, one retirement edge.**
Connections may be shared or cyclic; ownership and authority are not copied
by copying a connection. Ordinary values retain value semantics and D1.

This contract owns the requirements for composing existing facts. It does
not redefine the heap, Slot, its ABI, or the cleanup algorithm. Their owners
remain [Ownership Clean](27_ownership_clean.md),
[Slot Capability Calculus](08_slot_capability_calculus.md) and
[Slot ABI single-owner rule](13_slot_abi_single_owner.md).
The existing [graph design model](proofs/OwnershipGraphLinks.v) checks one
candidate store/link algorithm; it is reused, not a production graph owner.

## Stable surface and target surface

The covered Slot boundary and canonical ordinary-value formal core retain
their current scope. Automatic C/LLVM cleanup refinement remains OPEN.
Stored non-owning graph references are a target surface, not currently
accepted source syntax or a completed graph implementation.

No Slot is required for each ordinary value or each graph link. An ordinary
local has its inferred owner; a resource or identity-bearing graph has an
actual lifetime boundary. Slot is used where that boundary is material.
Pure graph computation need not become a subject/action workflow.

## Design provenance

**The user brought the Qt-inspired graph ownership proposal to Pergyra.**
The source pattern is Qt's parent/child object ownership, together with
non-owning references that are invalidated when their targets are destroyed.
Qt documents parent-owned children in
[Object Trees & Ownership](https://doc.qt.io/qt-6/objecttrees.html), and
non-owning guarded-pointer clearing in
[QPointer](https://doc.qt.io/qt-6/qpointer.html).

The AI assistants' contribution is checking, formalizing, red-teaming and
repairing that proposal, not originating the idea of bringing it from Qt.
Pergyra-specific adaptation includes Slot authority and loans, independent
root/node reuse protection, external-footprint admission and compiler-owned
cleanup. These are separate obligations, not guarantees imported from Qt.
This attribution concerns the graph/teardown design; it does not attribute
all ordinary-value liveness, move/copy or cleanup elaboration to Qt.

The borrowing is conceptual, not adoption of Qt's complete API or memory
safety behavior. Qt itself documents stack-object destruction-order hazards.
Clearing a QPointer-like connection alone proves neither access authority nor
a stable loan or concurrent dereference safety in Pergyra.

## The common lifetime contract

Every managed backing belongs to exactly one admitted owner. A live owner
has an owned footprint and a retirement obligation. A reference identifies
a target and the owner under which access is permitted; it does not own
the target, grant authority, or keep an owner alive through hidden RC.

There are three roles, not three memory-management modes:

| Role | Responsibility | Existing authority |
|---|---|---|
| Owner | lifetime, footprint and finalization responsibility | canonical ownership facts; resource lifetime facts at Slot/zone boundaries |
| Access | which target may be read or changed in this lifetime | admitted borrow/place facts and covered Slot capability/lease predicates |
| Retirement | consume the owner's one finalization obligation | canonical cleanup facts or the resource's own cleanup edge |

A live external borrow contributes a lifetime obligation for the owner.
Persistent internal connections do not act as owners or recursively keep a
cyclic graph alive. Every actual access still requires a valid owner and
target. Ending the owner invalidates its non-owning links.

Ordinary-value cleanup continues to use the last-use/exit decisions of doc
27. If a resource boundary owns those values, payload cleanup belongs to
that boundary's finalization edge; it is not an independent local drop of
the same backing. Affine handles keep their existing cleanup edges and
are not ordinary values passed through deep-copy/drop glue.

## Identity and access judgments

The names below describe facts, not new keywords, wire fields or ABI types.

```text
AccessValid(owner, target, mode)
  = owner is live in the admitted lifetime
  ∧ target belongs to that owner's current footprint
  ∧ target identity is current
  ∧ the required access right is valid
  ∧ the actual borrow, alias and address-stability obligations hold

RetireValid(owner)
  = this boundary's lifetime has ended
  ∧ its storage ownership and finalization edge are admitted
  ∧ no live loan or pin forbids retirement
  ∧ any required release authority is valid
```

Owner identity, owner lifetime/epoch and target identity are distinct.
**A fresh Slot generation does not prove that a deleted/reused node inside
the Slot is fresh.** A dynamic node identity must carry its own reuse
protection or be proven non-reusable during every admitted access.

**Storage is reused, so storage is never an identity.** A freed address may
hold the next allocation at once. A link that remembers an address or a
block names whatever reuses it, with no failure. Every identity that a
stored reference relies on must survive reuse:

- a node: (slot, generation), checked on every access;
- a store or root: a handle with a generation, or an id that is never
  reissued and fails closed on exhaustion;
- a root slot: released as a tombstone that keeps its generation and
  reclaimed at the next one.

A link therefore carries, or is rooted in, the root handle it was issued
under, and access checks both generations. Ordinary values need none of
this: they store no references, so reuse of a dead block is invisible to
every name.

Missing owner binding, membership, lifetime, access or exit facts refuse
the affected operation. Matching variable names, ordinals or raw addresses
is not an owner-binding certificate. One graph's node ID cannot resolve in
another graph merely because the numeric ID is equal.

The canonical `{ value, occupied }` PgySlot ABI is not a generational node
handle. The generational Slot manager is a separate existing runtime
materialization. Neither may be silently reinterpreted as the other;
graph profile admission needs the actual identity/lifetime evidence.

## Shared connections and mutation

One owner may hold A, B and C while links express A to B, B to A and
multiple links to C. The owner inventory, not traversal of connection
edges, determines the storage to retire. Edge multiplicity and cycles do
not duplicate ownership or recursively determine release.

Read sharing and mutation rights are separate. Mutation admission needs
exclusivity for the actually accessed places, including overlapping views
within one routine. Possessing a graph-wide write right alone does not
prove that two simultaneously live mutable aliases are safe.

Direct references need proven address stability and a bounded valid loan.
Long-lived links across node deletion or backing movement need checked
resolution. A stored owner identity is metadata, not a duplicated public
Slot or capability token. Link representation remains a target decision.

Copying ordinary values keeps D1. Transferring graph ownership must preserve
identity and invalidate the former ownership frontier. An independent
snapshot needs a declared identity/link reconstruction contract; no hidden
deep copy, shared backing or copy-on-write fallback is selected here.

## One retirement edge

All fallible admission checks precede destructive payload retirement.
An invalid token, stale owner or live pin must not leave a freed payload
behind a still-live resource handle. Validation, payload finalization and
owner invalidation must remain coherent against intervening mutation.
Physical atomicity, locking and fallible finalizer behavior require runtime
refinement; a conjunction of model predicates does not prove them.

Compiler-synthesized cleanup and explicit resource retirement cannot both
consume the same obligation. The resource finalizer is the one root edge;
it may invoke owned-payload glue, but no second drop may free that payload.
Return, error, cancellation and partial initialization use admitted exit
facts; they do not bypass Slot release or finalizer contracts.

Owner-end reclamation is not automatic discovery of unreachable nodes
inside a still-live owner. Individual deletion, remaining links and
isolated cycles require an admitted deletion/liveness contract. The current
graph design model checks generations that retire rather than wrap, and
refuses deletion/drop/growth forbidden by an active borrow; it does not
implement these rules in the compiler. Retention can grow
without violating owner-end cleanup, so bounded memory is a separate claim.
No tracing collector, reference counting or unproved retention fallback
is introduced. Observable resource finalization may not be delayed merely
to batch physical allocator operations.

## Tradeoffs and limits

The goal is GC-like authoring convenience from ownership evidence, not
universal superiority to GC. The following tradeoffs remain even if the
production implementation is correct; current model gaps are listed below.

| Design choice | Benefit | Cost or limitation |
|---|---|---|
| One owner, many non-owning connections | Shared and cyclic connections do not multiply cleanup responsibilities | Ownership must stay acyclic. Reparenting needs an ancestor guard; copying a link does not grant independent shared lifetime or release authority |
| Reclaim at admitted node deletion or owner end | Cleanup does not depend on tracing connection reachability | An owned but unused node or isolated connection cycle can remain under a long-lived owner. No ownerless nodes is not a bound on retention or logical leaks |
| Clear stored links into deleted targets | Stored connections need not keep dead objects alive | A deletable-target field must expose possible absence; a long-lived checked handle may fail to resolve. Ordinary values and statically bounded access must not inherit universal Slot/Option rituals |
| Maintain reverse and child indexes | Find incoming connections and an owner's children without tracing the whole connection graph | Metadata, mutation bookkeeping and ancestor checks cost memory/time. No whole-heap tracing does not mean free or constant-time teardown; a large owner can cause a cleanup burst |
| Preserve ordinary value semantics and D1 | One backing has one cleanup responsibility; a last-use move can avoid copying | Independent values may require copies. A graph snapshot needs explicit identity/link reconstruction. Cheap shared references in a GC design can beat those copies |
| Infer ownership and synthesize cleanup | Users need not write carriers, restoration chains or deep-drop procedures | Analysis, call summaries, branches, loops, escapes and partial initialization add compiler work. Correct inference and acceptable compile cost require production evidence |
| Guard loans, pins and reuse | Old identities cannot silently acquire new storage, and live access constrains retirement | Retirement or growth can be refused while an external loan/pin is live. Finite generations must fail closed on exhaustion; identity freshness alone is not permission or address stability |

"Release always succeeds" in `OwnershipTeardown.v` means an exact retirement
unit exists for an admitted live forest node/root in that model. It is not
unconditional production release: the raw forest model does not issue release
authority or account for active external loans/pins. The importing
`OwnershipTeardownAuthority.v` below implements that bounded admission boundary;
production obligations still come from `RetireValid` and their actual owners.
A checked immutable model
value is not a physical pointer that remains safe after another thread frees
its storage.

Current evidence is bounded. The teardown model's crowded old-row filtering
and new-row append remain linear, and its list uniqueness observer is
quadratic; see the
[teardown locality receipt](../audits/ownership_teardown_redteam_locality_2026-10-08.md).
Exact child-index/unit correspondence is not yet an executable indexed walker
with a proved cost bound. The
[root-epoch OCaml receipt](../audits/ownership_teardown_root_epoch_2026-10-08.md)
measures the functional model, not native allocator, compiler, GUI or GC performance.
Owner/root/node generations still need arena-domain binding, opaque handle
issuance and finite-generation refinement; a guessed numeric handle is not
admitted evidence.

Concurrency, check/use atomicity, finalizer order/reentrancy, cancellation and
FFI remain runtime-refinement boundaries. Logical link invalidation alone
does not establish them or a real-time/tail-latency guarantee. The comparison
in [doc 27](27_ownership_clean.md#0-adopted-name-and-comparison-boundary) is
against a specified ideal nonmoving full-heap sweep under equal cost policy,
not all generational, compacting or concurrent collectors. Neither general
speed nor greater memory safety than a correct GC has been established.

The intended fit is identity-bearing resource hierarchies and graphs with
meaningful ownership boundaries; ordinary local values remain ordinary
values. If real programs require annotation/Option/carrier proliferation,
an immortal root retains most data, or copies, compile analysis and cleanup
bursts dominate, revisit the ownership boundary or representation. Do not
hide the issue with a weaker safety check or silent GC/RC fallback. Validation
must follow the existing implementation order and real C/LLVM lifecycle
chain, not another independent proof/performance track.

## Store-local cycle reclamation (optional scope adopted; implementation OPEN)

Added 2026-10-09. The table above keeps one cost even for a correct
implementation: an isolated cycle under a long-lived owner stays allocated.
Two importing proofs check a candidate-bounded selection rule for retiring
it. They do not yet prove an implementation without a whole-store scan.
On 2026-10-09 the user selected this as a **store-local optional feature**,
not the default ordinary-value cleanup policy. It is local reachability
tracing and must stay distinguishable from non-tracing ownership cleanup.
Where checks run, actual work accounting and production admission remain
implementation obligations; the acceptance row for isolated cycles still
describes the baseline without the optional policy.

The baseline graph lifecycle is authorized domain-directed node deletion and
compiler-owned cleanup of the remainder at store-owner end. Forgetting a node
removal can retain its storage until then. This does not require users to write
manual backing/deep-drop or root-registration procedures. A stale-link refusal
prevents access to a reused identity; it does not establish that an earlier
deletion preserved later successful program behavior. Borrow/pin and cleanup
authority checks are separate requirements. Neither baseline performance
superiority nor a deployed public graph API is established by this model.

**Local check** ([`OwnershipGraphCycleReclaim.v`](proofs/OwnershipGraphCycleReclaim.v)).
Every node has a count of the edges that name its current identity (slot and
generation). For a candidate set C the check counts the edges among C and
subtracts that from the stored count; the
remainder is the edges that arrive from outside C. A candidate held by a root
or by an outside edge is kept, together with everything it reaches inside C.
That closure is computed within a round budget and then checked. The rest of C is
deleted through the existing `ODelete`, so per-node release, borrow refusal
and stale-link refusal are inherited. The old result-ignoring sequential
composition is **not** whole-batch failure atomic: a later borrowed node can
refuse after an earlier candidate was deleted. The checked counted batch
below replaces that path with functional original-state refusal, not a
physical rollback algorithm. Production still requires
complete canonical retirement units, whole-batch preflight and a stable,
non-refusing commit before reuse is published. Graph candidate sets must not
be silently expanded through rooted/pinned ownership descendants.

Implementation boundary: `internal` currently folds the entire slot-list
spine, inspecting edge payloads only for C; list indexing is not constant
time. The original language's `reclaim_step` also recounts `indeg`.
`counted_reclaim_projects` now connects a maintained-count operation to that
reference decision, but does not prove physical candidate-only traversal or
a work/latency bound. `fuel` counts closure rounds, not total visits or cost.

- `trial_garbage_with_unreachable`: for every candidate set, root list,
  budget and counter that never under-counts, a returned node is unreachable
  from the roots. Over-counting only retains.
- `reclaim_preserves_root_view`: the deletes keep the graph invariant, other
  stores, the reachable set, every reachable slot and every root link's
  resolution.
- Change records: `counts_track_every_operation` keeps a maintained counter
  equal to the recount across the seven existing graph-machine operations,
  under `CountsExact`, `AllEdgesIssued` and `op_admitted`. A delete resets
  the deleted identity's count (`count_after_delete_self`). Admission allows
  past-generation stale edges but excludes future/unissued vacant-current
  identities. `counted_run_preserves_counts` packages admitted graph/count
  transitions. Successful `counted_reclaim_projects` refines the recount
  decision; it is an accepted-only projection, not equivalence for every
  refused raw candidate list. `counted_reclaim` now validates current live
  candidates and uses an explicit checked batch, not the result-ignoring
  general sequential runner. Duplicate/stale/missing/borrowed candidates and
  selection deferral preserve the complete original counted state.
  `count_after_snapshot` is a slot-list copy lemma, not a language snapshot
  operation. Moving a node between stores is not a model operation.
- Falsifiers: a pure in-count test does not free a cycle; subtracting without
  the closure deletes a reachable node; a too small budget defers; an insert
  that names a reclaimed raw slot index observes the reclamation; and a
  counter that decrements by slot index after reuse under-counts and deletes
  a reachable node.

**Root completeness** ([`OwnershipGraphRootCompleteness.v`](proofs/OwnershipGraphRootCompleteness.v)).
The roots are not trusted. A small language has values that hold links and
views inside aggregates, a liveness checker over certificates, calls with
suspended frames, handled errors, inserts, edge writes, deletes and a reclaim
statement. What must be kept is defined by the language, not by the
compiler: the reference semantics never reclaims, and whatever the rest of
its run reads, follows, writes or deletes must survive.

- `links_of_complete`: every link or view a projection path can extract from
  a value is enumerated. Records, arrays, enum payloads, inout packets and
  closure captures are modelled as aggregates.
- `rcheck`: loop heads, call `keep` sets and reclaim root sets are checked
  certificates, and every statement that can fail keeps the handler's live-in.
- `reclaim_simulates`, `reclaim_preserves_every_reference_run`: under `FunsOK`,
  `rcheck` and the issuing/graph invariants, every terminating reference run
  is reproduced by the reclaiming run with the
  same environment, trace and outcome. At each statement boundary every
  deleted slot is unreachable from that point's live roots. A missing root
  is a semantic failure; an extra one only delays reclamation.
- Reclaim and explicit delete in the original language use a fixed rights
  map (`reclaim_without_right_is_identity`). The new `ledger_rights` adapter
  derives permission from `OwnershipTeardownAuthority.v`, including current
  root epoch/holder: grant admits, transfer revokes the old holder, consumption
  revokes, and copied handles supply no right. `checked_ledger_reclaim_simulates`
  connects this permission projection, maintained counts and checked roots
  for accepted batches. Refusal reasons propagate explicitly and preserve
  original state; they are not projected into a legacy no-op.
  Issuing the store-to-root binding, synchronizing graph/forest retirement,
  and transferring the forest's whole-unit lease/pin checks are still OPEN.
  This is not complete authority composition or a dynamic-rights language run.
- Falsifiers on the same semantics: a link held only inside an aggregate,
  only by a suspended caller, or only by a derived view, and a returned link
  dropped from the callee's roots. Each turns a successful reference read
  into a refusal under the wrong root producer or certificate. The checker
  refuses the certificates that would omit the caller's or the result's link.

Boundary: allocation choices are inputs; a reference run that fails an
allocation has no transition, and reclamation can only make more choices
admissible. Temporaries are named (ANF). View identities are values or frame
roots; physical leases, pins, addresses and range validity are not modelled,
and the borrow table is empty between statements.
Links into other stores live in values. Weak links that reclamation may
clear are not provided. The production producer of liveness, frame maps and
value layouts, runtime counters and their overflow, concurrency, cost and
the C/LLVM refinement remain OPEN. Gate: `tests/graph_cycle_reclaim_smoke.sh`
kernel-checks seven models and `tests/coq/GraphCycleReclaimAudit.v`, which pins
the proposition types and concrete reuse/authority/root falsifiers.
`tests/graph_cycle_reclaim_selftest.sh` plants deletion, weakened-theorem,
new-assumption, source-drift and receipt-overwrite regressions in isolated
copies. Source hashes bind each focused receipt before/after its run; that
does not prove continuous immutability or a main-thread P0 baseline.

For implementation, use the
[lifecycle boundary matrix](ownership_lifecycle_implementation_boundaries.md)
alongside these owner contracts. It distinguishes stable physical preflight
and non-refusing commit from a returned mathematical original state, and
requires root/count/authority facts from the same validated generation.
`checked_ledger_reclaim` is not an evidence issuer: its simulation theorem
requires `CountsExact`, `AllEdgesIssued` and `SimInv`. A caller supplying empty
L and R does not establish that no roots are live. Nor do action hold-release
or compensation completion in [29](29_action_scoped_references.md) establish
transaction rollback. The matrix is a navigation aid, not another semantic
owner or a new prerequisite track for ordinary-value cutover P1.

### Snapshot, inventory and cache observations

A full-store data snapshot and a root/count/authority evidence snapshot are
different contracts. The current pure `snapshot` copies the full slot inventory;
its relative-edge/footprint-length lemmas do not issue a second physical store
or prove allocation non-overlap, affine-payload copying or failure-atomic
publication. An independent data copy needs fresh owner/identity/storage and
admitted payloads; cleanup of a failed partial destination must preserve the
source. Hidden shared mutable history or COW is not that independent-copy
contract.

Full inventory copying/enumeration and strong cache key lookup can observe
nodes with no external node-link roots. The checked reclaim language does not
yet include those operations. Their discoverable inventory must be preserved
by the root/read-footprint contract until explicit domain removal, unless a
distinct logical/weak contract is adopted. Do not silently narrow a promised
full-store snapshot to a reachable-closure copy or evict a strong cache entry
because its node link disappeared from user variables. An already published
independent copy has its own owner and does not retain the source by sharing.

Cached compiler/retirement evidence is valid only in its admitted generation;
application-result caches also need a content revision, not just identity
generation. CPU locality is not cache-entry semantics, and neither is proven
fast by using the word store. The independent consumer now pins the borrowed-
batch partial-deletion and full-inventory-snapshot falsifiers, alongside
baseline retention. The partial runner is now retained only as a falsifier;
the actual checked batch refuses without publishing earlier deletes.
The importing canonical batch below also checks all requests against their
initial forest and rejects overlapping exact units. Its original candidate
missed shrinking-parent/root cases; independent red-team found and corrected
that gap. Scope and remaining producer/physical obligations are in the
[atomic bridge audit](../audits/graph_atomic_bridge_redteam_2026-10-09.md).
Implementation recommendations, primary research and
unmeasured costs are in the
[bounded review](../audits/graph_store_policy_snapshot_review_2026-10-09.md);
the [closure plan](../agent_work_directives/graph_store_integration_closure_plan_2026-10-09.md)
keeps integration dependent on the active ownership cutover, not a new rung.

## Fact ownership and consumer chain

The ordinary storage owner remains doc 27 and its canonical machine. Slot
capabilities and ABI rows retain their existing owners. The future graph
binding/membership producer must be named before production adoption.
This contract is not a second MIR fact table or an added CLOSED registry row.

The production chain to refine is source ownership and lifetime boundaries,
semantic admission, typed MIR facts, resource ABI projection, C/LLVM and
self-host emission, then runtime access/finalization. The resource ABI
entrypoint is `mir_abi_resource_runtime_row_for_type_name` in
`src/compiler/mir_abi_resource_runtime.c`; backends consume its admitted rows.
The current I1-I8 order is unchanged: storage/copy glue before any new drops.

The last consumer uses one admitted snapshot/generation of the relevant
owner facts. Backends do not infer graph membership or recover missing
ownership by scanning source, inspecting pointer equality or retrying a
legacy path. Cross-worker/async/FFI carriage needs an actual transfer or
snapshot contract, not a captured growable backing pointer.

## Bounded formal evidence

Rocq 9.3.0 and `rocqchk` check every file below. The only assumptions are
SlotCalculus's two approved abstractions (the token verifier and the
slot-id bound). The focused gate is `tests/memory_boundary_composition_smoke.sh`.

**Every model admits reuse.**

### Executable teardown authority boundary

[`OwnershipTeardownAuthority.v`](proofs/OwnershipTeardownAuthority.v) imports
the existing forest and invokes its existing `teardown`/`root_drop`; it defines
no second heap or destructive algorithm. Requests carry copied identity, not a
caller-chosen permission bit. Trusted machine state binds the executing context
and holds one cleanup-right cell per root (epoch, holder). Creation issues the
cell to its owner, transfer replaces its holder, root retirement removes it.
Subtree cleanup consumes the retired node identities, not the responsibility
for other surviving nodes under the same root. No operation restores a snapshot.

The admission order is current target, actual cleanup owner, complete unique
retirement unit, then absence of any lifetime borrow/pin in that unit. All checks
precede either existing destructive transform. Refusal returns the identical
state. A borrow/pin can only be issued by the cleanup owner; the designated
borrower can end it but cannot acquire cleanup authority by holding it. Lease
IDs are issued monotonically. Borrow/pin are lifetime protection only here:
neither promises immutable field contents or concurrent read/write exclusion;
an incoming non-owning field outside the unit can still be cleared by teardown.

The model proves whole-run forest/rights/lease invariants, one-step right
provenance, consumption and non-resurrection across later admitted runs,
complete bounded parent-path lookup, and positive legitimate node/root cleanup
under the required no-loan condition. Permanent tests retain both raw-forest
permission-gap witnesses and show refusal at the authority boundary on the same
`st_ab` input, with a valid-state and owner-cleanup/link-clearing positive control.
Fresh extraction observes these actual functions, not a new policy simulator.

The finite unit verifier scans the forest bound and checks membership closure.
This is a sound bounded model certificate verifier, not CL4's production
indexed walker or a claimed cheap compiler hot path. Its bound/membership work
and quadratic list uniqueness are explicit costs. The extraction exposes
record constructors and scheduler fixtures for inspection; it is NOT an opaque
native security API, and bounded OCaml ints do not refine unbounded naturals.

Still OPEN: authentic non-reused execution-context issuance and live transfer/
borrow destinations, root namespace authority, protected current-state storage,
actual access/mutation and reparenting consumers (CL2), the indexed walker,
bounded identity exhaustion, physical allocation, concurrency/finalizers and
compiler cleanup synthesis. No C/LLVM, installed-driver, CI or SoT closure is
implied. The ownership/DX implementation hold and I1–I8 order are unchanged.

### Importing original-state atomic batches

[`OwnershipTeardownAtomicBatch.v`](proofs/OwnershipTeardownAtomicBatch.v)
composes the existing authority boundary. All requests are first checked
against the same original state: current identity, actual holder, complete
unique unit and no active lifetime lease. Every pair of exact units must be
disjoint, and target identities must be unique even when units are empty.
Under the forest invariant, each supplied unit is the original `UnitExact`.

Accepted execution additionally records every canonical retirement of those
same supplied units. A node-only batch leaves the root right intact; later
root cleanup consumes it once. Any refusal preserves the entire initial
AuthorityState and its reason. The independent consumer checks late nested
borrow/pin and foreign-holder refusal, duplicate/overlapping units, and
shrinking parent/root units that only become valid after earlier deletion.

This is functional publication atomicity, not rollback of actual frees. The
pure initial check invokes mathematical canonical retirement; no-fail native
commit under stable preflight remains unproved. The two graph/canonical
batches are not yet a synchronized algorithm over authenticated physical
units. Gate and tradeoffs:
[atomic bridge audit](../audits/graph_atomic_bridge_redteam_2026-10-09.md).

### Existing allocator and reuse composition

- The graph model's allocator is adversarial. Each operation that needs
  storage takes the blocks as an argument, and any choice of distinct,
  non-live blocks is admitted, including blocks freed by the previous step.
  Its `gexec` is a graph-fragment transition, not whole-heap allocation
  authority. Whole-heap composition uses `gexec_framed` with the external
  footprint admitted by `HeapSplit`; a missing footprint is `RNoFrame`,
  never an inferred empty heap. The check precedes the fragment transition.
  A vacant-slot insert checks its payload blocks, not the unused table
  candidate; growth checks both the table and payload. Existing graph-live,
  duplicate, generation and borrow guards still apply.
- SlotCalculus releases a slot to a tombstone that keeps its generation,
  and reclaims it at the next generation, as the runtime does. Before this
  change every free id was claimed at generation 1, so the previous
  occupant's handle read the next one (`gen_one_reclaim_resurrects`). The
  ABA theorem in doc 08 did not apply to the model until then.

[`MemoryBoundaryCompositionAudit.v`](../../tests/coq/MemoryBoundaryCompositionAudit.v)
composes the canonical machine, SlotCalculus and the graph design model
[`OwnershipGraphLinks.v`](proofs/OwnershipGraphLinks.v). It defines no
heap, allocator, interpreter, token verifier or binding issuer.

| Claim | Theorem or witness |
|---|---|
| Ordinary values are reuse-ready: a dead block is in no footprint, so two names that reach one address reach one block | `dead_block_unreferenced`, `reuse_never_aliases_names`, `canonical_reuse_witness` |
| An address is not a node identity: after B's block is reused by D, the address view admits the old link and reads D | `address_link_admits_reused_node` (counterexample) |
| Generation links survive the same reuse: B's link is refused, D's admitted | `reused_blocks_do_not_revive_link`, `stale_node_refused_under_live_root` |
| An admitted access reaches only the owner's live storage | `access_targets_owned_live_storage` |
| Root validity and node validity are independent; access needs both | `root_validity_does_not_admit_stale_node`, `node_validity_does_not_admit_stale_root` |
| A link into another store does not resolve under this root | `foreign_store_link_refused` |
| A stale root handle stays refused through any later Slot steps, reclaim included | `stale_handle_never_admitted`, `reclaimed_root_refuses_old_handle` |
| Reissuing a store id without a generation resurrects links; a rooted link is refused | `store_id_reuse_resurrects` (counterexample), `rooted_link_refused_after_root_reuse`, `rooted_link_survives_store_id_reuse` |
| Retirement with the rest of the program live: graph drop and canonical drop free the same footprint, other owners keep theirs, the root becomes a tombstone | `retirement_agrees_split`, `retirement_with_other_values_live` |
| Every framed step/run preserves graph invariants and separation from external owners; an overlapping allocation or missing frame leaves the state unchanged | `framed_step_preserves_ownership`, `framed_run_preserves_ownership`, `external_allocation_refused`, `missing_frame_refused` |
| New store, vacant insertion and growth preserve the actual whole-heap split using canonical `free`, not an assumed post-state | `new_store_allocation_agrees_split`, `vacant_insert_allocation_agrees_split`, `growth_allocation_agrees_split` |
| Growth may reuse its own old table for a node; subsequent graph/canonical/Slot retirement executes and the other owner's value survives | `framed_growth_split`, `framed_growth_retirement_executes` |
| A graph-only allocator can take another owner's live block; whole-heap admission refuses new-table, node and grown-table overlap before transition | `graph_fragment_can_take_external_storage` (counterexample), `framed_allocation_refusals`, `framed_vacant_insert_refuses_external_node` |
| One block has one canonical owner; after retirement a second canonical drop, graph drop and Slot release are all refused | `one_canonical_owner_per_block`, `retirement_consumes_once` |
| Growing a table under a borrow puts another object at the borrow's address | `unguarded_grow_dangles` (counterexample) |

The audit's retirement theorem splits the canonical heap into the graph
heap and the other owners' storage. It no longer needs the whole program to
be a graph. Its graph retirement consumer uses the same framed boundary as
allocation/growth. The allocation proofs update the whole heap with the
existing canonical `free` operation and derive its separation from the
pre-admission checks; they do not assume the desired output disjointness.

`Ho` is an admitted footprint of all other live owners, not an arbitrary
caller whitelist. Frame/root issuance remains a production obligation.
Ordinary-value physical address placement must still be injective on the
whole live heap. `reuse_never_aliases_names` consumes that premise; it is
not a proof of a physical allocator maintaining placement across steps.
The framed theorems establish separation of modeled Block footprints;
physical address placement, copy-before-free and failure atomicity remain
runtime-refinement obligations.

Some refusals hold because the predicate states the requirement; the audit
marks them "definitional". They type-check the contract and do not test a
machine. Missing-binding refusals and the live-borrow refusal are of this
kind.

Still OPEN:

- stored-reference source semantics, and the issuer of the root/footprint
  binding;
- typed node layout, and production refinement of deletion, reuse,
  generation exhaustion and live-loan/alias admission, including global
  frame issuance and physical placement preservation;
- store-id exhaustion in an implementation that never reissues ids;
- runtime atomicity and finalizers;
- cancellation, async and FFI;
- actual C/LLVM cleanup and performance.

Abstract edge metadata is not proof of zero-cost physical graph storage.

## Acceptance and falsifying cases

| Input | Required result |
|---|---|
| Multiple links and a cycle within one owner | connections do not duplicate storage ownership; owner end retires one footprint |
| Missing or wrong root binding | no access or retirement through a guessed owner |
| Root released, stale generation or unissued token | explicit refusal; no successful dereference or destructive cleanup |
| Pinned owner retirement | refused before payload destruction |
| Root still live but target missing/reused | root validity alone cannot admit the node |
| A deleted node's exact storage reused by a new node | the old link is refused; an address or block identity would admit it |
| A root slot released and reclaimed; a store id reissued | the previous handle and every link rooted in it are refused |
| A live external owner's block chosen for a new table, node or grown table | refuse before transition; the original graph state is unchanged |
| External footprint missing at whole-heap admission | `RNoFrame`; never substitute graph-only or empty-frame admission |
| Growth reusing only its own retired table with another owner live | admit the reuse; preserve the whole-heap split and the other owner's later value |
| Overlapping mutable views, grow or owner escape | admission preserves alias, address and lifetime obligations or refuses |
| Explicit and synthesized cleanup of the same root | exactly one consumption, never two independent drops |
| Long-lived owner repeatedly gaining isolated cycles | retention is visible; no claim of automatic intermediate recovery |

The access, reuse, allocation/frame and cleanup rows have bounded logical
audit witnesses (`one_canonical_owner_per_block`, `retirement_consumes_once`,
`framed_growth_retirement_executes`). The graph design model also checks its
alias/growth and retention cases, but these are not compiler admission or
execution evidence. Escape and all production refinements remain
obligations; documentation counts close none of them.
Graph source support, automatic cleanup and SoT/self-host substitution
remain OPEN until their named execution and negative gates run.
