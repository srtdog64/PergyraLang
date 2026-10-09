# Current Work Handoff

Updated: 2026-10-09 KST (Asia/Seoul). Navigation only. Current source, the
SoT registry, admitted contracts, and executable gates override this note.

## Active self-host context — ownership-cutover P1 (GPT, 2026-10-09)

Latest user instruction reopens the whole automatic ownership transition;
all other writers are stopped. One shared tree, no additional implementation
lane. Active directive:
`agent_work_directives/ownership_cutover_execution_2026-10-09.md`;
normative owner: `semantics/27_ownership_clean.md`; complete dependency/deletion
plan: `agent_work_directives/ownership_cutover_plan_2026-10-08.md`.

Frozen parent checkpoint is
`5651c916c87e030cc3ef579180abdfbf1814d1cc` on main. The same-folder local
candidate is `codex/ownership-clean-cutover-2026-10-09`; no worktree was made.
Its checkpoint contains the subsequent production resource-flow repair,
importing direct-control model/gates and documentation. Resolve its exact
HEAD with Git on resume and inspect its diff from the frozen parent;
`gmon.out` remains excluded. Candidate checkpointing is not main landing or
P1/full-cutover completion.

Validated implementation checkpoint:
`5ca69b4fffcc56f1e4103e914b8c041efb1aa027`. Tracked/index diff stayed zero
while the entire checkpoint input was measured: exit 0, 88889 ms, sampled
private 1319.8 MiB (1.289 GiB), complete capture and no endpoint binding
changes. The full MIR hash remains `52290d05...985b402`. Receipt:
`checkpoint-memory/checkpoint-full-driver-mir.summary.json` below the log root.
The subsequent handoff/audit commit records this result; it changes navigation
only, not compiler/proof/gate input behavior. Do not relabel the result as a
measurement of a later all-input checkpoint or a complete P0 matrix.
The checkpoint's complete tracked inputs stayed frozen through the reached
baseline gates. Full P0 is not green: full driver refused 3072 MiB; likeness
and include caps are red; component gate exceeded 60 s; the same-source
isolated self-host driver is missing. No installed/fixed-point/remote CI
completion is inferred from unit or model gates.

Delivered source chain: `driver_run_pipeline` -> semantic value declaration
admission/snapshot/nested/seal producers -> existing ResourceFlowUniverse
identity owner -> unchanged HIR/RIR/MIR carriers -> MIR validation/JSON.
The owner excludes only CLASS/TYPE_PARAM and validates retained memo hints
against its existing declaration matcher. Missing owner fails closed;
no downstream filtering, second identity table, copy or annotation increase.
This removes the measured obstruction, not a real C-path substitution or
CLOSED SoT row. Registry status remains unchanged.

Actual isolated production compiler SHA-256
`262768f6...919b0` emits the SAME complete driver MIR under the unchanged cap:
exit 0, 91047 ms, sampled private 1319.5 MiB (1.289 GiB), 10,561 declared
bindings unchanged. Artifact 378143208 bytes, SHA-256
`52290d05...985b402`, byte-identical to the independent scratch repair.
The last new semantic parsed-source case and proof/docs edits followed that
receipt, so it is not yet a future commit's all-input receipt. Heap counters
remain UNMEASURED. Semantic 3191/0, HIR/DIR/RIR/AIR/MIR units, DIR/RIR/loop
identity and native-only ArrayDrop/clock/scalar C/LLVM gates pass. Include
caps remain red on unchanged 702/785/706 LOC fixtures (cap 699).

Adopted P1 implementation: direct recovery epilogue, not status-guarded
production code; compiler-local writable Slice uses whole-backing static
focus, not the resource/graph lease ledger. Actual lexer/AST census of all
2531 self-host files: 76 Slice constructions, all immediate call arguments;
32 default-mode Slice formals; no local construction or Slice return. This
does not prove transitive readonly effects. Read-oriented call lifetimes
must be admitted separately, not broken by applying CL7's no-call rule to
every Slice. The new direct-control model is a forward source-control
refinement, not reverse/physical/MIR emitter proof. Fresh importing preflight
PASS: 10 kernel-verified modules, zero assumptions. Full formal integration
PASS: 78 modules plus approval binding consumer, only two existing approved
Slot abstractions. Documentation quality and evidence lifetime also PASS.

Next falsifying inputs were run through the ACTUAL compiler: field inout,
ordinary readonly temporary and backing growth after the last Slice use
still refuse. Growth before the last view use also refuses and MUST stay
refused. Their scratch sources and diagnostics are in the p0 log root below.
An actual generated-C/WSL sanitizer falsifier also reports an 8-byte Array
leak without manual release; its historical manual-release control exits 0
with stdout 2 and no sanitizer finding. This confirms automatic cleanup is
not implemented; it is not an LLVM/full-sanitizer or regression claim.
No manual local/carrier/copy workaround is an accepted fix. The next whole
chain is admitted ordered source expression/place facts -> call normalization
and disjointness -> final snapshot lifetime/recovery facts -> actual C/LLVM
consumers, including self-host admission. Actual call-result ABI producer,
physical inout bundle erasure and Slice lifetime issuance remain OPEN.
P2-P7 activation is gated by those P1 dependencies; no automatic drops or
retired manual paths have landed.

Receipts: `audits/ownership_clean_production_repair_2026-10-09.md`,
`audits/ownership_cutover_frozen_baseline_2026-10-09.md`,
`audits/ownership_slice_dx_census_2026-10-09.md`. Logs:
`.tmp/ownership-cutover-2026-10-09/p0-5651c916-7aeb6d7b3bc44252ba1e80bf2027449b/`.
Official bin hashes remain `f6559da9...cbc1c9` and `707dcd40...78ec7`.
No push/install/GUI-ready message is claimed. Do not resume an archived
checkpoint or increase the cap to conceal missing production facts.

## Historical archive — lookup only

### Earlier ownership-cutover preparation snapshot (GPT, 2026-10-09)

Latest user directions, "구현 시작해 클로드가 시작했으니까" and "전체 작업해",
reopen the full transition. The earlier scope holds below are history, not the current
authorization. Dependency gates and official-installation boundaries remain.
Active directive:
`agent_work_directives/ownership_cutover_execution_2026-10-09.md`.

Same `main @ a75da80435e0d051f71cfacf76d44ea825ed87d7`; current snapshot:
31 short-status entries at GPT's latest continuation check, staged 0, including
excluded `gmon.out`; verify with Git on resume. Shared preflight changes are
preserved. Claude's CL6/CL7 models passed GPT's independent importing audit;
GPT's audit/pressure/profile-gate changes remain uncommitted. This is not a
frozen tree. Claude owns model definitions; GPT owns importing integration
and the subsequent production cutover after its dependency gates. No
production source/runtime, CI workflow, registry-status or official-binary
change is claimed. An input-bound receipt is not a frozen P0 matrix or heap
allocation counter.

Last observed gates: importing focused kernel PASS (nine modules, no
assumptions); full formal integration PASS (77 modules plus approval
consumer, only two existing approved Slot abstractions). Fresh isolated
native LLVM-enabled compiler build and native-only C/LLVM ArrayDrop PASS.
Native unit shard's batteries passed, but subsequent MIR integration lacks
a same-source self-host driver. Static single-owner/protocol/reachability
checks PASS; CI-profile stale step count repaired and verified with three
negative consumers. Its clock/scalar native checks also PASS on C/LLVM after
test-only Windows line-ending correction, with exact Boolean comparisons
and their narrowing/ABI negatives retained. Component contract timed out at 60 s; likeness fails
sentinel 73 > 20. No frozen P0, complete native matrix, DRV-2 or remote CI
PASS. Receipt:
`audits/ownership_cutover_p1_integration_and_p0_obstructions_2026-10-09.md`.

Active executable diagnostic: fresh reference SHA-256 `8b26d1d...d24af754`
refuses 3072 MiB on the complete `driver_bootstrap_main.pgy` input. V3/V4
admitted-row counters locate the amplification: 4,730,448 CLASS declarations
versus 25,434 actual VARIABLE rows, copied into per-function snapshots and
loop states. The existing ResourceFlowUniverse owner lacks a declaration/value
admission distinction. HIR/RIR/MIR preserve its projections; do not fix this
by discarding required facts in a downstream consumer.

Separate scratch candidate SHA-256 `0daa8b6b...3e60531` excludes only CLASS
and TYPE_PARAM at that owner and its snapshot/declaration/seal producers.
Same 2531 bound self-host inputs, no drift, unchanged cap: exit 0 in 64190 ms,
peak private 1320.5 MiB, complete native MIR issued (378,143,208 bytes;
SHA-256 `52290d05...985b402`). MIR carries the same 25,434 VARIABLE rows and
52,498 loop states instead of 6,729,502. Heap counters remain UNMEASURED.
Semantic unit battery 3107/0; HIR 26/0, DIR 15/0, RIR 26/0, AIR 147/0,
MIR 217/0. Stable-ID typed/generic parameters, nested Slot lifetime, missing-
universe and type-as-release refusals, DIR/RIR identity, loop summary and
native C/LLVM ArrayDrop gates PASS. The old producer fails the new row-count
oracle; two existing argument fixtures are byte-identical. This is a verified
scratch repair, not production source change, official P0 or substitution.
Sources, probes and logs: `.tmp/ownership-cutover-2026-10-09/`; full receipt
and limitations are in the audit above. Official binaries are unchanged.

Latest continuation: the same candidate emits C for that complete input:
exit 0, 104087 ms, peak private 1461.1 MiB (1.427 GiB), same 3072 MiB cap,
all 2538 declared bindings unchanged. Native oracle C is 53,340,103 bytes,
SHA-256 `5ec64ef8...9303b4`; existing MIR/AIR/region and artifact-identity
admission ran. No full-artifact machine-code compilation, self-host driver,
fixed point or installed-pair result is implied. Nested-declaration and
seal-only Future aliases/values PASS; separately named v3 probe retains the
SAME context/symbol across universe generations and verifies changed stable
index, snapshot and sealed parameter identity. Its executable is
`3fc53649...25f8357`; it does not prove epoch-wrap or concurrent safety.
Fresh focused preflight kernel also PASS (nine modules, zero assumptions),
using the explicit project OPAMROOT after an initial missing-prefix refusal.
This continuation changes coordination/audit/handoff and scratch probes only;
production source and official compiler hashes remain unchanged.

Latest scoped red-team repair: `scripts/measure_build_pressure.ps1` and its
existing consumers now separate unowned candidate observation from process
authority, fence selection/sampling/retirement with creation identity, reject
capture failure, stream raw bytes with bounded stage buffering and preserve
occupied receipts. Default repeated Make calls select fresh run directories;
explicit destinations remain collision-refusing, with both receipts' bytes
checked after repetition. Windows contract PASS: 21 executable cases plus
synthetic identity controls. Final same-input MIR run PASS in 49248 ms,
private 1319.6 MiB (1.289 GiB); all 2538 declared endpoint bindings unchanged, full stdout
hash remains `52290d05...985b402`, pending stage buffer max 1 character.
The script's fresh hash is `4f3571f1...7c86612`; input hashes are endpoints,
not continuous immutability, and OS peaks are sampled, not heap counters or
a kernel quota. Unknown reparented workers invalidate attribution; they are
never silently adopted or terminated. No production compiler/proof owner
edit, manual annotation/copy workaround or self-host closure is claimed.
Receipt: `audits/ownership_gate_redteam_2026-10-09.md`; coordination:
`agent_work_directives/ownership_gate_redteam_2026-10-09.md`.
Fresh focused Rocq kernel again PASS (nine modules, zero assumptions), as do
documentation/direction/syntax and native diff checks. Current snapshot is
31 paths, staged 0, 30 UTF-8 text inputs plus preserved `gmon.out`. The earlier
auxiliary WSL Git check is red on existing CRLF fixture interpretation; no
shared source was normalized to hide that environment difference.

Latest continuation extends the scratch identity falsifier: a fresh context
with a retained symbol memo, or boundary epoch reuse, maps two values to index
0 and loses a sealed row in the previous candidate. New scratch owner checks
the memo against its EXISTING declaration entry before reuse; all three
re-entry cases and v3 controls PASS. Native candidate `5377a5e0...d3872fc`
passes semantic/HIR/DIR/RIR/AIR/MIR unit batteries and native-only C/LLVM
ArrayDrop. Same full input emits byte-identical MIR in 50397 ms, sampled
private 1319.2 MiB (1.288 GiB), 2542 endpoint bindings unchanged. No practical
epoch-wrap/source exploit, arbitrary stale-frame safety, speedup or production
migration is claimed. Fresh importing preflight again PASS (nine modules,
zero assumptions), as do documentation/direction and native diff checks.
The latest user instruction confirms that all other writers have stopped. Receipt:
`audits/resource_flow_generation_memo_redteam_2026-10-09.md`.

Next boundary: with the renewed stop confirmation, commit the second checkpoint
and execute P0 against that exact input without gate-input writes. Record that
checkpoint's P0, including its red memory result,
then migrate the demonstrated value-admission repair into the existing
universe header/source and snapshot owner, including the validated memo
identity guard, with the importing semantic
ratchet, rebuild and rerun this same input. P1 still
needs source-place/expression and physical call/view refinements plus actual
ABI/lifetime issuance before P2-P7. Partial proof/gate green does not activate
automatic drops or retire the old ownership paths. No commit/push/install
or GUI readiness was claimed.

### Earlier model and collaboration snapshots

## Latest scoped proof work: exiting callee and writable view (Claude, 2026-10-09)

CL6/CL7 of agent_work_directives/claude_gpt_ownership_collaboration_2026-10-08.md.
New importing proofs, no existing proof/core/teardown/preflight file changed:
semantics/proofs/OwnershipCleanCallLowering.v (8db20314...572026) lowers an
exiting callee to the core's ordinary procedure table and proves end-to-end
recovery of every inout plus the outcome packet on normal, early-return and
handled-error exits (target soundness via elab_sound, no catch rule);
semantics/proofs/OwnershipCleanViewScope.v (f7c7a5c5...8fd04) models a writable
view as a whole-backing focus and proves the backing keeps its length, with
growth/transfer/release of the suspended backing refused statically.
Kernel check of the 5 modules: PASS, no assumptions
(.tmp/rocq93_ownership/cl6-cl7-kernel-claude.log). Both registered in
tests/formal_semantics_smoke.sh (two lines, collaboration exception 1); the
full formal gate rerun is logged in
.tmp/rocq93_ownership/formal-smoke-cl6-cl7-claude.log. Decisions handed to GPT:
direct-jump epilogue refinement (flag lowering lengthens lifetimes), the DX
cost of not naming a backing while a writable Slice is live (count uses
first), and the lease-vocabulary sentence of 27 §5.10.3. Docs: 27 §5.10.5,
semantics README, docs/102. HEAD a75da804, no stage/commit/push.

## Latest scoped work: four cutover prerequisites (GPT, 2026-10-09)

User confirmed the four-item proposal: ABI owner, multi-inout normalization,
Slice lifetime evidence and reviewed local baseline checkpoint. This lifts
the hold only for that bounded preflight, including required importing proof
work. Production compiler/runtime/backend consumer migration, full cutover, official install,
push and GUI delivery remain held. Same worktree; no discard/stash/reset.

Active directive: `agent_work_directives/ownership_cutover_preflight_2026-10-09.md`.
Local baseline `a75da80435e0d051f71cfacf76d44ea825ed87d7` (456 reviewed input
paths, tree `513cc18b1785c4f623045a38463a2b5cbdd6f612`) precedes source edits.
Post-commit frozen-input cleanup model/extraction, documentation and full
kernel gates PASS (72 modules plus approval consumer; only two approved Slot
abstractions). Tracked diff 0 at validation; pre-existing `gmon.out` excluded.
No push/install/native/DRV-2/memory-pressure/remote CI result is claimed.
The prior contradiction review remains documentary evidence, not P1 closure.
The user confirmed other writers stopped for checkpoint-through-baseline
validation. Latest clarification leaves production C/LLVM consumer migration
in the later transition; only ABI/multi-output/view contract decisions and
their necessary importing proof preflight are in scope now. Doc 27 §5.10.1–3
records the coupled vocabulary, recovery-before-failure order and static
write-through lifetime boundary. Importing proof edit inventory is fixed in
the directive. The importing gates ran, but full P1 and production issuance
remain OPEN for the refinements below, not merely for a missing test run.

Current uncommitted scope: 15 preflight docs/proof/gate paths plus the preserved
untracked `gmon.out`; staged 0, same HEAD. No compiler/runtime/backend edit
after the baseline. Supplement hashes: CallRecovery `6b82ea27...`, Views
`8bfd2b6d...`, independent Audit `fa8b1e60...`; full identities and logs are in
`audits/ownership_cutover_preflight_2026-10-09.md`.

Last observed importing integration after final repairs: focused fresh kernel
PASS (seven modules, zero assumptions); formal semantics/full kernel PASS
(67 owners + eight permanent consumers = 75 modules, plus approval binding
consumer; only the two existing approved Slot abstractions). SoT structural
gate PASS (95/201, statuses unchanged), documentation/shell syntax/diff checks
PASS; strict UTF-8 on 15 inputs and all 75 final module hash matches PASS.
Two actual read-only installed Claude CLI reviews ran. The second found no
remaining High/new Medium inside scope; GPT repaired the four Low follow-ups
afterward and reran these gates, not a third Claude approval.

Next falsifying chain: a real normal-table callee/caller lowering must restore
every inout plus independent result/error before the actual handler runs.
The view proof is a dynamic ghost oracle, not static liveness: final-MIR
issuer and complete/current caller scope, snapshot/state linearity, complete
effect sets and the whole-instruction view frame remain OPEN. Source ordered
expression/place and pointer/bundle erasure refinements, general payload/glue,
mutable exclusivity and graph binding also remain. Dedicated guarded backing
drop/scalar frames do not protect the base machine's other operations.
No new self-host substitution rung or production SoT closure is claimed;
P2-P7 consumers and the full landing matrix remain held.

## Previous scoped work: cutover contradictions, implementation held (GPT, 2026-10-09)

Latest user direction is "우선 모순쪽만 정리해놔 바로 구현은 마지막 체크한번 더
하고". It narrows the immediately preceding implementation request. Current
scope is documentation reconciliation and read-only final co-review; no P0
full baseline, proof changes, native/self-host/runtime implementation,
bootstrap, installed-binary replacement, commit/push or GUI notification.
The existing ownership/DX compiler hold below remains active. Do not resume
the superseded manual own/carrier chain or infer approval from a repaired plan.

HEAD `3658548d24bca3d721e4f1974ac7a10da99f7aa8`; entry snapshot dirty 442 entries,
staged 0. Final read-only snapshot at 00:39:33 KST: dirty 443 short-status
entries (456 expanded untracked paths), staged 0, same HEAD. Plan:
`agent_work_directives/ownership_cutover_plan_2026-10-08.md`,
status REVIEW/implementation held. The I1-I8 directive and collaboration card
use the same hold, copy-propagation order, final-generation analysis,
dependency gates, canonical JSON identity, negative-fixture exception and
candidate-CI sequencing. Doc 27 §5.10 names missing production refinements
OPEN; no theorem/model hash changed. Existing ABI row status is not widened
to ownership-clean metadata. Prior model results below remain scoped history.

Receipt: `audits/ownership_cutover_contradictions_2026-10-09.md`. Two actual
read-only Claude reviews completed; the second confirmed the first review's
four High documentary issues were repaired. Its remaining two Medium and
six smaller wording issues were subsequently reconciled and checked by GPT,
not a third Claude review. The active plan now distinguishes the limited
Allocator/TextBuilder ABI join from the OPEN collection/String owner, source
normalization from target cleanup equivalence, and baseline/actual tested CI
SHA from dirty diagnostic observations.

Last observed gates after the final substantive repair: documentation quality,
strict UTF-8/local links/text hygiene for eight documents (48 local targets),
scoped tracked diff check, and SoT registry structural check PASS (95 authorities,
201 carriers; statuses unchanged). Four ownership-clean core hashes unchanged.
No fresh kernel, compiler/runtime, bootstrap, installer or remote CI validation.

Next boundary: user confirmation after this final documentary check. The
formal P0 baseline is BLOCKED until an approved snapshot method provides
gate-input diff 0 without discarding WIP. P1 still needs actual multi-inout +
independent return normalization, ordered temporary/short-circuit evaluation,
Slice/getter backing lifetime and overwrite/discard/failure payload proofs.
Candidate path injection exists in source; full isolated installer/bootstrap
and actual same-SHA CI remain unrun. Documentation consistency is not P1
proof closure or compiler behavior. No implementation approval is inferred.

## Latest scoped work: executable teardown authority and Claude co-review (GPT, 2026-10-08)

Direct user request reopened CL1 implementation under the collaboration
directive's exception 2. This is a bounded formal-model scope, NOT a successor
self-host rung or an ownership/DX compiler hold lift. The existing active
self-host card and I1–I8 order below remain unchanged.

HEAD `3658548d24bca3d721e4f1974ac7a10da99f7aa8`; observed shared dirty state:
453 Git entries, zero staged paths, no commit/push/installed-driver replacement.
Owner: doc 28 requirements and importing `OwnershipTeardownAuthority.v`, hash
`ca8aa6f11df6bccad9127ebe01e3cc9cb49c28a3c72f8b808f906f4d95960a86`.
Forest/teardown owner `OwnershipTeardown.v` unchanged (`4babfc26`). Root-right
issuance/transfer/consumption, owner-approved lifetime loans/pins, complete-unit
pre-destructive admission, unchanged-state refusal and old-identity exclusion
are implemented/proved over the same forest. No second heap or raw Step API
fallback. Final consumers: permanent redteam and freshly extracted observer.

Last observed gates: focused teardown redteam PASS (four modules, no assumptions,
62 authority cases, 18 source mutations + one extracted approval + two cost
oracles); formal semantics/full kernel PASS (65 owners + seven consumers,
existing two Slot assumptions unchanged); docs, evidence lifecycle and shell
checks PASS. Actual installed Claude completed two read-only reviews. First
High (foreign reference can mint a pin) repaired; second review found no new
High/Medium inside scope. Four Low proof/gate issues subsequently strengthened
and independently rerun by GPT, not presented as a third Claude review.

Next falsifying input for the next agreed composition boundary: a legitimately
issued cross-context lifetime loan reaching a real access/mutation consumer;
retirement must preserve the storage/Slot binding and reject stale context,
wrong owner or descendant pin before any physical destructive action. Native
context/recipient issuance, protected current-state linearity, actual access/
mutation/reparenting and storage composition, finite exhaustion, indexed walker,
concurrency/finalizers and compiler synthesis remain OPEN. Bounded full-bound
certificate checks are not the production hot-path walker.

Receipt and exact hashes: `docs/audits/ownership_teardown_authority_2026-10-08.md`.
Completed scope: `docs/agent_work_directives/teardown_authority_implementation_2026-10-08.md`.

## Latest scoped work: unit-wide local reads, Claude/GPT split, OLD directives (Claude, 2026-10-08)

User asked for the two review follow-ups, a written Claude/GPT split and OLD
marking instead of deletion. OwnershipTeardown.v is now
4babfc268dee283f377c0e0ae337d8afffdda594133e4f0ea1a3f416c0b93369 (supersedes
the f260e3bb root-epoch card below): released_unit_local_read_none_forever and
dropped_root_unit_local_read_none_forever cover every member of a released or
dropped unit. OwnershipTeardownRedteam.v is
fe021db2894417c3040cb01a26ac7270125813bba7f33256efaa2ac95e9e971e: adds
root_drop_has_no_holder_permission_premise (retained OPEN, like the release
one) and saved_child_local_refused_after_root_drop. Observed PASS:
tests/ownership_teardown_redteam_smoke.sh (3 modules, 0 assumptions, 11 + 2
mutations refused), documentation_quality_smoke, evidence_lifecycle_adequacy.
Split: agent_work_directives/claude_gpt_ownership_collaboration_2026-10-08.md
(Claude CL1 authority/loan boundary first; GPT GT1 gate upkeep, GT3 = I1-I8
only after the ownership/DX hold lifts). 132 directives dated before
2026-10-08 carry an OLD banner; content and status lines unchanged;
mir_builder_ownership_chain_2026-10-07.md stays unmarked (active card).
HEAD 3658548d, no stage/commit/push.
Later the same night: the user decided mixed manual/automatic ownership is a
defect, so GT3 is now a one-shot cutover planned in
agent_work_directives/ownership_cutover_plan_2026-10-08.md (P0-P10, eight
landing gates that must pass together, five user decisions, measured chain:
436 compiler + 460 test manual releases, ~9k lines of old decision owners,
48 exact-key JSON readers, ~52 shallow copy sites, 3 GiB cap already at 3.006
GiB on 09-28). Plan only; hold unchanged; no code changed.
Memory follow-up (read-only): audits/compiler_memory_pressure_2026-10-08.md.
The 09-28 gen2.exe run hit the cap right after semantic verdict, before C
emission (source stage +1,221 MB, statement +695 MB, no drop over 20 MB in
917 samples); the C oracle uses 2.365 GiB for the same source-to-MIR job, so
most of the cap is pipeline structure (whole program in one process, MIR JSON
text handoff), not ownership alone. Today's installed self-driver (707dcd40)
compiles the 67 examples it admits (<= 155 lines) at <= 30.9 MB. Plan P0 now
also measures peak live bytes vs cumulative allocation and records command,
input and executable hash. Comparison with Zig/Rust/Go/DMD is in the audit.

## Latest scoped documentation: Qt provenance and tradeoffs (2026-10-08)

The user brought the Qt-inspired parent/child ownership and non-owning link
invalidation proposal. AI assistants checked, formalized, red-teamed and
repaired it; they did not originate the proposal to bring it from Qt.
Doc 28 now records this provenance and the official Qt sources, separately
from Pergyra-specific authority/loan/generation/frame obligations and ordinary
value liveness. Docs 27/207, the graph design audit and semantics navigation
link to the same provenance/tradeoff boundary.

Tradeoffs include long-lived-owner retention, nullable deletable-target links,
index/ancestor/copy/analysis costs and cleanup bursts. Forest-model unit
existence is not unconditional resource release or caller authority; actual
loan/pin, physical finalizer/concurrency and C/LLVM evidence remain OPEN.
There is no general faster/safer-than-GC claim or Slot-per-value requirement.

Observed PASS: documentation_quality_smoke.sh; ownership_clean_direction_smoke.sh
(27 retired manual-chain owners, structural only); scoped UTF-8, links and
whitespace. Logs: .tmp/ownership-tradeoffs-provenance-2026-10-08/.
This session changes six documentation files, not proof/compiler/runtime code.
HEAD 3658548d24bca3d721e4f1974ac7a10da99f7aa8; the last read-only Windows Git
snapshot counted 436 dirty entries and zero staged paths. Shared-tree counts
changed during the documentation checks; this is a timestamped observation,
not a frozen checkout. No stage/commit/push/install or GUI message here.

Source-drift check found two differences from the prior 71-module kernel
snapshot, outside this documentation edit scope:
OwnershipTeardown.v now 4babfc268dee283f377c0e0ae337d8afffdda594133e4f0ea1a3f416c0b93369;
OwnershipTeardownRedteam.v now fe021db2894417c3040cb01a26ac7270125813bba7f33256efaa2ac95e9e971e.
Those edits are preserved. No fresh Rocq/extraction/integration/remote CI gate
was run here; the older checkpoint below is historical, not a current-tree
kernel PASS. Its claim boundaries and the active ownership/DX hold remain.
The next falsifier stays authority issuance and active loan/pin refusal,
followed by arena binding and physical refinement; no new implementation
or proof/performance track is opened by this clarification.

## Prior model checkpoint: root epochs and checked saved locals (2026-10-08)

Historical receipt for the source hashes observed in that session. The source
drift above means its all-71-hashes statement is not a current-tree assertion.

User asked to repair the remaining model boundaries where possible. Plan:
agent_work_directives/ownership_teardown_root_epoch_2026-10-08.md.
Receipt: audits/ownership_teardown_root_epoch_2026-10-08.md.
Canonical SHA f260e3bb628e0c6f26bc597da7f14ac167a416aa85a3982c9b75f999bd05c98a.
Root Alloc/Attach/RootDrop now share current live epoch admission; drop
advances the epoch and RootNew preserves it. Old root identities remain dead
through arbitrary redeclaration/reuse runs. Every reached constructor,
invariant/witness and independent consumer is migrated; no raw-id fallback.
Saved node locals have a freshly extracted checked read which stays None after
retirement/reuse. Immutable source-slot snapshots remove repeated evaluation
of older functional heaps; this is not a stable physical loan or cache.

Observed PASS: focused 3-module fresh Rocq 9.3.0 kernel, zero assumptions;
98,304 field updates, 16,384 teardowns, 16 parent/four uniqueness controls,
64 reuse rounds and 20,608 checked identities; 11 guard/check mutations,
two cost-oracle mutations and unknown-command refusal. Full kernel: 71
owner/permanent-consumer modules plus approval export/binding; only the two
existing approved Slot abstractions, all 71 current source hashes match.
64-owner formal inventory, kernel refusal self-test, documentation/lifecycle
registration and scoped text/Bash/Make/CI wiring checks pass. Remote CI not run.
Warnings/theory dependencies retained. Initial unwrapped formal invocation
refused missing Rocq; admitted wrapper rerun passed, with no declared skip.

The fixed reuse execution first used 93 seconds CPU without finishing and was
stopped by its verified owned PID/path only. After one-read snapshotting, the
entire unchanged extracted self-test was observed at wall 0.10s/user 0.09s.
This is bounded functional-model/OCaml evidence, not allocator/GC performance.
The large-row and quadratic unit-observer limitations remain.

HEAD 3658548d24bca3d721e4f1974ac7a10da99f7aa8; preserved dirty main,
305 entries in Windows Git, staged paths zero. No stage/commit/push/install,
GUI readiness message or SoT status change. Authoring skill kept user syntax
unchanged: identity checks are internal ownership machinery, not user rituals.
Next falsifier: real release authority issuance and active loan/pin refusal,
then cross-arena/root binding, exact indexed unit issuance and physical
placement/growth/free/finalizer/concurrency refinement. These are OPEN.
The active self-host ownership/DX hold below is unchanged; model completion
does not reopen that compiler implementation or establish executable CLOSED.

## Prior scoped checkpoint: node teardown red-team and locality (2026-10-08)

Historical checkpoint; the successor above owns the latest model hashes and
resolved root/local scope. Remaining claims below describe the earlier tree.

User reopened the complete teardown mechanism for red-team improvements and
bounded performance checks. Plan:
agent_work_directives/ownership_teardown_redteam_locality_2026-10-08.md.
Receipt: audits/ownership_teardown_redteam_locality_2026-10-08.md.
OwnershipTeardown.v SHA d633dfcd7e71417d8a6ba0d4702f532988eb839d0da7d93f41b599568f78f33a
now admits only unique exact node/root retirement units. Field/parent updates
remove entries only from the actual old target/owner, preserve the scan
specifications under exact indexes, and return unrelated rows without copies.
Forest, RC, arbitrary-run proofs and concrete final consumers are migrated.
The scan specifications are not execution fallbacks.

Observed PASS: focused 3-module fresh Rocq 9.3.0/rocqchk, zero assumptions;
98,304 extracted field updates, 16,384 teardowns, 16 parent cases, four
schedule controls, four guard mutations, one sharing-oracle mutation and
unknown-command refusal. Full production snapshot checks 64 owners plus
seven permanent typed consumers (71 modules and approval export/binding);
only the two existing approved Slot abstractions. All 71 current hashes match.
Kernel gate negative self-test also passes. Warnings/theory dependencies are
retained. Documentation/registration, Bash, Make, YAML and scoped text/link
checks pass. Formal CI includes the extraction gate, but remote CI was not run.

OCaml 4.14.1/O2 fixed inputs (512/4096/16384, 64 repeats, median five)
show lower observed update CPU/allocation against the old scan specification.
Crowded old-row filtering and crowded new-row append remain linear. The
NoDup list observer is quadratic and is not the chosen production issuer.
No native compiler, GUI, allocator or GC-speed comparison is implied.

HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8, preserved dirty main;
303 status entries in the Windows Git view and the index is empty.
No stage/commit/push/install/GUI message or SoT change.
The active self-host ownership/DX hold below is unchanged. Next falsifier:
bind the actual release authority/loan/pin and root/arena-domain identities
before destructive retirement; then a certified exact/unique indexed walker
and physical/finalizer/concurrency refinement. Saved locals still need checked
access. Model/extraction evidence is not executable CLOSED.

## Latest scoped work: proof red-team repair and reuse (2026-10-08)

User reopened all 35 findings and requested parallel reuse-family repair.
Receipt: audits/proof_model_redteam_remediation_2026-10-08.md; whole-chain
plan: agent_work_directives/proof_redteam_reuse_remediation_2026-10-08.md.
Disposition is 24 bounded-model/runtime repairs, nine corrected claims and two
existing repairs reverified, not 35 production-safety closures.

Observed main integration PASS: frozen 63 owners plus six permanent regression
consumers and approved-assumption export, Rocq 9.3.0 / rocqchk; all 69 current
hashes match. Only the existing two Slot abstractions; warnings/dependencies
remain visible. Source-bound intent inline/linked x trace0/1 and clock mock/real
positive/negative probes pass. Fresh private native LLVM18 compiler
bccaacdf605518d184eaf23594a18bf8b5e7a50b4f606b6b48a48f602f531224 passes
clock C/LLVM i64 ABI, admitted scalar call widening and narrowing/incompatible
refusal, unchanged stdlib C/LLVM, semantic 3107/0 and transpile 1006/0.
MIR 217/0 and reached topology/facts/render gates also pass. Standalone
slot-scope/capability/budget and lane-scheduler execution pass. Strict-C11
profile checks compile one shared runtime TU for seven profile consumers;
root source-test fan-out compiles 18 TUs, not 18 test executions. Fresh
certificate extraction agrees on 262144 valuations, four length controls and
41 envelope/owner controls. Existing unreachable/clipping warnings remain.
Windows real-clock inline/linked probes pass, not a Windows compiler matrix.
Full component structural gate remains rc=1 after its 14-checker subgate;
no whole self-host/installed-driver/remote CI green claim is made.

Runtime handles do not recycle public identities, even when physical registry
storage is reused. Now is monotonic Long ms, never a Unix timestamp. Clock
timestamp consumers and admitted LLVM argument ABI are connected without new
user annotations. Rollback/cancel/region/capability/receipt/scope repairs have
permanent typed negatives; general compensation/fairness/issuer refinement is
not established. GC costs are independent abstract policies, not universal
speed evidence. The local ignored authoring skill was updated and validated.

HEAD stays 3658548d24bca3d721e4f1974ac7a10da99f7aa8, preserved dirty main;
300 status entries and empty index at the final integration snapshot.
No staging/commit/push/install/GUI message or SoT status change.
The active ownership/DX hold below still applies. Next memory-algorithm
falsifier remains actual Slot/root/graph issuance, complete frame inventory,
physical placement, copy-before-free and failure-atomic single retirement;
survey: audits/memory_boundary_executable_closure_survey_2026-10-08.md.
Do not call the existing conditional graph-frame proof executable CLOSED.

## Latest scoped work: graph allocation and external ownership (2026-10-08)

The user requested the allocation/growth connection proof after the mixed-heap
counterexample. Bounded formal implementation is complete; production remains
OPEN and the active self-host hold below is unchanged. Scope/receipt:
agent_work_directives/graph_allocation_frame_composition_2026-10-08.md.

OwnershipGraphLinks.gexec is explicitly a graph fragment. Whole-heap consumers
use gexec_framed with the external footprint admitted by HeapSplit; None refuses
RNoFrame, and allocation overlap refuses RNotFree with the input state unchanged.
The owner proves storage coverage for every operation and framed step/run
preservation. MemoryBoundaryCompositionAudit derives actual new-store,
vacant-insert and growth whole-heap updates through canonical free. The existing
retirement agreement now consumes the framed route. An own-table-reuse growth
reaches graph drop, canonical texec and Slot release, retaining another owner's
(9,0) block and value; no release token is fabricated.

Observed PASS: focused five-module fresh Rocq 9.3.0 / rocqchk plus approval
consumer; full inventory / 63-module corpus; documentation quality; ABI ownership
shape; scoped whitespace. Only the existing two Slot abstractions; warnings and
theory dependencies remain visible. A scratch-only removal of the frame guard
is rejected by the kernel at external_allocation_refused. Logs:
.tmp/memory-boundary-composition/{kernel,allocation-formal,allocation-guard-mutation}.log.
Current hashes: Core 57c55889..., Slot 86d34484..., graph 035bf8a8...,
audit 20f1866a..., gate 2a08dc68.... Core and Slot were not edited here.
HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8; preserved dirty main,
173 status entries and empty index at the receipt. No src/runtime/ABI or installed
binary edit, stage, commit/push, remote CI, GUI message or SoT closure in this scope.

Next falsifier: the production issuer must certify the complete external
footprint against canonical live ownership and preserve that generation through
allocation/growth and retirement. Physical placement injectivity, copy-before-free,
failure atomicity, stored-reference source semantics and C/LLVM remain OPEN.
An arbitrary caller-supplied empty frame is not an admitted HeapSplit; these
proofs are not a physical allocator or whole-language memory-safety verdict.

## Latest scoped work: Slot / automatic cleanup / graph composition (2026-10-08)

The user selected one coherent lifetime contract. Adopted requirements:
docs/semantics/28_memory_boundary_composition.md. One storage owner, access
rights bound to that owner, one retirement edge; shared/cyclic graph links
do not become owners. Ordinary values keep D1; no public Slot per value or
link. Vision and doc 207 section 14 point to the contract. This is not a new
implementation rung or a takeover of canonical proof cores.

tests/coq/MemoryBoundaryCompositionAudit.v imports the existing Core,
GraphLinks and SlotCalculus. With admitted same-root/footprint correspondence,
both invariants, no active graph borrow and the Slot release guard, graph
ODrop and ordinary TDrop agree on the heap, preserve their invariants and
admit release of the same Slot. A concrete cyclic/multiply-linked graph
satisfies both invariants. Missing/wrong footprint, stale/released, token,
pin/borrow and missing/reused-target refusals are retained. This is one
retirement represented in two models, never two successive physical frees.

Observed PASS: focused 5-module fresh Rocq 9.3.0 / rocqchk plus approval
consumer, full inventory and 63-proof fresh corpus, documentation quality,
ABI ownership shape, Bash syntax, local UTF-8/links and patch whitespace,
Make target resolution and YAML parsing. Only the existing two approved Slot
abstractions; no added axiom/admit or unsafe kernel feature. Existing warnings
and theory dependencies are visible, not suppressed. Existing Slot C/LLVM
baseline also passes 4 positive + 7 refusal cases per backend through its
declared native-pipeline gate, not self-host substitution.

Current hashes: Core 57c55889..., graph 6045e9ea..., Slot 99c06b1b...,
audit c96920a3..., gate 36ffdd13.... Canonical sources were unchanged here.
Logs: .tmp/memory-boundary-composition/{kernel,formal,slot-baseline}.log.
Exact receipt: agent_work_directives/memory_boundary_contract_composition_2026-10-08.md.
HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8, preserved dirty main
(171 status entries, empty index at receipt). Installed native f6559da9...
and self-driver 707dcd40... are unchanged. No production edit, SoT closure,
staging, commit/push, install, remote CI or GUI message in this scope.

Next falsifier: issue the actual graph/Slot/canonical-root correspondence,
carry it through deletion/growth and one coherent retirement edge on C/LLVM,
and refuse missing evidence before payload destruction. Binding issuance,
stored-reference source/observation semantics, runtime atomicity/finalizers,
async/FFI and performance remain OPEN. Existing I1-I8 implementation order
and the active self-host card below remain authoritative; do not promote
this bounded audit to whole-language memory safety or graph implementation.

## Latest scoped work: ownership automatic-memory naming and GC comparison (2026-10-08)

The user adopted "소유권 기반 자동 메모리 관리": GC-like authoring convenience
from ownership evidence, as a core ordinary-value lifecycle mechanism. Vision,
AGENTS, doc 27 section 0 and doc 207 section 12 record it without changing D1,
C2 or the existing implementation chain. Plan/receipt:
docs/agent_work_directives/ownership_automatic_memory_gc_comparison_2026-10-08.md.

OwnershipCleanGCComparison.v imports the unchanged canonical machine. It
proves read coverage, exact-retention bounds, correct-GC exact-heap agreement,
and conditional operation-cost savings against an ideal-root full-heap sweep.
Its shared allocate/observe/retire workload uses canonical source execution,
elab and texec. The typed audit preserves positive premises and counterexamples
to universal strict speed/safety superiority. No production GC or second
ownership owner was introduced.

Observed PASS: focused 3-module fresh Rocq 9.3.0 / rocqchk check, zero axioms;
full formal inventory / 62-module corpus plus approved assumption consumer;
current canonical extraction and 30 fixed cost cases; documentation quality;
Bash syntax / diff check / Make target resolution. The existing extraction
observer initially failed at borrow_all, then concurrent repair was verified by
the final PASS; this chat did not implement that repair. Existing warnings
remain visible. The full corpus has only the two approved Slot abstractions.

Core c629a6a3..., comparison 9ae1efcf..., audit 1b943593..., focused gate
3f2acea0.... Logs: .tmp/ownership-cleanup/gc-comparison/. HEAD remains
3658548d24bca3d721e4f1974ac7a10da99f7aa8, preserved dirty main (165 entries at
the receipt, empty index). No stage/commit/push/install, remote CI, GUI message,
compiler/runtime edit or SoT closure in this scope.

Next implementation falsifier remains physical C/LLVM cleanup refinement and
actual copied-byte / peak-memory / release-tail costs on the same source/value
semantics. Abstract 6-vs-9 operation counts are not measured runtime speed.
Correct GC can be equally memory-safe and match exact retention. Do not quote
this proof as universal GC superiority or a successor self-host rung.

Documentation follow-up (2026-10-08): the user requested recording the shared
graph proposal. docs/audits/ownership_graph_links_design_2026-10-08.md separates
one storage owner from non-owning links, retaining ordinary value semantics
and avoiding a public Slot per link. Doc 207 section 13 links the memo.
Owner-end cleanup is not automatic reclamation of isolated cycles inside a
live owner. Stored references, deletion, alias access and escape need a future
contract/model; no implementation track or semantic owner was opened.
Observed PASS: documentation quality, strict UTF-8/local-link/whitespace checks
for the new memo, and scoped diff check. HEAD is unchanged at 3658548d...;
the shared tree has 167 dirty entries and an empty index at this snapshot.
Only the memo, doc 207 navigation and this handoff were edited in this scope.
No proof/compiler/runtime edit, stage/commit/push or CI claim; the existing
implementation falsifier above remains active.

## Latest scoped work: LSP collection integration (2026-10-08)

The user authorized the remaining keyword/LSP integration work. This bounded
slice does not merge the ownership-clean C/G edit scopes or open a successor
self-host rung. Plan/receipt:
docs/agent_work_directives/lsp_collection_integration_2026-10-08.md.
HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8, preserved dirty main;
host Git observed 160 dirty entries and an empty index before this handoff
update. No staging, commit/push, install, remote CI, GUI message or SoT status
change occurred. Counts include concurrent/unrelated work.

Implemented the common native fresh String-result summary for bounded
if/block returns, retaining borrowed/reassignment/unknown-control refusal.
All eighteen native full-LSP transition failures from the fixed-input census
are gone without changing MIR transition/validation or LSP source. Slice
iteration now derives the semantic element from its existing type-shape owner
and C foreach ABI from its existing Slice runtime owner, not the Array ABI.
No own/ref annotation, new language syntax or source-copy workaround was added.

Observed PASS: current native full-LSP MIR and C build; actual 28-item completion
parity and fragmented live-session success/negative controls on the native-built
Pergyra server; fresh/borrowed/loop/reassignment controls on C and LLVM; Make
native-boundary regression on C/LLVM; official private codegen gen2 == gen3
(100773 C lines); actual gen2 Slice Int/String/empty iteration and unsupported
ABI refusal; category/registry/golden/regeneration and native iteration facts.
Native full-LSP has nine existing unreachable warnings, the Slice fact import
probe five, codegen seed sixteen. These are not warning-free receipts.

Whole self-host integration is still OPEN. The private official DRV-2 build
refuses at ast_collection_ownership_verdict_owner.pgy, node 214839,
borrow_boundary_escape / aggregate_release_plan_unproved (Array<String>).
Gen2 on full lsp/main.pgy separately refuses at
ast_collection_call_argument_verdict_owner.pgy, node 37049,
unproved_formal_element_use_entry (Array<String>). Neither produced an
executable. These are first refusals, not a complete next-chain census.
C2 section 5 exists; the outstanding facts are implementation/admission
obligations, not a missing contract document. Before repairing that chain,
map the reached producers, formal-entry/aggregate obligations and all callers
to the shared C3/C5 decision; do not bypass it with copies, modes, omitted
receipts or native-built installed drivers. C4/C6 ownership stays unchanged.

The full component structural script and its traced run returned 1 after
the fourteen-test checker subgate passed, around the DIR digest-assignment
inventory query. The isolated grep finds its declared owner; whole-script
cause remains UNRESOLVED. Do not call the component script green. Its DIR
owners and script are unchanged in this slice.

Private candidates/receipts: .tmp/lsp-collection-integration/; native C hash
58c6e36c..., LLVM-enabled native 37c7575d..., gen2 97bea858..., gen2/gen3 C
f9c57509.... Installed native f6559da9... and driver 707dcd40... stay unchanged.
Next falsifiers: the same two full-program inputs through admitted current
Pergyra entry/release facts, then the unchanged full completion/live-session
gates through an admitted private driver. No automatic-cleanup, leak/UAF,
full self-host LLVM, installed-driver or remote-CI green claim here.

## Latest scoped proof work: read-only alias elision (2026-10-08)

The user's "do all" continued the proposed bounded readonly proof/extraction
work. Plan: docs/agent_work_directives/ownership_clean_readonly_elision_2026-10-08.md.
No takeover of the concurrent native/self-host C3/C5 implementation was
confirmed; preserve that split and the unrelated LSP/keyword changes.

HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8, dirty main, empty
index. Host Git observed 160 dirty entries at the verification snapshot;
that is the shared tree, not this slice's patch size. No staging, commit,
push, new worktree, installation or production/backend/SoT change here.

OwnershipCleanReadOnly.v imports unchanged Core (f8ac63cf9a9a...) and
Composition (e12836730e707...). Current readonly SHA-256:
14b0122898faf83bffb13836087a6c6bbdd0bf97ef255d08e57fe28a5eb5d8bb.
It checks local whole-value readonly aliases, substitutes reads with the
root, preserves valid-source trace and inherits canonical closed cleanup.
No runtime loan/pointer or second heap model. Original admission is required
before rewriting so an unused alias cannot erase an undefined-root error.
Writes, storage/consumption, calls and live-out/borrowed aliases refuse.

Observed local PASS: ownership_cleanup_smoke.sh fresh 4-module kernel with
zero axioms; 15 decision + 24 refusal + 40 sink/composition + 32 readonly
controls; 30 unchanged cost cases and two branch witnesses. Existing texec
proves the same [7,7] trace/empty heap with abstract allocations 3 -> 2 and
TCopy sites 1 -> 0. Full formal_semantics_smoke.sh passes 60 fresh proofs
plus approval consumer, only the two approved SlotCalculus assumptions.
Same complete-source direction/selftest passes under 60 s in native Git
Bash, superseding the previous WSL timeout for this host/input receipt.

Logs: .tmp/ownership-cleanup/readonly-{extraction-final,formal,direction}-2026-10-08.log.
Receipt: readonly-cost-2026-10-08.json in that directory, schema v4, eight
source hashes rechecked. Doc 207 section 10 explains the algorithm/diagram;
the dated research audit records exact hashes, controls and warning scope.

Next integration falsifier: member-projection alias lifetime/invalidation,
then the admitted typed/SSA facts and actual C/LLVM/self-host glue. Copied
SField remains a copied value; this is not C2's projection-elision core.
C2 doc 27 section 5 exists. The production mir_lower chain still has no
ownership-clean producer/pass from that contract. Physical allocator,
sanitizer, installed-driver, self-host parity and exact-SHA CI remain OPEN;
formal completion is not compiler substitution or SoT closure.

## Previous scoped work: verified cleanup composition (2026-10-08)

User explicitly requested Rocq improvements and monad-like branch composition.
Bounded plan: docs/agent_work_directives/ownership_clean_composition_2026-10-08.md.
Added OwnershipCleanComposition.v importing the unchanged canonical core;
proved sequential unit/associativity, guarded branch bind, exact texec
equivalence of Skip normalization, inherited INV/CORR/closed cleanup, and
refusal/copy-site preservation. No new heap/ownership machine or source syntax.

HEAD is 3658548d24bca3d721e4f1974ac7a10da99f7aa8, preserved dirty main;
host Git observed 152 dirty entries and an empty index at this receipt.
Other work is concurrent: that count is a snapshot, not this slice's change
count. No commit/push, production code, install, SoT status or GUI change here.
Core hash f8ac63cf9a9a... is unchanged; composition hash e12836730e707... .

Final ownership_cleanup_smoke.sh PASS: 3-module fresh kernel, zero axioms;
15 decision + 24 refusal + 40 sink/composition controls and 30 fixed cost
cases. The observer now consumes explicit Modes and the current routine
signature. Exact proof propositions are typed extraction consumers. GUI
inline syntax 23 -> 11, copies 0 -> 0; this is not allocation reduction.
Full formal_semantics_smoke.sh PASS: 59 modules plus approval consumer, only
the two approved SlotCalculus assumptions, no admits/unsafe kernel features.
The direction residue scan timed out at 60 s; it is NOT current-green evidence.

Receipt schema v3 binds seven source inputs. Its Git metadata query is now
scoped to those inputs after the whole-repo WSL query exhausted a 300-s run;
all semantic inputs, repetitions and budgets are unchanged. Final logs:
.tmp/ownership-cleanup/composition-{extraction-final,formal}-2026-10-08.log.
Exact hashes and limitations: docs/audits/2026-10-08_ownership_cleanup_recent_research.md.
Diagram and next proof proposal: docs/207_compiler_owned_cleanup_algorithm.md §9.

Next proposed falsifier: elide a read-only local copy across a branch without
letting its owner drop before the loan's last use; mutation/escape must refuse
elision. This is not yet a proof/implementation rung opened by this receipt.
C2 now exists in docs/semantics/27_ownership_clean.md §5; do not revive the old
"missing C2 document" blocker. The active C/G implementation split still owns
actual native/self-host cleanup and physical-backend validation.

## Previous scoped work: automatic-cleanup algorithm documentation (2026-10-08)

User requested a checked core-algorithm document, explanatory diagrams, and
recent-paper applicability review. Added docs/207_compiler_owned_cleanup_algorithm.md
and docs/audits/2026-10-08_ownership_cleanup_recent_research.md with index/facade
links. These are explanation and review, not semantic owners or a new rung.
HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8 with the pre-existing dirty
tree preserved; this slice edits documentation only and ignored review examples.

Pinned OwnershipCleanCore SHA-256:
f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00.
Fresh Rocq 9.3.0 / Stdlib 9.2.0 kernel check passes for the model and 15
documentation examples (two modules, zero assumptions, no admits/unsafe
features). This newer sink-mode core supersedes the d4cca76d API for this
receipt, not its historical evidence. The 6979f767 intermediate snapshot
failed proof compilation; do not treat that transient as a failure of f8ac63cf.

The standard ownership_cleanup_smoke.sh reaches a passing extraction kernel
check but FAILS at ownership_clean_driver.ml:32 because the observer still
uses the old elab(s,L,B) signature. Its old performance receipt is not evidence
for elab(M,s,L,B). Direction residue smoke passes for 27 retired owners;
that result is structural only. Scratch and exact commands are in the review.

Research candidates: interprocedural mode convergence; storage-root/loan
dependencies including capture; abstract-to-physical cleanup refinement.
RC fallback, idempotent physical free, and unproved arena substitution were
not adopted. Existing C1/C2/C3/G3 ownership and implementation boundaries
remain active; no proof/policy/production edits, SoT closure, install,
commit/push, exact-SHA CI or GUI message occurred in this documentation slice.
Next falsifier: the same model hash through the updated formal extraction
observer, then admitted typed cleanup/glue and actual backend memory tests.

## Latest scoped work: primary categories / semantic fact owners (2026-10-08)

The user reopened the 147-word audit and authorized the bounded classification
correction. Source implementation and focused consumers are verified; full LSP
integration is OPEN. HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8,
uncommitted, with 138 status entries including preserved unrelated work. This
does not change the ownership-clean C/G split or open a self-host successor rung.

The registry's axis is a spelling-level primary category, not semantic checker
dispatch. Six changes: case/default/match -> EXECUTION, on -> DOMAIN,
unsafe/extern -> RESOURCE. all/any stay GENERAL because selected world/join/type
uses are distinct; role remains DOMAIN. No MODULE axis or action-clause owner
merge. docs/42, docs/151 and docs/199 distinguish fact ownership, generic-carried
dimensions and primary categories. Stable IDs, enum values, context/support,
tooling and highlight scopes are unchanged. Generated metadata/readiness and
inventory are regenerated from their canonical owner, never hand-edited.

Latest local PASS: 147 category schemas, 64 doc examples, 8 actual Rocq arms,
18 negative controls; full registry/projections, language golden and generic
matrix. Actual metadata execution passes eight C/LLVM routes (all 147 rows and
three invalid-category controls). Current native protocol source and the actual
Pergyra completion owner on C/LLVM each emit the registry-derived 28-item JSON.
No theorem changed; no whole-language proof or executable substitution claim.

Full LSP protocol gate passes its native probe, then fails default-driver C
emission on unsupported Slice<String> in collection_runtime_owner.pgy. The
diagnostic native-pipeline full-LSP build fails MIR ownership admission at
LspSquigglePolicySnapshotJson, binding 105522, local fields (nine warnings).
There is no before-change full-LSP baseline in this receipt; keep both failures
visible and do not grant admission or restart manual-own workarounds here.
Next integration falsifier: full src/self_hosted/lsp/main.pgy on the admitted
collection/ownership implementation, then the unchanged protocol parity gate.

Installed native/driver executables remain unchanged; focused Pergyra runs use
explicit --native-pipeline, not installed-default self-host substitution. No
staging, commit/push, remote CI or GUI message occurred. Directive and exact
scope: docs/agent_work_directives/keyword_axis_contract_alignment_2026-10-08.md;
runtime receipts/runners: .tmp/keyword-axis-validation/. The user's untracked
docs/audits/semantic_axis_full_audit.md remains preserved historical input,
not semantic authority. Do not resume this verified category slice as a new
optimization, ownership-clean or SoT work queue.

## Latest scoped work: ownership-clean cores complete, fail-closed summaries (Claude, 2026-10-08)

HEAD is still 3658548d, uncommitted. No compiler, runtime, or installed-binary
change; no staging, push, remote CI, or GUI message.

- Graph links (user request, after the cores): OwnershipGraphLinks.v
  (8dba4bc6...) checks the one-owner/many-links proposal, with 0
  assumptions. Results and the decisions the model forces are in
  docs/audits/ownership_graph_links_design_2026-10-08.md.
- Reuse reinforcement (user request): every memory-boundary model now
  admits physical reuse.
  - SlotCalculus.v (a05e7a54...) releases to a tombstone and reclaims at
    the next generation. The earlier generation-1 claim resurrected handles.
  - The graph allocator is adversarial.
  - MemoryBoundaryCompositionAudit.v (77536025...) replaces the
    address-based link, splits the heap at retirement, and adds rooted
    links.
  - Last green gates: memory_boundary_composition_smoke, coq_kernel_check
    (63 proofs) and formal_semantics_smoke
    (`.tmp/rocq93_ownership/*-reuse-claude.log`).
  - Receipt: memory_boundary_contract_composition_2026-10-08.md, "Reuse
    reinforcement".
- Proof-model red team (user request): 35 findings across the corpus (7 High,
  1 fixed: SlotCalculus unguarded unpin), each with a recompiled probe, in
  docs/audits/proof_model_redteam_2026-10-08.md. Probes:
  `.tmp/proof-redteam-2026-10-08/`. Runtime-bug candidates: intent handle
  reuse in the conflict waiver, and `Now()` as an int32 realtime clock. Not
  fixed here; each model's owning lane decides.
- Teardown (user request): OwnershipTeardown.v (6b8e1711...) moves ownership
  to each node (Qt-style tree) with scoped roots, non-owning links, a
  reverse index and an owner-keyed children index. Proves every live node
  belongs to a live root, no dangling stored link, release and root drop
  always succeed, units come from the children index, and no step acts
  through a released handle after reuse; 0 assumptions. Two independent
  red-team passes; all findings fixed or disclosed. The ancestor check
  stays mandatory (ownership_cycle_permanent). Registered in
  formal_semantics_smoke. Results: "해체 정리" section of
  docs/audits/ownership_graph_links_design_2026-10-08.md. Design check only;
  no adoption, no implementation.
- All proof cores are in. OwnershipCleanCore.v (SHA-256 57c55889...) has
  places (focus), unpack (overlapping projections, dead-aggregate parts),
  and one-value regions. OwnershipCleanExits.v (0258fb65...) adds
  break/continue/return/throw/try; an error before a pack releases the
  built parts.
- Summaries fail closed in the model. `Modes` holds `option` summaries,
  `no_summaries` replaced `borrow_all`, and a missing or wrong-length
  summary is refused at the call and at the routine. Inference ascends from
  no summaries (`infer_ascends`).
- ReadOnly is now ff572b83...; Composition is unchanged (3bb896b3...).
- 27 §1, §4, and §5.4 make the storage-model change (one owner per backing,
  deep copy glue) a precondition of any drop.
- Last green gates:
  - `tests/ownership_cleanup_smoke.sh`: 5 files, 0 axioms, summary and exits
    controls (`.tmp/rocq93_ownership/cores3-smoke-claude.log`);
  - the full fresh corpus, 62 proofs including the other chat's
    OwnershipCleanGCComparison.v
    (`.tmp/rocq93_ownership/kernel-cores3-full-claude.log`).
- Implementation goes to GPT:
  agent_work_directives/ownership_clean_implementation_2026-10-08.md, items I1
  to I8. The normative contract is 27_ownership_clean.md section 5.
- Next falsifiers:
  - I1: no copy path of a covered type shares a backing, under ASan/LSan;
  - I4: observer counts on the 27 section 5.8 fixtures, and the refusal of
    calls with missing summaries.

## Latest scoped work: ownership-clean C1 sink + C2 contract (Claude, 2026-10-08)

Scope: Claude's C1 and C2 items in
agent_work_directives/ownership_clean_work_split_2026-10-08.md. HEAD is still
3658548d, uncommitted. No compiler, runtime, or installed-binary change; no
staging, push, remote CI, or GUI message.

- `OwnershipCleanCore.v` now covers calls with inferred parameter modes. It
  is sound for every mode table (borrow or sink), and inference is outside
  the trusted base. SHA-256 f8ac63cf9a9a47389863761b3c84a49279b7e683f807956950844bc3d0d69d00,
  3131 lines, 150 theorems, no admits or axioms. Rocq 9.3.0 compile,
  `rocqchk`, and the fresh 58-proof corpus kernel check pass.
- Falsifier closed: GUI calls go from 2 copies (borrow-only) to 0
  (`gui_calls_sink_moves_only`). A caller that reuses the text pays 1 copy,
  at the call site.
- `settle_tail` proves that release placement needs only the operands of each
  statement. The cubic cost GPT measured belongs to the list-based model, not
  to the algorithm.
- `27_ownership_clean.md` section 5 is the C2 MIR ownership contract, so G3
  is unblocked.
- GPT's G1 harness must follow the new API (`elab M s L B`, 4-tuple
  routines, single `TCall`); until it does, its OCaml driver cannot compile.
  Expected values: `.tmp/rocq93_ownership/sink-controls-claude.log`.
- Adopted (user, 2026-10-08): docs/207 sections 9 and 10, the composition laws
  and read-only alias elision, normative in 27 section 2.6. Wider admission
  and drop commutation are scratch-checked in
  `.tmp/ownership-cleanup/claude-refinements/` and move into the repository
  proofs next.
- Next falsifiers:
  - projection aliases (`let items = h.items` must not demand `Clone`;
    needs the places core);
  - the exits core (return, break, error);
  - the C3/G3 shared fixtures in section 5.8.

## Latest scoped work: ownership-clean G receipt (2026-10-08)

User scope: ownership_clean_work_split_2026-10-08.md G items only. Main remains
3658548d24bca3d721e4f1974ac7a10da99f7aa8, uncommitted; native Windows Git reports
123 status entries at the concrete-code baseline receipt. Installed native/driver hashes are
unchanged. No staging/push/remote CI/GUI message occurred.

G0/G1 pass locally: project stable Rocq 9.3.0 / Stdlib 9.2.0 / opam 2.6.1;
58 fresh corpus proofs plus approval binding/export, kernel/version negative
controls, and current formal smoke. Latest OwnershipCleanCore SHA-256:
d4cca76d5d3ff463126452ddfd928a936ca2e5be19bf91f640deee9c865b3c8b.
The earlier line-1212 failure does not reproduce on this checked hash.
Freshly extracted elab(s,L,B)/callee elaboration passes 15 decision and
24 refusal controls and 30 fixed cost cases (model only, not runtime).

G2 source rollback is verified: 196 tracked files match 3658548d; 234 new
chain files are absent and backed up under .tmp/ownership-cleanup/g2-recovery/;
12 native/retention keep files match the pre-work snapshot, and 7536 source
imports resolve. Old v11-v14 continuation is SUPERSEDED. G6 restores the
reached inventory/caps and adds the retired-owner/search-failure ratchet.
Hard wiring, retention, CI profile, language golden and beta readiness pass.
Whole world/component inventories exceed the unchanged 60-s static budget;
no full inventory or executable-substitution claim is made.

G3 waits for C2's versioned MIR interface (27_ownership_clean.md §5 is absent
at this receipt); G4 waits for C5; G5 waits for actual C3/C5/G3/G4 replacement
gates. Baseline manual releases/analyzers remain. Resume ownership-clean from
the work split and Claude's active implementation card, never from the old
manual-builder card or the historical source-count snapshots below.
Next falsifiers: changed model/API/hash, missing admitted interface facts,
or a retired manual-chain owner returning. Full gate/log and recovery map:
docs/audits/ownership_clean_g_receipt_2026-10-08.md.

Requested concrete-code baseline is PASS on the pinned installed native
compiler (explicit --native-pipeline, not the default self-host route): five
programs on C/LLVM, 30 exact stdout checks, and two compile-only borrowed-deep-
drop refusals. Source/compiler/runtime hashes were stable during the 16.952-s
run. Receipt: .tmp/ownership-cleanup/code-baseline/receipt.json; repeat runner:
.tmp/ownership-cleanup/code-baseline/run_baseline.ps1. The compiler was not
rebuilt here and C2/C3 are absent, so this is no automatic-cleanup performance,
leak/UAF, current-source substitution, installed-default or CI claim. Next
falsifier is the same source/output set on the admitted C2/C3+G3 implementation,
with copy/drop counts and leak/UAF evidence; do not compare model time with
baseline whole-process time as if they measured one operation.

## Latest scoped work: proof checker assurance (2026-10-06)

The user explicitly reopened the Rocq/checker review and requested all three
findings be improved. Implementation and local verification are complete;
changes remain uncommitted on main @
bbc9a713ce0ebbf4ab805037a207e967ee5b0a1a. This is a completed scoped proof/gate
slice, not a successor self-host rung. The active self-host card below is
unchanged. Coordination/evidence:
docs/agent_work_directives/proof_checker_assurance_2026-10-06.md.

AIRBinding/BinaryAdequacy now import the real coordination-expanded machine,
without private config/action/guard copies. The finite certificate core has
reflection theorems and a fresh extraction/execution gate; the Stage 1 smoke
uses scripts/proof_certificate_admission.py, which re-reads bound payload facts.
AssumptionBudget pins the two API types/domain; the kernel gate consumes and
kernel-checks its actual-API export links. Empty/missing approval and same-name
type/domain changes fail closed. CI registers the finite gate independently of
native bootstrap. No SoT registry status or self-host replacement count changed.

Final local evidence: WSL Coq 8.18.0/OCaml 4.14.1, 57 corpus proofs and approval
consumer kernel-checked (two approved assumptions, no unsafe features); fresh
checker extraction with zero assumptions, 262144 finite valuations + four
length controls + 41 envelope controls; kernel negative self-test; actual
57-proof formal smoke, proof spine, language golden, beta freeze, shell syntax,
CI YAML and diff hygiene all pass. Current-source native C build and live
AIR/MIR envelope gate pass; 12 native compiler warnings remain. Executable:
.tmp/proof-checker-assurance/bin/pgy.exe, SHA-256
f2c676a25d124be77a66f70c3268b4975f29ac39d7824fa6bcc4f36cb36ebb77.
Receipt logs are in .tmp/proof-checker-assurance/. Existing untracked
docs/audits/self_host_remaining_bridge_census.md and gmon.out remain excluded.

No commit/push, new Actions run or GUI message occurred. Rocq 9.0.1 exact-source
CI remains unobserved, and no full compiler/backend, signed issuer,
cryptographic verifier, installed-driver inout cleanup or GUI readiness claim
is made. Next falsifiers for this scoped work are a changed required finite
predicate, missing approved export, or changed payload fact under admission;
run the named gates, not a keyword-only replacement. Do not resume this
completed slice as an independent optimization/SoT track.

## Active self-host context

Current authorized scope (2026-10-09): documentary contradiction repair and
read-only final Claude/GPT review only. Compiler/runtime/proof implementation
and P0 execution remain held until post-review start confirmation. The active
navigation owners are `agent_work_directives/ownership_cutover_plan_2026-10-08.md`
and `agent_work_directives/ownership_clean_implementation_2026-10-08.md`;
the old work-split/manual chain is superseded. Doc 27 remains semantic owner,
and its production refinements are OPEN. Do not infer permission from older
cards or a documentation PASS. The new review receipt is
`audits/ownership_cutover_contradictions_2026-10-09.md`.

Previously completed scoped result: all 35 proof-audit findings have a disposition
and local integrated evidence in the top red-team repair receipt. It includes
two real runtime fixes, not compiler ownership-clean implementation or a new
substitution rung. Installed artifacts/CI/SoT and physical allocator closure
remain OPEN; the architecture hold below is unchanged.

### User-directed ownership/DX architecture hold (2026-10-08)

IMPLEMENTATION HOLD supersedes the execution continuation below. The user
stopped the current direction and selected automatic ownership cleanup, not
tracing GC, then requested an algorithm review. Preserve all dirty work and
safety falsifiers; do not restart the old own/carrier/copy workaround chain,
install artifacts, commit/push, or send GUI readiness from its partial gates.
HEAD remains 3658548d24bca3d721e4f1974ac7a10da99f7aa8; review entered with 476
preserved status entries, and adds only docs/rules plus ignored scratch probes.
The existing installed native/driver hashes remain unchanged.

Review and algorithm map:
docs/audits/ownership_dx_architecture_recheck_2026-10-08.md. Recommended internal
algorithm family: admitted physical ownership/loan facts, ownership def/use,
and CFG cleanup elaboration. No tracing collector or universal RC is selected.
This is a design target, not implemented automatic aggregate cleanup.
The unlanded Owned Functional Updates recommendation is explicitly ON HOLD.
Vision/substrate/syntax/region documents now distinguish target from support.

Last observed review gate: installed-native C/LLVM builds accept named inout
and named ref controls; field inout and ref aggregate call-result actuals both
refuse before emission. Negative programs are not run. Source lexical counts:
HEAD 98 own /814 inout /5888 ref; working source 194/860/6214 with the memo's
fixed exclusion boundary. No complete fixed-input failure census exists;
the preceding full codegen diagnostic remains empty without a PASS receipt.
Review-only language golden and hard self-host contract-wiring gates pass;
seven-document local link checks and diff hygiene pass. These are not behavior
or substitution closure. Final review tree has 480 preserved/new status entries.
No fresh seed/DRV-2, automatic cleanup, installed/default public validation or
exact-SHA CI claim is made. Tracked build/analyzer sessions are no longer active.

Before any implementation resumes, enumerate the full reached failure set,
resolve physical sharing/C2 and row-table lifetime unknowns, and fix the complete
source-place/storage/effect/exit -> MIR -> backend -> consumer migration plan.
Next falsifiers include branch-only move, partial aggregate initialization,
temporary/out-store loans crossing cleanup, shared backing through distinct
roots, and explicit plus synthesized double cleanup. Do not grant from a mode,
heap result, scope name or a successful narrow fixture. The original DRV-2 rung
remains open; this review neither substitutes a C path nor opens a new rung.

### SUPERSEDED: DRV-2 manual builder chain / v11-v14 (2026-10-08)

The entire continuation below, including v11-v14 source matrices and seeds,
is historical and MUST NOT be resumed. User-authorized G2 restores the reached
owners and consumers to 3658548d, preserving the named proof/native/retention
work. Removed source and fixture files are recoverable under
.tmp/ownership-cleanup/g2-recovery/. The exact inventory and acceptance scope
are in docs/agent_work_directives/ownership_clean_g2_rollback_2026-10-08.md.
The then-current `ownership_clean_work_split_2026-10-08.md` is historical;
current navigation is the cutover plan and I1-I8 directive named in the Active
card above, with implementation held. No new compiler or GUI readiness claim
follows from rollback.

Latest reached executable rung: official table-query/admission seed PASS on
the preceding preserved dirty source; newer source edits supersede its binding.
Actual gen1/gen2 and the owner-written receipt exist; gen2 SHA-256
204da7ec47b172a97fefd568f94d3632ceb4efb08b9ed5923c224245404cb7ee.
Log: `.tmp/mir_builder_chain_table_query_admission_official_seed.log`;
receipt: `.tmp/mir_builder_chain/codegen-bootstrap-table-query-admission/codegen-seed.output.receipt`.
The real installer FAILED emitting DRV-2 from that typed-source seed in
`.tmp/mir_builder_chain/driver-bootstrap-table-query-admission` (log
`.tmp/mir_builder_chain_table_query_admission_drv2.log`), exit 1 before C
publication or installation: aggregate_release_plan_unproved, checkpoint 4,
node243236. Full hash-guarded flow diagnostic maps it to return result in
CanonicalizeMirArtifactWithAdmittedTopology, not the earlier writer binding.
The original table.names leaf is already Retired at OutputReady; the terminal
request wrongly requires Live for non-Void results. Log:
`.tmp/mir_builder_chain/canonical_writer_flow_trace.log`.
The phase-only candidate was falsified: SC3BON accepted simultaneous inout
carrier/ref carrier.header retirement, and a full analyze-only census found
the metadata sibling case also admitted. The current retiring-call alias owner
checks every physical argument spine and every sealed Retired relation at both
construction and final exclusivity, without granting from ref/type/name.
Fresh computed copies use existing String/scalar owners; projected nominal
local/forwarded/assignment copies use the exact declared-place no-copy fence.
The two new concealed-alias counterexamples were observed accepted, then
refused by v6. All ownership-negative inputs remain analyze-only.

The frozen source matrix 8AwlXY passed C/LLVM 42 positives/93 negatives and
native runtime wfhzxg passed 19 storage + four terminal positives per backend.
Independent review then falsified field-view forwarding, post-retirement
caller read and local handoff. The in-flight seed was invalidated/stopped;
none of these negative inputs was emitted/run. Current definition completion
and physical retiring-call consumers now track those loans, fresh rebinds,
assignments and conditional writes. Existing all-path String allocation-domain
facts discharge only copied result loans, not operand/effect/storage authority
or a whole nominal argument merely because one leaf is Retired.
v10 source-only census passed 46/103, and native runtime 467MTC executed
19 storage + nine terminal-result/view positives per C/LLVM. The later paired
matrix 2PHN4l has no bound final PASS receipt and is superseded. Three additional
analyze-only falsifiers were admitted by v10: a retired operand in a sibling's
index, direct nominal descriptor capture, and sibling-only own rebind.
v11 refuses all three and keeps fresh factory rebind admissible; its complete
47-positive/107-negative census has zero mismatches. Exact demanded leaf
definition receipts now keep site/binding/full path; root restoration also
requires an independent RHS, never merely a live-preserving own result. New
five-case extension is now included in the bound yn0zah matrix: 47 positive +
112 falsifying cases per C/LLVM and generation/physical/body/return-choice
controls PASS, exit 0; its native/inputs/imports hashes and binaries.sha256 are
owner-written. Native runtime 11enTO executes 19 storage + nine terminal-result/
view positives per C/LLVM PASS. Structural IXFoQJ PASS within timeout60:
2803 cap requests, 1123 extractions and 772 reuses; existing caps unchanged.
The exact-generation shape controls also execute independently in C/LLVM.
Fresh official seed FAILED before gen1 in codegen-bootstrap-terminal-generation-v11;
log .tmp/mir_builder_chain_terminal_generation_v11_official_seed.log. Reached
preflight codegen_nominal_array_declaration root control imports the actual
MIR statement code. The analyze-only full diagnostic identifies node71423,
SemanticAstCollectionAggregateTransferFieldViewsReady -> RetiredPlaceRead,
unproved_formal_indexed_read_entry. Exact formal permission/effect observation
finds inferred_types unknown=0/descriptor-retired=0 at the enclosing loop;
resolved_leaf_types stays readable. A raw indexed String local invalidated
that formal, and the all-direct sequence-query candidate still fails because
legacy text primitives have no source borrowed-call receipt. The revised
DefinitionComplete gives only that query an independent Concat-owned type
cell and deep-retires it immediately after the Bool consumer. Physical oracle
uemxCC PASS: copied String/backing retire once after the last query, original
metadata stays live. Revised IXFoQJ structural PASS, unchanged caps: 2804
requests, 1125 extractions, 774 reuses. Unchanged MIR root source diagnostic
PASS (body_ok=true, no diagnostic), exit0 within the original300-second budget;
log .tmp/mir_builder_chain/type_row_owned_cell_root.diagnostic. Bound source
matrix CpW21r PASS: 47 positives/112 falsifiers per C/LLVM plus generation,
physical, body and return-choice controls; exit0, owner-written checksums and
binary receipt complete. Native C semantic suite: 3107 passed/0 failed on the
current isolated build. Revised native runtime I8kDma PASS: C/LLVM each execute
19 storage and nine terminal-result/view positives, plus existing physical
index/type-cell controls and the new view-classification physical free oracle.
This native-only run does not establish installed/default or MIR lanes.
Fresh official seed FAILED during gen1 in
codegen-bootstrap-type-row-lifetime-v12, before seed publication/DRV-2.
Checkpoint21/node124625 is the failure-path release in
SemanticAstArtifactVerdictContractReady. Real exits are mutually exclusive;
the current demand uniqueness owner conservatively lacks disjoint-cleanup
proof. Root is migrating the bounded five-owner set to materialize the outcome
before one final table retirement, preserving guards and error identity.
The complete source inventory and objective card are in the active directive's
Reached callable-table single retirement endpoint section. The bounded set is
now migrated. fXMJBB executes all four actual contracts and the existing table
generation/canonical/payload controls per C/LLVM PASS, with owner-written source
and artifact checksums. The one-retirement structural ratchet and unchanged-cap
IXFoQJ component gate PASS (2804 requests / 1125 extractions / 774 reuses).
NiRdf3 matrix and v13 official seed were invalidated/stopped before publication
when read-only review found an unnecessary copied diagnostic overwritten on
the capability success path. That copy now occurs only in the analysis-failure
branch; initial outcome cells are literals. Neither partial run is current
evidence. Freeze the revised source for the v14 matrix/official seed/DRV-2
continuation; v12/v13 are not current admitted seeds.
wYisBW re-executes the four single-retirement contracts and prior callable
generation/spelling/payload controls per C/LLVM PASS on the revised source;
eight existing warnings per contract build remain. rpuch2 paired source matrix
PASS: 47 positives/112 falsifiers per C/LLVM plus generation/physical/body/
return-choice controls, exit0 and source/binary receipts; nine existing analyzer
warnings per backend remain. v14 official seed FAILED during gen1 at
aggregate_release_incomplete/node59080. Parser-only mapping identifies
`view = snapshot.environment` in CodegenTypeEnvStateAppendValueBinding; this
protected nominal projected copy has no source-placement receipt. MIR root
qPZrcN passes four actual executions/eight refusals; call-argument ztqATn passes
two/twelve. No v14 seed is published. The active directive's Reached type-
environment snapshot consumers card owns the next bounded inventory: both
production projections, their real query/emitter effects and last consumers.
Compiler source was unfrozen only for that migration. The flat String/factory
candidate was falsified by actual factory-return and Bool out-store admissions;
new String member-consumer and residual-view tracking guards refuse both in
Main. Canonical source-selected stdlib input-borrow contracts for Substring,
SubEqualsWithLen and SubIndexOfWithLen now have native identity/arity/ordinal
negative controls (region-escape unit PASS), a generated self-host projection
and registry negatives. They grant input borrowing, not output exclusivity.
The unsafe candidate factory/extra query entrypoint are removed. The existing
global-index lookup materializes only the selected String result, matching the
local Substring result boundary; tables remain shared and precedence unchanged.
Actual binding/statement/assignment consumers borrow snapshot.environment
directly until the one snapshot retirement endpoint. Current source-only
frontier and independent-query-result positive PASS; both formerly admitted
falsifiers now refuse at aggregate_release_element_borrow in Main. Current
source observer: .tmp/mir_builder_chain/type_env_string_projection_stdlib_observer.exe.
Full codegen root analysis/native rebuild are in flight; paired matrix, physical
oracle, fresh official seed, actual DRV-2, install and CI remain unverified.
Do not weaken the projected-copy fence or clone global indices.
Updated structural endpoint/allocation ratchet and language golden PASS. Live
Windows publication contracts pass through the progress gate, then explicitly
fail because no prover is installed there; IXFoQJ WSL Coq8.18/OCaml4.14 proof
spine/corpus57/kernel/262144 finite+4 length+41 envelope controls PASS. This is
local Coq8 evidence, not exact-SHA Rocq9 CI or a full compiler-suite claim.
Its preceding failed MIR import root now passes actual source-C generation and
execution: eW4HUM four executions/eight pre-emission refusals PASS; call-argument
ZlW1fK two executions/twelve pre-emission refusals PASS. Fresh gen1/gen2 seed
evidence remains required after migration;
do not weaken the permission boundary or remove the root control. Actual
DRV-2 remains pending. Root is sole implementation/integration owner; the
scoped reviewer is read-only. Commit/push scope includes all preserved dirty
work, including gmon.out; earlier scoped exclusions do not apply. No new
installed driver, default-driver cleanup, commit/push, exact-SHA CI or GUI
readiness message yet. All ownership-negative inputs remain analyze-only.

Supporting current-source gates: cOB5sr C/LLVM each 38 positives/83 falsifiers
and generation/physical/body/return-choice controls PASS; production probe
FK3GZx C/LLVM 200 spelling controls, copied payload after retirement, retired
admission, stale foundation and three forged table universes PASS. Exact-source
WSL verification snapshot 4nJXQm component inventory PASS inside the unchanged
60-second gate: 2793 size requests, 1118 function extractions/770 reuses.
This is a verification copy, not a Git worktree or implementation checkout.
Hard contract PASS. A world gate attempt used the old installed pair and its
manifest compilation was interrupted at the 60-second boundary (exit143);
that is incomplete evidence, not a semantic verdict or a green world gate.

The prior node116967 refusal is identified, not open speculation: diagnostic
and parser-only lookup trace the table through AnalyzeTyped/capture to the raw
local canonical-name binding60597. Queries now borrow the cell directly; their
last equality consumer uses admitted StringLength/CharCode primitives without
a new whitelist or allocated spelling. Persistent graph publication remains
copied. Table generation/raw matching live in the independent admission owner;
the expression environment retains row/scope production, and the generic C
alias delegates to the one typed factory. Existing source caps stay unchanged.
Read-only review found no new production permission bypass, dual owner or
retirement-order regression in that preceding scope. Five test-only shape
witnesses now have independent positive baselines and final table cleanup; two
stale-admission witnesses and the verdict witness also retire after their last
consumer. These changes still need fresh bound gate/seed evidence; they do not
close the cross-actual semantic blocker.

Everything below in this active card records reached supporting/previous
graphs. The current continuation is terminal-result/cross-actual lifetime above,
not an older seed.

- Objective: root alone completes the MIR builder input/result/restoration/
  retirement chain through fresh seed, actual DRV-2, installed/default cleanup,
  the full authorized dirty commit/push and exact-SHA green required CI. Follow
  `docs/agent_work_directives/mir_builder_ownership_chain_2026-10-07.md`.
  No parallel implementation, unrelated optimization or general C2 track.
- Checkpoint: main @ `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, dirty source
  including preserved proof, native
  assignment, MIR and user work. The user authorizes the entire preserved dirty
  publication scope (including gmon.out); earlier proof-slice exclusions above
  are historical. No commit, push, install or GUI readiness message yet.
- Current independent callable-table candidate (supersedes the graph below):
  analysis no longer owns callable storage. One declaration foundation feeds
  both analysis and the separate table; body/domain/codegen receive that same
  table, and JSON/C roots retire it after their writer. Raw/mutable entrypoints
  compare all table rows with its real producer; matching POD headers are not
  payload admission. Capture and return projections materialize retained text.
  Native C generation PASS for codegen (0 errors/16 warnings) and the actual
  compiler entrypoint (0 errors/21 warnings). The production resource probe
  passes C and LLVM execution: copied graph payload after retirement, retired
  admission refusal, stale foundation and three forged table universes. Log:
  `.tmp/mir_builder_chain/table-resource-generation.log`; bound evidence:
  `.tmp/self_hosted/callable-table-resource.uY0Fmp`.
  These are bootstrap/component evidence, not installed-driver closure.
  A first official seed attempt stopped at Windows output-path conversion
  (`/d/.../native.c` reached the native executable unconverted); the valid
  codegen control had run, but native publication failed. No semantic rejection
  or seed receipt is claimed. Fresh official attempt v2 FAILED with normal
  MSYS path conversion: MIR root control, checkpoint 21, node 23939,
  MatchArtifact's single expected-table release. Identified gen0 SHA-256:
  cd1c8dd505ab9ed017696d43942444db7f0ab8e7b3f1ca7956ba2788e3c4739e;
  log:
  `.tmp/mir_builder_chain_table_resource_v2_official_seed.log`.
  Hash-guarded native diagnostic observes same demand/origin/field/requirement
  for two distinct producer returns (empty failure vs fresh normal table), not
  two executable releases. Root now records source-bound invocation return
  choices; same-demand alternatives still require nonrepeating retirement and
  every independent constructor/source/physical-placement guard. Caller
  function/site and synchronous return context are bound; missing/crossed facts,
  same-return duplicates, different calls and repeated demands refuse.
  Focused C-source analyzer v3 PASS: conditional producer positive, repeated
  release, shared placement, loop-held release/read and deferred-return refusal;
  provisional same-return/call/site/caller/generation/shape guard unit PASS.
  Targets are analyzed, never emitted/run. Analyzer SHA-256:
  57fee5cb318fbf1b1c277639b2a87a8c2cbd279a2a35147fd1801add2dfe19c7.
  Logs: `.tmp/mir_builder_chain/return_choice_v3_*.log`; focused compile
  0 errors/9 warnings. New fact publication and initial-state owners keep the
  existing 600/315/190 source caps unchanged; no SoT row or substitution count
  has been promoted. Full current-source matrix 4mpiSV PASS: C/LLVM each
  38 positives/83 falsifiers plus the generation, physical/transfer occurrence,
  body and return-choice controls; source/import/executable bindings rechecked.
  Log: `.tmp/mir_builder_chain_table_return_choice_source_matrix.log`.
  Fresh official return-choice seed passes the previous MIR root control,
  then FAILS self-source admission: node 116967,
  aggregate_release_element_borrow, String. Parser-only location is CheckCUnit's
  AnalyzeTyped call in program_entry_owner.pgy; returned payload versus transient
  table-element binding is under investigation, not a proved ownership leak.
  New gen0 SHA-256:
  270ba3933263357e3ba041af279a996b19e622819a59c1fad937f40274cdb2a9;
  log: `.tmp/mir_builder_chain_table_return_choice_official_seed.log`.
  No gen1/gen2 or seed receipt. Fresh static inventory, fresh official seed,
  actual DRV-2, installed/default cleanup,
  full authorized commit/push and exact-SHA required green CI remain pending.
- Superseded frozen source graph:
  `e97527f2c88be37b3ca885ec688882ed0ae5fac2b14805ffea1a9dc6ecc04d15`.
  The routine input now projects exactly seven consumed semantic families
  rather than the complete analysis. Producer-time generation metadata is
  checked without reading retired traversal lanes; raw deep admission and
  the existing receipt still own payload validity (including count-only enums).
  Current source matrix b6964J PASS: C/LLVM each 37 positives/79 falsifiers,
  all generation/dependency/physical/transfer and body guards, with input,
  import and executable bindings revalidated. Log:
  `.tmp/mir_builder_chain_routine_view_source_matrix.log`. The unchanged
  60-second structural gate PASS: 2780 cap requests, 1115 extractions/765
  reuses; log `.tmp/mir_builder_chain_routine_view_component.log`.
  Production generation probe PASS: two positives, 15 generation guards and
  actual raw same-count mixed-enum refusal. Log:
  `.tmp/mir_builder_chain_routine_view_generation.log`. These are supporting
  gates, not current seed/DRV/default-driver closure.
  The non-installing previous-seed diagnostic on this graph FAILED at node
  131519, the new SelfMirRoutineSemanticFacts initializer in
  SelfMirProgramFactsBeforeCanonicalIdsObserved. Do not infer its demanded
  leaf from the previous refusal. The hash-guarded multisite diagnostic clone
  completed: sibling String views remain unproved, and review confirmed graph
  target-name publication retained a table cell. This falsifies the old view
  as a sufficient resource boundary and motivates the candidate above. Log:
  `.tmp/mir_builder_chain/demand_path_131519_trace.log`. Fresh official current
  seed, actual DRV-2, installed/default cleanup and publication remain pending.
  The following b2ff98d2 receipts are superseded source-graph evidence.
  Actual DRV-2 on the previous 34358cba graph FAILED at the same receipt query,
  node 219269; the shifted node id was not progress to another consumer.
  The driver now publishes `DriverRung2VerifiedBodyFacts` without analysis;
  verification updates the caller's analysis by the existing inout contract,
  and projection owns the two independent inputs. Canonical rebinding reads
  constructors from the returned live projection, not an alias across transfer.
  Old combined-carrier/API and unused verification wrapper are deleted/ratcheted.
  The mixed own-analysis verification wrapper is also deleted; all five real
  callers now verify a local analysis before transferring analysis and verified
  body independently. The analyzed small combined carrier reproduces the
  receipt-call refusal; separate inputs with real owned-push/restoration pass.
  Previous source matrix fPNyL3 PASS: C/LLVM each 37 positives/79 falsifiers,
  with generation/dependency/transfer identity, physical occurrence and body
  control guards. Native/input/import manifests revalidate. Log:
  `.tmp/mir_builder_chain_projected_fields_source_matrix.log`. Earlier graph
  receipts are superseded; this matrix analyzes targets, never executes unsafe
  ownership-negative programs.
  The new split fixture is source evidence only: native C compilation refused
  its mixed element ownership, so no runtime success is claimed for that input.
  The non-installing diagnostic using the identified previous gen2 FAILED on
  previous graph 77cf35c7 at node 80396, inside
  SemanticAstBodyTypeBundleAdmissionReceiptReadyFor, with
  aggregate_release_plan_unproved. The earlier split graph failed at the
  initializer producer (node 76433). Neither is a fresh seed/DRV/install receipt.
  Exact observation is complete: relation 24, analysis formal 80391, demanded
  function_tables/names path 38840/34768, live phase 1; the first rejected
  sibling argument is analysis.signatures at physical edge 276379, ordinal 1
  of SemanticAstCollectionFormalEffectsReadyFor. The identified release clone
  reproduced that refusal; the O0 diagnostic was stopped without a verdict.
  Current source proves actual used field/count/index and all String-element
  consumers, not a type/ref/Bool grant. Unused unsupported nominal fields do
  not grant permission to use them. Descriptor captures, returns, mutation,
  forwarding, unknown used fields and retiring sinks remain unproved. POD
  indexing closes only String-lifetime obligations, never general mutation,
  backing-address escape, copy or storage authority. Exact canonical String
  array/slice predicates replace equivalent substring queries, not type policy.
  Native C/LLVM projected-query runtime PASS (projected-query-pass); the split
  inout owned-String fixture remains source-only with its native refusal visible.
  Previous full structural gate PASS: unchanged 60 seconds, 2778 cap requests,
  1115 extractions/765 reuses, 123 MIR files/93 ref signatures. Log:
  `.tmp/mir_builder_chain_projected_fields_component.log`. Static inventory is
  not executable closure. Fresh official seed PASS at
  `.tmp/mir_builder_chain/codegen-bootstrap-projected-fields`, log
  `.tmp/mir_builder_chain_projected_fields_official_seed.log`: gen0 built
  with 16 warnings/zero errors, call-argument 2 executions/12 refusals, status
  diagnostic controls and nominal receivers 4 executions/8 refusals PASS;
  actual gen1 -> gen2 completed. Gen2 SHA-256:
  `49cfcca34b32a64b05ef16b2e10e86821dfc7b554c96fa9754973969eb5dcdc2`;
  emitted C `ba36a6d0a1392683d54f6ae860c64a84af01488d90d570d2353bad568ab65568`.
  This seed-only receipt is not gen2/gen3 fixed point or whole-compiler evidence.
  Actual fresh DRV-2 FAILED before C emission/installation at
  `.tmp/mir_builder_chain/driver-projected-fields`, log
  `.tmp/mir_builder_chain_projected_fields_actual_drv2.log`: checkpoint 4,
  aggregate_release_plan_unproved, node 131494. Parser-only resolution names
  SelfMirProgramFactsBeforeCanonicalIdsObserved in mir/artifact_lower_owner.pgy,
  the SelfMirRoutineInput initializer. Its exact observed relation is 25,
  value formal analysis 131448, function_tables/names path 38840/34768,
  live phase 1; ordinal 1/edge 461474 requests move access 1 from mode 0.
  Log `.tmp/mir_builder_chain/demand_path_131494_trace.log`. This identifies
  the old whole-carrier failure, not the new view's demanded relation.
  Only the official installer may replace
  bin/pgy-self-driver.exe after its bounded smoke and machine-manifest parity.
  Installed binaries are unchanged. Default gates/commit/push/CI remain pending.
  C analysis/table cleanup now follows its final emission consumer; the
  installed-mode gate requires exactly one emission-done before one table
  release marker. This ordering has not yet been executed through a new driver.
  Conditional full declared paths, exact locations/restoration holes and
  source-owned control/dependency sealing feed result/placement admission.
  Concrete storage, generation, alias, reservation and publication owners remain
  distinct. Old global restoration reconstruction and own-Void grant are gone.
- Reached production carrier changes: borrowed CodegenTypeEnvState Borrow/View
  returns are deleted. Scope forks and local-row snapshots own independent
  Array<String> storage; global/preseal rows and indices remain shared.
  One block retirement follows the statement dispatcher, replacing nineteen
  branch-local retirement sites without relaxing scalar-own or defer guards.
  Seed 32c8db3d passed bounded controls but refused gen1 at node 87599 because
  the admission-error bundle reused specialization actual-type rows as leaf-type
  rows. Those fact families now have independent descriptor sources; paired
  independent/reused error-row controls preserve the retention refusal.
  EmitLet moved byte-identically to let_declaration_emit_owner.pgy; its old
  definition is ratcheted away and declaration/ABI predicates follow the owner.
  The existing 800-line legacy cap was not raised; the new owner is capped at 150.
  Seed cd75e802 refused the release reader at node 88010,
  unproved_formal_element_use_entry. The reached metadata cell now crosses its
  query through array_storage_type_cell_lifetime_owner.pgy: fresh String ->
  one-cell owned array -> query -> exact local retirement. The raw metadata
  table remains borrowed/live. An inline copied String passed analysis but
  leaked; query-before-literal-transfer failed the existing exclusivity guard.
  Neither attempt is a completion receipt. No Bool/name/builtin grant was added.
- Previous full source receipt on 46dd4299 (all-import binding superseded):
  `.tmp/self_hosted/aggregate-release-source.8YW5al`, C/LLVM each pass
  30 positives/66 falsifiers and generation/dependency/transfer laws.
  Log `.tmp/mir_builder_chain_owned_type_cell_final_source_matrix.log`.
  Native/input/import manifests independently revalidate. Analyzer SHA-256:
  C `10dd584f608087fda9c9ad4c853da1b755899efc64f2942de0bea43e4f8b8a8f`,
  LLVM `0cb001d7fc332fcc719c3ebbf05266e3bb095b9d683ca5cf9d3cbae14a9b1877`.
  These gates analyze inputs, not target runtime or installation.
- Previous full structural inventory PASS, log
  `.tmp/mir_builder_chain_owned_type_cell_full_component.log`: unchanged
  60-second budget on the read-only ext4 source mirror
  `/home/c/.cache/pgy-mir-builder-validation/source.YoMSAC`, not a Git
  worktree/implementation branch. 2773 cap requests, 1115 function extractions,
  756 reuses; 123 MIR mutable-target inventory files and 93 readonly signatures.
  This is source/removed-path evidence only. Earlier mounted-tree timeouts and
  eMR4Mc expanded-matrix timeout remain incomplete, not green receipts.
- Previous bounded runtime: production type-cell owner native-C actual-free
  probe `.tmp/self_hosted/array-storage-type-cell.0uIp3K`, four independent
  cell/backing pairs retired exactly once after query completion; original
  metadata remains live until its own final drop. Log
  `.tmp/mir_builder_chain_type_cell_lifetime_runtime.log`. This is not a
  default-driver or general C2 claim.
- Earlier focused runtime evidence: type-env DXO5vG on graph 32c8db3d passes
  C/LLVM values, identity guards and sixteen lifetime refusals per lane; its
  actual-free probe r7hNj0 verifies four independent snapshots/shared global
  backing/exactly-once local frees. Its all-import receipt is superseded by later
  production carrier/module edits. Native semantic tests pass 3107/0 on the
  unchanged native C sources, log
  `.tmp/mir_builder_chain_borrow_capture_native_semantic.log`.
  MIR append/copy/retire rerun X5Yt9n passes on 46dd4299, log
  `.tmp/mir_builder_chain_owned_type_cell_mir_copy_retire.log`; this is actual
  bounded runtime evidence, not an installed/default-driver result. The final
  type-env rerun on 46dd4299 passes C/LLVM values/identity/sixteen lifetime
  refusals and four actual-free snapshot controls: pCUm7J / 0272Z9. These
  all-import bindings are superseded by the later compiler-only migration.
- Official seed from frozen 46dd4299/source-bound native f9b33615 PASS at
  `.tmp/mir_builder_chain/codegen-bootstrap-owned-type-cell`, log
  `.tmp/mir_builder_chain_owned_type_cell_official_seed.log`. Native gen0 ->
  actual Pergyra gen1 -> actual Pergyra gen2; argument controls 2/12, status
  controls and MIR receiver controls 4/8 PASS. Gen2 SHA-256
  `88adc2a446c736f412c1f8072be2f022d19c7bedce995b57b74b5151db198444`.
  Seed-only is not gen2/gen3 fixed-point or whole-compiler evidence.
- Actual DRV-2 from that gen2 FAILED before driver C emission/installation:
  build `.tmp/mir_builder_chain/driver-owned-type-cell`, log
  `.tmp/mir_builder_chain_owned_type_cell_actual_drv2.log`, node 219242,
  aggregate_release_plan_unproved/checkpoint 4 in DriverRung2MirProjectionJson.
  It combines a MIR-facts String result with an inout projection's terminal
  analysis lifetime. The new facts-only DriverRung2MirFactsJson removes that
  combined universal claim. All four callers explicitly retire their concrete
  projection after the JSON last consumer. Old combined entrypoint is deleted;
  conditional preservation/retention guards remain unchanged.
- Current narrow source pair passes: copied JSON result survives retirement;
  returned original element refuses aggregate_release_element_borrow/Main.
  The positive also executes through native C/LLVM with copy:value. Current
  full structural gate PASS, log
  `.tmp/mir_builder_chain_projection_split_final_component.log`, same 60-second
  budget and 2773/1115/756 cap/extraction/reuse counts. 530-code-line legacy
  driver cap is unchanged. Current source matrix PASS on bc7e3ca1:
  `.tmp/self_hosted/aggregate-release-source.TzJ1D0`, C/LLVM each 31 positives
  and 67 falsifiers plus generation/transfer/location/source-control laws.
  Native/input/import/binary receipts independently revalidate; analyzer C
  169e3ebfbec27eaaafdf8567ebb9840f58826ae6719ac69810d3aee45ec4498b,
  LLVM 228b2ffbbdc895ee33949407ee86b0e1ec188b6e0e660f7fbc221a49883ff054.
  Native public ArrayDrop enIzQ8 passes 19 positives per C/LLVM, 32 source
  refusals and declaration-order controls; production type-cell actual-free
  k06lfN passes. Logs projection_split_final_source_matrix and
  projection_split_native_array_drop. Keyword inventory officially regenerated
  and checked; SoT 95/203 and 70/23/2 unchanged. Default/MIR lanes are not yet
  claimed. Official source-bound seed on bc7e3ca1 PASSED:
  `.tmp/mir_builder_chain_projection_split_current_official_seed.log`, gen2
  SHA-256 `456c3e7e35e4ad446828142f3003327543d9d6cce2ee32437b39ca22c4012706`.
  Actual DRV-2 then refused node 39071/checkpoint 4 in
  SemanticAstArtifactAnalysisMatches, log
  `.tmp/mir_builder_chain_projection_split_actual_drv2.log`. No C emission or
  installation occurred. New raw admission edits supersede bc7 all-import
  bindings; they are not silently reused as current-source receipts.
  Interrupted f08a8edf runs are incomplete, not closure evidence.
- Current reached seam: DriverRung2RequireRawAnalysisAdmission deeply checks
  raw/mutable analysis with the existing value-input query contract before the
  own-transfer boundary. The attempted ref wrapper failed native admission;
  its diagnostic run was stopped, not counted green. Canonical MIR facts now
  use a distinct result binding instead of reading the consumed input.
  Both raw consumers (writer probe and safe mutable-analysis falsifier) migrated;
  five mixed raw-check/own-transfer wrappers are deleted. Deep fact predicates
  remain unchanged. No Bool/mode/type grant or shape-only admission is added.
  A non-installing current-driver diagnostic uses the identified bc7 seed at
  `.tmp/mir_builder_chain/driver-raw-value-admission-diagnostic`; this is
  discovery, not fresh source-bound seed/DRV closure. Native compilation now
  admits the corrected query/canonical-result boundaries. The native runner's
  footer was invalidated by a concurrent script-only deduplication; that run
  is not a gate PASS. Independently executing its identified binary
  `05e7074cadc95a4a4c0f2730708db74d985ca35c5688d1362ab30a25edc90127`
  observes exit 1/deep-proof diagnostic/no body-types entry, log
  `.tmp/mir_builder_chain/raw-value-mutation-preflight.out`. Full structural
  inventory passes, log raw_value_admission_component_final_129, unchanged
  60-second budget and 2773/1115/761 counts. The runner remains under its
  unchanged 130-line cap by removing duplicate checks, not conditions.
  Source matrix ouiDCz passes C/LLVM 31 positives/67 falsifiers on 164cbba4;
  all-import binding is superseded by the fatal diagnostic transport edit.
  The final default-driver mutation
  gate and writer byte parity remain mandatory after actual installation.
- Latest whole-driver diagnostic on 164cbba4 refuses node 219096 in the
  admitted-analysis verifier's public diagnostic call. The public formatter
  may return the original message and carry String-containing diagnostic facts.
  Current 8a873313 independently materializes only the fatal diagnostic's fixed
  String fields; original diagnostic/span policy and Exit(1) remain unchanged.
  The owned-carrier transport positive passes source admission and actual C/LLVM
  execution; original forwarded metadata/borrowed result negatives still refuse.
  Source 32/67 matrix passed on 8a873313 with nnFaiK; manifests independently
  revalidate. Its non-installing discovery refused node 219167/checkpoint 4 in
  DriverRung2MirProjectionFromVerifiedFactsObserved, at the exact body receipt
  query. Neither the receipt check nor its generation/count predicates are
  deleted to get past the refusal.
- Current reached metadata-query seam: the old String-only formal owner is
  deleted. ast_text_formal_borrow_owner owns the same no-retention fixed point;
  ast_text_borrow_projection_owner owns exact physical member consumption.
  Admitted constructor facts cross every formal-effect producer. Closed
  String/POD struct/tobject shape only selects a candidate; all String edges,
  exact forwarding, deferred uses and retiring writable call graphs still
  decide admission. Artifact/signature/constructor/surface counts and digests
  must identify the same positive current generation. One map feeds aggregate
  preservation and member-element escape admission; no type/ref/Bool grant.
  Exact constructor lookup replaces redundant canonical text reconstruction.
  The child-type result is independently published; its recursive query is
  the final consumer, not proof of whole compiler String lifetime closure.
- Supporting first metadata matrix passed C/LLVM 33 positives/72 falsifiers
  on 4ad3eb4c, receipt GhtHun, with independently revalidated manifests.
  The current 34358cba rerun adds a retiring metadata writer, a shared DAG's
  safe POD edge plus retaining edge, and jointly stale/mixed-signature controls:
  33/73 matrix xURHSc passed in C/LLVM, including all three stale/mixed
  generation controls and the retiring-writer map refusal. Safe metadata
  query execution also passes native C/LLVM with metadata-query-pass; binaries
  475a45558ade970c23aa249c2c6469d0ee9e379619bbf12b13b383ab9dc75ea5 and
  011a3e8d84d2370bec02fda954ef6bfad1ea400315285a618304e76c25597f1b.
  Fresh official seed passed at
  .tmp/mir_builder_chain/codegen-bootstrap-metadata-query, log
  .tmp/mir_builder_chain_metadata_official_seed.log. Gen2 executable SHA-256
  e9d2843f250aed2a69feba3b05aa7f0619a5591e746aa052ab4b3e1863a17996;
  C source 97174475763130bef924fdcb996409e535749d385786f3976edbba7960a4878f.
  Argument 2/12, status and nominal receiver 4/8 controls pass. This is
  seed-only, not gen2/gen3 full fixed-point. Actual DRV-2 is running at
  .tmp/mir_builder_chain/driver-metadata-query with an isolated output/receipt,
  log .tmp/mir_builder_chain_metadata_actual_drv2.log; installation is unchanged.
  No DRV-2 PASS is claimed. Structural inventory found two unchanged cap blockers:
  formal-use signatures 439/437 and matrix runner 165/155. The completed
  runner's metadata subgate now lives in its named 50-line owner; unchanged
  runner cap 155 is met, and both standalone C/LLVM controls pass. Finish frozen runs
  before editing; do not edit a running Bash script or hash-bound Pergyra input.
  Canonical constructor retention/body detachment/post-table-retirement reads
  remain whole-driver falsifiers, not assumed solved by the query map.
- Active executable rung: fresh official seed -> actual DRV-2 -> installed
  default cleanup on the current frozen source. Build publication remains atomic; the old
  launcher/driver/physical manifest are backed up under
  `.tmp/mir_builder_chain/pre_install_owned_type_cell`. Native bin/pgy.exe
  has not been replaced yet. No native driver-source retry is admitted.
  Keep unchanged 1800-second integration and 300-second focused budgets.
  Do not edit src/self_hosted .pgy during hashed gates or builds.
  Next falsifiers: actual driver build, installed/default inout/plain-result release and required
  current-commit CI. No native driver-source retry or stale receipt is admitted.
- Registry boundary: 95 authorities/203 derived carriers; CLOSED=70,
  BRIDGE=23, ACTIVE=2 unchanged. Collection ownership/general C2 are not closed
  by these supporting gates. Latest observed Actions 37469063492 on b9a75ba2
  is red at the older named Array<String> routine boundary, not this source.
- GUI boundary: readiness only after actual installed/default evidence and
  completed scoped validation. Consolidated GUI intake is recorded separately
  in `docs/audits/gui_compiler_boundary_backlog_2026-10-07.md`; its C-shadow,
  alias/visibility, aggregate Slot, ADT/FFI and performance reports are not
  independently verified current facts or successor implementation rungs.
  Do not claim whole GUI readiness from one inout cleanup contract.

## Historical archive boundary: superseded execution cards

Everything below is lookup evidence, not an active implementation queue.

### DRV-2 MIR builder ownership-chain candidate (2026-10-07)

- Active candidate: source-owned control selection for the same DRV-2 full-path
  result chain, graph
  `c3e34496f98ec589f24f0bb600798c6813ce41d1753a83fff43dad097765dee3`.
  HEAD remains `3658548d24bca3d721e4f1974ac7a10da99f7aa8`, main; 189 dirty
  entries were observed, including the preserved proof/assignment/MIR candidate
  and user gmon.out. Root is the sole implementation/integration owner.
  `ast_body_flow_control_selection_owner.pgy` now owns If/Match arm order,
  literal reachability, implicit continuation and While/For zero iteration.
  BodyFlow's actual branch/loop folds consume that projection; the old inline
  selection and OfIf/OfMatch paths are deleted. The later source-only verdict
  remains at its original assembly phase. No owned-result grant was added.
- Last green focused gate: source receipt
  `.tmp/self_hosted/aggregate-release-source.g1A1cD`, log
  `.tmp/mir_builder_chain_control_selection_source_matrix_sealed_scope.log`,
  explicit inner exit 0. C/LLVM both pass twenty release positives/forty-six
  falsifiers, generation/transfer guards, six physical-occurrence controls,
  seventeen source control selections with malformed-input guards, and fourteen
  existing/new body-return controls. All four hash manifests independently
  revalidate. Analyzer SHA-256: C
  `c1ff73b2b87c15e0144155e9907da08c4ad59a84ef8eac7b67ff0b2c0915f21c`,
  LLVM `3e31bd1e5ce2f5ed2aedaf3a3b4a0840ba9212ced91bbd5d355b83207d7cbcff`.
  Both builds report zero errors/nine warnings. Supplied programs are analyzed,
  never emitted/run; this is neither target runtime nor driver evidence.
  Earlier unit failures were a borrowed-ref copy in the test and its invalid
  one-argument Result Err binding. The test uses explicit inout fault injection
  and Result<Int, String>; old identified analyzers independently admit the
  corrected input. No compiler permission was relaxed to satisfy the test.
- Current compound owned-result falsifier remains refused by both identified
  g1A1cD analyzers at UpdateCfgAppend -> UpdateRowsAppend, node 49,
  aggregate_release_plan_unproved. Logs
  `.tmp/mir_builder_chain_control_selection_compound_c.log` and `_llvm.log`
  are byte equal. Observer exit 0 is not success: both report body_ok=false.
  Next: the conditional full declared input/output path, token/location and
  restoration relation, all return/join/backedge outcomes, typed operation and
  call dependencies, and recursive component seal from the reconciled audit.
  Keep EMPTY unchanged on a skipped path, outer-scope loop carriers valid, and
  iteration-source freshness distinct from the carried target. Do not grant
  from type, own mode, an owned-push flag or a visited marker.
- Control selection explicitly requires one admitted analysis generation
  (FromAdmittedFacts). Its projected digest/count is not an independently sealed
  permission receipt. Same-count cross-artifact fact mixing remains an untested
  relation-admission falsifier; bind facts at the existing one-time owner
  boundary before using the projection as result evidence. No per-control AST
  reconstruction or independent control-policy owner is authorized.
- Last production integration was on the preceding fixed graph
  `055515e1811a313a400d147c758d77bfdfba381d0664bb6179ef71f39c070286`,
  not current graph c3e34496. Official isolated seed-only passes, inner exit 0, in
  `.tmp/mir_builder_chain/codegen-bootstrap-leaf-placement-055515e1`, log
  `.tmp/mir_builder_chain_leaf_placement_seed.log`. Independent artifact-receipt
  validation passes and gen1/gen2 C are byte equal, SHA-256
  `cb3a47c17ac30fb5c0852bba5f3a3a916f146132d0e8779ac2523d5544546cb4`;
  gen2 executable SHA-256
  `98f6c0dbecabe70ff401e122fe1154aa8c28356784f1b254326b5f04a3675e90`.
  Source graph revalidates after the build. Fresh call-argument (two executions/twelve refusals),
  bootstrap-status and MIR-root (four executions/eight pre-emission refusals)
  prefixes pass; MIR receipt
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.RRpHpC`.
  This seed-only result is not a gen3/full fixed point. Actual source-bound
  DRV-2 refuses, inner exit 1, in `.tmp/mir_builder_chain/driver-leaf-placement-055515e1`,
  log `.tmp/mir_builder_chain_leaf_placement_driver.log`, normal release profile
  and unchanged 1800-second integration budget. Native emits only the physical
  manifest; the fresh Pergyra-built gen2 alone attempts driver source. Pre/post
  source graphs match. Refusal is aggregate_release_plan_unproved, node 123287;
  independent parser observation (241300 nodes) maps it to
  SelfMirCfgAttachLastDestructure -> SelfMirDestructureFactRowsAttach. Log
  `.tmp/mir_builder_chain_leaf_placement_parser.log`. No driver artifact is
  admitted. Do not reuse this seed as current c3e34496 evidence.
- Scoped caps, old-selection residue ratchet, shell syntax and diff hygiene
  pass. The broader SoT edge gate fails at the existing lexer.language_word_registry
  enforcement text `146 rows`, absent from tests/language_keyword_registry_smoke.sh.
  That script was unchanged; no lexer or unrelated SoT repair was opened.
  Full component inventory remains unobserved beyond its prior 60-second
  timeout; no green prefix is a full pass. No installed overwrite, commit/push,
  GUI message, CI-green, SoT/C2 closure or substitution claim.
- Previous bounded checkpoint: graph
  `b5e1380fb07fbe0a3b5a965a5c742881f332e1849308ab40c4d5658da4e8ce23`
  passed nineteen positives/forty-four falsifiers, generation/transfer identity
  guards and six ordinary/shared-node occurrence owner controls per C/LLVM,
  observed exit 0, receipt `.tmp/self_hosted/aggregate-release-source.p1JkX9`.
  Analyzer SHA-256: C
  `d42607145f54089e044d0d345c00ef8955ab76fb52c78ba273d9ba8e9f54134e`,
  LLVM `a5b9fdb9edae5a8c9adbd6eaf7062fb0ef477a416301450d79df02395c4f2193`.
  Four hash manifests independently revalidate; nine warnings each remain.
  That matrix did not seal sibling independence: root subsequently reproduced
  `aggregate_leaf_sibling_alias_negative.pgy` accepted by both analyzers.
  Compile-only logs `.tmp/mir_builder_chain_leaf_sibling_alias_c_source.log`
  and `.tmp/mir_builder_chain_leaf_sibling_alias_llvm_source.log`. The new
  source-generation positive passes the identified prior C analyzer; this is
  not current-candidate parity or runtime evidence.
- The isolated b5e1380f official seed-only run was deliberately cancelled
  after the accepted falsifier, explicit inner exit 143, unchanged pre/post
  graph. Its call-argument and bootstrap-status prefixes pass; MIR-root/gen2
  are unobserved. Log `.tmp/mir_builder_chain_intermediate_alias_seed.log`.
  No produced stale seed, successful prefix or analyzer pass is DRV-2 evidence.
- Historical graph2f checkpoint: C/LLVM aggregate release gate passes sixteen
  positives/thirty-nine falsifiers plus generation/transfer identity guards,
  observed exit 0, receipt `.tmp/self_hosted/aggregate-release-source.mWZe8P`.
  Source, supplied inputs, native and analyzer hashes independently revalidate.
  Analyzer SHA-256: C
  `933ae7ac94e154b78c5d72fe5495ca1888a4bbdf20ba96e05cdffa273d50ff27`,
  LLVM `9b72d18ae6d6aa3729befc381ba07ae9a798caaa4adaf5279d2d5191a32a81e8`.
  Both retain nine warnings; supplied inputs are analyzed only. The transport
  owner seals an own RHS before admitting its formal's assignment write place;
  generation distinguishes an unrelated rootmost declared sibling-field write.
  Recursive proof depth is carried across the write-place boundary, not reset.
  Current fixed graph is
  `2f2ef0ac4574d110adad2e457429a442c64ccb88ecf2c488a8736522535b36e2`.
  Isolated official seed-only production later passed, observed exit 0, in
  `.tmp/mir_builder_chain/codegen-bootstrap-write-place`, log
  `.tmp/mir_builder_chain_write_place_seed.log`. Its gen2 SHA-256 is
  `1f408fd5e3b10c3628f03fa9bf0858f1a717433dac96bc78ab87c55afaa4c474`;
  no actual DRV-2 attempt or latest-source evidence follows from that seed.
  A current whole-driver source observer did not finish at the unchanged
  300-second budget (inner exit 124, empty output), log
  `.tmp/mir_builder_chain_write_place_full_source_observation.log`; no semantic
  verdict or permission to increase its budget follows.
- Next compound falsifier: `aggregate_owned_cfg_update_loop_probe.pgy` with
  its functions file covers recursion, wrapper return, conditional/zero loops,
  readonly-source owned-copy and last-consumer retirement. Identified native
  C/LLVM execute `0,3,4,4,first,even`, zero errors/warnings; this is behavior, not
  element-authority or instrumented allocation proof. Current C/LLVM source
  analyzers both refuse at `UpdateCfgAppend -> UpdateRowsAppend`, node 49,
  `aggregate_release_plan_unproved`. The shallow-copy mutation is analyzed only
  and reaches that same earlier refusal; its specific later rejection remains
  unproved. Neither input is recorded as a green source-matrix case. Read-only
  whole-chain review: `docs/audits/mir_builder_owned_result_chain_2026-10-07.md`.
  The next semantic dependency is exact demanded-leaf conditional owned-result
  preservation, all continuing branches/backedges and grounded recursive
  component sealing, including different nominal return wrappers. No mode/type
  grant, UNKNOWN promotion, growing-CFG clone or general C2/query track is chosen.
- Previous executable checkpoint (graph below): C/LLVM aggregate release gate
  passes fourteen positives/thirty-eight falsifiers plus generation/transfer
  identity guards, exit 0, receipt `.tmp/self_hosted/aggregate-release-source.a2WDSe`.
  Imports, supplied inputs and native hashes independently revalidate. Analyzer
  SHA-256: C `129cc956c3d12b4b9d5613047c1c10987d492f1e4de142b17bc803f85d8b9ab2`,
  LLVM `d697e78f2b44eef74a52f2ed9bd786a609cec1f62707bc82f7b63fd496d73f58`.
  Source fixtures are analyzed only; both builds retain nine warnings. Actual
  native named-parent/guard/copy/retire gate also passes, receipt
  `.tmp/self_hosted/routine_build_copy_retire.9Dnyle`. Scoped caps/shell/diff
  checks pass. Full component inventory again does not complete at the unchanged
  sixty-second cutoff; its structural checker and 123-file prefix are not a pass.
- Previous fixed source graph was
  `216e2fa2173cd2bd56b747c0b5a25722307a63752663b82ac05ed61963a2ed22`.
  Official isolated seed-only production passes, observed exit 0, in
  `.tmp/mir_builder_chain/codegen-bootstrap-field-transport`, log
  `.tmp/mir_builder_chain_field_transport_seed.log`. Fresh stage-0 call argument
  (two executions/twelve refusals) and MIR-root (four executions/eight refusals)
  gates pass; MIR receipt `.tmp/self_hosted/codegen_nominal_array_declaration/run.7zjrTd`.
  Gen1/gen2 C are byte-equal, SHA-256
  `4aa5809a5af865faa50c550eb0ef472da4e0cb476c382730108fcbcd5390f575`;
  gen2 executable SHA-256
  `43ecc9f475cdcd01c34feb6837ab4d33c572384b1464e4f113e4e029c7ed63b0`.
  Artifact receipt independently validates; post-build source graph equals the
  fixed prebuild graph. Normal release profile. This is seed-only, not gen3/full
  fixpoint. Actual isolated DRV-2 refuses, observed exit 1, in
  `.tmp/mir_builder_chain/driver-field-transport`, log
  `.tmp/mir_builder_chain_field_transport_driver.log`, using only that identified
  gen2 as its source producer. Native emits the physical manifest only; no
  native driver fallback or installed overwrite is used. Its boundary is
  `aggregate_release_plan_unproved`, node `123171`. The current parser observes
  241184 nodes and maps that site to `SelfMirDestructureFactRowsAttach` inside
  `SelfMirCfgAttachLastDestructure`; log
  `.tmp/mir_builder_chain_field_transport_ast_refusal.log`. Seed receipt and
  unchanged source graph independently revalidate after refusal. The raw file
  is a diagnostic, not an admitted C artifact.
- Current-generation origin census completed using an identified diagnostic
  clone of exact gen2 C, preserving all trace predicates/depth limits while
  recording transport/generation failures. Clone executable SHA-256
  `1b69a556c276ba3c641d194b8830a220958fef21ac7f7367379e4618cf9584d6`.
  Paired controls observe zero/one origin failures, inner exit 92 and empty
  output. Full log `.tmp/mir_builder_chain_field_transport_census_full.log`
  records sixteen requirements, 162896 call-argument inventory rows, fifty-one
  reached pairs, four origin failures and zero invalid requirements; observed
  inner exit 92 and empty stdout. The reached cuts are the whole-formal
  assignment write place in `SelfMirLowerLetFromArtifact`, control-flow-owned
  update in `SelfMirLowerBlockFromArtifact`, an unrelated nested-field write in
  `SelfMirAppendRoutine`, and ordinary/intent accumulation in
  `SelfMirProgramFactsBeforeCanonicalIdsObserved`. The latter loops genuinely
  change CFG values; unchanged-direct-field transport alone is not a complete
  demanded-leaf lifetime proof. A multiple-constructor-return source
  control passes; do not diagnose the real cut as duplicated return arms from
  syntax alone. Uniqueness/transitions/publication remain unobserved by census.
  The older `b817d741...` seed/census below is lookup evidence, not current source.
- Exact checkpoint: main @ `3658548d24bca3d721e4f1974ac7a10da99f7aa8`,
  uncommitted candidate plus the preserved proof-checker/aggregate-release,
  driver, documentation and test changes. Root owns integration. No parallel
  implementation, external message, commit/push or installed binary replacement
  occurred in this slice. Preserve unrelated dirty work and `gmon.out`.
- Objective: admit the current DRV-2 source graph through its complete MIR
  routine -> CFG -> nested row -> program publication -> last-consumer
  retirement chain. The existing P0/DRV-2 rung remains active; do not reopen a
  separate SoT/performance track or claim C2 closed. Plan/owner boundaries:
  `docs/agent_work_directives/mir_builder_ownership_chain_2026-10-07.md`.
- Native assignment now treats a consumed tracked binding's assignment target
  as a write place; only an admitted RHS restores the generation. Move-only
  restrictions and storage-authority admission remain unchanged. Mutable MIR
  targets carry `own`; source copies remain readonly through their final read
  and independent retirement. Named detached fields are restored before parent
  transfer/error returns. The obsolete `SelfMirRoutineWithCfg` is deleted.
  The expression-attachment owner binds three short readonly metadata views
  before CFG changes; no borrowed data is grown/released or retained by those
  consumers. Local read-root origin now follows admitted initializer/binding
  identities, rejecting readonly aggregate projections/intermediate copies
  that previously became independent read roots. This changes no cleanup
  authority or typed call-result policy and does not close general C2.
- Observed gates: native semantic suite 3107/3107; actual CFG/destructure String
  copy/retire probe; routine storage lifetime/internal admission; C/LLVM match,
  instruction-use and local-ref snapshots; current-source aggregate release
  8 positive + 20 falsifying cases and generation guards per backend all pass.
  Latest read-root gate: 7 positive + 16 falsifying cases per C/LLVM,
  `.tmp/self_hosted/member-indexed-read.8pASUo`; latest release receipt:
  `.tmp/self_hosted/aggregate-release-source.PMkV9O`. Both bind source,
  supplied inputs and native executable identities before/after execution.
  Inout source admission also passes C/LLVM: six positives, five ownership
  refusals and four missing/duplicate-identity refusals per backend, receipt
  `.tmp/self_hosted/inout-array-storage.XC5utV`; not installed-driver proof.
  Actual native public ArrayDrop executes eighteen positives per C/LLVM and
  checks thirty-four native C refusal/artifact-preservation controls, receipt
  `.tmp/self_hosted/public-array-drop.XZbkjg`; the public/installed-driver and
  issued-MIR stages were not run by this native-only invocation.
  The rebuilt current-source stage-0 analyzer also passes the MIR receiver
  root gate: four executions/eight pre-emission refusals, receipt
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.OTzp3X`.
  The 123-file signature/residue inventory passes independently. Full component
  inventory times out at the unchanged 60-second budget on Windows and WSL;
  successful prefixes do not make it green.
- Identified native oracle: `.tmp/mir_builder_chain/bin/pgy.exe`, SHA-256
  `f9b336156497e411e9f9754aad409bbb3411430bf5e9a5971dfad30e5302f6f1`.
  An earlier source snapshot emitted complete DRV-2 C with zero errors/twenty
  warnings; that whole-graph emission predates the read-frontier/origin edits.
  That artifact is diagnostic evidence, not an installed/replacement driver.
  The official production gate requires a Pergyra-built gen2 typed-source
  producer; do not silently use native driver emission to bypass that contract.
- Reached falsifier: official isolated driver production using seed
  `9029e9e2dd7eac281bf238537906dc2e0ae6aa647c93410ba03b38ae199931b5`
  refused `aggregate_release_plan_unproved` at AST node 214978. The actual
  parser maps it to `DriverRung2MirProjectionRelease` ->
  `SemanticAstExpressionFunctionTableFactsRelease(function_tables)`, after
  `projection.analysis.function_tables` is extracted. That gen2 seed predates
  the preserved release-lineage/restoration/element-borrow edits, so it is not
  current-source release evidence. The 240108-node full-driver observer was
  stopped without a verdict after an earlier executable MIR-read falsifier
  appeared; do not report it green/red or reuse its analyzer as current source.
  The first fresh seed attempt stopped before gen1 at direct own-member
  forwarding (`unproved_formal_indexed_read_entry`, node 73348), now repaired
  through the existing named field-generation seam. The source gate then
  exposed the readonly nested-alias root promotion; its original negative and
  chained/indexed/formal-copy controls now refuse.
- Fresh seed: official isolated seed-only bootstrap passes in
  `.tmp/mir_builder_chain/codegen-bootstrap-current`, normal release profile,
  fixed source graph
  `92a3ed5d18e711b113bcbc0c3b7903ad074db282bede84702915e7e486adefcb`.
  Gen2 executable SHA-256 is
  `37ba0b13319015421a91588c59c3f1b9342744c4e22e1aa05638ddcfa56f2064`;
  generated C is `0c2ce5008d38aa58b5b7a31d2df61c3e8eb99a0ca53d9410e7df0c55b42e028b`,
  byte-equal to gen1 C. Output receipt independently validates and the post-build
  source fingerprint matches the prebuild graph. This is the named gen2 seed
  boundary, not gen3/full codegen fixpoint or whole-compiler acceptance.
- Previous reached blocker: official isolated driver production from that
  gen2 refuses `aggregate_release_plan_unproved` at node 122198, log
  `.tmp/mir_builder_chain_official_driver_current.log`. The actual parser maps
  the 240167-node graph to `SelfMirCfgAttachLastDestructure` ->
  `SelfMirDestructureFactRowsAttach(destructure_facts, ...)`, trace
  `.tmp/mir_builder_chain_ast_trace_122198.log`. This was source-bound evidence
  at that checkpoint; the generation/publication candidate below is newer.
  Normal release profile; no replacement driver or installed overwrite was
  produced. Re-run from a new source-bound seed, not this older executable.
- Reached aggregate-plan census: the unchanged full graph has sixteen
  requirements and fifty-one matching requirement/call origin traces. Four
  fail: the `element_types` and `bindings` fields at each of
  `SelfMirDestructureFactRowsAttach` (syntax 122198) and
  `SelfMirDestructureFactRowsAppend` (syntax 124670). The other forty-seven
  origin traces pass; this does not observe uniqueness or later transitions.
  Log `.tmp/mir_builder_chain_aggregate_census.log`, observed exit 92 and no
  output artifact. It is not evidence that only two implementation tasks remain.
- Terminal-reason census also completes with the same fifty-one/four counts,
  exit 92 and empty artifact output, log
  `.tmp/mir_builder_chain_aggregate_census_trace.log`; original gen2 C is
  unchanged. Attach reaches the current-field guard on `build.cfg` in
  `SelfMirLowerDestructureFromArtifact`, syntax 122246, after whole-builder
  functional updates. Append cannot join the detached nested carrier's
  `target.instructions` receiver to a direct binding root. Parser evidence:
  `.tmp/mir_builder_chain_ast_terminal_trace.log`, 240167 nodes. No full-graph
  local-rebind terminal was observed: do not conflate the smaller control's
  cause with these actual sites.
- Implemented bounded dependency: exact aggregate generations replace the
  local/member no-write guards. Same-block whole updates and named physical
  field restoration trace to their existing origins. Provisional owned
  call/return/definition identities are sealed only after origin, uniqueness,
  reservation, restoration and borrower admission. Old callers remain
  consumed; deep-drop before return cannot be resurrected by field restoration.
  No nominal/mode grant, aggregate-result copy-out default or C2 closure is
  added. Conditional/loop-carried joins still fail closed.
- New falsifiers exposed two gaps before production: indexed field writes
  were admitted by the former direct-member test, and a field view created
  before an own call could be read afterward. Both now refuse at the exact
  root/use boundary. View rebinding consumes admitted collection-definition
  facts, not a latest textual write. A fresh independent String-array rebind
  is positive; other unproved container/nominal rebinds remain conservative.
  The canonical definition owner fills a caller-owned fresh scratch cursor;
  no internal-retire caller whitelist or returned-storage grant was introduced.
- Latest observed source receipt: `.tmp/self_hosted/aggregate-release-source.u7QqkJ`,
  eleven positives/twenty-eight negatives plus execution-context and transfer
  identity guards per C/LLVM, exit 0 within the existing five-minute budget.
  Named-result, repeated whole rebind and independent view rebind pass;
  moved caller/later argument, old field view, indexed write, missing restore,
  inout return and retired return refuse. These inputs are analyzed, not
  emitted/run. Imports, inputs, native executable and analyzer hashes were
  independently revalidated afterward. Analyzer SHA-256: C
  `2c5fafdb486f2dfb4272384cb003435e6789529c66e56fd7cec6267afb465041`, LLVM
  `7597ff09d12ba06d98a88d4db6ad822bf24eac8421fe0915507fbb7a909f7016`.
  Scoped owner caps, shell syntax and diff hygiene pass; nine analyzer warnings,
  native warnings and the timed-out full component inventory remain explicit.
- Expression-order continuation: `Observe(Grow(table), view[0])` was accepted
  by the prior source analyzers. Transfer views now compare the exact owned
  edge within the same syntax site. The opposite argument order is positive.
  Source receipt `.tmp/self_hosted/aggregate-release-source.BmLBLJ` passes
  twelve positives/twenty-nine negatives plus identity guards per C/LLVM.
  A subsequent unexecuted conditional view rebind was accepted: a selected
  definition is not a fresh/dominating one. Admission now also requires the
  definition owner's `fresh` fact; conditional and zero-iteration view rebind
  negatives now pass. Latest receipt
  `.tmp/self_hosted/aggregate-release-source.0DQcxn` passes twelve positives/
  thirty-one negatives and identity guards per C/LLVM, exit 0; imports, inputs,
  native and analyzer hashes independently revalidate. Log
  `.tmp/mir_builder_chain_generation_view_fresh_final.log`.
- Latest production falsifier: official fresh seed-only bootstrap in
  `.tmp/mir_builder_chain/codegen-bootstrap-generation-view-fresh`, log
  `.tmp/mir_builder_chain_generation_view_fresh_seed.log`, then isolated DRV-2
  production. Fixed source graph:
  `eda91ec432877f55b5f722fef8b809963985160995109697819621db386342bb`.
  Stage-0 build and call-argument/status controls pass, but the MIR-root source
  control refuses `unproved_formal_indexed_read_entry` at node 69150 before
  gen1/gen2. Parser artifact: 74606 nodes; the site is
  `SemanticAstCollectionAggregateTransferUsesReady` ->
  `SemanticAstCollectionMemberRootIdentityForNode`. Log
  `.tmp/mir_builder_chain_ast_root_read_trace.log` owns that mapping.
  The existing formal-effect observer confirms `inferred_types` is unproved
  element use (effect 5) in both transfer readers, while `resolved_leaf_types`
  and both root-identity reader arrays are read-only (effect 3). Observer log:
  `.tmp/mir_builder_chain_formal_transfer_reader.log`; diagnostic only.
  The alias reader passed an indexed borrowed type text into a retaining
  canonical type fact. A paired source probe rejects that path and admits an
  independent String copy. The alias reader now materializes only that small
  retained type text; no formal permission, metadata authority or growing-CFG
  clone is granted. Latest receipt
  `.tmp/self_hosted/aggregate-release-source.3LXuBd` passes thirteen positives/
  thirty-two negatives and identity guards per C/LLVM, exit 0. Source/input/
  native/analyzer hashes independently revalidate. Log
  `.tmp/mir_builder_chain_generation_type_text_final.log`. Fresh seed production
  now runs in `.tmp/mir_builder_chain/codegen-bootstrap-generation-type-text`,
  log `.tmp/mir_builder_chain_generation_type_text_seed.log`, followed by actual
  DRV-2 production. Keep the source fixed; do not call the old seed current.
  Current fixed source graph:
  `b817d741d856fc2bf1e799f0cc7b6c965c8e015b9edc3d74dc2b3df5010c5ec0`.
  The rebuilt stage-0 producer now passes the actual MIR-root boundary (four
  executions/eight pre-emission refusals), receipt
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.9rj5Xd`.
  Official seed-only bootstrap now passes (exit 0). Gen2 executable SHA-256:
  `fd36589f9ccb0b247aefe562114434ad58d1e68990cc3e3bd468896400ac5845`.
  Gen1/gen2 C are byte-equal, SHA-256
  `1c6efaead5577889fb2467543aa2c9547223dbe9663290304c53a9d2151b794a`.
  Seed output receipt independently validates and post-build source graph
  equals the prebuild graph. This is not gen3/full fixpoint. Actual isolated
  DRV-2 refused the fixed source (exit 1) in
  `.tmp/mir_builder_chain/driver-generation-type-text`, log
  `.tmp/mir_builder_chain_driver_generation_type_text.log`, using only that
  identified Pergyra-built gen2 as source producer. Native emits the physical
  manifest only; no native driver fallback or installed overwrite is used.
  The observed boundary is `aggregate_release_plan_unproved`, node `122811`,
  after roughly 7 min 16 s in source analysis; the raw file contains the
  diagnostic, not an admitted C artifact. Seed receipt and the unchanged
  source graph independently revalidate after the refusal. Current generated-C
  diagnostic census completed under `.tmp/mir_builder_chain/` against the
  exact gen2 C hash above, preserving every trace predicate and depth limit.
  It includes top-level own-edge recording from the current producer.
  Diagnostic executable SHA-256:
  `e52baebb9a1f040d12b46a2ca723f0ca12ec2d06310d6bad388cda66908ddf15`;
  log `.tmp/mir_builder_chain_generation_full_census.log`. Its paired origin
  controls observe zero/one failures, exit 92 and empty output. The parser
  reports 240780 nodes and maps `122811` to the destructure-row attachment
  inside `SelfMirCfgAttachLastDestructure`. This identifies the reached
  consumer, not a missing-fact remedy or a successful full plan.
  Complete current census: 16 requirements, 162679 inventory argument rows,
  51 reached requirement/call pairs, four origin failures, no invalid
  requirements; exit 92 and empty stdout. Source hashes revalidate. Requirements
  12/13 fail at `own build` binding 122816, CFG field 122237, use 122859 in
  `SelfMirLowerDestructureFromArtifact`; requirements 14/15 fail at the unnamed
  nested `target.instructions.destructure_facts` carrier in `SelfMirAppendCfg`.
  Forty-seven origin passes do not prove uniqueness/transitions/publication.
  Next slice restores named parents in both CFG producers, then closes only
  the actual current-generation dependency reached by this same rung.
  The earlier attempt in
  `.tmp/mir_builder_chain/codegen-bootstrap-generation-alias`, log
  `.tmp/mir_builder_chain_generation_alias_seed.log`, was stopped for the
  same-expression falsifier. Its exact wrappers/gen0 process were drained;
  the killed MIR-root control's empty output is not a semantic verdict.
  The two previous generation seed attempts were deliberately
  stopped for the newly accepted falsifiers, their exact helper process trees
  drained, and no gen2 verdict recorded. Do not reuse their partial artifacts.
  Once source-bound production reaches the actual MIR callers, reconcile the
  `build.cfg` generation and named `target.instructions` restoration chain.
  Cross-function carrier/publication context and branch/loop summaries remain
  unverified; do not infer their admission from the small positives.
  Each requirement/call is traced with independent mutable plan/trace storage;
  only immutable call inventory is shared. Small nested-carrier positive and
  value-formal negative controls agree (zero/one trace failures). Exit 92 means
  diagnostic completion, never semantic admission; no C artifact is published.
  Whole-program uniqueness and later transitions remain unobserved when an
  origin trace fails. Record the whole reached failure set and revise the owner/
  caller/generation/last-consumer plan before implementing a dependency repair.
  Ordinary/intent/destructure parity and default-driver inout cleanup remain
  pending until current-source production succeeds.
  Logs are `.tmp/mir_builder_chain_*.log`; release/retirement receipts bind
  source and executable identities. No SoT promotion, CI-green, public installed
  inout-cleanup or GUI-readiness claim follows from this checkpoint.

### Post-census candidate: named parents and unchanged-field transport

Current source is newer than the `b817d741...` gen2/census checkpoint above;
that executable is not current-source evidence. Both CFG producers now restore
every named detached parent before returning the owned CFG. The native actual
copy/retire probe passed, receipt `.tmp/self_hosted/routine_build_copy_retire.wxG57H`;
subsequent empty-instruction/type/binding guards also pass in the fresh receipt
`.tmp/self_hosted/routine_build_copy_retire.9Dnyle`, observed exit 0. The real
watched source-only backing/owned-String retirement and independent target copies
remain the gate oracle; native evidence is not installed-driver proof.

The demand-local generation view now accepts child-scope whole updates only
after every callee return/use physically preserves the exact demanded field.
Declared constructor projection has one local owner. The proof consumes current
execution contexts; unaccounted edges, duplicate/cross-wired fields, mutations
and missing facts fail closed. Neither `own` nor a nominal type grants storage
or element authority. General C2/loop joins remain OPEN.

A richer positive exposed a lexical-order error: an own call in a terminating
return was treated as consumption on a different later arm. The typed transfer
order view now ends only that taken return path; later arguments still refuse.
Fresh C source analysis admits the unchanged-field loop with readonly scalar
calls and an early own forwarding return. Six negative controls refuse captured
backedge views, field mutation, cross-wiring, duplication, later return arguments
and a conditional consuming arm that continues. Logs:
`.tmp/mir_builder_chain/aggregate_field_transport_*.terminal-c.log`.
This is focused source analysis only. The full C matrix then caught a missing
Let/Assign read domain: `aggregate_generation_conditional_update_negative`
was accepted. Statement facts exclude those rows by contract. Transport now
audits the reached function's existing expression-surface interval, not only
payload/control statements. Re-run the full matrix before seed production.
Full current C/LLVM gate now passes fourteen positives/thirty-eight negatives
and identity guards per backend, receipt `.tmp/self_hosted/aggregate-release-source.a2WDSe`;
hashes independently revalidate. The fresh official seed passes; actual DRV-2
refuses at node 123171, and current-generation origin diagnostics are running. No
installed driver, commit/push, CI-green, SoT/C2 closure or GUI claim is made.

## Historical self-host context (lookup only)

The following cards are historical evidence, not parallel work queues. Resume
only the active ownership-chain card above; reconcile its observations with
current source and gates first.

### Collection member places, GUI ArrayDrop and CI bootstrap time (2026-10-06)

Base: origin/main `18648b8b` (bootstrap analyzer closure, pushed). Three
commits on top, made in the `/d/pgy-ci` worktree, branch `ci-green`; this card
ships in the last of them.

- `18648b8b`: closure census codegen 156 → 0 and mir_lower 36 → 0; local
  `codegen_bootstrap.sh` printed `SELF-HOSTING OK`. Its CI run cancelled
  `self-host-codegen-bootstrap-linux` at the 30-minute limit (last green run
  26 minutes), so the Linux jobs were skipped again. Whole picture:
  `docs/audits/2026-10-06_self_host_bootstrap_analyzer_census.md`.
- Native GUI blocker (P0): Alrescha's `LayoutGui -> ArrayDrop` refused on
  C/LLVM. Two native defects: `case None: {}` (an empty set literal) was an
  unmodelled node in the inout preservation transfer, and the preservation
  summary depended on declaration order. Preservation is now decided after
  every body is checked. The unmodified `tests/gui_core_contract.pgy` builds
  and prints `GUI CORE PASS` on C and LLVM; `make test-semantic` 3062/3062;
  `public_array_drop.sh` native stage passes.
- Member places: member paths are storage places in collection ownership.
  This closes eight pre-existing use-after-free admissions after an
  aggregate release, admits nested readonly reads (the
  `IntentSubjectSlotSelect` adapter is gone), adds borrow-only String
  formals, and treats a plain struct parameter as a readonly root for field
  mutation. Audit: `docs/audits/2026-10-06_collection_member_place_shape_audit.md`.
- Bootstrap time: `SemanticExpressionGraphRootStartAt` took 87 percent of a
  gprof run of the codegen closure. Scanning back for the previous root cut
  that analysis from 553-672 s to 214 s with the same verdict.
- Gates: `collection_member_place_rules.sh` (C/LLVM, 15 positives and 20
  refusals) and `collection_bootstrap_closure_rules.sh` (8/16), both manual
  inventory rows; aggregate release source, entry requirement, member
  identity, borrowed descriptor read, component contract, owner-size and
  reachability.
- Next falsifier: the CI run of these commits, the first in which the Linux
  jobs behind the bootstrap can run.
- Open decision: copy-out aliasing. Mutating a copy of a non-String
  collection or of a struct can reallocate a buffer the source still names.
  The source has about 545 copy, mutate, restore sites, so the fix is move
  tracking for every collection type, not a refusal. See the member-place
  audit.
- Debts: `EmitStmtList` copies instead of owned transfers, the removed
  function epoch release, re-pinned caps (now also the formal-use and
  member-transition owners) and 6 owners on responsibility caps.

### Independent native lexer local checkpoint (2026-10-06)

Base main: 430f0b9103c971ca9bba6dda84140ae73ce81768. Only generated gmon.out
was dirty before this slice; it is preserved and excluded. This note ships in
the local lexer commit; inspect git log for the resulting SHA. No push or
Actions run is authorized by this checkpoint.

Native skip_whitespace now refuses unterminated block comments instead of
silently returning EOF, matching the existing self-host ScanTokens refusal.
Error tokens use the existing token-text/stream owner and receive initialized
stream identity and increasing ordinals. The existing lex diagnostic code,
reason and fix routing is unchanged. Error location remains the reached EOF.
Collection/inout owners and the active P0 contract below are unchanged.

Final source checks: GCC and Clang -std=c11 -O2 -Wall -Wextra -Werror builds
pass six unclosed-comment refusals, six valid controls and a literal-error
stream/ordinal control. The same test fails on the base lexer (EOF, not error).
parser_lexer_diagnostic_smoke.sh now reaches the executable regression and
passes locally; git diff --check passes. No full bootstrap, installed-driver,
Linux sanitizer, backend matrix or remote CI claim. This is a correctness
change; no speedup measurement or C-path substitution is claimed.


### P0: caller cleanup after synchronous inout

- Priority: finish caller ArrayDrop after synchronous inout before general SoT.
  Alrescha is a separate read-only consumer; no compiler coupling or whitelist.
- Checkout observed before this checkpoint commit: main @
  59a214ff26f9db0beaea0b4a46cfb012f3e2693f. Current dirty candidate replaces
  Bool-labelled type-row retirement with scope-owned String-array epochs and
  consumer-time serialization of admitted binding facts. All earlier user/worker changes are already
  published. This card travels with the implementation checkpoint; after its
  commit/push, expect no tracked dirty files. Verify actual HEAD/remote/status.
  Generated gmon.out remains preserved/excluded.
  Root owns P0 integration
  and Git; Main is stopped following the user's
  explicit choice. The user also authorized committing all current user/other-
  worker changes, including the new examples and deleted deployment guide.
  All 184 changed files were committed/pushed as checkpoint 4415adf4; the
  subsequent external documentation commit was fast-forwarded locally and
  runtime ABI scalar/registry correction 5deb67b4 and routine lookup migration
  8b1f17ec and instruction use publication f49be4dd were committed/pushed.
  Named-use compatibility 057c93c1 was merged with the external guild-economy
  example commit d5f90c54 as cf0fe236 and pushed; example behavior was not
  independently verified here. No local source edits were lost in that merge.
  LocalRef publication a59c01a6 and owned unique-use accumulation ad4f133a
  were committed/pushed as well.
  Shared readonly aggregate-call context df0df7a4 (26 files) is published.
  Slice length 004f4f24 was merged with the external async/type-alias/subject-
  address/intent changes as 7e054c1f and pushed without losing either side.
  Those external changes are retained, not independently certified by this note.
  Preserve/exclude generated gmon.out. This is not a green/installed claim.
- Objective/owner boundaries:
  docs/agent_work_directives/inout_array_release_2026-10-05.md.
  Native parameter-flow descriptor preservation, source call-preservation and
  untrusted direct-MIR lifetime owners independently supply cleanup evidence.
  Existing definition/order, formal-effect and member-read owners keep their
  facts. No mode-only grant, guessed callee, alias promotion or C fallback.
- Current reached seam: the conditional String-field cleanup and
  owns_local_rows Bool are deleted. CodegenTypeEnvState owns an Array<String>
  scope epoch, keeps its current local_rows as a view, and retires the array
  once after its last consumer. Child adoption copies into the parent epoch;
  the statement consumer then retires the child explicitly. Binding facts hold
  typed identity only; their formerly eager env_rows field is deleted and every
  former consumer is migrated. Global/preseal/local lookup order is unchanged.
  The sealed physical StringJoin call now supplies its actual runtime fresh
  heap-or-null result fact to the existing domain/exclusivity owners. String
  type, a Bool, a user return or a foreign target still supplies no authority.
  Current scope/join gate4 passed C/LLVM exact values, primitive-identity
  mutations and thirteen lifetime refusals/backend within 300s:
  .tmp/inout-array-release-type-env-state-gate4.log,
  .tmp/self_hosted/type-env-state-retirement.QYBasF. Existing nine observer
  warnings; source/input/native hashes were rechecked. The state owner is 115
  lines under its 140 cap; domain owner is 200 under its unchanged 200 cap.
  The preseal smoke target now reaches this executable gate. Structural checks
  forbid the retired Bool, scalar-field cleanup and eager binding rows.
  Native full-source C build passed with zero errors / sixteen warnings after
  six temporary-ref call sites were corrected by giving the resource-free
  four-String binding value an ordinary by-value serialization interface:
  .tmp/inout-array-release-type-env-native-gen0-check.log.
  Fresh original-input v55 terminated after unchanged MIR controls passed:
  gen1 source admission refused aggregate_release_plan_unproved at node 21859.
  Its reported GenericParameterFactRowsDrop extraction may be the first-
  requirement fallback; this is not sufficient evidence identifying the
  conflicting demand. The
  existing uniqueness/order policy now lives in one responsibility-named owner
  and preserves the failed call's syntax. Plan construction is 113/160 lines;
  the new uniqueness owner is 118/140. Old definitions are deleted. This changes
  diagnostics only, not alias, generation or repeated-release permission.
  .tmp/inout-array-release-codegen-bootstrap-v55.log,
  .tmp/inout-array-release-bootstrap-v55-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v55-source.sha256 were checked at
  terminal failure before the diagnostic change. The subsequent unchanged
  whole-codegen diagnostic observer exhausted 300s without output; its wrapper
  reported exit 1, not a semantic verdict. No larger allowance was introduced.
  Current aggregate diagnostic/parity gate8 passed six positives, sixteen
  refusals and generation/coverage mutation guards per C/LLVM. Repeated release
  and duplicate storage still fail at the same boundary, now naming Main's
  physical release call rather than the imported field extraction. Input,
  import and native hashes were verified by the gate:
  .tmp/inout-array-release-aggregate-release-gate8.log,
  .tmp/self_hosted/aggregate-release-source.1nSzkj. The original
  gen1/fixed-point/default-driver/GUI contract remains OPEN. No v56 was started.
  Next falsifier: the unchanged original compiler input's actual failed demand,
  then the fresh default C/LLVM synchronous inout/ArrayDrop consumer contract.
  v54 stopped at the six native unnamed-ref errors, not semantic parity.
  Component checker unit tests and changed shell syntax checks passed. Full
  Windows structural3 ended BASH_STATUS=124 under its unchanged 60s budget;
  earlier silent exit-1 observations were not proof of a missing enum import.
  The actual enum text_owner import is present. No full inventory PASS is claimed.
  Exact 59a214ff CI 37377911373 failed gen1 node 58906 / owned_string_drop;
  Windows/macOS/TSan/Rocq passed and downstream Linux jobs skipped. The earlier
  hosted runner acquisition issue did not recur. This candidate is not CI green.
  Named allocated-String own arguments remain admitted only at their exact
  physical consuming node, with all other local uses before it. An own String
  is by-value consumption, not a caller binding-slot write; inout/unknown domain
  exposures remain rejected.
  Named gate5 passed current C/LLVM values, thirteen semantic refusals/backend
  and one separate unsupported-async parser refusal/backend:
  .tmp/inout-array-release-named-string-own-entry-gate5.log,
  .tmp/self_hosted/named-string-own-entry.l9VhLg. Nine existing observer warnings.
  The existing collection-effect integration matrix now includes the seven
  parsed named-entry rows; the full matrix was not rerun in this checkpoint.
  Retention-registry smoke passed after its stale direct-reader assertions
  migrated to the actual shared exclusivity/read-context owners; no registry
  row changed. Expression-root regression gate6 also passed:
  .tmp/inout-array-release-expression-root-consumption-gate6.log,
  .tmp/self_hosted/expression-root-consumption.1zhB3w.
  Prior 59a214ff observers refused the old imported state probe at syntax 1622
  / owned_string_drop (superseded by the scope/join gate3 above):
  .tmp/inout-array-release-type-env-state-frontier-current-c.log,
  .tmp/inout-array-release-type-env-state-frontier-current-llvm.log.
  The ignored array-epoch prototype has source/C/LLVM-native probe evidence;
  it is not an original-input memory-cost or installed-driver receipt.
  Do not remove cleanup, grant Bool-based ownership, or copy and leak inputs.
- Prior expression-root seam: RewriteSemanticMemberAccess ->
  CodegenCExpressionTextCommitRoot. Signature-owned body availability no longer
  excludes owning formals, including its readiness consumer. Canonical builtin
  consumption identity is shared by formal use, negative storage effects and
  ordered transfer. The caller's existing own admission remains the storage/
  element authority; its exact current entry is not prior retirement.
  CommitRoot validates, copies either result, then retires once. No Bool-based
  alias proof, conditional-move grant, name exception or native bypass.
  Gate5 passed C/LLVM production/copy values, two fatal epoch modes, four source
  positives with formal_ready=true and fourteen lifetime refusals/backend:
  .tmp/inout-array-release-expression-root-consumption-gate5.log,
  .tmp/self_hosted/expression-root-consumption.588HWL.
  The changed effect owner is 210 lines under its unchanged 210-line cap;
  comments are excluded by the repository metric, blanks still count.
  Existing 123 own/indexed source verdicts match baseline/current in both
  backends. These are not full driver/fixed-point or replacement receipts.
  Original-input v53 passed four C controls/eight cycle refusals, then gen1
  source admission refused node 58906 / owned_string_drop. It names
  CodegenTypeEnvStateReplaceOwnedLocal -> ArrayDropOwnedStrings(retired) inside
  if retire_old. No gen1/gen2/fixed point/default pair/GUI receipt exists.
  Source/input/native hashes matched at terminal failure before the two
  whitespace-only cap wraps; gate5 observes the current bytes.
  .tmp/inout-array-release-codegen-bootstrap-v53.log,
  .tmp/inout-array-release-bootstrap-v53-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v53-source.sha256.
  Next owner: existing type-env state/owned String actual exclusivity, last
  consumer CodegenTypeEnvStateReplaceOwnedLocal. Reproduce its smallest
  imported source falsifier; neither retire_old nor a String/heap domain is
  permission. Then fresh unchanged original-input bootstrap/default pair.
  Do not revive 57194, query architecture, optimization or unrelated SoT.
  Full structural inventory ended exit 1 without its final PASS; its exact
  failure remains unidentified, and no full inventory success is claimed.
  Exact b477912d CI 37373270125 failed hosted runner acquisition before tests;
  all dependent jobs skipped. That is not compiler parity or green CI.
- Current focused evidence: aggregate gate6 passed C/LLVM 6 positive/16
  falsifying source inputs. Gate7 added generation/coverage mutation guards;
  its combined 300s run timed out after all C rows and LLVM rows 0..18.
  With native/source/input hashes unchanged, LLVM rows 19..21 and generation
  guards separately passed; all 22 C/LLVM outputs and guard outputs match.
  This is resumed row evidence, not a combined gate7 time-budget pass.
  .tmp/inout-array-release-aggregate-release-gate6.log,
  .tmp/self_hosted/aggregate-release-source.iVOx1C.
  Member gate8 passed current C/LLVM 5 positives/11 compile-only refusals:
  .tmp/inout-array-release-member-read-gate8.log,
  .tmp/inout-array-release-shared-context-member.sha256.
- Original whole-MIR-root source admission now passed in the fresh LLVM
  observer: body_ok=true, empty diagnostic, within the unchanged 300s budget.
  Native C/LLVM executions independently passed the exact six-line receiver
  identity/invalid-graph oracle, existing 15 warnings. Hashes were checked
  before/after. This removes the observed v43 aggregate refusal; it is not
  an official seed, fixed point, default installation or substitution claim.
  .tmp/inout-array-release-shared-context-whole-root-llvm.log,
  .tmp/inout-array-release-shared-context-whole-root.sha256,
  .tmp/inout-array-release-shared-context-mir-root-native.sha256.
- Official seed v44 built current gen0 and passed two call-argument executions
  plus twelve pre-emission refusals. It then passed the old ownership boundary
  and failed C emission of the original MIR control at unsupported collection
  runtime kind: Slice<String>. Source hashes matched through terminal failure.
  .tmp/inout-array-release-codegen-bootstrap-v44.log. Linux CI 37329776930
  independently failed that same kind; all four Windows/macOS/TSAN/Rocq jobs
  passed and dependent Linux jobs were skipped. Not a green/install receipt.
- Current reached fix: ArrayLength's Slice operand now consumes SliceRuntimeFact,
  not the array runtime owner. The private read-length symbol/block and existing
  direct-MIR constructor carry the same fact; missing length symbols fail closed.
  A descriptor snapshot evaluates the admitted operand once and retires nothing.
  Foreach/iteration support is unchanged; an early trial was withdrawn.
  Focused gate2 passed within 300s: native C/LLVM and freshly built self-host C
  each execute eight exact Int/String, default/ref and empty length rows; both
  native fact probes pass unsupported-family/missing-field guards and three
  fatal missing/unsupported-symbol modes. The self-host negative still refuses
  a compound operand whose codegen type fact is absent; no type-text guess.
  .tmp/inout-array-release-slice-length-gate2.log,
  .tmp/self_hosted/slice-length-codegen.bwqMT6.
  This pre-merge receipt was refreshed as gate3 on native v4 and passed.
- Official seed v45 on merged 7e054c1f passed source admission and emitted the
  original MIR control, then failed C compilation. Generated PgySlice_Int/String
  typedefs and public pgy_slice_get/copy/array_slice names collided with the
  required pgy_runtime.h definitions, whose Array descriptors are not private
  three-field arrays. All 6963 source/input/native hashes matched at failure.
  .tmp/inout-array-release-codegen-bootstrap-v45.log,
  .tmp/self_hosted/codegen_nominal_array_declaration/run.unnwy4/root-codegen_cc.log.
  Exact-head CI 37337689083 failed that same C compile stage; Windows/macOS,
  TSAN and Rocq passed, dependent Linux jobs were skipped. Not CI green.
- Current namespace fix: source SliceRuntimeFact owns pgy_self_slice_Int/String
  and private get/copy/array-slice symbols; source AbiLayoutCValueType consumes
  that fact. Compiler ABI rows and the direct-MIR public projection are unchanged.
  Gate4 passed within 300s on native v4: existing length/fact guards plus native
  C/LLVM and fresh self-host C HashMap-header coexistence, index/copy and exact
  values; the gate forbids reopened native Slice typedef/function definitions.
  .tmp/inout-array-release-slice-length-gate4.log,
  .tmp/self_hosted/slice-length-codegen.WPgus8.
  This fix is committed/pushed as 32d2b1a9. Default install remains OPEN.
- Official seed v46 passed the unchanged original MIR root: native/self-host C
  compile/exact execution, four controls and eight cycle refusals. Gen0 compiling
  its own source then refused borrow_boundary_escape, syntax 49433, ArrayPush.
  The production parser mapped that handle to GenericInstanceClosureFromRecipes
  in generic_instance_closure_owner.pgy: borrowed recipe/substitution text was
  retained in actuals. Source/input/native hashes matched after failure. Exact-
  head CI 37340961801 independently passed the root and refused that same node;
  four other jobs passed, dependent Linux jobs skipped. Not a seed/fixed point.
  .tmp/inout-array-release-codegen-bootstrap-v46.log,
  .tmp/inout-array-release-bootstrap-v46-boundary.log,
  .tmp/inout-array-release-ci-37340961801-codegen.log.
- Current generic owner fix: actual tuple projection, append and recipe
  publication copy retained text with ArrayPushOwnedString. Append copies the
  existing member text into a fresh owned buffer before adding new elements;
  native's unknown-uniform-ownership guard remains intact. Numeric identity,
  deduplication, expansion and missing-fact rules are unchanged.
  Generic gate4 passed within 300s, native C/LLVM tuple/finite/epoch values and
  caller cleanup independence, source owner admission, raw-retention entry
  refusal and unproved returned-array deep-drop refusal. That last value fixture
  executes natively but remains refused by source at ArrayDropOwnedStrings(projected);
  no general returned-String-array cleanup grant is inferred.
  .tmp/inout-array-release-generic-owned-actuals-gate4.log,
  .tmp/self_hosted/generic-owned-actuals.uRErHr.
  This fix is committed/pushed as bebfc164. Default install remains OPEN.
- Official seed v47 passed the original MIR root's four controls and eight
  cycle refusals, then gen1 source admission refused syntax 54263 /
  unproved_formal_indexed_read_entry. The production parser mapped it to
  CodegenValueWrapperUsageCollectType calling Contains after recursive inout
  growth. Source/input/native hashes matched through the terminal failure.
  .tmp/inout-array-release-codegen-bootstrap-v47.log,
  .tmp/inout-array-release-bootstrap-v47-boundary.log.
  Exact-head CI 37345034540 independently failed at the same node/boundary;
  Windows/macOS/TSAN/Rocq passed, dependent Linux jobs were skipped.
- Reached formal-effect observation: the recursive formal is mode 1 / effect 6
  (proved shallow mutation), with no unknown/escape site. Retirement and non-
  deep site 40176 in the imported owner fixture name the recursive call before
  membership reading; Contains is mode 0 / effect 3. The argument-effect owner
  already distinguishes sibling retirement from current descriptor retirement,
  but the formal call-effect carrier previously discarded that distinction.
  .tmp/inout-array-release-formal-descriptor-frontier-v1-observe.log,
  .tmp/inout-array-release-formal-descriptor-frontier-v1-site.log.
- Current correction carries that bit through the existing physical call pass
  into formal_descriptor_retiring_sites. One current-descriptor permission owner
  supplies synchronous read and shallow-forwarding entry; unknown/escape,
  consumption, deferred/reordered uses and sibling-view refusals stay distinct.
  No owned-element, deep-drop or recursive caller-release permission is inferred.
  The Slice membership/common indexed-type trials were completely withdrawn;
  the original wrapper implementation and original bootstrap/MIR input remain.
  A former "unknown" fixture contained only a proved synchronous shallow call;
  that success case is now separate from a genuinely unproved deferred mutator.
  This correction is committed/pushed as d2fe7ab6. The next observed integration
  boundary is the branch-local String retirement below, not the withdrawn Slice
  trial, general returned-array cleanup, or an unrelated SoT/performance track.
- Current focused gate4 passed within 300s on native v4: C/LLVM production
  wrapper and recursive-current-read value oracles, three source positives and
  twelve preserved refusals per backend. The fresh LLVM owner observer then
  admitted the actual import-composed wrapper input (body_ok=true), showing
  the recursive formal's sibling retirement at 40176 and no current descriptor
  retirement. Input/import/native/executable hashes matched before/after.
  .tmp/inout-array-release-current-formal-descriptor-gate4.log,
  .tmp/self_hosted/value-wrapper-view.8K3BIz,
  .tmp/inout-array-release-formal-descriptor-frontier-v2-observe.log.
  Recursive caller cleanup was independently refused by native during an
  initial fixture trial; that extra grant is not part of this read proof.
  The structural inventory also found an obsolete DIR intent-step read
  requirement contradicting its explicit borrowed action-contract input. Its
  existing owner-boundary ratchet is corrected without restoring the old read;
  this source-only inventory is not a behavioral or green-CI receipt.
  Its corrected whole inventory rerun reached the unchanged 60s limit without
  a terminal verdict: .tmp/inout-array-release-current-formal-descriptor-inventory2.log.
  Keep that omission explicit; no cap or time allowance was raised.
- Official v48 used fresh fixpoint-only mode with the same 1800s integration
  budget and original compiler/MIR input. It passed four original C controls
  and eight cycle refusals, then gen1 source admission stopped at syntax 55727 /
  owned_string_drop. The production parser maps this handle to
  CodegenExpressionMemberTypeFromGraph in expr_semantic_type_owner.pgy:
  ArrayDropOwnedStrings(retired_variant_key), where [variant_key] is created
  under IsSome(receiver_type). All source/input/native hashes matched at failure.
  No gen1/gen2 executable, fixed point, default installation or GUI receipt exists.
  .tmp/inout-array-release-codegen-bootstrap-v48.log,
  .tmp/inout-array-release-bootstrap-v48-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v48-source.sha256.
- Exact-code-head CI 37355012849 independently passed the original C controls
  and refused the same 55727 / owned_string_drop. Windows/macOS/TSAN/Rocq
  passed, dependent Linux jobs skipped. This is still RED, not whole CI green.
  .tmp/inout-array-release-ci-37355012849-codegen.log.
- Reached query lifetime correction: LookupQualifiedKindTypeMatches in the
  existing type-environment owner constructs and retires its temporary key
  before returning a scalar comparison. The member consumer constructs an
  escaping key only after success. Original row identity/lookup precedence,
  result spelling and owned-key construction are unchanged. Domain/transfer
  proofs have not been relaxed, and no original key is copied then leaked.
- Focused gate2 passed within 300s on native v4: C/LLVM actual member selection,
  missing/mismatched queries, global/preseal/local precedence, source admission
  of the actual query owner's retirement and eight existing refusal fixtures
  per backend. Input/import/binary hashes match. Native values have 13 warnings;
  observer builds have nine. Static shell syntax and git diff checks also pass.
  .tmp/inout-array-release-qualified-kind-match-gate2.log,
  .tmp/self_hosted/qualified-kind-match.difPMY.
- The earlier full-root literal guard diagnostic timed out at 180s without any
  output; no allocation-domain or guard verdict was observed. Its diagnostic
  source remains available and never emits or executes the supplied input.
  .tmp/inout-array-release-owned-literal-frontier-v1-observe.log.
- Query correction is committed/pushed as 276e6725. Fresh official v49 passed
  the same original four C controls/eight cycle refusals, then refused gen1
  syntax 55975 / ArrayPushOwnedString(fragments, terminated) in
  CodegenPrefixOwnedStatementLine. Source/input/native hashes match. Exact-head
  CI 37360522726 independently refused the same node; Windows/macOS/TSAN/Rocq
  passed, dependent Linux jobs skipped. Still RED, no gen1/gen2 or install.
  .tmp/inout-array-release-codegen-bootstrap-v49.log,
  .tmp/inout-array-release-bootstrap-v49-boundary.log,
  .tmp/inout-array-release-ci-37360522726-codegen.log.
- Statement-prefix gate2 passed within 300s on native v4: actual C/LLVM output
  values (zero warnings), two source positives and twelve preserved refusals
  per backend; observer builds have nine warnings. The original caller uses
  Concat, not a replacement allocation input. Direct Concat freshness uses the
  exact sealed call/argument context and independent heap-or-null allocation,
  not a heap-domain or function-name shortcut. One duplicate borrow policy is
  deleted; no ordering/alias/exposure/return-chain guard is removed. Current
  observers also admit the earlier query lifetime and preserve its three
  additional local borrowed/shadow/alias refusals per backend. Hashes match.
  .tmp/inout-array-release-owned-statement-prefix-gate2.log,
  .tmp/self_hosted/owned-statement-prefix.5ASIfM.
  Gate1 refused the direct Concat actual after the callee was admitted; it was
  not passing evidence. The exact missing fact and blocked executable rung
  are recorded in the directive before this next supporting proof commit.
- Statement-prefix repair is committed/pushed as 1ea75fa4. Fresh official v50
  passed the unchanged four C controls/eight cycle refusals, then refused gen1
  syntax 57194 / unproved_formal_element_use_entry at the expression-root call
  above. Source/input/native hashes match. No gen1/gen2, fixed point, default
  pair or GUI receipt exists. .tmp/inout-array-release-codegen-bootstrap-v50.log,
  .tmp/inout-array-release-bootstrap-v50-boundary.log,
  .tmp/inout-array-release-codegen-bootstrap-v50-source.sha256.
- Exact 1ea75fa4 CI 37364141275 terminated before any compiler tests: GitHub's
  hosted runner failed to acquire classify-changes after multiple attempts;
  every dependent job was skipped. This is infrastructure failure, not a CI
  reproduction of 57194, and not green. No blind retry was sent because local
  original-input admission already has a known semantic refusal.
- Next falsifying fixture: tests/self_hosted/parity/fixture/
  codegen_expression_root_commit_frontier_probe.pgy imports the actual epoch
  owner. Fresh current C/LLVM source-only observers both reproduce
  unproved_formal_element_use_entry; input/import hashes match. The supplied
  fixture was never emitted or executed. .tmp/inout-array-release-expression-root-frontier-c.log,
  .tmp/inout-array-release-expression-root-frontier-llvm.log.
  Next: identify the actual reached formal-use flags and the missing ordered
  observation/consumption fact; preserve borrowed, alias, post-drop, repeated,
  deferred and missing-fact refusal before another frozen original-input run.
- Current native v4 SHA-256:
  c4f4dd1fb3e735f5afa0516a7c9d0485e481f74833febedf0092ef1a8e074f98.
  Fresh GCC build passed with 12 observed warnings. Native public gate16 passed
  15 executed positives/backend and 30 preserved-artifact refusals, no public-
  driver/default installation claim. .tmp/inout-array-release-native-v4-gcc-build.log,
  .tmp/inout-array-release-native-public-gate16.log.
- Prior pre-merge native v3 SHA-256 (not current-head native evidence):
  4bdb8869388dce2a8c02a5791f1670ada26b9e1cacd47c0b7d8195353e4673ce.
  Native public gate passed 15 executed positives/backend and 30 refusals;
  four C/LLVM dev/release inout/drop assertions ran. Not installed evidence.
  Exact LLVM observer compile passed in 57.46 seconds under original 120s
  budget after the measured development machine-emission fix.
- Before current context migration, scalar/storage gate45: C/LLVM each 28 analyzer positives/41 compile-
  only refusals plus actual copied type and typed success/failure cleanup
  values, independent numeric-array snapshot mutation, binding type guards
  with explicit caller copies, and match fact snapshots/no-mutation, exit 0.
  Imported match-owner source admission also passed for C/LLVM.
  .tmp/self_hosted/collection-borrowed-descriptor-read.mNokLy,
  .tmp/inout-array-release-borrowed-read-gate45.log. Inner Break/Continue,
  same-scope reuse, retained raw scalars and shallow-to-owned mutation refuse.
- Gate46 reached the same 28/41 source rows and imported match/runtime ABI
  owner admission in C/LLVM, then passed all prior native value controls. The
  combined run hit its unchanged 300s budget before the final ABI execution;
  it is incomplete, not a full gate pass. With the source unchanged, the
  final C artifact and freshly compiled LLVM artifact separately passed exact
  last-binding/device/missing/malformed-length values using caller copies.
  .tmp/self_hosted/collection-borrowed-descriptor-read.9GMYnb,
  .tmp/inout-array-release-borrowed-read-gate46.log,
  .tmp/inout-array-release-runtime-abi-type.sha256.
- The routine ABI consumer's duplicate lookup was removed; its two branches
  now use the existing expression ABI lookup. Native C/LLVM exact values and
  last-binding/missing/malformed guards passed with the routine import, and
  both migrated calls/old-symbol deletion were checked. Receipt:
  .tmp/inout-array-release-routine-abi.sha256.
  Its full imported source observer reached the unchanged 60s limit without
  a verdict; do not infer source admission. The original MIR root remains the
  integration falsifier instead of repeatedly validating that cumulative graph.
- Before current context migration, member gate7: C/LLVM each 5 positives/11 compile-only refusals, exit 0.
  .tmp/self_hosted/member-indexed-read.eXURNt,
  .tmp/inout-array-release-member-read-gate7.log. Composed direct readonly
  views admit; copied/owned/inout/deferred roots and value-formal handoff refuse.
- Reached producer corrections use existing contracts: owned String insertion
  and scalar copy, mutable clause-array Clone, required canonical contract ref.
  Native value probes passed exact naming, participant selection, inherited
  names, cloned input survival, repeated resolver and copied-record values
  after input cleanup. Actual canonical generic/scalar parsing and String-array
  classification values passed native C/LLVM. These are not whole-root or
  default-installation receipts; detailed history is in the directive above.
- DIR gate6 passed exact oracle/mutation rows and native C/LLVM malformed-view
  guards. FactsReady still owns the public shape/receipt/range boundary; its
  inner membership predicate receives direct views derived by that wrapper,
  not external replacement views. No new artifact admission was added.
  .tmp/inout-array-release-dir-graph-participant-view-gate6.log.
- Latest official seed v32 passed call-effect construction and refused node
  70449 at unproved_formal_execution_context. Two scratch cleanup defers in
  the ownership verdict made its whole callable opaque under the unchanged
  execution-context contract. The old scan moved to ownership_scan_owner:
  same 21 success/error exits now fill the existing typed verdict and return
  Void. The admission/preparation caller alone retires both scratch buffers
  after the scan, for success/failure alike. No context relaxation, copied
  type arrays, new error API or private user-array release was introduced.
  .tmp/inout-array-release-bootstrap-v32-exact-boundary-context.log,
  .tmp/self_hosted/codegen_nominal_array_declaration/run.6vkeOW.
- Native AIR on the full analyzer import graph passed, existing warnings,
  .tmp/inout-array-release-ownership-scan-native-air.log, 0 errors/9 warnings.
  Gate38 then passed explicit typed success/failure cleanup and retained the
  deferred-context counterexample. No whole-root or installed proof yet.
  Official seed v33 refused node 70281: canonical verdict argument 7 retained
  the readonly result-plan Array<Int> member. Publication now clones that
  member through the existing Clone contract; no borrow/release rule changed.
  .tmp/inout-array-release-codegen-bootstrap-v33.log,
  .tmp/inout-array-release-bootstrap-v33-exact-boundary-context.log.
  Gate42 passed: snapshot independence plus raw-member ordinary-call refusal,
  matching the reached canonical verdict constructor function.
  Numeric member/extracted-array public cleanup remains outside the existing
  release frontier; no member cleanup claim is made by that new fixture.
  Official seed v34 passed verdict publication, then refused node 70720 in
  SelfMirCollectionOwnershipMemberSourceReady. The binding-type owner now
  copies only its selected local/parameter String; canonical field-type input
  also copies its selected scalar. Gate43's new value probe was refused because
  native ordinary String-result lifetime is unknown. Gate44 uses explicit
  caller copies to check values/missing/duplicate guards, not that missing
  lifetime grant. Gate44 passed values and missing/duplicate guards with those
  explicit copies. The native fail-closed contract stays unchanged. Logs:
  .tmp/inout-array-release-codegen-bootstrap-v34.log.
  .tmp/inout-array-release-bootstrap-v34-exact-boundary-context.log.
  Seed v35's handle was missing after goal continuation; process inventory
  confirmed no matching bootstrap/codegen process. Its log stops after status
  checks without a semantic verdict or gen2 receipt. This is incomplete
  execution, not admission or a timeout-based semantic verdict.
  .tmp/inout-array-release-codegen-bootstrap-v35.log.
  Official seed v36 passed binding-type lookup and refused node 71293:
  match case publication retained raw indexed String payloads. The existing
  match owner now copies retained scalars and validates incoming binding rows
  before destination mutation. Gate45 passed values and unchanged-row guards;
  destination deep-release authority is not promoted. Seed v37 passed that
  boundary, then refused node 71641 in SelfMirExpressionRuntimeAbiLocalType.
  The existing owner now reads inventories by ref and copies its selected
  String. Last-binding lookup and missing/malformed guards remain unchanged.
  No native ordinary String-result lifetime grant is inferred.
  .tmp/inout-array-release-codegen-bootstrap-v36.log.
  .tmp/inout-array-release-codegen-bootstrap-v37.log.
  Seed v38 passed that query, then refused node 71741 in the duplicate routine
  lookup. Linux CI 37307402549 independently refused the same node/boundary.
  .tmp/inout-array-release-codegen-bootstrap-v38.log,
  .tmp/inout-array-release-bootstrap-v38-exact-boundary-context.log,
  .tmp/inout-array-release-ci-37307402549-codegen.log.
  Seed v39 passed the duplicate lookup deletion, then refused node 71932 in
  SelfMirRoutineAddInstruction -> SelfMirCfgAddInstruction: raw use Strings
  were retained. The existing instruction-row owner now copies each retained
  use and accepts the input by ref; the routine only forwards that view.
  Native C/LLVM use text after source cleanup, empty-row offsets/counts and
  instruction/block identities passed. Existing copied/raw-row source controls
  passed in both observer backends; no destination deep-drop right was granted.
  .tmp/inout-array-release-codegen-bootstrap-v39.log,
  .tmp/inout-array-release-bootstrap-v39-exact-boundary-context.log,
  .tmp/inout-array-release-instruction-use.sha256.
  Seed v40 stopped before self-host admission: native rejected two unnamed
  use-array results at the new routine ref boundary. Both simple-statement
  consumers now bind their uses for the call. Original native MIR root C/LLVM
  compilation/execution passed its unchanged six-line identity/mutation oracle.
  .tmp/self_hosted/codegen_nominal_array_declaration/run.Kvgg26/root-native.err,
  .tmp/inout-array-release-mir-root-named-uses.sha256,
  .tmp/inout-array-release-mir-root-named-uses-llvm.sha256.
  The gate now prints the native owner's failure cause instead of only its
  wrapper label. Self-host root admission has not yet been rerun on this fix.
  Seed v41 passed native named-use compatibility, then refused node 71966:
  SelfMirInstructionLocalRefRowsAttachExpr0 retained raw local-ref Strings.
  The existing LocalRef row owner now takes readonly inventories and copies
  retained String text in attachment/append paths, without destination deep-
  drop promotion. Native C/LLVM shadowing/version selection, source String
  cleanup survival, malformed/repeated no-mutation guards and nonempty-prefix
  append offsets passed. Existing copied/raw-row source controls also passed;
  original native MIR root C/LLVM kept its exact six-line oracle.
  .tmp/inout-array-release-codegen-bootstrap-v41.log,
  .tmp/inout-array-release-bootstrap-v41-exact-boundary-context.log,
  .tmp/inout-array-release-local-ref.sha256,
  .tmp/inout-array-release-mir-root-local-ref.sha256.
  This new oracle does not claim numeric version-scratch cleanup: the first
  native draft's ArrayDrop there was refused, and the release rule is unchanged.
  LocalRef owner is 142 effective lines under the existing 180 cap.
  Seed v42 then refused node 72305 in SelfMirExpressionGraphUsesAppend at
  unproved_formal_shallow_mutation_entry. Linux CI 37314795454 independently
  refused that same node/boundary. The first insertion-copy candidate passed
  native values but not source admission. The existing unique-use owner now
  uses ArrayPushOwnedString; graph-use and assignment consumers copy selected
  indexed text before the ordinary String formal. No permission rule changed.
  New source copied/raw pair passed both C/LLVM observers, with the raw case
  keeping the exact formal shallow-mutation refusal. Native C/LLVM source-
  cleanup survival, first-occurrence order/deduplication, invalid-graph no-
  mutation and original MIR-root six-line values passed. Structural mutation-
  graph pins and Bash syntax checks passed; not full inventory/integration.
  .tmp/inout-array-release-bootstrap-v42-exact-boundary-context.log,
  .tmp/inout-array-release-ci-37314795454-codegen.log,
  .tmp/inout-array-release-unique-use.sha256,
  .tmp/inout-array-release-mir-root-unique-use.sha256.
  Next falsifier is whole-root admission on the original MIR control, not a
  narrow formal-effect count, native value check or elapsed-time inference.
- OPEN: full integration10 timed out at its original 60s actual C unit; its
  unchanged isolated unit later passed seven rows in 46.35s, not full-shard
  evidence. A development C whole-MIR-root observer timed out at 300s; the
  LLVM observer reached a semantic refusal. Neither timeout is admission.
  Fresh seed/fixed-point/installed pair, full C/LLVM integration, 15 structural
  cap failures and current-head CI remain OPEN. The all-current-work checkpoint
  is published; subsequent CI repair remains required. The new user examples
  are included without an execution claim. Official seed v43 refused aggregate
  finalization with node=-1; a fresh diagnostic observer identified its exact
  WithFunctionTables call. Independent whole-root body admission now passes,
  but the gen0 nominal-root codegen consumer has not yet established official
  route evidence on this source. Seed v44 is the next exact falsifier, followed
  by fixed-point/driver receipts; v44's terminal Slice failure and current
  focused fix above supersede that earlier next-step note. Never install from
  an observer or stale seed.
  SemanticAstBodyTypeBundleFromAnalysis remains the body owner and
  mir_collection_receiver_root.pgy the unchanged official consumer input.
  Installed P0 is OPEN, not a CLOSED row or self-host substitution claim.
  The collection cap block was directly rechecked: 16 -> 15 after the reached
  context/control split, without raising caps; not a full inventory pass.
  The new fact/builder/read-target/member owners independently pass their
  unchanged caps. .tmp/inout-array-release-collection-caps-current.log.
  semantic.hashmap_collection_ownership stays ACTIVE. Current authority-edge
  attempts 3/4 returned no verdict in their 60s budget; no current gate pass.
  CI 37305685187 was
  cancelled by the external documentation push; 37305703308 failed a stale
  derived-fact symbol. The corrected live pin and three existing local-view
  registrations passed the narrow authority-edge gate: 95 authorities/200
  derived carriers and 96th-owner refusal. No status/count authority changed.
  .tmp/inout-array-release-sot-authority-edge-current2.log.
  Current published CI 37316971496 completed with failure at the same Linux
  v43 aggregate-finalization boundary. Windows, macOS C-only, TSAN and Rocq
  passed; dependent Linux jobs were skipped. Logs:
  .tmp/inout-array-release-ci-37316971496-codegen.log. Not current green.
  Later df0df7a4 CI 37329776930 reached the Slice emission failure described
  above; its exact job log is .tmp/inout-array-release-ci-37329776930-codegen.log.
  Next after seed: serial full integration, official fresh driver,
  public all-lane gate and default C/LLVM execution of Alrescha's
  F:/JDW_project/alrescha/tests/repros/array_inout_release_frontier.pgy.
- Current official bin hashes are still native 88526595396deba7532991e55d6ae3d74f260e3f34c3c15bea9ecf117a55f3de
  and driver 707dcd40049a1697a5827b2a7c8d3cf509573aa3c0031f2eee338c9fa0d78ec7,
  independently read here. The consumer's current receipt still reports native
  C/LLVM inout/drop refusals. Tested isolated native v3 is not that installation.
- No completion message sent to Alrescha GUI 프레임워크 1단계. Send it only after
  complete P0 verification/current CI. Then resume the reached SoT rung.

## Historical archive boundary

Everything below is lookup evidence, not an active work queue. Current source,
the top card, exact executable receipts and the SoT registry override old status.

### P0 evidence history

- User priority: finish the GUI-blocking caller `ArrayDrop` contract before
  reopening general SoT closure. Alrescha is a separate, read-only consumer at
  `F:\JDW_project\alrescha`; no compiler integration or callee whitelist.
- Verified local HEAD and `origin/main`:
  `d0fa49ea95928c007a6670f2e2fbfb7b7479441b`. Main independently published
  indexed-signature materialization, exact formal-use order and fresh-generation
  cleanup slices. The P0 implementation below is still uncommitted; Root owns
  its integration. Preserve local `gmon.out` outside the integration commit.
- Objective card and edit boundaries:
  `docs/agent_work_directives/inout_array_release_2026-10-05.md`. Native demanded
  parameter-flow ownership carries a separate descriptor-preservation facet;
  self-host source and direct-MIR consumers independently prove preservation.
  No six-bit Slot mask reinterpretation or previously absent permission grant.
- Last legitimate consumers: native `semantic_array_storage_call_argument`,
  source `SemanticArrayStorageCallArgumentEscapes`, and direct-MIR storage
  lifetime admission. Plain element mutation may preserve caller cleanup;
  descriptor rebind, alias retention, resource results and deferred/worker
  handoff remain negative obligations.
- Observed native gate: 15 executed positives per C/LLVM and 30 compile-only,
  preserved-artifact refusals; exit 0. Receipt:
  `.tmp/inout-array-release-native-v2-gate.log`, evidence
  `.tmp/self_hosted/public-array-drop.h3c9Ps`.
- Demanded flow owner gate: PASS including the unchanged 4096-work budget and
  independent 4097-demand refusal. Receipt:
  `.tmp/inout-array-release-param-flow-v2.log`.
- Earlier C/LLVM admission observers: six positives, five ownership
  refusals and four missing/duplicate synthetic-identity refusals each; exit 0.
  Receipt `.tmp/inout-array-release-source-admission-gate2.log`, evidence
  `.tmp/self_hosted/inout-array-storage.BAv0fj`. These are analyzer verdicts,
  not installed-driver or input-program execution evidence.
- The benign Alrescha `array_inout_release_frontier.pgy` ran successfully using
  the native C/LLVM routes. Fresh default/installed C/LLVM remains OPEN.
- Latest terminal official seed v9 passed the previous indexed-read,
  repeated-call and terminal-tail boundaries but failed before generation 2:
  node `10473`, `aggregate_release_source_not_live`; evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.oQXyuf`, log
  `.tmp/inout-array-release-codegen-bootstrap-v9.log`. Exact parser mapping is
  the return of `SemanticAstGenericParameterFactRowsFromOwnerNode` in
  `ast_generic_parameter_fact_owner.pgy`. Terminal constructor inputs were
  incorrectly required to have a continuing escape bound. The current
  reservation candidate instead consumes their exact admitted input identity
  at ordered completion, preserving all prior escape/element obligations.
- The fresh-generation cleanup candidate passed focused C/LLVM analysis:
  four positives and ten compile-only ownership refusals each. Evidence:
  `.tmp/self_hosted/collection-owned-generation.bXzBSA`. Terminal return and
  continue branches do not revive a consumed generation; same-scope use,
  duplicate transfer and nested repeated/deferred use remain refused. The
  current candidate reuses the existing retirement owner's loop-order proof.
- Readonly-field prerequisite: three positives and eight compile-only refusals
  per C/LLVM analyzer passed. Evidence
  `.tmp/self_hosted/member-indexed-read.d42Idl`, log
  `.tmp/inout-array-release-member-read-gate5.log`. This reuses the ordered
  member pass and propagates a negative whole-root borrow obligation through
  exact forwarding edges. Copies, transfers, field moves and deferred use do
  not become read permission. This is focused analysis, not installation.
- Current-local-read and repeated-call candidate: five positives and eleven
  compile-only refusals per C/LLVM analyzer passed in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.ZS61LG`, log
  `.tmp/inout-array-release-borrowed-read-gate4.log`. The negative effect owner
  and existing definition closure carry descriptor consumption separately from
  sibling invalidation. Exact current owned storage receives shallow copyout;
  alias, unknown, detached and repeated consuming cases remain refused.
- A later terminal-site candidate passed 6 positives/11 refusals in
  `.tmp/self_hosted/collection-borrowed-descriptor-read.aA3bHJ`, but Root
  independently found C/LLVM both wrongly accepting
  `.tmp/inout-array-release/terminal-hides-later-retention-negative.pgy`.
  It is **not an accepted integration candidate**. Findings:
  `docs/audits/inout_array_release_terminal_bounds_2026-10-05.md`.
- Source-issued MIR contract shard: five executed positives per C/LLVM,
  two source refusals and five MIR storage-lifetime refusals per backend; exit
  0. Evidence `.tmp/self_hosted/inout-array-storage-mir.54XVsd`, log
  `.tmp/inout-array-release-mir-contract-gate2.log`. Scaffolds were native-built
  calls into current production owners, not an installed driver. Subsequent
  cleanup-owner changes still require a fresh integrated receipt.
- The user selected Main interruption and sole completion here. Main was
  observed idle/interrupted; Root now owns source, integration and Git. No
  new Main work queue or worktree. Preserve all pre-existing P0 edits.
- Current terminal/continuing descriptor candidate: C/LLVM each passed 11
  analyzer positives and 19 compile-only ownership refusals, including normal
  path retention, compound terminal retention, stale sibling read, legitimate
  short-circuit RHS/own cleanup, owned aggregate return and borrowed aggregate
  refusal. Receipt `.tmp/inout-array-release-borrowed-read-gate14.log`, evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.M0zIPw`. This is not
  installed-driver or self-host fixed-point proof.
- Full collection integration7 reached its final compiler-scale C actual
  producer unit but timed out at the unchanged 60-second limit; LLVM was not
  reached. Receipt `.tmp/inout-array-release-collection-integration7.log`,
  evidence `.tmp/self_hosted/collection-inout-effect.F86gp4`. A diagnostic
  run on the same source input observed 152.48 seconds and an element-use
  refusal, not semantic PASS. Do not enlarge the gate budget to call it green.
- Next falsifiers: the exact owned aggregate return at the unchanged official
  seed, then default installed C/LLVM caller cleanup. Seed v10 was interrupted
  during gen0 compilation when its process handle disappeared; no receipt was
  completed. Fresh official seed v11 uses
  `.tmp/inout-array-release/codegen-bootstrap-v11` and
  `.tmp/inout-array-release-codegen-bootstrap-v11.log`; inspect the live process
  or final result rather than inferring success from generated files.
- Seed v11 has since terminated with an owned-mutation compound-return refusal:
  node `10504`, `compound_terminal_storage_effect`, evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.OyB1fW`. It passed the
  prior aggregate reservation boundary. Exact parser mapping is the final
  Boolean return in `SemanticAstGenericParameterFactContractReady`, which
  populates two fresh arrays before reading their current elements. The current
  terminal-entry candidate requires unshared exact storage for owned-content
  mutation; aliases and constructor capture still prevent that proof.
- Measured formal-use lookup delta: the unchanged compiler-scale C actual
  producer unit passed all seven formal observations in 49.55 seconds, within
  its original 60-second budget. Receipt
  `.tmp/inout-array-release-identity-context-c-actual.log`, executable/hash
  `.tmp/inout-array-release/identity-context-c.exe` and
  `.tmp/inout-array-release-identity-context-c.sha256`. Only the repeated global
  retention identity lookup changed to the existing context-bound owner.
  Full C/LLVM integration and post-terminal-entry revalidation are still OPEN.
- Fresh paired installed-driver parity, seed/fixed-point completion and
  exact-head CI are OPEN. Published run `37244277987` for `d0fa49ea` failed its
  nominal MIR-root bootstrap at `unproved_formal_indexed_read_entry`; it is not
  green. Do not mark the SoT row CLOSED or
  send a completion handoff to `Alrescha GUI 프레임워크 1단계` yet.
- Optimization remains permitted only at an observed operation blocking this
  named closure step. No new cache, query engine or parallel implementation rung.
- Current focused terminal-entry receipt: C/LLVM each passed 13 analyzer
  positives and 21 compile-only refusals. Evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.pDJC5H`, log
  `.tmp/inout-array-release-borrowed-read-gate18.log`. Complete direct-inout
  formal-use and absent-retention facts admit chained terminal mutation;
  formal retention, aliases and same-expression constructor capture remain
  refused. This is not installed-driver evidence.
- Collection integration9 passed its full C leg, including all 371 source
  inputs, forged-identity checks and the unchanged 60-second actual producer
  unit. It then timed out compiling the LLVM source observer at the existing
  120-second limit, before LLVM execution. Evidence
  `.tmp/self_hosted/collection-inout-effect.McarMK`, log
  `.tmp/inout-array-release-collection-integration9.log`. Later source changes
  and the additional formal-chain fixture require a new integrated receipt.
- Seed v12 passed the fresh-local terminal mutation case and refused node
  `31679` in `SemanticAstIntentExpressionSeedEnvironment` at
  `compound_terminal_storage_effect`. Evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.5UKIwY`. The current
  direct-formal candidate passed focused gate18; official seed v13 later passed
  that boundary and refused node `51211`, `unproved_inout_copy_entry`, in the
  generic-specialization environment assignment. Evidence
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.8F0FHm`; exact mapping
  `.tmp/inout-array-release-bootstrap-v13-exact-boundary-context.log`.
- LLVM compile diagnostic on the exact unchanged observer input took 228.07
  seconds; pipeline timing attributes 214.394 seconds to the backend. Receipt
  `.tmp/inout-array-release-source-context-llvm-profile.log`. This diagnostic
  used a separate 300-second inspection bound, not a larger acceptance budget
  or a collection integration PASS. The timestamped diagnostic subsequently
  took 144.82 seconds: approximately 7 seconds IR emission, 34 seconds IR
  optimization and 95 seconds aggressive target object emission. The existing
  dev profile now selects unoptimized machine emission while keeping the O2 IR
  pipeline, mandatory verification and release's aggressive policy. This
  candidate still requires a fresh native executable and unchanged integration
  gate. The local Clang native build failed linking unavailable `libomp`;
  an isolated rebuild uses the existing GCC toolchain instead.
- The collection structural size inventory currently has 17 cap failures,
  including retained pre-existing owner growth. Do not report the complete
  component contract green or raise those caps silently. No SoT row closes
  from a focused observer, seed or documentation update.
- Focused gate19 passed 14 positives and 23 compile-only refusals each on
  C/LLVM. Evidence `.tmp/self_hosted/collection-borrowed-descriptor-read.6KckA7`,
  log `.tmp/inout-array-release-borrowed-read-gate19.log`. Call admission now
  consumes the existing conditional fresh-generation proof; its duplicated
  narrower definition check was removed, and the file is 105 counted lines
  under its restored 115-line cap. No intervening loop/deferred boundary was
  admitted. Official seed v14 is running on that frozen candidate; judge its
  terminal receipt before installed-driver work or further semantic edits.
- Fresh GCC native v3 build completed with existing repository warnings;
  SHA-256 `4bdb8869388dce2a8c02a5791f1670ada26b9e1cacd47c0b7d8195353e4673ce`.
  Native public ArrayDrop revalidation passed 15 executed positives per C/LLVM
  and 30 native compile-only preserved-artifact refusals. Evidence
  `.tmp/self_hosted/public-array-drop.paDMfA`, log
  `.tmp/inout-array-release-native-v3-gate.log`. This native-only stage did not
  execute public self-host or direct-MIR lanes despite their summary text.
- The inout/drop assertion also ran successfully with native v3 on C/LLVM in
  both dev and release profiles: four fresh executed artifacts, pinned by
  `.tmp/inout-array-release-native-v3-profile.sha256`. Full collection
  integration10 subsequently timed out at its final C actual producer unit
  (unchanged 60 seconds), before the LLVM leg. Evidence
  `.tmp/self_hosted/collection-inout-effect.xYfWKR`, log
  `.tmp/inout-array-release-collection-integration10.log`. Do not infer a full
  integration PASS from the earlier small-case or native receipts.
- Exact LLVM source-observer compilation with native v3 completed in 57.46
  seconds under the original 120-second limit; backend timing is 49.917 seconds.
  Receipt `.tmp/inout-array-release-source-context-llvm-dev-v3.log`. This fixes
  that compile-budget blocker but is not the complete C/LLVM integration gate.
- Official seed v14 passed conditional mutation and refused node `51242`,
  `unproved_indexed_read_entry`, at generic-call capture inside an inner work
  loop. Evidence `.tmp/self_hosted/codegen_nominal_array_declaration/run.XgxCl9`,
  exact mapping `.tmp/inout-array-release-bootstrap-v14-exact-boundary-context.log`.
  Next falsifier: a fresh outer-loop descriptor read repeatedly before its
  generation's later owned cleanup. Preserve real prior/nested/deferred
  consumption, alias and escape refusals; do not skip a zero retirement bound
  in a read consumer.
- Separate unchanged-input C pressure diagnostic: parse 5359 ms, analysis
  3172 ms, body 69438 ms, total 78.36 seconds; body verdict remains
  `unproved_formal_element_use_entry`. Receipt
  `.tmp/inout-array-release-body-stage-pressure-run.log` and its SHA manifest.
  It is diagnostic, not gate success. The same-input timestamped rerun finished
  in 48.73 seconds with that refusal; no further performance change followed.
- Integration10's isolated, unchanged C actual producer unit subsequently
  passed the exact seven expected formal rows in 46.35 seconds under its
  original 60-second limit. Receipt
  `.tmp/inout-array-release-integration10-c-actual-isolated.time` and adjacent
  raw/error/expected-row files. This does not supersede the full shard timeout.
- Focused gate20 rejected the first generation-relative retirement candidate
  at `unproved_indexed_read_entry`. Gate21 then rejected its terminal-scope
  revision at `owned_argument_storage_not_live`: an early terminal own call
  preceded a later continuing cleanup. Neither rejected candidate is accepted.
- Current focused gate22 passed C/LLVM analysis: 15 positives and 26 compile-
  only ownership refusals each. Evidence
  `.tmp/self_hosted/collection-borrowed-descriptor-read.X2bzgH`, log
  `.tmp/inout-array-release-borrowed-read-gate22.log`. Known, unique consumption
  is projected at its generation-relative site; nested reads prove membership,
  not repeatable transfer. A terminal own entry may precede a later continuing
  bound, but cannot erase a prior one. Post-transfer, nested-consumption,
  terminal-hides-normal-consumption and deferred negatives remain refused.
- Official seed v15 is running alone on that frozen candidate at
  `.tmp/inout-array-release/codegen-bootstrap-v15`, log
  `.tmp/inout-array-release-codegen-bootstrap-v15.log`. Seed/fixed-point,
  fresh installed-driver, full collection integration and current CI are OPEN.

### Earlier collection checkpoint

The previous collection-ownership checkpoint below is lookup evidence only,
not the active work queue. Current source, exact executable receipts and the
SoT registry override its older status.

### Checkpoint

- Branch: `main`.
- Published compiler checkpoint: `8abceacd04aad7144edd87f7495e160205f2d0ab`
  (`checkpoint: integrate owned-result and borrowed-view contracts`). Root
  verified that `origin/main` matches this SHA after push.
- This 63-file source checkpoint includes owned-result evidence,
  borrowed String views for signature binding, collection mutation policy,
  source-module/location ownership, and their gates. The follow-on commit
  carrying this card changes only inventory transport, its checker fixtures,
  and coordination/handoff documents; compiler implementation remains at this
  checkpoint. Resolve the inventory revision with
  `git log -1 --format=%H -- tests/self_hosted_component_contract_smoke.sh`.
  Only `gmon.out` is intentionally excluded from publication. Verify local HEAD,
  dirty paths, and remote HEAD when resuming.
- Root owns integration and Git publication; Main owns the reached compiler
  implementation. Parallel edit scopes are fixed in
  `docs/agent_work_directives/ownership_checkpoint_green_2026-10-05.md`.
- `deployment_optimization_guide.md` was preserved unchanged in published user
  document commit `9157646e538a0528a85188f54012a059cf6e0c0a`. Preserve `gmon.out`
  locally and exclude it from staging. Neither is
  compiler semantic authority or evidence for this rung.
- This remains an OPEN executable-rung checkpoint, not ownership SoT,
  installed-driver, bootstrap, CI, or whole-compiler closure.

### One active executable rung

The active registry row remains
`semantic.hashmap_collection_ownership` (`ACTIVE`). No registry row is promoted
by this checkpoint.

Production entrypoint:
`SemanticAstCollectionOwnershipVerdictFromResolvedFactsWithFormalEffects` in
`src/self_hosted/semantic/ast_collection_ownership_verdict_owner.pgy`.

Direct native C consumers remain reachable through collection ownership,
storage-release, and type-checker paths. Therefore Pergyra has not yet replaced
the complete C-owned compiler path.

### Objective card

- Objective: move aggregate and lexical collection lifetime decisions behind
  exact Pergyra owners until the reached production path no longer depends on
  C-owned reconstruction.
- Priority: stable identity and one owner; fail-closed missing facts; delete the
  former owner; add a negative ratchet; then parity and bounded performance.
- Fact owner: the self-host collection ownership verdict and its admitted
  formal-use, exact-leaf-transfer, environment, and storage-lifetime facts.
- Last reached consumer: the MIR-root self-host bootstrap route exercised by
  `tests/self_hosted/parity/fixture/mir_collection_receiver_root.pgy`.
- Forbidden fallback: name/address/container-position identity, shallow copy as
  ownership transfer, a whitelist for the reached call, or retaining the old
  cursor/storage owner beside the replacement.
- Last terminal falsifier: bootstrap node `46983`,
  `unproved_formal_element_use_entry`, type `Array<String>`, in
  `codegen-nominal-array-declaration`. The mapped function is
  `SemanticExpressionGraphCollectionReceiverMutationFact` in
  `ast_expression_graph_collection_mutation_owner.pgy`; its `types: Slice<String>`
  parameter is the reached node. The reduced import graph passes, but the
  full-root formal-effect chain still rejects this entry. Numerical node
  movement is not closure.
- Executable rung: BLOCKED at that formal-use entry, not closed by inventory
  fixes. The missing fact is a successful full-root formal-element-use proof
  for the reached `types` parameter and its call chain. The producer is
  `SemanticAstCollectionFormalEffectsFromResolvedFacts` through the formal-use
  and fixed-point owners; the last admission consumer is
  `SemanticAstCollectionCallArgumentVerdict`, followed by the MIR-root bootstrap.
  The falsifying input remains `mir_collection_receiver_root.pgy` in the full
  import context. Do not substitute a reduced graph's PASS for this proof.

### Published implementation

- Lexical expression environments now own their lifetime directly. The former
  expression-environment storage-lifetime owner was deleted.
- Initializer environment rows replaced the separate cursor owner and its old
  cursor gate; the retired owner and gate were deleted.
- Aggregate member moves carry an exact owned-push leaf receipt. One positive
  loop fixture and eight alias/borrow/defer/restore/duplicate/push negatives
  ratchet the admitted shape.
- Owned formal forwarding admits the exact shallow-forward case while rejecting
  use after the transferred value is dropped.
- Indexed String copy policy admits the owned results of `StringJoin` and
  `TextBuilderFinish` without treating arbitrary calls as copies.
- Three resolved call-target names and one resolved call return type now
  materialize independent String results instead of returning indexed borrows.
- Owned-result plan, definition, and return owners derive grounded fresh
  `Array<String>` results from exact declared callable identity. The verdict
  carries `fresh_owned_result_function_syntax_ids`; this is not a name allowlist
  or permission to promote unknown/borrowed results.
- Routine body retirement covers 94 backing leaves:
  `67 Array<Int>`, `24 Array<String>`, and `3 Array<Bool>`, including the new
  owned-result function-ID carrier. The two deep destructure arrays retain
  `ArrayDropOwnedStrings`; other backing arrays use
  `CompilerRetireArrayStorage` at their existing last-consumer boundary.
- `SemanticAstSignatureParameterTypesBind` consumes its `own Array<Int>` index
  row, borrows three `Slice<String>` views (generic names, actual types, prior
  bindings), and returns an independent binding row. Callers retain the String
  backing arrays and retire their temporary inputs after return. This API is
  not a general proof of Slice non-retention or GUI `inout` release.

### Observed verification

These are observed receipts, not results for every later revision. Root checked
the cited Main receipts and its own structural checks; this documentation
refresh does not run compiler or parity gates.

- `routine_build_storage_lifetime_owner.sh`: PASS, 94-leaf body census
  (`exec-ceb9cb8b-c271-4d5b-915c-33386c8fe824`). This is backing-retirement
  coverage, not a whole-program lifetime verdict.
- `collection_owned_result_owner.sh`: three positive and nine negative cases
  per C/LLVM PASS (`exec-0a8dc1f3`). The probe analyzed source fixtures; those
  fixtures were never emitted or run.
- `generic_return_probe_parity.sh`: native LLVM-only PASS (`exec-563b114f`).
  Installed C failed with `compiler_internal_builtin` against a stale admitted
  caller registry; full C/LLVM and installed-driver parity are not established.
- Collection-policy gate with `BACKENDS=llvm`: native C oracle plus native LLVM
  probe execution/refusal PASS (`exec-29c8ffc3`), not installed C evidence.
- Root's changed component-inventory predicates and the generic-return gate's
  structural prefix PASS. `bash -n` and `git diff --check` PASS. The full
  component inventory on both Windows and mounted WSL exceeded its unchanged
  60-second budget (exit `124`); PowerShell had previously collapsed the native
  shell result to `1`. Neither run is a full component PASS or a semantic
  rejection receipt.
- Follow-on inventory transport isolates GNU make's response-file expansion in
  a temporary `BUILD_DIR`; installed/standalone/admitted bootstrap counts remain
  `0/1/0`. Root independently ran the actual checker on two positive and eight
  negative fixture cases plus the existing 14 lexical unit tests: PASS, exit
  `0`, within 60 seconds. Syntax and diff checks PASS. The production Makefile
  graph check on mounted WSL reached exit `124`; full component completion is
  still OPEN. GNU 3.x fixture support is present but GNU 3.x/macOS was not run.
- The reduced collection-mutation import graph produced `body_ok=true` with
  no effect-5 rows (`exec-c8b28a2c`). The current full-source native C emission
  succeeded with 0 errors and 16 warnings (`exec-730c1fe7`). The subsequent
  full-root seed failed (`exec-33d6597b`, node `46983`), so neither reduced
  analysis nor native emission establishes root bootstrap success.
- Correction to the previous exact owned-push leaf claim: one positive and eight
  negative cases produced C/LLVM analyzer verdicts. They were not emitted-fixture
  runtime executions.
- Seed bootstrap terminal receipt
  `exec-b71cb70a-ddb1-4e86-9a6c-be6b9cfa147e`: exit `1`, node `46954`,
  `unproved_formal_element_use_entry`, `Array<String>`, work directory
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.nbZkMd`.
- Mapping receipt `exec-e02fe355` identifies the function above at
  `ast_expression_graph_collection_mutation_owner.pgy:134`, with
  `SemanticExpressionGraphCollectionMutationFact` declaration/return atom.
- Latest seed `exec-33d6597b-bdda-4644-b830-8b35b92df0c3`: exit `1`, node
  `46983`, same `unproved_formal_element_use_entry`, work directory
  `.tmp/self_hosted/codegen_nominal_array_declaration/run.bTaLNs`. Mapping
  `exec-ced9ee76` identifies the `types` Slice parameter of the reached function.
- Prior exact-head [CI run 37211217648](https://github.com/srtdog64/PergyraLang/actions/runs/37211217648)
  on base `1e2fd61d` completed with failure: bootstrap failed, six downstream
  Linux jobs skipped, and five other jobs succeeded. It is not current green.
- Published-checkpoint [CI run 37223614427](https://github.com/srtdog64/PergyraLang/actions/runs/37223614427)
  on `8abceacd04aad7144edd87f7495e160205f2d0ab` selected `run_full=true`.
  [Codegen job 111498732648](https://github.com/srtdog64/PergyraLang/actions/runs/37223614427/job/111498732648)
  failed in step 4 at `2026-10-04T18:20:46Z`: node `46983`, boundary
  `unproved_formal_element_use_entry`, diagnostic type `Array<String>`,
  nominal MIR-root control refusal, Make exit `2`. Native/codegen seed
  publication steps 5 and 6 were skipped. The CI diagnostic type is not the
  source parameter spelling `Slice<String>` from the local mapping above.
  The run completed with failure: 5 jobs succeeded, codegen failed, and 6
  downstream job families were skipped. This published checkpoint is not green.

### Explicit OPEN boundaries

- The last seed bootstrap failed. Published mutation-view changes do not supply a
  successful fixed-point or same-input terminal receipt by themselves.
- Installed `pgy-self-driver.exe` admission is stale. Native/analyzer build
  success is not installation or installed-driver evidence.
- Publication is complete for the source checkpoint above. Exact-head CI green,
  fixed-point bootstrap, full C/LLVM
  parity, full component gate completion, and the platform matrix are OPEN.
- Optimize only an observed operation blocking the next named closure step,
  behind its existing owner and on the same semantic input. Prior timing or
  source repetition does not prove a current bottleneck or authorize a separate
  cache/query/performance track.
- The GUI prerequisite remains separate: caller storage after `inout` is OPEN
  until callable non-retention/exclusivity proves that release is legal.
- The row stays ACTIVE until owner and last-consumer migration, missing-fact
  refusal, old-path deletion, negative gates, installed-driver evidence, and
  exact-head CI all exist.
- Do not hand off completion to `Alrescha GUI 프레임워크 1단계` until the required
  verification is complete. Alrescha remains a separate framework at
  `F:\JDW_project\alrescha`, not a compiler integration or semantic workaround.

### Language and IDE boundary

Pergyra owns machine-verifiable `WHAT MUST HOLD`: state, invariant, authority,
ownership, capability, effect, transition, intent, type, and boundary. Human
`WHY` or rationale remains in ADRs, issues, requirements, design notes, commits,
and discussions. IDEs may link those external artifacts by stable semantic
identity, but prose is not compiler authority and must not become language
syntax. `docs/00_vision.md` owns this boundary.

### Next falsifying case

Use the same `mir_collection_receiver_root.pgy` semantic input and the last
mapped receiver-mutation owner to interpret the next terminal bootstrap
receipt. Check exact formal-use evidence, view lifetime, and the last consumer
against current source. Fix only the reached owner seam; do not substitute a
smaller input, name whitelist, native bypass, compatibility fallback, duplicate
SoT row, or a general query/cache architecture. A green claim requires terminal
receipts for the exact published source HEAD, not this navigation snapshot.

Older checkpoints are evidence in Git history, not an active work queue. Do
not revive them unless the current source, registry, or reached falsifier points
back to them.
