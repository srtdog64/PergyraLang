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

### Measured next blocking operation, not a new precision track

The private-local candidate's first full run (clone47d+12 dirty paths,
native SHA256 `df956fdaa406efd25ab9df6b8e003a29d73c0e7337b620b8bcf8b3e0756681e4`)
fails10.44s/719984KiB. Its corrected phase unit passes4161/0; the original
BodyFlow closure now has55 routines/2090 nodes/false0, all3 private calls
issued/current. The new first failed demand is
`SemanticAstCollectionMemberCaptureBinding` param1 of type
`SemanticAstFunctionSignatureFacts`, not the previous BodyFlow demand.
Work4096 =120 evaluations+1696 backing+2280 graph. One top-level demand
rechecks the same three failed no-store closures40 times each:
Binding2 nodes, Provenance34, Subject21; unique graph routines35.
Receipts: `private-canonical`, `private-unit-second`, A's private census logs.

Selected cost repair: behind the existing no-store selector, remember only a
failed selection for the exact routine within the same immutable top-level
summary demand. This is not a new permission or a cross-query cache. A failed
selection still requires the existing conservative backing analysis; do not
skip current evidence or suppress any effect. A new demand must retry, and
stale/missing/escape/work-budget negatives remain unchanged. Do not broaden
ArraySet/Pop, nonempty-literal or indirect-call permissions to avoid measuring
this repetition. B's queued single-seed String-loan consumer is sequentially
integrated into the same existing backing owner; source windows do not overlap.

The integrated A/B candidate builds without warnings after root's const-owner
and diagnostic-site repair. Root observed phase4218/0, summary runtime/negative
and unchanged demand controls PASS8.24s, and readonly-map24/58 PASS.
Four returned-String Slot/inout falsifiers now refuse before MIR; literal/read
controls and C/LLVM private-local read remain valid. First full input remains
RED (11.43s/719788KiB, first diagnostic line218/work4096). Its independent
failed-demand inventory is the next investigation, not more local exemptions.
`production-c-size-test-smoke` ran its host counter/cap controls but stops at
the unavailable installed self-host driver; it is OPEN, not passed or replaced
with a native fallback. Local stable Rocq prefix is unavailable too; the actual
published proof job must execute before any full-CI claim.

Sealed-input isolated inventory is complete:3188 unique routine/ordinal keys
from4273 routines/8851 pending rows;692 cold failures across151 routines,
all work4096, incomplete0, elapsed27.865s. Scratch exits86 before MIR.
Receipt `a-inventory.log`; this is not the warm production verdict. Further
telemetry disproves a40-round oscillation: MemberCapture is8 parameters times
5 normal convergence rounds. False-selection reuse admits that demand.

The current warm BodyFlow failure is now measured:85 evaluations+280 backing
+3731 graph=4096. Same55-member closed set is reconstructed65 times
(3575 visits), distinct membership sets1; `a-positive.log`. Selected next
repair is an admitted closed-set proof within one top-level summary demand,
behind the existing index/graph owner. Reuse must recheck every proof member's
current header, roots/claims, false mutable-output selector and exact typed
outgoing call facts; missing/stale members refuse. Do not merely stop charging
the existing repeated traversal. Construct/expand the graph once, then consume
its validated proof without rebuilding its queue. No positive reuse outside
the solver or across demands, no weaker work/time budget, no new cache owner,
and no ArraySet/Pop or indirect-call permission. Preserve same-query leaf
corruption/refusal, next-query restoration, original unique-work limits and all
behavioral escape gates before retrying the unchanged canonical.

Root integrated that proof and the independent2800 review's opaque-inout
falsifier. A local's original literal is not static permission after an unknown
call can replace its binding. The existing publication owner now joins exact
current String value evidence to a registered synchronous const/copy input;
nonretention alone is not a no-mutation fact. Missing/bodyless/indirect/member
mutation facts refuse, while known user callees still consume current formal
mode and WRITE summaries. Four permanent analyze-only controls cover opaque
Slot/inout sinks and direct/bodyful mutation; a new read-only static-local
positive executes identically on C and LLVM. No dangerous negative is run.

Root observations on clone27d plus14 frozen candidate paths: native build
warning-free; phase4245/0; summary including original4096/4097 and recursive
controls PASS8.68s; readonly-map24/58 and HIR/RIR/MIR/AIR propagation PASS19.38s;
component/generator and retention/region controls PASS10.50s. Receipts:
`proof-build`, `proof-unit`, `proof-summary`, `proof-chain`, `proof-component`.
Same full canonical is still RED11.02s/720000KiB/18410 errors/16 warnings;
its primary moved from line218 to line71, work4096. This is not V1 closure.
A has the sole WSL lease for one scratch warm-demand measurement, then must
restore all source/objects/native bytes. Root holds publication/bootstrap;
the next action is the reached failure's owner/cost, not another permission.

The next warm primary is `EmitStmtList` param5 at
`CodegenSemanticTaggedEnumVariantTagFromGraph`:4096=183 evaluations+3475 backing
+438 graph, with respective unique counts183/2566/275 (3024 total).
`RuntimeCallCName` repeats679 backing nodes in epochs1/2. Closed-set proof
reuse is observed22 times, current replay85, refusal0, union147; this is the
old cold key2669, not evidence for another builtin exemption. The next bounded
repair separates final output/expression-cache bit growth from changes to
the actual iterative inputs, local binding origins/destinations. Keep OR
merges, binding-triggered convergence, recursive pending/building and pass
currency unchanged. Final effects/outputs are not inputs to expression
evaluation. Falsifiers must include late binding growth/loops, output-only
growth, recursive pending, all escapes and the original demand controls.

Adjacent alias investigation also reaches the same String mutation owner:
the `dd3d42418` native admits `using held as alias; Fill(alias)` followed by
storing `Echo(held)`. C's alias emitter substitutes the original place; a new
alias declaration ID is not a new mutation authority. Repair by resolving
only admitted/current LET `local_alias` claims to their original storage root,
on both source and mutation comparisons. Ordinary value-copy declarations
stop the walk and stay separate. Missing/stale/cyclic alias facts fail closed;
do not infer pin/view-call authority or introduce a name lookup/new owner.
Root analyze-only receipt `/root/pgy-alias-dd3d42418-20261016.glo2BE`: using
alias rc0/MIR30239 (falsifier), ordinary copy rc0/MIR30237 (positive), field
alias rc1/MIR0 (existing conservative boundary). B edits this facet first;
A's convergence edit waits for B's source freeze. C owns paired fixtures;
root owns stale-fact/whole-chain execution. Unsafe extern inputs are never run.

Root repeated the combined alias/convergence implementation: phase4285/0,
including alias identity/flag/initializer corruption and restored-query checks;
output-only, loop-backward origins and a legal nested using-output loop preserve
output/CALL relations. The proposed Array loop was rejected by the existing
tracked-shallow-assignment guard and was replaced with the legal alias case,
not made admissible. Summary/recursive/4096/4097 controls PASS8.60s;
readonly-map24/58 plus HIR/RIR/MIR/AIR PASS21.76s; component/registry PASS9.74s.
Receipts: `binding-unit-final`, `binding-summary-final`, `binding-chain`,
`binding-component`. Full canonical still RED11.11s/719860KiB/18410 errors/16
warnings, primary now line9 rather than71. Receipt `binding-canonical`.
These are coherent repairs, not bootstrap acceptance. Next is the same3188-key
cold inventory delta plus one warm measurement to classify the remaining set;
do not extrapolate completion from the first diagnostic's movement.

Updated isolated census on4f19 uses exactly the same3188
(routine name,declaration ID,ordinal) keys:692->515 failures,151->121 routines,
177 resolved, no new failures, incomplete0. All remaining failures are work4096.
Body45.531s/process54.62s, diagnostic exit86. Warm root remains EmitStmtList
param5 at RewriteSemanticHashMapCall:219 evaluations+3427 backing+450 graph;
category-unique3736, but unvisited dependencies remain unknown. RuntimeCallCName
now uses679 nodes in one epoch. Source/object/binary/dirty snapshot restored.
This is failure-set inventory, not admission or hard substitution progress.

Independent frozen4f19 review found no new bypass in its narrow scope. Preserve
its two-level alias mutation/source/assignment falsifiers as permanent
analyze-only negatives, not runtime tests. Root repeated the enlarged summary,
recursive and original demand controls: PASS12.23s, phase4285/0.
Receipt `alias-review-controls`; reviewer raw evidence remains in
`.tmp/redteam-4f19d8760-20261016/`. Neither result approves the full canonical.

The next unknown is the scope of the reached backing operation: its API asks
for one ordinal's effects/output row, but initializes all formal origins and
demands callees for any origin. The last consumer copies only the requested
row into the existing summary entry. Measure actual work initiated by other
origin bits before selecting a representation change. Do not simply delete
seeds or publish an uncomputed row as ready. Deferred activation would need
explicit per-row currency, recursive activation/pending and output-axis
preservation. A diagnostic observation may classify this cost; it must not
change the algorithm, work cap or admission. No implementation is selected.

The observation falsifies a narrower assumption: the first EmitStmtList
episode hits4096 inside callees before its own backing CALL traversal, so
there are0 direct root-fact requests to attribute. Within reached nested
facts, requests without their own requested-origin bit consume650 work
(305 calls), versus62 with it (330 calls). The observed712 is only the
instrumented callee-request span sum, not the whole4096 or a saving forecast;
inclusive/exclusive totals agree and nested duplicate contribution is0.
One absent-bit request, RewriteSemanticMemberCall input1 ->
RewriteSemanticArraySliceMemberCall argument4, consumes583. Observation only,
11.78s/exit89 before MIR; source/objects/native/dirty10 restored exactly.
Receipt `a-formal-attribution`; the root and nested scopes must not be conflated.

Candidate closure objective: align the existing returned-backing producer's
query identity with its existing summary entry `(function ID,input ordinal)`.
Priority: current typed identity, computed-row readiness, all output destinations,
recursive pending and lifetime, unchanged negative/work gates, then cost.
Current chain: evaluate one entry -> eager all-input backing matrix -> select
one effects/output row -> entry.output_dependencies -> call output query and
sealed summaries. The existing summary entry is the last row consumer.
Investigate an ordinal-owned backing frame inside the existing index, with
one input origin and the complete ref/inout destination axis, not a general
query engine or new semantic owner. Map every creation/current/read/destroy
site and direct unit consumer before implementation. Unknown: exact frame
lifetime and recursive cross-input currency. An uncreated/uncomputed frame
is not a ready empty result; no old matrix fallback or cap increase is allowed.
Preserve >64 output columns, zero-formal loan queries, cross-input recursion,
private-store mode, loop propagation, alias/Slot/channel/return escapes and
the original4096/4097 controls. A must return the complete map/plan first;
root has not authorized production edits or another diagnostic run.

The whole-chain review is now complete and root authorized this bounded
implementation. Current -> intended ownership: one eager function matrix ->
ordinal-owned lazy frames in the same program-point index; published identity
stays `(function ID,input ordinal)` at the existing SummaryEntry. Each frame
has origin_count/words1 and the full destination_count/wordsN/ceil(N/64), exact
input ordinal/ID, and independent building/complete/pending/pass/mode/epoch.
All formal bindings exist; only the requested one has an origin bit. Keep
every ref/inout destination, including columns64+; do not compute other input
rows or interpret their absence as zero. Matrix extraction is removed, not
retained as a compatibility read.

Independent edit scopes of this one chain: A owns internal.h, index producer,
single-input evaluation/B loan1x1 and summary producer/cache/output consumers;
B owns structural current-validation and frame/vector destruction in existing
store_destroy; C owns high-output65 literal/dynamic behavioral fixtures; root
owns direct phase-unit migration/faults and the single integration gate.
An internal output_producer receipt distinguishes BACKING_FRAME from the
existing CLOSED_SELECTOR proof. UNPROVED is not a ready empty row. Frame-ready
consumption requires its exact current input identity and completed nonpending
frame; selector-ready consumption independently replays its admitted no-output
evidence. Structural currency alone must not require completed states during
SCC reentry. No missing-frame/old-matrix fallback. The existing entry owns the
projected result; all frame allocations remain context-owned until destruction,
then shared claims are freed once by the index. No new fact owner or syntax.

All three implementers froze their assigned files without building. Root is
integrating the existing68-parameter cross-word unit, input-order isolation,
uncreated/wrong-input frame and producer faults, loop/alias/recursive/private
controls, and new output65 literal/dynamic analyze-only pair.
First gate is native build+phase; then summary/unchanged4096/4097, full unchanged
canonical and measured cost, IR/Map/component/registry, independent frozen
review, and the original bootstrap/install/fixed-point/memory/CI chain. The
actual effect on execution cost, total refusals and memory is still unknown.

Root integrated this candidate on clone3f428ce86 with nine source/test/directive
overlays. Native build PASS3.48s; phase4342/0 and summary (including unchanged
independent4097/oversized4096 controls) PASS12.73s; Map24 runtime positives/58
analyze-only negatives and HIR/RIR/MIR/AIR PASS20.80s; component/registry
PASS10.54s. Full unchanged canonical is still RED8.01s, peak719980KiB, exit1,
18410 cascading errors/16 warnings, first work4096 at line110. These results
do not establish canonical admission, bootstrap, driver, memory or CI green.
Receipts: `input-frame-build`, `input-frame-summary-scoped-final`,
`input-frame-chain`, `input-frame-component`, `input-frame-canonical`.

The first integration run caught completed-output validation accumulating
unrelated finished demands' graph work. The existing graph owner now charges
a fresh synchronous outside-solver validation to a local counter with the
same4096 bound; inside the solver the original demand counter is unchanged.
Two-member/one-unit outside-validation refusal and restored-budget controls
pass, alongside the original4097 independent and4096 single-closure tests.
No budget increase, uncharged graph walk or ready-result fallback was added.
The output65 external pair is MIR-only: the native C emitter's pre-existing
64-argument ceiling prevents runtime execution. Existing68-parameter API
coverage and all previously registered C/LLVM runtime controls are preserved.

Next diagnostic scope, not a selected production change: the named escape
consumer in `slot_analyzer_summary.c` asks for one slot_name, while
`slot_escape_mask_in_program` builds the all-name escape map and selects that
row afterwards. Its AST_CALL path demands every eligible argument-root callee.
The all-name consumer in `slot_analyzer.c` must retain full collection; named
access in `slot_analyzer_access.c` already filters roots. Measure this base
escape request work on the same input before choosing a repair. Classify
exact binding, different binding, same-name shadow, alias candidate and missing
identity, preserve typed argument checks/nested traversal, and retain the
existing formal/backing/return/channel chain. Source repetition alone is not
proof of savings. Repeat the3188-key census and warm attribution; require a
coherent map and falsifiers before another production edit.

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
