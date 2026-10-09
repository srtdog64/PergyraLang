# Ownership-cutover gate red-team repair

Status: `SCOPED REPAIR VERIFIED / PRODUCTION CUTOVER STILL OPEN`.
Base: `main @ a75da80435e0d051f71cfacf76d44ea825ed87d7`, with the preserved
28-path dirty snapshot. The user's request is "레드팀으로 리뷰해보고 수정할거해".
This directive does not reopen P2-P7 or override the frozen-P0 boundary.

## Objective and chain

- Objective: prevent the active cutover's admission/verification receipts
  from accepting missing evidence, destroying earlier evidence or exercising
  process authority that the measured run does not own.
- Priority: ownership and explicit failure, retained source/output identity,
  negative evidence, unchanged 3072 MiB budget, then patch size.
- Fact owners: ResourceFlowUniverse's existing scratch admission candidate;
  the importing preflight consumer (not Claude's model definitions); and
  `scripts/measure_build_pressure.ps1` for process observation/output receipts.
- Last consumers: HIR/DIR/RIR/MIR and native artifact emission for value facts;
  `tests/build_pressure_contract_smoke.sh`, its Windows executable self-test,
  and P0's reporting consumer for measurements. These are not automatic-drop
  producers, semantic registry closure or self-host substitution.
- Forbidden: manual annotation/copy workarounds, discarded value facts,
  accepted incomplete capture, process-name based shared-worker termination,
  overwritten old receipts, heap counters inferred from OS private memory,
  production compiler changes before the fixed baseline, or false CI claims.

## Fixed repair boundary

Trace source CLI -> pressure owner -> observed/owned process selection ->
output/stage drain -> file bindings -> verdict/receipt -> existing consumers.
Inspect the scratch declaration filter's producers and reached identity
consumers without a parallel implementation lane. Confirm discovered gaps
with controlled fixture streams and synthetic process-table rows; do not
run shared-process termination probes or third-party exploitation.

Required pressure-owner changes, if falsified: separate inferred observation
from termination authority; keep missing/unattributable observation explicit;
check the selected CIM creation identity again against a held process object
before BOTH memory observation and termination, not only at tree selection;
make output/capture failure invalidate success; stream output rather than
retain whole compiler artifacts; preserve occupied receipt paths. Keep the
existing CLI, budget and numeric command result inspectable. No Job-object,
general cache, profiler or automatic-memory-management implementation track.

Final caller inventory reaches six Make pressure routes with stable labels
and no explicit OutDir. Retaining immutable receipts must not break a normal
second invocation: the owner selects a fresh per-run child of its default
output directory, while an explicitly supplied OutDir remains exact and
collision-refusing. No consumer reads a fixed default summary filename;
the CLI reports the selected receipt path. Verify two real invocations with
the SAME omitted-OutDir label, both complete receipts retained byte-for-byte.

## Edit scopes and integration

- GPT owns the measurement script and its existing Windows/static consumers,
  scratch falsifiers, this directive, dated audit and active handoff update.
- Claude's proof definitions and normative semantics remain read-only.
- Production compiler/runtime/backend source, installed binaries, registry
  status, commits/push and full P0 remain at their existing separate boundary.
- Integration gate: existing `build_pressure_contract_smoke.sh` with the
  Windows executable receipt self-test; static/syntax/document gates at 60 s,
  focused Windows gate at 300 s. Focused importing Rocq gate at 300 s only
  if its permanent consumer changes. No new full-corpus gate execution lane.
- Falsifiers: unrelated synthetic worker; lost/faulted output drain; occupied
  output path; long no-newline output; stale symbol memo/new universe; aliases
  versus actual Future/generic values. Distinguish source findings, executed
  fixture results, implementation repairs and still-open production duties.

Preserve old scratch probes/receipts and `gmon.out`. Findings belong in
`docs/audits/`; this file coordinates review, not compiler semantics.

Result: four confirmed gate-boundary defects repaired. Existing Windows
contract PASS with 21 executable cases and synthetic identity controls;
same-input native MIR byte hash preserved under the unchanged budget.
See `../audits/ownership_gate_redteam_2026-10-09.md` for receipts and limits.
