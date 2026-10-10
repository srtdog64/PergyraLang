# Graph atomic bridge implementation

Status: **IMPLEMENTATION COMPLETE — SCOPED MODEL BRIDGE; production integration OPEN**.
Date: 2026-10-09 KST. Base: `main @ 85fff5aa339ae136ac0797c4a6a95a5285ada70e`.
Existing dirty graph/proof/publication work and `gmon.out` are preserved.

The user requested a whole-chain survey, conditional parallel implementation
and independent adversarial review. Three read-only surveys found that the
condition for a complete production graph implementation is not met. This
directive closes reached model-to-model batch obligations; it does not open
P2-P7, invent a public GraphStore ABI or replace the active P1/DRV-2 rung.

## Objective card

- Objective: make the graph selection boundary refuse the entire batch on any
  current-identity/borrow failure, and compose retirement with the canonical
  authority machine without a second cleanup ledger.
- Priority: one authoritative state, explicit failure and unchanged-state
  refusal; admitted success and authority preservation; negative evidence;
  cost remains unmeasured.
- Owners: `OwnershipGraphCycleReclaim.v` owns graph selection/count deltas;
  `OwnershipTeardownAuthority.v` owns cleanup rights, leases and canonical
  retirement. An importing canonical batch owner may compose that retirement,
  but cannot grant a right, bypass its admission or create another heap.
- Last consumers in this slice: `OwnershipGraphRootCompleteness.v` and the
  independent importing graph audit. Runtime/allocator/backend consumers are
  not yet connected.
- Forbidden fallback: ignored failed deletes, partial published state,
  arbitrary `authorized=true`, caller-chosen store/root bindings, native
  rollback of already freed memory, or calling this production CLOSED.
- Shared gate: fresh `tests/graph_cycle_reclaim_smoke.sh` and its negative
  selftest through the admitted Rocq runner, followed by documentation gates.
- Falsifiers: a borrowed last candidate after an earlier success; duplicate,
  stale or missing candidate; wrong holder/root epoch; borrowed/pinned member
  inside a canonical unit; consumed root right; omitted snapshot inventory.

## Surveyed end-to-end chain

`driver_run_pipeline` -> native/self-host semantic place and call admission ->
ordered normalization and joined call/output/origin/view facts -> final MIR
analysis -> native/self-host JSON -> C/LLVM and installed-driver consumers ->
physical storage preparation, authority admission, retirement and reuse.

The current source admits only direct-variable inout. Native LLVM does not
lower field inout as a value-result address; the C/self-host backends also own
argument-order materialization today. Existing expression graphs carry call
and binding identities, not final call recovery/view lifetime certificates.
Thus simply removing the field guard or accepting a runtime root list is not
a sound implementation of this chain. The complete P1 dependency map remains
in `ownership_cutover_execution_2026-10-09.md` and doc 27 section 5.10.

At the graph boundary the current `counted_grun` ignores results, while the
canonical forest clears incoming fields and the graph model retains stale
links. Graph/root bindings are supplied inputs, not production-issued facts.
The first repair therefore has an explicit model-only completion boundary.

## Independent edit scopes and dependency order

1. Graph batch lane: `OwnershipGraphCycleReclaim.v` and reached consumers in
   `OwnershipGraphRootCompleteness.v`. Replace reclaim's ignoring runner with
   an explicit checked atomic batch; preserve the general sequential runner
   only as its existing program semantics, not an alternative reclaim path.
   Check every delete succeeds with its exact current live identity; prove
   refused state equality, accepted sequential projection, count preservation
   and the existing empty-borrow simulation. Do not weaken a premise to make
   an unrelated state satisfy the old theorem.
2. Canonical batch lane: one new importing module under `semantics/proofs/`.
   Compose existing authorized `retire` operations, return the original full
   AuthorityState on any refusal, and prove accepted execution, invariant and
   root-right lifetime. Independent review discovered initially invalid units
   becoming valid after an earlier child deletion. The complete change set now
   also checks EVERY request against the ORIGINAL state, pairwise exact-unit
   disjointness and target uniqueness (including empty roots). Accepted
   execution requires that preflight AND the original supplied canonical
   sequence; no shrinking-unit repair. No authority/base-forest edits, mutation ABI, native
   callback or independently maintained graph heap.
3. Main: independent consumer/fixtures, focused gate/selftest registration,
   docs and active handoff. Review both lane diffs and recompile the combined
   modules independently. After implementation, assign different reviewers
   to the other lane; no agent certifies its own patch as final acceptance.

Private intermediate functional states are a proof representation. Native
retirement still requires a stable whole-batch preflight and a non-refusing,
allocation/callback-free commit; real frees cannot be rolled back. No worktree,
shared process-name kill, official binary replacement or Git publication.
Static checks budget 60 s, focused checks 5 min and an integration shard 30 min.
Author output is an implementation candidate until independent main gates and
cross-review pass. Review findings/results are in
`../audits/graph_atomic_bridge_redteam_2026-10-09.md`.

## Observed completion boundary

Parallel candidates, cross-author correction and independent cross-review are
complete for this model slice. Main's fresh eight-module gate and eleven
planted-regression controls pass. The whole formal integration also passes:
74 model modules, nine independent default consumers and the separate
assumption-approval consumer; the two existing approved Slot abstractions
are unchanged. Documentation quality and evidence-lifetime checks pass.
No executable C path was replaced, no source-level graph API was introduced,
and no compiler/installed-driver/CI closure follows. The remaining production
obligations below remain the continuation boundary, not completed work.

## Remaining production decisions, not invented facts

- Source GraphStore/GraphLink and operation identity, type/layout/resource ABI.
- Issued store/root/context and physical node/slab bindings; copied links must
  not acquire cleanup responsibility or write exclusivity.
- Canonical authorized field mutation and reverse-index projection; baseline
  clear-on-retire differs from raw graph stale-edge observations.
- Compiler-issued final call, derived/returned-view and complete root facts.
- Payload-specific clone/drop/layout contracts and physical snapshot failure
  cleanup; full inventory semantics cannot become reachable-only copying.
- Exact-width counters, permanent exhausted identities, physical allocator
  footprints, installed-driver evidence and exact-SHA green CI.

Flat ownership and trivial payload are possible *admitted first profiles*,
not implicit restrictions on the language. They require source admission and
negative gates before native implementation can claim that profile.
