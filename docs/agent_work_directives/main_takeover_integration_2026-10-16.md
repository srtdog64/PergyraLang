# Main-only takeover and P0 integration

Status: ACTIVE; integration and acceptance are not yet established.
Date: 2026-10-16 KST. Integration owner: GPT in the main checkout.

## Objective and boundary

- Objective: preserve all inherited work, integrate the published performance
  changes and Claude's P0 lane into main, then verify the actual installed
  compiler chain on that integrated source before publication.
- Priority: recoverability, stable semantic identity, current owner evidence,
  preserved refusal behavior, installed artifacts, measured memory, exact-SHA CI.
- Inputs: shared main `af8ede6f0` with 231 tracked changes and 317 untracked
  paths; published main `571ae8c3a`; Claude lane `3721a901c` (including the
  unverified memory change `3cc0ecc47`). The clean lane's fixed-point receipt
  belongs to `b8e66a387`, not to the combined source.
- Fact owner: existing semantic ownership, focus-recovery and body-bundle
  owners. MIR producers and admitted consumers retain their existing identities.
- Last consumers: admitted C/LLVM codegen, gen2/gen3 fixed point, installed
  driver CLI, dependent compiler shards and the published implementation CI.
- Forbidden: throwing away inherited WIP, choosing an entire merge side without
  semantic review, native fallback for public acceptance, weakening a negative
  oracle, reduced canonical inputs, increased memory/time budgets, or claiming
  old receipts validate the integrated source.

## Complete execution order

1. Record inherited shared WIP as an explicitly unverified local checkpoint.
   Merge the committed Claude lane into main and reconcile overlapping schemas,
   consumers, generators and probes. Do not create another implementation branch.
   Retain recoverable Git objects before deleting merged branch names. Preserve
   worktree files and ignored artifacts; they are not acceptance evidence.
2. Read the actual remaining-job logs under `/root/pgy-ci` before reusing any
   result. Check input revision, dirty probe delta, executable and completion.
3. Survey the memory change's complete chain: per-event verdict constructors ->
   final focus producer -> body-bundle `collection_focus_recovery` -> readiness
   and receipt digest -> codegen view and all probes. Missing/stale evidence
   must still refuse; do not retain a parallel verdict-owned carrier fallback.
4. Validate the merged source narrowly first: native build, component checks,
   formal-key, field-read, carrier, round-trip and focus-recovery behavior.
   The previously red non-CI focus gate remains an explicit obligation, not an
   exemption. Distinguish C/LLVM-built probes from emitted target execution.
5. Measure full DRV-2 consumer peak and replay both source admissions, unchanged
   codegen bootstrap, installed-driver build and gen2/gen3 fixed point. Use the
   repository's limits; the scratch oversized ReadFile build is diagnostic only.
6. Run the remaining five self-host bootstrap targets and policy corpus, then
   Linux core/self-host and backend-compare shards on the integrated revision.
   Reconcile any newly reached owner with this chain before changing semantics.
7. Inspect the final diff, commit coherent repairs on main, publish main and
   verify required GitHub jobs at the exact published SHA. Skips are not passes.
   Update the active handoff with source/executable/receipt identity and any
   genuinely remaining obligations. Do not reopen unrelated SoT/emitter tracks.

## Falsifiers and budgets

Preserve corrupt/stale focus carriers, forwarded opaque calls, growth through
aliases, wrong-field restoration, old-holder writeback, missing MIR ownership
states and closed-arm/match round trips. Check stable admitted output as well
as actual refusal locations; an earlier censored refusal is not attack coverage.
Static owner checks: 60 seconds; focused parity: 300 seconds; integration shard:
1800 seconds. One heavy bootstrap/consumer run at a time in shared WSL.

The main merge is complete. The resumed three-implementer workflow now has
three non-overlapping scopes on the same main checkout and the same P0 chain:

- A: inherited native parameter-publication/flow owners and their behavioral
  controls. Determine the complete canonical refusal causes; preserve the 4096
  cap and setter/escape negatives. No source annotations or copies to bypass it.
  The measured selector also reaches the existing builtin-retention registry:
  The reached Option tag observation `IsSome` has no retained payload, but
  lacks a row. Extend this same owner, regenerate its Pergyra projection, and check
  exact typed stdlib identity/arity, forged identity, user shadowing and
  retained `Some` payload negatives. C and LLVM both inspect only the tag;
  neither the constructor nor payload extraction receives a no-retain grant.
  This is a reached dependency, not a parallel retention policy or name bypass.
  `IsNone` remains native-only and is not part of this correction.
- B: structural reconciliation, existing JSON row-owner placement and static
  gate machinery. Preserve real behavioral tests and remove inherited forbidden
  source/doc phrase assertions. No semantic admission edits or blanket cap raises.
- C: the existing focus-recovery behavioral gate and its reached producer/
  consumer obligations, including the known nominal-forward red. Preserve
  corruption/restoration controls; distinguish optional focus from a permission.

Root owns integration, main commits/publication, the full source/binary-bound
canonical/bootstrap/install/memory/CI gate and the active handoff. No agent
creates a branch/worktree, commits, pushes, kills a process, or runs a heavy
shared WSL bootstrap. Request the root's compile lease before a large probe.
The common acceptance remains the complete integrated compiler chain, not
three independent green reports. Unexpected cross-scope dependencies return
to root and this map before edits.

## Observations

- The inherited main worktree and Claude lane use different formal-effect and
  focus carrier layouts. They must not be combined by file timestamp.
- WSL was restarted before takeover: the handed-off remaining-target process
  is no longer running. Its log, not a running-job assumption, owns the result.
- Publication and CI-green claims remain OPEN.
- The bounded `IsSome` correction passes the native publication phase unit
  (3708 checks, zero failures, repeated independently by root) and generator drift. The full
  canonical still refuses: its first owned error moves from the 4096 demand
  cap to `parameter program-point index allocation failed`; the unchanged
  18408 total includes cascading publication/borrow failures. Diagnose actual
  index currency/allocation before choosing the next repair. The local heap
  rebinding experiment did not unblock the chain and was removed completely.
- Index failure classification now identifies a current, prepared index whose
  claims reject an unreachable constructor call after `Die`. The selector
  visited that dead statement even though the existing CFG/execution owner
  supplied no live call publication. The intended repair consumes the existing
  execution fact before descending a dead statement and snapshots its presence
  and reachability for current replay. Stale/missing execution evidence must not
  create a no-output permission; reachable opaque calls remain conservative.
  Paired live/dead calls and changed-deadness controls falsify this repair.
- The next fixed-demand census (same input/cap) is `EmitStmtList` parameter 5,
  `ref CodegenSemanticBodyTypeFacts`: 159 summary evaluations + 3776 backing
  nodes + 161 graph nodes = 4096. The reached pure type lookup pays for opaque
  scalar arguments and zero-argument local constant calls. Repair this facet
  coherently: use current physical-scalar argument evidence only to exclude
  carried backing (still visit the argument expression), and let the original
  publication identify an exact call-level local callee for the existing
  closed-body graph. No ordinal is invented for a zero-argument call. Typed
  identity, arity, binding and header currency remain mandatory. Preserve
  actual inout/heap-alias walks and separate RETURN/CALL/CHANNEL/result-origin
  facts. Falsifiers: retained heap setters, opaque/extern/indirect zero-argument
  targets, changed callee/body/type facts, and the unchanged work-budget cases.
- Root's clean Linux `b791c0b7a` observations: component contract PASS 11.45 s
  within 60 s; native summary behavior and unchanged 4096/4097 demand controls
  PASS 10.01 s, including the 3708-check publication unit. Receipts:
  `/root/pgy-main-20261016.component-joined.log` and
  `/root/pgy-main-20261016.native-summary-controls.log`. Focus recovery remains
  blocked before probe execution: `c-probe-compile`, not a restoration result;
  `/root/pgy-main-20261016.focus-main.log`. No integrated bootstrap, installed
  driver, memory or current GitHub CI acceptance yet.
- Native candidate checkpoint `59af37fb6` implements the sealed dead-statement,
  physical-value and original call-level graph corrections, without new owners
  or language syntax. Root independently repeated its publication unit: 3761
  checks, zero failures (0.02 s). An instrumented full-input run outlived the
  300-second budget and was terminated on its verified own PID at 306 s. That
  is INCOMPLETE_BUDGET, not successful admission. Remove instrumentation and
  measure the identical input with `timeout 300` before deciding whether any
  reached repeated operation is actually closure-blocking.
- Structural reconciliation: routine generic-constraint JSON projection moves
  unchanged from the aggregate serializer to the existing routine-parameter
  JSON owner (already imported by both consumers). Function identity and
  emitted bytes stay stable; there is no second projection or fallback.
  This keeps the aggregate owner below its unchanged 600-line ceiling. Small
  per-responsibility cap deltas below 600 cover the lane's actual owned-text
  copies and final bundle field, not compressed code or permission changes.
- Shared WIP checkpoint: `78cc0ba23`. Merge resolution keeps the lane's coherent
  copy-edge/last-use API and final focus schema instead of the incompatible
  earlier typed-edge API; the earlier source remains recoverable in this
  checkpoint. The inherited native publication and typed Map query checks,
  indexed import membership, and both independent inout fixture sets are kept.
  The remaining non-overlapping WIP is included and must be validated too.
- A Windows mapped-file replacement refusal occurred while resolving one
  owner. Mechanical resolution now uses atomic replacement with previous
  conflict contents retained under `.tmp/p0-merge-recovery-20261016/`.
