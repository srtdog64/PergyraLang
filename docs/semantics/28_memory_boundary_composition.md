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
