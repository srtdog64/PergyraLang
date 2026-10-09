# Ownership-cutover gate red-team review and repair

Date: 2026-10-09 KST. Base: `main @ a75da80435e0d051f71cfacf76d44ea825ed87d7`.
Scope: the active value-fact admission candidate, importing proof boundary
and the verification/pressure receipt chain. Not a repeat audit of the entire
63/70-module historical proof corpus, production automatic cleanup or CI.
Directive: `../agent_work_directives/ownership_gate_redteam_2026-10-09.md`.

## Confirmed findings and repairs

| Finding | Before repair | Repair and falsifier |
|---|---|---|
| High: observation was also termination authority | Default name/time scanning puts an unrelated new compiler worker in the same rows subsequently passed to PID-based termination. A controlled synthetic process table confirmed inclusion; no real shared worker was terminated. Root creation also used a five-second tolerance. | Descendant rows and unowned detached candidates are distinguished. Only validated owned rows contribute memory or termination authority. Creation identity is checked at selection, held-handle sampling and retirement, using CIM's microsecond precision rather than a reuse-admitting time window. Unproven attribution/changed sample identity invalidates success with 90. Synthetic ownership, older-child/reused-parent, nearby-root-creation and actual receipt refusal controls PASS. |
| Medium: incomplete capture could be green | A generated copy with a controlled IOException produced command exit 0, wrapper exit 0 and `output_capture_complete=false`. | Capture failure is explicit. A live failed pump aborts this run's owned command instead of leaving an unread pipe blocked indefinitely; invalid success becomes 90. An already completed command's exit 7 remains 7, with the separate capture failure recorded. Both fault controls PASS. |
| Medium: capture retained whole compiler artifacts twice | A 1,048,576-byte no-newline fixture leaves 2,097,152 UTF-16 characters in the output/stage builders before final string copies. This is a measured retained-payload count, not an OS/heap peak. | Raw bytes stream directly to separately owned log files; no whole-output text builders. Only known stage prefixes are buffered, bounded to 4096 characters. Oversize stage evidence fails explicitly. The 4 MiB/no-newline/invalid-UTF-8/NUL/CRLF/trailing-stage fixture preserves an independent expected byte hash; stage and stderr metadata survive. |
| Medium: repeated labels destroyed prior evidence | An isolated occupied stdout log was removed/replaced and the old wrapper still exited 0. | All five receipt files are checked and reserved with `FileMode.CreateNew`, including the summary. Explicit occupied stdout/summary refuse before child launch and preserve their bytes. Omitted OutDir selects a fresh default run directory; two real same-label invocations preserve both complete receipts. All four controls PASS. |

The final caller inventory includes six Make pressure routes with stable
labels and omitted OutDir. Rejecting collisions in their shared default
directory would break normal repeated builds. The owner now chooses one
fresh `run-<id>` directory by default and reports its actual summary path;
an explicitly supplied OutDir remains exact and collision-refusing. No
consumer reading a fixed default summary filename was found. The repeated
invocation control hashes all five first-run files again after the second
invocation, rather than trusting different directory names alone.

Additional scope corrections: `input_change_scope=before-after-only`
explicitly avoids promising continuous immutability from endpoint hashes.
`memory_peak_scope=observed-samples-only` and `memory_measurement_state`
distinguish sampled memory from an unobserved fast command. These are not
heap allocation counters or an OS-enforced memory quota. Non-positive
interval/drain and invalid budgets refuse before command launch.

These fixes do not infer ownership of reparented MSYS workers. Default-mode
detached names are candidates, not owned workers. If any cannot be attributed,
the receipt is invalid rather than counting their memory or killing them.
`detached_worker_attribution_complete` is scoped to observed candidates; in
root-only mode no detached attribution is requested. Whole-build/job coverage
remains OPEN and must not be inferred from that flag or a native-only run.

## Reviewed value/proof boundary

The candidate's CLASS/TYPE_PARAM filter remains with the existing universe
owner and its snapshot/declaration/seal producers, before cached admission.
Source declarations in `type_checker_class_decl.c`, `type_checker_program.c`
and generic binders confirm those kinds are type names, not value storage.
Actual generic/Future/value parameters retain value kinds. The previous
typed/generic stable-ID, nested Slot, missing-universe, type-as-release,
generation-reentry and IR identity/ArrayDrop falsifiers remain supporting
evidence; no new source/value omission was established in this review.

The importing models are still conditional on admitted scopes/tables and
successful source traces. The whole-backing writable-view rule excludes
calls, escape and simultaneous views; it is not a production Slice pointer
or subrange refinement. The multi-inout adapter restores the modeled bundle
through ordinary-table calls; it does not establish source place evaluation,
physical pointer erasure or production ABI issuance. P1's OPEN obligations
remain explicit. No Claude-owned proof definition or normative contract was
edited to conceal them, and no automatic-drop path was enabled.

## Observed final verification

Existing Windows contract gate: PASS, with **21 executable receipt cases**
and synthetic identity controls. The nine existing cases are retained; new
controls cover both capture outcomes, occupied stdout/summary, long binary
output, oversized stage, invalid interval/drain, unowned candidate and changed
sample identity and both repeated default-output invocations. Last log:
`.tmp/ownership-redteam-2026-10-09/run-6e537320b0754cdb9c40e77465afdad4/contract-final-v3.log`.
Executable artifacts:
`.tmp/build-pressure-receipt-selftest/run-f9b70b2ae5194362bd265162dc942a0f/`.
This does not include real shared-process termination or an ASan/runtime audit.

Fresh focused Rocq consumer PASS: nine kernel-checked modules, zero declared
abstractions/admits/unsafe features, using the explicit project OPAMROOT.
Documentation quality, ownership-direction self-test, PS/Bash syntax and
native Git diff checks PASS. Final snapshot: 30 short-status paths, staged 0;
all 29 text inputs strict UTF-8 (`gmon.out` excluded). Full formal/native/P0
matrices were not rerun or relabeled green.

The final owner also runs the SAME complete self-host source-to-MIR input,
with the SAME candidate compiler SHA-256
`0daa8b6b9818b9d9d137ca56d53a33127fef811be92b156b0cf673e6c3e60531`:

- argv: `--native-pipeline --mir-json src/self_hosted/compiler/driver_bootstrap_main.pgy`;
- receipt `full-input-v3/full-driver-streaming-redteam-v3.summary.json` under
  the scratch run above: exit **0**, **49248 ms**, peak sampled private
  **1319.6 MiB (1.289 GiB)**, unchanged **3072 MiB** threshold;
- 55 memory samples; capture complete, empty capture failure, no creation
  mismatch; all 2538 declared endpoint bindings unchanged (2531 self-host
  sources plus four candidate inputs, command, probe and extra executable);
- stdout **378,143,208 bytes**, SHA-256
  `52290d05634c1a95e81d10c3804f7745cd6d05f3fb6b18edbe7252a92985b402`,
  byte-identical to the earlier complete MIR artifact;
- maximum pending stage buffer **1 character**, not a copy of the giant JSON;
  0 semantic errors and the same 21 existing warnings.

The earlier v1 owner run (49950 ms, 1319.8 MiB, before the held-process
sampling check) and v2 run (50974 ms, 1319.1 MiB, before the default-path
repair) are retained separately. No speedup is claimed from these runs:
the observer changed, these are single dirty-tree native runs,
and no observer/heap peak or complete self-host matrix was measured.

Final SHA-256 identities:

- measurement owner `4f3571f145ebf4589177b0cc3330796510f475549c078d8da5158fe327c86612`;
- static consumer `f759365125cfe3a47ab7b842afd672cc778ab9de8f9a20b4780500ef80d52d6c`;
- executable consumer `7ac83beea6923e6f2cb3e925033309a5a55819c9aafca4cdd40c1400ec996ef4`.

Old owner/probes remain in the scratch run (`measure_build_pressure_before.ps1`,
`pressure_precheck.ps1`, `before-fault`, `before-occupied`). Before-owner hash
is `bd6838cead384990181d9426c79f3c7df2a6f544d5b0ea58dc1c6aef89ed7961`.
No destructive shared-process probe, broad process-name kill or evidence
cleanup was used.

## Limits and next boundary

No production `src`/`.github` diff, official installation, registry status
change, checkpoint commit/push or remote CI operation. Official compiler/
driver hashes remain `f6559da9...1c9` / `707dcd40...78ec7`; `gmon.out` is
preserved. Native Windows Git diff check passes. The auxiliary WSL Git check
finished red because that host treats existing CRLF fixture bytes as changed
lines/trailing whitespace; do not normalize the shared repository to hide it.

This repairs evidence/control boundaries, not automatic ownership cleanup.
Fresh all-writer freeze, second checkpoint and full P0 remain prerequisites
for production admission migration. General heap-live/cumulative counters,
reparented-worker ownership, P1 source/physical refinements, P2-P7, installed
pair/DRV-2 and exact-SHA remote CI remain OPEN. Do not mark a SoT row CLOSED
or notify the GUI worker from these results.
