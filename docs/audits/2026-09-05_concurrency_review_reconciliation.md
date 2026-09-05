# Concurrency review reconciliation — 2026-09-05

Status: documentation and focused-example verification COMPLETE; compiler/runtime findings remain OPEN.

## Objective and scope

- Objective: distinguish current execution, owner contracts, model theorems and
  proposals in the supplied architecture review; retain difficult real examples
  with exact output and rejection checks.
- Priority: accurate semantic identity/ownership, explicit limitations, preserved
  existing inputs, exact runtime evidence, then presentation.
- Fact owners: docs/113 and docs/178/181 plus their existing semantic/MIR/runtime
  owners. This report and the example guide are navigation, not semantic SoT.
- Last consumers: existing C/LLVM projections and the focused test's runtime
  output/diagnostic checks. No owner moves in this change.
- Forbidden fallback: invented executable syntax, model-as-implementation claims,
  native retry after self-host refusal, accepted-invalid execution, quiet skips,
  copied fixtures, or a second implementation track on the active enum rung.
- Verification: `tests/async_model_positioning_smoke.sh` and
  `tests/concurrency_examples_smoke.sh`; syntax, links and scoped diff checks.

The main task owns mixed-arity tagged-enum source changes, shared progress,
registry, integration wrapper and handoff files. Those are not edited here.
This side task owns only the concurrency guide/facade/direction clarifications,
the focused example gate, its existing-fixture stdout goldens and this report.

## Checkpoint

- Observed HEAD: `bf8b33d078b27c41cc6cdb7ffed2e8fa5c62ef22`.
- Exact [CI run 33922587191](https://github.com/srtdog64/PergyraLang/actions/runs/33922587191)
  is completed/success, 30 jobs, all successful. This excludes uncommitted work.
- Native binary before execution: SHA-256
  `0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`.
- Installed self-driver: SHA-256
  `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`.
- The worktree was already dirty in Makefile, native/parser/runtime paths,
  self-host enum paths, Intent/audit/progress/navigation documents and generated
  inventory. Protected user paths and unrelated changes were preserved.

## Review decisions

| Review claim | Reconciled finding |
|---|---|
| CI green | Verified for the exact published SHA, not current dirty files. |
| Concept deletion proves irreducibility | Bounded evidence against tested encodings, not all possible encodings. |
| One-step Intent may be merely decorative | Action count is neither necessary nor sufficient. Exact outcome-to-terminal attribution is already a one-step falsifier. General INT-1~4 and self-source plan production remain separate. |
| ConcurrencyPlan is an existing all-ID struct | The integrated shape is a direction. Current `SelfMirParallelCaptureRows` is real and still contains names/kinds as strings; no whole-program all-ID plan completion is asserted. |
| 49 proofs | The corpus has 49 `.v` modules, not necessarily 49 theorems. Kernel/adequacy evidence does not discharge implementation obligations. |
| Slot stale theorem | Its suspension model uses unbounded `nat`; `uint32_t` wrap-around and implementation refinement remain outside it. |
| Safe parallel implies arbitrary serial lowering | Only the admitted independent computation subset. Communicating arms still require progress-preserving scheduling. |
| any replaces unordered | First-give selection is not unordered collection, first-success, or quorum. |
| ResourceCarriage must be a new calculus | Common carriage reasoning is justified; a new general architecture is a proposal, not a consequence established by two defects. |
| Runtime/IR performance priority | Historical live-set figures are not remeasurements of current dirty code. No new memory/performance claim is made here. |

## Explicitly not repaired by this task

The prior audit's affine-Future aggregate hole, Zone spawn ABI mismatch and
external MIR ingestion findings remain open findings, not tests to execute as
invalid binaries. No new reproduction or compiler/runtime repair is performed.
General capability loans, resume revalidation, exceptional cleanup, first-success,
quorum and deterministic tick commit remain the separately marked worklist in
[advanced examples](../concurrency/advanced_examples.md). Module Build remains
post-self-host. This side task does not set a successor implementation priority.

## Observed verification

- Original `bash -n` invocation supplied both script paths, which checks only
  the first script; it was not evidence for both. The 2026-09-05 integration
  review ran `bash -n tests/concurrency_examples_smoke.sh` and separately
  `bash -n tests/async_model_positioning_smoke.sh`: both exit 0.
- `timeout 60 bash tests/async_model_positioning_smoke.sh`: PASS, including all
  nine lifecycle rows and explicit current/model-only example boundaries.
- `PGY_BIN=/d/PergyraLang/bin/pgy.exe timeout 300 bash tests/concurrency_examples_smoke.sh`:
  PASS, exit 0 within the five-minute budget. Eight unique canonical programs,
  sixteen native C/LLVM executions, exact stdout after line-ending normalization,
  exit 0 and empty runtime stderr for every execution. Five existing negative
  sources produced ten owned semantic rejections with no output artifact; none
  was executed.
- `timeout 60 bash tests/async_direction_adequacy_smoke.sh`: PASS. This checks
  model/source vocabulary bindings; it is not a new Rocq kernel execution.
- Four touched Markdown documents: strict UTF-8 and 45 local links PASS.
- All fourteen scoped files: strict UTF-8, final newline and no trailing
  whitespace PASS. Scoped tracked `git diff --check` PASS.

Retained evidence: `.tmp/concurrency_examples.U3vARj/receipt.txt`, plus per-case
compile and runtime output. The receipt includes HEAD, the native compiler hash,
canonical source/golden hashes, each runtime/negative outcome and final exit 0.
Native binary and installed self-driver hashes were unchanged at the end.

One existing C compiler warning was observed for `parallel_join_any_blocked`:
the deliberately parked receive's local `parked` is unused. It is retained in
`parallel_join_any_blocked_c.compile.err`; no warning-free build claim is made.
The canonical Pergyra input was not changed to conceal the warning.

The eight `expected.stdout` files are consumed automatically by the already
registered backend-compare cases. The new focused runner is manual; no extra CI
job, Makefile edit, source rebuild, installed-driver replacement, full matrix,
full Rocq kernel run, performance measurement or self-host parity run occurred.
All fourteen scoped changes remain uncommitted. No commit or push was requested
for this side-conversation mutation, and unrelated dirty work remains preserved.
