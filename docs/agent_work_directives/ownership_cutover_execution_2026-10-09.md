# Ownership cutover: execution preparation

Status: `ACTIVE P1 / REACHED MEMORY OBSTRUCTION REPAIRED; FULL CUTOVER OPEN`.
Current local base: `main @ 5651c916c87e030cc3ef579180abdfbf1814d1cc`, with
subsequent production resource-flow, direct-control and documentation edits.
The same-folder working branch is now `main`, local HEAD `aa7f0d65`, with
pending CI/enum-owner/documentation corrections. By explicit user request,
the former candidate was renamed locally and remotely to `backup` at that
SHA; branch conversion preserved every modified/untracked file byte. No
additional worktree or writer was introduced. This is a workflow change,
not P10 acceptance, green CI, automatic-cleanup activation or main publication.
Implementation checkpoint `5ca69b4fffcc56f1e4103e914b8c041efb1aa027` was frozen
through a fresh full-driver run: exit 0, private 1.289 GiB, unchanged cap and
MIR hash, no declared endpoint drift. Later navigation edits record that
receipt; P1/whole cutover, installed pair and remote CI remain OPEN.
The original preparation base was `a75da80435e0d051f71cfacf76d44ea825ed87d7`;
its older dirty-input observations below are historical. Excluded untracked
`gmon.out` is preserved. Full P0/installed/fixed-point/remote CI are not green.

The user's latest instruction, "구현 시작해 클로드가 시작했으니까", reopens
implementation. It supersedes the earlier implementation hold, not the
dependency gates in `ownership_cutover_plan_2026-10-08.md`. This directive is
coordination, not a new semantic owner or evidence of P1 closure.
The subsequent instruction "전체 작업해" confirms the full transition scope.
Claude reports CL6/CL7 complete; GPT now independently checks the two new
models through the existing permanent preflight consumer. An older hold in
the plan or collaboration history does not override these latest directions.

## Shared objective card

- Objective: prepare an identifiable, repeatable P0 measurement before the
  automatic-ownership implementation changes its compiler input.
- Priority: preserved source identity, explicit measurement scope, fail-closed
  input binding, unchanged 3072 MiB ceiling, then implementation size.
- Fact owner: `scripts/measure_build_pressure.ps1` owns command execution,
  process-tree pressure and its execution receipt. Doc 27 remains the owner
  of ownership-clean semantics.
- Last consumers: the pressure contract gate, its executable receipt self-test,
  and the P0/candidate comparison audit. A receipt identifies the supplied
  paths; it does not certify that a caller supplied every gate input.
- Forbidden: accepting a changed declared input as a stable run, attributing
  another lane's process to this run, treating working-set/private memory as
  heap live or cumulative allocation bytes, or calling this preparation P0
  completion, P1 closure or compiler substitution.
- Integration owner/gate: GPT; `tests/build_pressure_contract_smoke.sh`,
  including the Windows execution self-test. Missing runtime coverage is
  explicitly reported on non-Windows hosts, not presented as that test passing.
- Falsifiers: declared input/tool/executable changes during the run; missing
  input; command failure; altered argv; Unicode path damage; missing input
  coverage mislabeled as a source baseline.

The next dependency seam has its own bounded objective: consume the actual
core-call recovery and static view-scope theorems in
`tests/coq/OwnershipCutoverPreflightAudit.v`, with the focused entrypoint
`tests/ownership_cutover_preflight_smoke.sh`. The model files remain Claude's
owners; the audit checks exact theorem types, positive traces and alias,
escaping-control, backing growth/transfer and view-escape refusals. No
`recovery_exec` theorem may substitute for the ordinary-table call witness.
This closes only an importing proof obligation, not source-place evaluation,
physical descriptor/pointer refinement or production ownership issuance.

## Verified affected chain and bounded change set

The six pressure-owned Make routes (native compiler, self-host compiler,
full driver and its MIR shards) enter the existing PowerShell measurement
owner. That owner starts one command, observes process-tree memory and stage
output, then writes CSV/logs and `*.summary.json`. The existing contract gate
is reached from `build-source-inventory-test-smoke`. No production code reads
the summary as an ownership fact. The memory audit is a reporting consumer.

Required changes are confined to that chain:

1. The measurement owner records cwd, exact argv, resolved command hash,
   probe hash and explicitly supplied input/executable hashes before/after.
2. Changed declared bytes invalidate a successful command's receipt. An empty
   input list is explicitly unbound, never a clean-checkpoint claim.
3. The same owner still owns pressure/stage metrics; heap live/cumulative
   counters remain UNMEASURED, not guessed from private memory.
4. Update the existing structural gate for the receipt version and execute
   its Windows positive/negative consumer. No new test framework or CI lane.
5. Freeze and commit the complete gate-input set before the full P0 run;
   this script change is itself one of those inputs. The current dirty run
   cannot be relabeled as `a75da804`'s frozen baseline.

Reached dependency (observed during integration): the gate-reachability owner
reports five proof wrapper scripts without execution roots. Their permanent
regression consumers are already in `coq_kernel_check.sh`; do not add five
duplicate full-corpus CI executions. GPT adds literal, focused Make entrypoints
for those existing wrappers and reruns the reachability gate. This is P0
gate registration, not a new proof, a scheduled execution result or SoT closure.

This is baseline infrastructure, not the P2 storage/glue or P3-P7 consumers.
The source-to-MIR route currently enters `mir_lower` from `driver_app.c`,
constructs typed routine facts, recomputes analysis and runs the existing DCE
before `mir_validate`/C or LLVM emission. The future ownership-clean owner
must consume refreshed final-generation analysis, not those earlier facts.
The existing runtime-operation identity and layout owners remain authoritative;
the designed call-contract join still has no production issuer. The complete
replacement/deletion inventory remains in the cutover plan, not this receipt.

## Independent edit scopes

- Claude's CL6/CL7 definitions are completed importing owners, not an active
  parallel edit lane. The user confirmed every other writer stopped. Preserve
  those definitions and consume them through the independent focused audit.
- GPT: the measurement owner, its structural/executable gate, the five
  focused Make execution roots and their reachability check, this directive,
  GPT's current handoff card, the demonstrated production obstruction repair,
  direct-control refinement and subsequent implementation after its dependency
  gates pass. The checkpoint/reached baseline precedes production changes.
- Do not spawn another implementation lane or overwrite the official binaries.
  If a new overlap appears, stop only that edit and resolve its owner.

## Commands and budgets

- Static owner gate: 60 s; focused Windows receipt gate: 300 s.
- P0 integration shards: 1800 s, at the existing cap. Keep exact command,
  semantic input, source snapshot, actual compiler and artifact hashes.
- Generated probes/logs stay under fresh `.tmp` paths. Never delete
  `gmon.out`, scratch evidence, another lane's outputs or shared processes.
- Shared-tree freeze must cover the second checkpoint through P0 validation,
  including proof, document and gate writers. The latest user instruction on
  2026-10-09 confirms that the other writers have stopped. Record the second
  checkpoint before its baseline execution; do not edit gate inputs during it.

## Remaining boundaries

The reached frozen baseline and actual production-obstruction repair are
recorded below. The COMPLETE P0 installed/fixed-point/sanitizer/platform matrix
and heap-live/cumulative counters remain incomplete, not PASS. The receipt
does not implement allocation counters. The all-writer stop remains confirmed.
CL6/CL7 passed GPT's independent focused consumer
(nine kernel-checked modules, no assumptions) and the full formal integration
(77 modules plus the approval binding consumer; only the two existing
approved Slot abstractions). These are bounded core/view-scope results;
production call-contract binding and source/physical refinements remain OPEN.
P2-P7 do not begin while
their P1 dependency gate is red. Candidate publication, main landing,
installation and GUI readiness retain the cutover plan's separate gates.

Outputs here are implementation candidates and observed gate receipts, not
an automatic-memory-management implementation or a CLOSED SoT row.

### Next producer/consumer chain (verified current source)

The actual next falsifiers are field inout, readonly ordinary temporary and
backing growth AFTER the last writable Slice use. Growth BEFORE the last use,
root/descendant overlap and duplicate actuals must stay rejected. The no-manual-
release emitted-C probe also leaks 8 bytes; its explicit historical cleanup
control is clean. Do not report that reference as the final language contract.

The production entrypoint is still `driver_run_pipeline`. Reached admission:
`type_checker_helpers_late.c` forbids non-identifier inout and compares only
identifier spellings; `type_checker_ownership_call.c` requires named ref/own
boundary sources; Slice facts in `type_checker_ownership_let_slice.c` are
lexical and are read by assignment and Array mutation admission. Those are
existing guards, not final normalized place/lifetime certificates.

Downstream C calls use `transpiler_expr_call_user_emit.c`: inout arguments
become addresses and argument ordering is currently materialized there.
LLVM `llvm_expr_boundary_projection_helpers.c` passes a local address only
for identifier value-result arguments; a member currently reaches expression
value emission instead. Thus removing the semantic field guard alone does
not implement field inout on both backends. The self-host semantic verdict
owner also requires a direct binding. All three guards/consumers are within
the same planned normalization change, not three independent exemptions.

Required owner-directed sequence: typed source place/temporary identity and
ordered one-evaluation graph -> normalized call operands and disjointness ->
call outputs/origins/recovery and backing dependencies -> fresh final-MIR
analysis -> C/LLVM consumers of the SAME facts. The call-contract join in
doc 27 remains the selected target; no backend-specific ownership table or
all-borrowed default is permitted. Affine/authority named boundaries remain.
Source-routine call modes are not assumed to describe runtime result origin.

Before expanding that patch, fix the normalized fact schema and source/core/
physical refinement, include all current carriers/JSON consumers in the
existing whole-cutover inventory, and check alias/index/short-circuit/partial
failure and inout restore-before-handler cases. A lexical scan for a later
use is not the final lifetime issuer. Reusing pre-normalization liveness is
forbidden. No P2-P7 automatic glue/drop activation until these P1 dependency
checks pass; no new syntax, manual carrier or named-local recipe as a fix.

## Frozen checkpoint and reached production repair

The renewed user-confirmed all-writer stop permits the local checkpoint
`5651c916c87e030cc3ef579180abdfbf1814d1cc` (30 reviewed text inputs;
`gmon.out` excluded). Its tracked/index diff stayed zero throughout the
baseline gates. Full formal: 77 modules plus approval consumer, PASS. Ten
native unit batteries and native C/LLVM clock/scalar checks PASS. Existing
likeness RED (sentinel 73 > 20), component inventory timed out at 60 s, and
MIR integration refused the absent same-source isolated self-host driver.
The frozen full-source measurement bound all 10,559 tracked files with no
endpoint drift: exit 88, 131209 ms, sampled private 3114.3 MiB, cap 3072.
The complete installed/fixed-point/sanitizer/platform matrix is dependency
blocked, NOT PASS; exact heap counters remain UNMEASURED. Do not call this a
green full P0 or use the old installed pair to fill those missing rows.

The next bounded operation is the measured ResourceFlowUniverse producer
repair, not P2-P7 or an ownership-drop activation. Its objective is to admit
actual value declarations once with a validated identity memo, leaving every
required value fact and negative resource check intact. Priority: one
declaration identity owner, fail-closed current binding, unchanged evidence
lifetime and artifact behavior, then memory cost. The owner is
`type_checker_flow_universe.c`; reached producers are snapshot classification,
nested declaration capture and function seal. HIR validates routine-local
rows, RIR/MIR carry them, and MIR validation/JSON are the last consumers.
No downstream row filtering or second identity table is allowed.

Complete change set: one shared CLASS/TYPE_PARAM exclusion at existing owner
admission and all three producers; validate epoch/index hints against that
owner's existing entry matcher; import kind, generic/Future/Slot, missing-owner
and ordinary/fresh-context/wrapped-generation cases into the existing semantic
unit battery. Preserve source/ABI identity, scope teardown, all other symbol
kinds and projections. No new source annotation/copy is needed.
Acceptance: importing negatives fail the old producer; semantic/HIR/DIR/RIR/
AIR/MIR units, existing flow/loop and native C/LLVM cleanup gates pass;
the SAME complete driver emits the previously checked full MIR under 3072 MiB.
The snapshot frame lifetime and production cleanup/refinements remain OPEN.
Baseline logs: `.tmp/ownership-cutover-2026-10-09/p0-5651c916-7aeb6d7b3bc44252ba1e80bf2027449b/`.

## Measured P0 obstruction and bounded diagnostic scope

The unchanged production source reaches `driver_run_pipeline` -> semantic ->
HIR -> DIR -> RIR -> AIR -> `mir_lower` on the full
`src/self_hosted/compiler/driver_bootstrap_main.pgy` import-composed input.
The fresh native executable is SHA-256
`8b26d1d82cf94481ff76a26eaa83b151e6f38c4f3b2099dc4c5c7e17d24af754`.
Both direct-process pressure runs bound all 2531 tracked self-host sources,
observed no byte drift and stopped at the unchanged 3072 MiB cap: 3210.3 MiB
private at 78.958 s, and 3080.2 MiB at 78.205 s with existing stage timing.
The initial last reached owner was `mir_lower`. V4 subsequently identifies
the repeated declaration-row/state payload issued by ResourceFlowUniverse;
exact heap allocation counters remain UNMEASURED. These are dirty-tree
diagnostic observations, not the second checkpoint's official P0 results.

- Objective: identify the allocating MIR operation that blocks this exact
  next validation step, without changing semantics or the ceiling.
- Fact owner: existing `mir_lower`, routine facts and scratch arena; the
  existing pressure owner measures process memory. Its OS private-byte
  observation is not a complete heap-live or cumulative-allocation counter.
- Last consumer: MIR validation/serialization; those stages were not reached
  by either refused run.
- Forbidden: a smaller source graph, larger cap, skipped safety check,
  substituted old executable, generic optimization track, or edits to the
  production source before the baseline boundary is resolved.
- Bounded change: instrument only a scratch copy of `src/compiler/mir.c`
  under `.tmp/ownership-cutover-2026-10-09/`, reusing this run's already-built
  native objects and a separately named executable. Record memory before/after
  the existing per-routine operations and the arena's own scoped counters.
  No second semantic authority or production owner is introduced.
- Continuing scope: count the reached admitted semantic/HIR/RIR resource
  rows and loop states at MIR entry in a separate v3 scratch copy. This
  distinguishes actual payload size from sampled net private deltas before
  choosing a storage repair; no AST reconstruction or production edit.
- The v4 admitted-kind census confirms 4,730,448 CLASS declaration rows
  versus 25,434 VARIABLE rows on that same input. The current snapshot
  classifier treats nominal type declarations as owned values. A bounded
  scratch candidate may exclude CLASS/TYPE_PARAM declarations at the existing
  ResourceFlowUniverse admission owner and its three reached producers
  (snapshot, nested declaration and function seal). Keep all other symbol
  kinds, value classification, identity epochs and missing-fact failures.
  Required falsifiers: typed value/parameter flow remains, nested Slot rows
  survive scope teardown, use-after-release/branch consume remains rejected,
  and same-input full MIR validation/serialization finishes under the cap.
  This is not permission to change production before the checkpoint boundary.
- A further importing falsifier exercises the nested-declaration and
  function-seal producers with Future type aliases without any prior branch
  snapshot. Actual Future value bindings must survive both paths; aliases
  must not acquire flow identity. Run this with the same scratch owner, not
  an alternate type classifier.
- Extend that importing falsifier through same-context universe retirement
  and re-entry, retaining a value symbol's old memo epoch. Bind a different
  first value in the new generation: the retained value must acquire index 1,
  not silently reuse index 0; its snapshot and sealed parameter fact must
  agree. This is a bounded generation-admission check, not an epoch-wrap or
  concurrent-lifetime proof.
- After the full MIR diagnostic completes, the next bounded native consumer
  check is source-to-C emission of the SAME complete driver source. Existing
  `driver_app` region admission -> `c_runner_execute` -> `compiler_emit_c`
  owns this branch. Write only a separately named `.tmp` native-oracle C file,
  retain the 3072 MiB/1800 s limits and bind the actual executable/source.
  This does not compile/install a replacement driver: the DRV-2 build owner
  forbids using native source compilation as self-host evidence. Do not
  claim a same-source installed pair, fixed point, P0 or a substituted path
  from the oracle's output. Missing/stale MIR/region/artifact identity must
  retain its existing refusal; no backend fallback or source edit is allowed.
- Gate/falsifier: compile the diagnostic, check unchanged emitted MIR on a
  bounded existing fixture, then rerun the SAME full input under 3072 MiB.
  Changed output, absent counters, input/tool drift or a cap refusal stays
  explicit. A counter identifies an operation; it is not a production fix.

The native unit shard compiled and executed its unit batteries, but its
`test-mir` integration stopped when the isolated installation had no self-host
driver. Do not relabel that shard PASS or point it at the stale official pair
to hide the missing same-source installation.

## Reached memo-generation identity boundary (2026-10-09 continuation)

The existing universe bind fast path trusts a Symbol epoch/index memo without
checking the current declaration entry. A controlled importing fixture
enumerates ordinary re-entry, fresh-context reuse and SIZE_MAX epoch reuse:
the latter two issue the other value's index and lose a sealed row. This is
a local binding-API counterexample, not observed source-level reachability.

The bounded scratch repair validates the hint with the EXISTING entry matcher
before reuse; ordinary authoritative lookup/bind and the value-admission
decision remain. No second identity owner, cleared-symbol workaround,
annotation increase or downstream fact deletion. Current function-scoped
snapshot restoration remains a separate lifetime obligation; this guard is
not an arbitrary stale-frame, concurrency or generation-wide safety proof.

New separately named owner/binaries live only below
`.tmp/ownership-cutover-2026-10-09/generation-redteam-2653728bc3254a67aa6318d063601641/`.
The previous candidates and their receipts are preserved. Three importing
cases, existing v3 admission controls and six native unit batteries PASS.
Same full input: unchanged 3072 MiB, byte-identical MIR, 50397 ms, sampled
private 1.288 GiB. Existing native-only C/LLVM ArrayDrop gate also PASS.
The confirmed all-writer freeze, second checkpoint and actual P0 still precede production
migration; include both declaration admission and validated memo reuse in
that ONE owner/consumer plan. Receipt:
`../audits/resource_flow_generation_memo_redteam_2026-10-09.md`.

## Adopted implementation decisions (latest user instruction, 2026-10-09)

Use a direct recovery epilogue for production, not status-guarded source
lowering. The flag-based CL6 theorem remains the importing reference; the
direct-jump refinement and pointer/bundle erasure must be checked before the
production path is admitted. Do not initialize every local merely to satisfy
the reference lowering's conservative liveness.

Use CL7 whole-backing focus for compiler-local writable views. Resource/graph
leases keep their existing issuer; they are not a second compiler-view ledger.
Before P7, census actual Slice construction and backing mentions through the
last view use, including aliases and calls. Record unsupported cases rather
than silently copying a write-through view or adding user annotations.

These choices fix implementation direction, not P1 closure. The source-place,
ordered-expression, call-result ABI and physical-descriptor obligations in
doc 27 remain required, with one final-generation fact owner.

### Direct-control refinement slice (2026-10-09 continuation)

- Objective: turn an exiting `XStmt` into a finite, label-addressed control
  graph without the CL6 status/guard variables or entry initialization of
  source locals. Preserve branch, loop, handler, return and error traces.
- Priority: existing source execution and output identities; exact exit
  routing; no new heap or catch authority; then representation size.
- Owner: `OwnershipCleanDirectControl.v` owns only the source-control graph
  construction. `OwnershipCleanExits.v` and `OwnershipCleanCallLowering.v`
  still own cleanup elaboration and ordinary-core call recovery respectively.
- Chain: `XStmt` -> structural path labels and instruction lookup -> direct
  control trace -> existing reference lowering/cleanup propositions. This
  slice does not claim a MIR, C or LLVM graph consumes the model yet.
- Last consumer: the importing preflight audit. Forbidden: calling a forward
  simulation whole-compiler refinement, introducing a recovery catch rule,
  treating graph labels as source variables or activating automatic drops.
- Required changes: one importing model, exact theorem consumers and concrete
  early-return/error/local-loop witnesses in the existing focused gate;
  registration in the full formal gate, plus the documented remaining
  reverse/physical correspondence obligations.
- Gate: existing ownership preflight, then full formal integration. Falsifiers:
  executing a skipped arm, continuing after return, a local break escaping
  its loop, or routing a handled error past its handler. The actual field
  inout/readonly-temporary/last-view-use source probes remain red and are not
  weakened by this model slice. No additional implementation lane is opened.

Observed result: direct-control plus the importing consumer PASS in the
10-module focused kernel (zero assumptions). Full formal integration PASS:
78 modules plus approval binding consumer, only the two existing approved
Slot abstractions. Forward simulation is not reverse/physical/emitter
refinement; P1 and production automatic-cleanup activation remain OPEN.

## Reached static CI-profile obligation

The P0 static inventory reached `tests/self_host_ci_profile_smoke.sh`, which
still requires four full-only steps in `build-linux`. The unchanged workflow
has five: dependencies, download, admission, core execution, and the existing
native clock/scalar-call ABI checks. Its single Markdown-only step is unchanged.

- Objective: make the profile consumer check the existing workflow rather
  than reject its fifth full-only step; do not modify CI execution scope.
- Owner: `.github/workflows/ci.yml` and the existing profile gate; neither
  becomes a compiler or ownership fact owner.
- Last consumer: P0/profile validation and the shared CI contract shard.
- Forbidden: deleting that native ABI stage, accepting it on Markdown-only
  changes, loosening the one-scope-owner checks, or treating the structural
  gate as a remote job result.
- Change: require five full-only steps, the native stage name and both of
  its exact existing commands. Keep one Markdown-only step.
- Gate/falsifier: the same profile gate and shell syntax check; removing or
  unguarding either native check must not satisfy the profile contract.

Executing those two existing native checks exposed a Windows output-boundary
obligation: the generated native program writes CRLF, while the shell oracle
writes LF. The first clock output is exactly `true\r\ntrue\r\n`; both gates
fail at byte 5 before reaching LLVM. Following the existing backend/parity
tests, normalize only line-final CR in the test consumer, retain the raw
output, and compare every Boolean line exactly. Do not change runtime output,
strip other whitespace, suppress errors or remove the narrowing/ABI negatives.
The integration gate is both existing scripts on the same fresh executable;
an incorrect Boolean or missing line must still fail comparison. Both fresh
native C/LLVM gates now PASS, including LLVM Long ABI and scalar refusal
checks. Incorrect-Boolean and missing-line comparator controls both refuse.
This is focused native evidence, not public/self-host route or remote CI
completion. Full identities and limitations are in
`../audits/ownership_cutover_p1_integration_and_p0_obstructions_2026-10-09.md`.
