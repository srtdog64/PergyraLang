# Graph store policy, snapshots and cache review

Status: **BOUNDED REVIEW VERIFIED; implementation OPEN**.
Base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e` plus the existing
uncommitted graph model/consumer/publication slice. Date: 2026-10-09 KST.

Follow-up: the model batch boundary described below has since been repaired
and independently red-teamed in the
[atomic bridge implementation audit](graph_atomic_bridge_redteam_2026-10-09.md).
This review's original receipts remain historical; production integration and
the inventory/snapshot, issuer and cost obligations remain OPEN.

The user selected optional store-local cycle reclamation, not a replacement
for ordinary compiler-owned automatic cleanup. This review incorporates the
requested store snapshots, cache locality, and explicit deletion/store-end
baseline. The [original scoped review](graph_cycle_reclaim_review_2026-10-09.md)
remains historical evidence; this note does not upgrade its proof premises.
Requirements belong to [doc 28](../semantics/28_memory_boundary_composition.md),
ordinary cleanup to doc 27. The
[closure plan](../agent_work_directives/graph_store_integration_closure_plan_2026-10-09.md)
does not open a parallel compiler implementation rung.

## Decision and comparison

| Profile | What determines retirement | Costs and limits |
|---|---|---|
| Baseline graph lifecycle | authorized domain deletion; owner-end cleanup of the remainder | no cycle-collector count/trace bookkeeping; forgotten nodes retained until store end; teardown may burst |
| Optional store-local reclaim | complete observable roots, exact counts, bounded candidates and canonical whole-unit admission | metadata/edge updates, root evidence and reclamation work; deferred cycles may remain |
| Detached data snapshot | a new owner of independent copied data/topology | copying/peak-memory cost; stable source and atomic publication; no copied rights or shared mutable backing |

**Recommendation:** implement the baseline first. Fix optional-policy choice at
store construction, with exact counters from creation only for ON stores.
An OFF store must not pay hidden collector bookkeeping, although ordinary
identity, lease and teardown indexes still have their own costs. Immutable
detached snapshots can use owner-end cleanup without dynamic cycle counting.
This is a proposed implementation profile, not an existing public graph API.

Domain deletion is not manual memory housekeeping: it represents removing a
document node/cache entry. The compiler still owns cleanup of the store and
ordinary payloads. Requiring users to write recursive drops, field restoration
or root registration to make the mechanism safe would violate the selected DX
direction. Smaller meaningful store lifetimes can bound retention, but inventing
a public Slot for every value is not the solution.

## Three independent model reviews

- GPT-6 Astra: algorithm/research and snapshot semantics.
- GPT-6.1 Sol: actual compiler producers, backends and runtime consumers.
- GPT-5.6 Sol: authority, atomicity, finite identity/counters and observations.

All were read-only and received the later snapshot/cache/manual-deletion
questions. The main task independently read the relevant sources and compiled
two integration falsifiers. Model agreement is supporting review, not proof.
Their counter-policy suggestions differed: exact checked counters versus
sticky Unknown/saturation with later recount. The initial recommendation is
**exact checked counters**, matching `CountsExact`; Unknown would require a
new no-under-count and reset proof and must not be a silent fallback.

## What the current source establishes

- `OwnershipGraphLinks.v:g_delete` checks store, current slot generation and
  a node borrow before freeing its payload; `ODrop` checks store borrowing.
  `unreachable_nodes_retained` demonstrates an isolated cycle retained by a
  live store. These are model operations, not a deployed graph API.
- A generation check prevents an old link acting on a reused node. It does
  **not** make a semantically premature deletion correct: later use may return
  stale instead of the reference program's successful result. Cleanup rights
  and whole-unit descendant loan/pin admission belong to the canonical
  teardown authority and are not proved merely by `g_delete`.
- `StoreRootBindings` is an input map to a root/epoch. `ledger_rights` checks
  that root's actual holder, but no producer yet establishes that the root
  owns the requested graph store and its physical footprint. A valid right to
  root A cannot be treated as a right to arbitrary store B.
- `checked_ledger_reclaim_simulates` assumes `SimInv`, whose liveness comes
  from the small checked language. It is not a real MIR certificate issuer.
  Existing MIR DCE recomputes analysis after changes; this review does **not**
  report stale post-DCE analysis as an observed bug. The missing obligations
  are instruction checkpoints, frames, recursively typed link/view layouts
  and publication at calls/returns/errors. CFG cleanup roots are block
  indexes, not a graph reachability root inventory.
- The public `src/runtime/slot_pool.h` explicitly excludes tree/graph/smart-
  slot APIs until implementation and end-to-end tests close. Current native
  slot checks must not be presented as this graph authority implementation.

## Integration falsifier 1: sequential retirement is not atomic

Start with `gcyc`, borrow its second garbage candidate, then select `[2; 3]`.
Selection still succeeds. Existing `counted_grun` discards per-operation
results: the first `ODelete` succeeds and the second refuses `RBorrowed`.
The heap shrinks from five blocks to four and the first link becomes stale.

This is **outside** the checked-language theorem's empty-borrow-at-boundaries
premise, not a contradiction of that theorem and not a reproduced native
vulnerability. It falsifies using the sequential runner as a production
failure-atomic batch. Preflight the complete unit/batch, freeze relevant
epochs, perform a non-refusing commit, and publish reuse last. Refusal must
preserve storage, counts, indexes, identities and authority together.

Arbitrary graph garbage and a Qt ownership subtree are not the same unit.
Automatically adding a garbage parent's rooted/pinned child can over-delete;
deleting just the parent can violate forest ownership. The smallest proposed
first profile is **new flat-owned stores**, not silently flattening existing
Qt-style hierarchies. Later hierarchy needs disjoint exact units inside the
garbage set. A shared table/slab remains store-owned, never node-freed.

The two edge representations also need a real refinement: graph `g_delete`
leaves incoming identities stale, while canonical teardown `phase1` clears
incoming fields via its index. Treating the two post-states as literally equal
would be wrong. One canonical edge owner or an explicit observational mapping
must account for clearing, raw enumeration and duplicate-edge multiplicity;
two independently updated edge/index tables are not a solution.

## Integration falsifier 2: inventory is an observation

The model's pure `snapshot` copies all slots. Before reclaim its copy resolves
the isolated node at `(2, 0)` to data `3`; after reclaim the corresponding copy
resolves nothing. The reclaim language `RStmt` has no full snapshot, key
lookup or inventory enumeration statement. Its simulation therefore does not
prove those observations unchanged.

Consequences:

- A strong cache directory is a root: a later `get(key)` can find an entry even
  when no user variable holds a node link. Entry eviction is an explicit domain
  policy, not something inferred from absence of handles.
- A promised full-store snapshot/enumeration keeps otherwise discoverable
  inventory observable. Either represent that inventory in the root/read-
  footprint contract, or specify a distinct logical/weak surface. Do not
  silently change full-store copying into reachable-closure copying.
- An already published independent snapshot owns its copy; it need not keep
  the source alive. Shared persistent history would be a different lifetime
  and cleanup mechanism, not this independent snapshot contract.
- A collector evidence snapshot binds MIR/store/root/count/lease/authority
  generations. It is not a data copy. Stale evidence must be refused.
- Application-result cache keys need a content revision as well as identity:
  generation protects reuse, not freshness after an in-place data edit.

Current `snapshot_preserves_relative_edges` proves copied data/topology at
model level; `snapshot_copies_footprint` proves equal footprint lengths, not
allocator non-overlap, issuance or atomic publication of a second physical
store. Implementations need fresh storage/root/identity, admitted copyable
payloads, a stable source interval and partial-destination cleanup on failure.
Do not bit-copy Slot rights, leases, affine handles or escaping physical views.

## Research checked against primary sources

This is a targeted 2024-2026 selection, not a claim to cover every latest paper.

| Work | Relevant evidence | Decision for this project |
|---|---|---|
| [Arborescent GC, ISMM 2025](https://olimel.net/static/pdf/ismm25.pdf) | synchronous cycles using a maintained dynamic forest; reference replacement and incoming-index costs; its Ribbit experiment has about 4.5x median slowdown versus its mark-and-sweep (§5.2) | useful mutation/publication obligations; do not replace the current mechanism or treat that number as a Pergyra prediction |
| [Snapshottable Stores, ICFP 2024](https://iris-project.org/pdfs/2024-icfp-snapshottable-stores.pdf) | capture/restore with shared mutable history and record elision; Iris proof of persistent core without transactions; snapshots are not thread-safe (§1.2, §3) | motivates snapshot-frequency/changed-fraction cases; not independent immutable copying or adoption of hidden shared history |
| [Place Capability Graphs, OOPSLA 2025](https://pm.inf.ethz.ch/publications/GrannanBilyFialaGeerMedeirosMuellerSummers25.pdf) | analysis of Rust place capabilities across control flow | reference for typed producer/consumer mapping, not a reason to expose Rust lifetime syntax |
| [InvalML, OOPSLA 2025](https://cse.hkust.edu.hk/~parreaux/publication/oopsla25/) | type/effect tracking of invalidation with inference | informs view/growth invalidation cases; no second analysis engine introduced |
| [VerusBelt, PLDI 2026](https://iris-project.org/pdfs/2026-pldi-verusbelt.pdf) | semantic foundation with explicit layout/translation/erasure limitations (§6) | keep model proof, trusted producer and physical refinement separate |
| [Immutable cyclic RC, ISMM 2024](https://www.microsoft.com/en-us/research/publication/reference-counting-deeply-immutable-data-structures-with-cycles-an-intellectual-abstract/) | SCC treatment for deeply immutable cycles | immutability matters; no need to add RC when a snapshot owner already retires the whole store |
| [Iso, PLDI 2025](https://www.steveblackburn.org/pubs/papers/iso-pldi-2025.pdf) | request-private GC with mark-region layout and opportunistic copying | locality is not exclusive to ownership, and modern GC is not one uniform baseline |

The pasted comparison needs four qualifications. Ownership repairs can cross
API boundaries too; Rust lifetimes are often inferred/elided, not universally
hand-written ([official book](https://doc.rust-lang.org/book/ch10-03-lifetime-syntax.html)).
Trees may fit ownership naturally, but copies, retention and teardown bursts
can dominate cost. GC languages can provide deterministic explicit resource
cleanup ([Java AutoCloseable](https://docs.oracle.com/en/java/javase/25/docs/api/java.base/java/lang/AutoCloseable.html));
Pergyra's integrated authority contract is not an impossibility theorem about
GC. Graph/cache cost relative to GC requires the same workload and observations.

## Performance boundary and acceptance

CPU cache locality and application-cache semantics are different questions.
Packed nodes/edges and sequential copying may help locality; random edges,
sparse holes, large payloads, reverse indexes and count writes can offset it.
No native cache, throughput or tail-latency result was measured in this review.
The manual baseline can omit collector-specific work, not all identity/index/
allocator costs, and is **not proved fastest for every workload**.

At executable integration compare the same layout with policy OFF/ON, then
layout variants separately. Include short-lived AST/MIR-style trees, long-lived
cyclic churn, strong-cache hit/eviction, sparse stores, rare/frequent snapshots
and small/large changed fractions. Measure retained/peak memory, metadata,
copied bytes, edge-write work, actual visited nodes/edges and tail retirement
latency. Root analysis and store-end cleanup are part of the cost. `fuel`
counts rounds, not visits or a time bound. No general benchmark/optimization
track is opened before the named implementation boundary exists.

## Fresh evidence and remaining limits

- Independent focused gate: PASS; six models plus the independent consumer
  (seven kernel-checked modules), zero declared abstractions/admitted proofs.
  Original fresh receipt: `.tmp/graph-cycle-reclaim/run.EWgdZ7/`.
- Independent scratch falsifiers: PASS through the admitted Rocq 9.3.0 runner;
  four kernel-checked modules, zero declared abstractions/admitted proofs.
  Receipt: `.tmp/graph-cycle-reclaim/multimodel-batch-2026-10-09/kernel.log`.
  Probe SHA-256: `9f4df8a2c816a8e23f7069dba6728521abf86c59843120d7d0dfc4ff1574a544`.
- Durable consumer follow-up: PASS through the focused seven-module gate;
  receipt `.tmp/graph-cycle-reclaim/run.vkPNzl/`, zero declared abstractions,
  no admits/unsafe kernel features, bound sources unchanged at endpoints.
  The new consumer pins both integration cases and baseline retention. Its
  SHA-256 is `b8cd319c83c35146f4356cfaa47638a6a2ffc6184f8f72b1887620fe64ea2d6c`.
  Selection/root models remain byte-identical to the original scoped review.
- Expanded selftest: PASS, seven planted regressions refused for the expected
  reasons (missing original/batch/snapshot falsifiers, weakened theorem, new
  axiom, source drift and receipt overwrite).
  Receipt: `.tmp/graph-cycle-reclaim/selftest.WYblAA/`.
- Documentation-quality and evidence-lifetime gates: PASS. These are
  document/manifest checks, not production graph semantics. Shell syntax,
  strict UTF-8/local links of the new documents and `git diff --check` pass.
- Last inspected exact-HEAD remote run
  [37883531407](https://github.com/srtdog64/PergyraLang/actions/runs/37883531407)
  is RED (`backend-compare-toolchain-linux`). It does not validate the dirty
  graph slice. No full P0/memory matrix or installed-driver integration was
  run for this review, and no CI-green/production CLOSED claim is made.

Remaining: actual root and binding issuers; graph/forest/index refinement;
whole-batch admission/commit; finite-counter/identity refinement; physical
snapshot/failure paths; real work-budget receipts; C/LLVM/self-host source
integration. Concurrency, cross-store edges, weak/finalizer behavior and
escaping physical views are excluded from the initial proposed profile until
their contracts and falsifiers exist. Existing P1/DRV-2 remains the executable
compiler rung; this review must not be used to bypass it.
