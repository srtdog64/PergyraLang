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

No delegated edit scope is active during the main integration. Independent
review/proof outputs remain separate evidence, not a second semantic authority.

## Observations

- The inherited main worktree and Claude lane use different formal-effect and
  focus carrier layouts. They must not be combined by file timestamp.
- WSL was restarted before takeover: the handed-off remaining-target process
  is no longer running. Its log, not a running-job assumption, owns the result.
- Publication and CI-green claims remain OPEN.
- Shared WIP checkpoint: `78cc0ba23`. Merge resolution keeps the lane's coherent
  copy-edge/last-use API and final focus schema instead of the incompatible
  earlier typed-edge API; the earlier source remains recoverable in this
  checkpoint. The inherited native publication and typed Map query checks,
  indexed import membership, and both independent inout fixture sets are kept.
  The remaining non-overlapping WIP is included and must be validated too.
- A Windows mapped-file replacement refusal occurred while resolving one
  owner. Mechanical resolution now uses atomic replacement with previous
  conflict contents retained under `.tmp/p0-merge-recovery-20261016/`.
