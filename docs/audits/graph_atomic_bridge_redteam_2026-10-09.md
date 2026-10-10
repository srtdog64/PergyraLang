# Graph atomic bridge: implementation and independent red-team

Status: **SCOPED MODEL IMPLEMENTATION VERIFIED; production integration OPEN**.
Date: 2026-10-09 KST. Observed HEAD: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`.
This report describes uncommitted model/consumer/gate changes, not a new
compiler checkpoint, installed driver or CLOSED SoT row.

The user requested a complete skeleton survey before conditional parallel
implementation, followed by aggressive independent review. Surveys by
GPT-6 Astra, GPT-6.1 Sol and GPT-5.6 Sol found unresolved source/physical
contracts. The conditional whole-production implementation was therefore not
opened. Independent editing was limited to the reached graph/canonical batch
model boundary under the
[directive](../agent_work_directives/graph_atomic_bridge_implementation_2026-10-09.md).
The active executable compiler rung remains ownership-cutover P1/DRV-2.

## Whole-chain readiness, checked against source

| Boundary | Current owner or route | Remaining obligation |
|---|---|---|
| Production entry | `src/compiler/driver_app.c`, `driver_run_pipeline` | preserve the actual source route and admitted snapshot |
| Source calls and places | native `type_checker_helpers_late.c`, `type_checker_ownership_call.c`; self-host `ast_expression_storage_place_identity_owner.pgy`, `ast_collection_member_place_owner.pgy`, `ast_inout_argument_alias_verdict_owner.pgy` | field/index inout, evaluated index identity, ordered temporary/alias facts, returned/derived view dependencies |
| Final MIR carrier | `mir_types.h`, `mir.c`, SSA/use/liveness owners; doc 27 section 5.10 | actual joined call/output/origin/view producer and post-normalization generation |
| Persisted facts | native `mir_json_expression_graph.c`; self-host expression-graph projection/read owners | carry and strictly admit the same final place/recovery/lifetime facts; current graph fields do not carry them |
| Last emission consumers | native C user-call emitter; LLVM boundary-call emitter; self-host call-order/emission and installed direct-MIR routes | consume admitted facts instead of deciding addresses/order/restoration separately |
| Canonical retirement | `OwnershipTeardownAuthority.v`, new `OwnershipTeardownAtomicBatch.v` | model batch is checked; authenticated store binding, graph/forest projection and physical commit remain absent |
| Physical storage | runtime allocator, current inline Slot ABI and separate SlotManager | actual graph node/table/slab layout, authority issuer, payload glue, finite identity/counters and stable whole-unit admission |

The generated plain Slot ABI is `{value, occupied}`. Separate SlotManager
generation/pin state is not the compiler's graph ownership implementation.
No GraphStore source type, runtime operation row, snapshot clone/drop glue or
public graph API was fabricated by this patch. A copied context ID, token or
MIR snapshot number is not an issued affine cleanup responsibility.

## Implemented model boundary

### Graph selection and counted deletion

`counted_reclaim` no longer uses the result-ignoring `counted_grun` as its
execution path. It checks the raw current candidate inventory, selects, and
calls `counted_delete_batch`. `CountedBatchAccepted` and `CountedBatchRefused`
distinguish duplicate/stale/missing identities, active borrow, missing store,
selection deferral, denied ledger authority and incomplete checked roots.

Every accepted deletion actually returns `GUnit`; success provides `NoDup`
and the full admitted sequence. Any refusal returns the exact original
`CountedGraph`, including its count function and all graph state. Successful
execution preserves exact counts under `CountsExact` and `AllEdgesIssued`.
Ledger/checked-root consumers propagate that explicit result; they do not
project refusal onto an old runner or turn it into silent success.

The reference projection is **accepted-only**. For example, duplicated raw
candidates can be refused where the old reference selects an empty set. The
existing whole checked-language simulation still has its own empty-borrow
premise; it is not equivalence for every call to this stronger boundary.

Raw candidate indices intentionally name the current slot incarnation. An
old generation-bearing Link is a different contract and is refused. A future
deferred candidate queue needs an issued generation/epoch; this function does
not authenticate one. Exact counts are maintained-invariant premises, not
rechecked by recounting at every call. A fabricated zero count violates those
premises and can select a reachable node. The producer remains OPEN.

### Canonical authorized batch

The new importing owner uses the existing canonical `retire`, forest and
cleanup ledger. It introduces no second heap, authority table or lease issuer.
All requests are checked against one **original AuthorityState**, with exact
units, current identity, holder and lease quietness. It separately checks
retirement-unit intersections and repeated target identities, including empty
root units. The public entrypoint cannot skip this original-state check.

Success requires both original-state `InitialBatchAdmitted` and a real
canonical admitted sequence using every supplied request. Under the original
forest invariant, every supplied unit is the original `UnitExact`; units are
pairwise disjoint and targets have `NoDup`. Node-only batches retain the
root cleanup right. Later root cleanup consumes it once. Refusal preserves
the complete original authority state and the canonical reason.

## Independent adversarial review and correction

GPT-5.6 Sol found a model-level high-priority flaw in the initial canonical
candidate (`b5e3e81e...`): checking only evolving intermediate states allowed
a child delete to make an initially incomplete parent unit admissible. Removing
all nodes similarly made an initially invalid empty-root unit admissible.
The initial audit's full-unit overlap cases did not catch those shrinking
paths. Concrete probes compiled and kernel-checked with zero assumptions.

The whole boundary, not those call sites, was corrected by GPT-6.1 Sol:
original-state checks, all-pairs unit exclusion and target uniqueness were
added. Both shrinking probes now return `InvalidUnit` with the exact original
state. A successful nonempty node batch still executes and retains its root
right. The main consumer pins the general initial-state propositions and
those concrete regressions. GPT-5.6 Sol independently re-reviewed and reran
the probes after correction; no remaining blocker was found in this scoped
model contract.

GPT-6 Astra independently reviewed the other lane's graph batch. It verified
late-borrow, wrong-store and mixed-generation refusal, all-success admission
and exact state return. It also caught an audit expectation error: an already
closed isolated cycle can be reclaimed with zero closure rounds. The audit
now checks that success and uses `chain_store` for genuine zero-round deferral.
`fuel` remains rounds, not work/latency. No performance result is inferred.

## Fresh verification

- Main integration: admitted Rocq 9.3.0 / Stdlib 9.2.0, fresh **8-module kernel
  PASS**, zero declared abstractions/admitted proofs/unsafe kernel features.
  Receipt: `.tmp/graph-cycle-reclaim/run.lAmOuT/`; source hashes are checked
  before and after. Existing nested-list automation warnings remain visible.
- Graph independent probe: fresh four-module kernel PASS, zero assumptions,
  `.tmp/graph-cycle-reclaim/adversarial-astra-atomic/AtomicGraphReview.v`.
- Canonical independent probes: fresh four-module kernel PASS, zero
  assumptions, `.tmp/graph-cycle-reclaim/adversarial-sol56-atomic/AtomicBatchAdversarial.v`.
  Corrected probe SHA-256:
  `9c718d7addc50bd478d84d25db90f6db930b918287a7f6f84ed8e02ef6f6adea`.
- Bound corrected canonical model SHA-256:
  `c875fb77be12c02853aff73263edbafefffd38029ddbf9c8a2a861b79df0c3f1`.
  Graph selection model:
  `5dfc3c87c12254635c106df7a31b52a1011dd0c76cf40320a1c0673d67abd751`.
  Root/ledger consumer:
  `4efacfcf72d6a1a0bcf2f430859e42cdb819fd37258a5b9ca82c2cb2fa7deb49`.
- Expanded planted-regression selftest: **11 negative controls PASS** at
  `.tmp/graph-cycle-reclaim/selftest.Ieu9v0/`. Besides the existing seven
  receipt/root/snapshot/assumption controls, it rejects an omitted corrected
  late-borrow regression, an omitted shrinking-unit regression, a weakened
  canonical refusal theorem and a graph wrapper publishing intermediate
  refused state. Final documentation/full-corpus integration is checked
  separately; focused success alone does not establish that result.
- Documentation quality, evidence-lifetime enum/manifest correspondence
  including its negative self-test, shell syntax and `git diff --check` pass.
  The three corrected model hashes still match the focused receipt.
- Fresh full `tests/formal_semantics_smoke.sh` through the admitted runner:
  **PASS**, 74 model modules plus nine independent default consumers
  (83 compiled/kernel-checked), with the separate approval export/binding
  consumer also kernel-checked. Its assumption budget is the two existing
  approved Slot abstractions, not zero. Deprecation, nested-list automation
  and approval load-path warnings remain visible. The concurrent action-scope
  model is in that fixed snapshot; its separate importing action-scope audit
  is not in the kernel gate's default consumer set. This is not full P0 or
  installed-driver verification.
- Read-only Actions refresh confirms exact-HEAD run
  [37883531407](https://github.com/srtdog64/PergyraLang/actions/runs/37883531407)
  completed RED at `backend-compare-toolchain-linux`. It does not validate
  these dirty models. No commit, push, official binary replacement, native
  graph test, installed-driver parity, P0 matrix or performance run occurred.

The first author's root-level scratch was preserved, not deleted, under
`.tmp/graph-cycle-reclaim/canonical-author-initial-2026-10-09/`. Existing
dirty work and `gmon.out` were not overwritten. No shared process-name kill,
worktree or AGENTS.md rule was introduced. Production `src/` and `bin/`
remain unchanged in this scope.

## What this does not close

Functional private staging can discard intermediate values. Actual freed
memory cannot be rolled back. The initial check currently evaluates pure
canonical `retire`; it is **not** a side-effect-free native preflight. A
non-refusing native commit under stable whole-batch admission is still OPEN.

The graph and canonical batches are individually admitted; a proof that they
retire the same physically issued units is still missing. Authenticated
store/root bindings, canonical field/index mutation, final MIR roots and
call/view facts, full inventory/snapshot observations, payload glue, finite
counters/identity exhaustion and actual allocator correspondence remain
OPEN. Whole-inventory copying cannot become reachable-only copying, and
strong directory/cache entries stay observable until domain removal.

Thus the next source-integrated work still follows doc 27's P1 producer and
consumer chain. A new native substrate or green proof count cannot replace it.
No whole-language, automatic-cleanup, native-atomicity or superiority-over-GC
claim follows from this bounded implementation.
