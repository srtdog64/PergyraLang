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

## Current bounded native closure decision

The query-local DAG repair at `3dc597701` restores a real full-input verdict
within budget (10.10 s, exit1, peak597032 KiB); it does not admit the program.
The next complete failed demand is `SemanticAstBodyFlowOfBlock` parameter1:
132 summary evaluations +3496 backing nodes +468 graph nodes =4096. The
Block/Statement/If/Match/Loop SCC repeatedly analyzes local `flow` and `child`
whole-value replacements as if they could write an external output.

The existing typed-assignment/publication owner distinguishes an original
ordinary local binding replacement from a storage write. Produce the fact only
at successful `type_check_assignment` completion, after Slot/view sugar and
subject/borrow/assignability guards, with an exact local identifier binding.
Snapshot assignment/target/RHS identity, shape, binding and the existing typed
binding name; do not retain scope Symbols. The selector joins this fact to the
same original LET with `is_alias=false`. A bare identifier alone is insufficient:
`using` aliases and pin views can write through; Slot assignment has write sugar.
Keep formal/member/index writes conservative and traverse the full RHS. Seal
and replay the LET shape/alias flag and typed replacement evidence; missing or
stale facts cannot grant permission. Reuse the existing selector and index
claims, including their former scalar-local path, not a second owner.
The old heap-rebinding experiment was removed; this narrower decision requires
the measured demand above and behavioral alias/Slot/stale/RHS falsifiers.

The independent frozen review also identifies a reached caller-consumer gap:
`ref String` can bypass already-produced CALL/CHANNEL facts via broad COPY_ONLY
branches. B repairs the existing caller/summary/diagnostic consumer chain;
A owns typed publication/selector/claims. Preserve String value classification,
result loans and pending-to-finalized publication. Fix consumption, not source
annotations or blanket freshness. Pass the existing typed callable declaration
and parameter mode from the sole call-check consumer into the String boundary;
name lookup must not downgrade an indirect ref mode to default. Temporary
String arguments still register their exact call/site identity (a diagnostic
label is not ownership evidence); unknown/bodyless ref callees fail closed.
Existing literal-to-Slot/inout C/LLVM positives distinguish static String
storage from a dynamic borrow. Keep String CALL/CHANNEL summary/MAY facts and
apply them at the actual call boundary, not as an unconditional String formal
rejection. Only the current typed literal row (String, AST_STRING, copy-safe)
has the static-backing permission for a known body. Allocating String calls can
also have `copy_escape/origin_ready`; those bits alone do not transfer a ref's
cleanup responsibility. Preserve all previous literal runtime positives and
dynamic/record-field/Concat-temporary retaining negatives. Do not expand this
into general String provenance inference. A prior semantic error also prevents
constructor publication; retain its typed diagnostic rather than adding a
misleading missing-fact cascade to an already-refused program.
Scalar backing exclusion does not prove a
callee has no receiver/capture: exact local calls still enter the closed-body
graph, sealed builtin/stdlib identity is distinct, and opaque/indirect/member
targets remain conservative. Add direct owner-level negatives; CLI admission
alone is not evidence of an escape. Both repairs feed the same unchanged
canonical and original positive/negative/work-budget integration gate.

### Complete SCC census after `b9beae91e`

Scratch-only exhaustive traversal observes 55 exact reached routines and 2090
nodes, with 15 false premises and no incomplete row: ToInt input5, StringTrim
input1, three ArrayPush calls' receiver/value6, and empty literal3. Every
call-target/header and local-assignment premise passes. The original full
verdict remains work4096/exit1. Receipt: `a-scc-census.log`; source and binary
were restored byte-for-byte. Do not infer further causes from the error cascade.

Selected first slice, behind the existing owners, not separate implementation
tracks: exact ToInt/StringTrim input retention rows (not result freshness) and
current typed empty-literal purity. Measure the unchanged canonical with just
these facts before deciding whether private local array precision is necessary.
Empty literals alone contain no evaluated element or retained backing; keep
nonempty construction conservative, with changed-count/identity negatives.
ArrayPush retains its value and MUST NOT get a blanket no-retain row. Before
any private-array repair, resolve whether the current storage/exclusivity owner proves
the receiver has not escaped or acquired an alias. An EMPTY_LITERAL origin or
an effect receipt emitted after a diagnostic does not by itself prove this.
Preserve element-to-return/output origins and all CALL/CHANNEL/drop negatives.
Scopes: B existing retention registry/projection and conversion fixtures; C
selector/claim empty-literal branch and existing phase unit; A read-only local
storage/effect producer-to-last-consumer proof survey until its unknown is
resolved. Root integrates and runs the shared original canonical/work guards,
query C/LLVM and bootstrap/install gates. No larger work/time/memory budgets.
Private ArrayPush precision is explicitly DEFERRED until that measurement:
CALL suppression without carrying the inserted value's RETURN/output origins
is unsound, and current exclusivity flags alone do not prove nonretention.

The B/C-only measurement is now observed, not hypothetical: phase4105/0,
canonical exit1 in10.93s/621612KiB. A's exhaustive repeat has55 routines,
2090 nodes, exactly6 false premises (three ordinary ArrayPush calls, receiver
and value), incomplete0. Work4096 is58 evaluations+2404 backing+1634 graph;
ToInt/StringTrim/empty literals now pass. Receipts: `bc-canonical`,
`a-bc-demand`, `a-bc-census` in `/root/pgy-main-20261016.*.log`.
Select the private-local mutation slice only across the complete existing
publication -> selector/closed-call graph -> slot escape -> backing origins ->
returned/output relation -> evaluator chain. It must distinguish a checked
fresh local receiver from an alias/formal/escaped receiver, with exact current
call/binding/storage facts and the entire no-external-output graph. Do not
suppress CALL in isolation: inserted values returned through an array or a
nominal wrapper, stored in an output, or sent on a channel keep those effects.
Missing/stale evidence stays conservative. A owns this existing native chain;
no new fact family, broader builtin grant or source workaround is selected.

Independent b9 review also falsifies a separate last-consumer claim on this
same ownership path: RETURN-only ref String loans can reach caller-owned Slot
Write or inout String storage from a retiring local record. Ordinary read/log
loans and static literals must remain valid. Root and B first map an existing
owner repair: carry the exact returned loan's actual backing through locals
and wrappers, then discharge the final storage obligation after publication
and summaries are sealed. An empty formal-origin bitset is not proof of an
independent local lifetime. Avoid an all-local dense origin matrix on every
routine; settle the demand and lifetime representation before editing. A and
B may not concurrently edit shared publication/backing owners. Four frozen
analyze-only falsifiers and the valid controls are in
`.tmp/redteam-b9beae91e-20261016/`; no unsafe runtime execution is allowed.

B/C narrow acceptance: phase4105/0; summary behavior including both C/LLVM
reader/Trim-return runtime cases and original4096/4097 controls PASS10.18s;
retention registry/region behavior and component/generator checks PASS11.86s.
Trim-return is a valid independent-result path, not a fabricated negative.
The old region probe now distinguishes borrowed Write receiver from retained
payload and an unknown builtin. Self-host identity parity and the full
canonical/bootstrap/installed chain remain OPEN; these receipts do not close P0.

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
- Closure-blocking cost record: the uninstrumented `59af37fb6` implementation
  also ended INCOMPLETE_BUDGET (`timeout 300` child exit 124; wall 308.81 s,
  user 308.60 s, maximum RSS 596896 KiB, no compiler verdict). All seven native
  implementation/test blobs in the Linux build match the frozen commit; native
  binary SHA-256 is `27a98b788157828ef32186b9cf7089028263a8e8438864bf1c2bd448a9746225`.
  Input remains the full import-composed `src/self_hosted/codegen/main.pgy` at
  this tree, not a subset. Receipt: `/root/pgy-main-20261016.a-final-empty-canonical.log`.
  The blocked gate is full native admission within 300 s. The reached candidate
  operation is transitive backing/claim/publication current-validation; its hot
  attribution is not yet measured. Instrument only those existing owners in a
  scratch source, then change only the proven repeated operation. No global
  cache, relaxed safety, reduced input, new owner or increased budget. Recheck
  stale facts, shared diamonds/cycles, all escape controls, original demand
  limits and this same full-input gate before returning to bootstrap/install.
- Scratch attribution now confirms repeated current-validation of a shared DAG,
  not repeated visits required by a cycle: one root validates 722 routine calls
  for 20 unique routines (36.1x, cycle count zero, depth five). The early sample
  has 49152 backing validations over 258 boundaries, 50232 claim validations
  and 1836842 publication value reads; a common leaf is replayed 20560 times.
  Keep the repair local to one synchronous current-validation boundary: enqueue
  each exact routine once and still validate every reached fact/call/header.
  Discard the visited set on return, so a later query must detect changed facts.
  This is not a cross-query cache or an unchecked generation assumption.
- The later scratch sample reaches 9808 calls for 19 unique routines in one
  boundary (no semantic result was claimed; the verified own profile PID was
  stopped after 78.57 s). A implemented the local queue in the existing
  validation owner, removed the old recursion flag, and preserved all fact,
  header, call and argument checks. Root independently repeated its 3797-check
  unit, including shared-diamond/cycle leaf corruption and restored next-query
  controls: zero failures. A reports the uninstrumented full-input gate now
  returns a real refusal in 10.10 s (peak597032 KiB, exit1), rather than timing
  out; the next primary is the unchanged 4096-work guard at source line343.
  This removes the measured cost blocker, not the semantic refusal. Return to
  the original demand's complete dependency set; bootstrap/driver/CI stay OPEN.
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
