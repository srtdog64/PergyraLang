# Test harness red-team review reconciliation

Updated 2026-09-15. REVIEW INTAKE only: no semantic authority, progress counter
or implementation queue. Observed HEAD and `origin/main`:
`5e259ae47e16130b061336309ea1e99ee5d19587`. The working tree carries another
lane's uncommitted Zone/spawn packet and six unmeasured `Slice<T>` edits (see
the last section); nothing here depends on either.

Input: the user's 2026-09-15 red-team review of the test harness (pasted in
session, not preserved as an attachment). It grades test volume and adversarial
depth high and harness reliability -- oracle independence, artifact freshness,
reachability, pin semantics -- low, and argues the second matters more now.

Method: repository reads (source, gates, commit messages, audits, handoff) and
one Python scan of script references. No gate was executed for this intake;
shell execution became unavailable partway through, so every "live" finding
below is established by reading the exact lines, not by a red run.

## Verdict on each claim

| Review claim | Verdict | Evidence |
| --- | --- | --- |
| `mir_json_parity.sh` read a stale reconstruction left by an earlier run | Confirmed | `docs/audits/2026-09-13_tri_compare_type_family_frontier.md` §"A second gate was answering with a month-old artifact"; repaired in `40fa0d34` |
| ...and the reorder exposed a real **lowering** defect | **Corrected** | The surfaced `MIR intent evaluation phase disagrees with its target kind` was a fixture defect. `intent_nested_direct.pgy` delegated to an intent with `on:`; it now spells `intent:` (line 57, `a9b842e6`). `intent_completion_projection.py:184` pins `arg0 == "intent"` for that shape, so producers and consumer were right. The frontier audit's diagnosis section said otherwise and is corrected in place. |
| The semantic parity oracle graded itself | Confirmed | `8df77c92` (2026-08-08): the C oracle leg delegated to the self-host driver and reported `let_type_mismatch` instead of `PGY_SEM_TYPE_MISMATCH`. The class is still live elsewhere; see F1. |
| A test file existed but had never run | Confirmed | `5a2d39f2`: `direct_mir_inferred_option_local_owner.sh` had no target or reference since `c363a94a` and exited 1 silently under `set -e`. The class is still live; see F3. |
| "Both compile" counted as agreement; execution showed 65 / 20 / 16 / 3 over 104 programs | Confirmed, with scope | `docs/audits/2026-09-06_language_word_deletion_execution_matrix.md` §"September 10 executing census", observed at `70b704b7`. This is a historical manual observation, not a current-head result, and `collect_runtime_divergence.py` is not reached by any gate. The later 78 / 19 / 7 figure is a different metric (source-to-MIR admission). |
| The collector records binary and source SHA-256 and disclaims equivalence | Confirmed | `collect_runtime_divergence.py:11`, `:136-177`; it also checks the binaries did not change during the observation. |
| The InstructionId owner permits sparse IDs and checks uniqueness by sorting | Confirmed | `src/self_hosted/mir_lower/program_instruction_identity_owner.pgy:37-74`; IDs reset per routine at line 43. |
| The mutation generator produces four malformed inputs | **Corrected** | `direct_mir_document_admission_mutations.py` writes three malformed documents (leading comma, extra root close, cross-block duplicate ID) and one valid sparse-ID control. |
| Latest external-MIR push 30/30 green; `gen2 == gen3` at 190,082 lines | Confirmed | `docs/current_work_handoff.md:83-86` (`09d514cb`, run `34935370223`). |
| The handoff itself says Platform full, Self-host parity and the language-word matrix were not rerun for that packet | Confirmed | `docs/current_work_handoff.md:144-147`. |
| Orchestration is fail-fast, so one red hides later gates for weeks | Confirmed | `.github/workflows/self_host_parity.yml:94-136` is one `make` invocation over ~40 targets without `-k`; `mir_json_parity.sh` exits on the first fixture. Commit history records the effect repeatedly (`d9331de3` "weeks-shadowed", `2efea28f`, `7d4c4cde`, `edfadb96`). The exact phrase "shadowed behind earlier red" does not occur; the class does. |
| Golden and pin churn invites "update the expectation" | Confirmed | This window alone: `58ed4765` (field-adjacency pin), `cb7c8149` (`owner_set_sha256`), `89d00fea` (census 795 to 801), `13ecb4c6` (registry count), the `RESULT_USE_MIN` ratchet. `docs/188_redteam_golden_sot_review.md` R2 recorded exact-string pin fragility on 2026-07-17; that finding was not generalized. |
| No compiler mutation testing exists | Confirmed | Existing mutants are **input** mutants (`loop_statement_execution.py`, the document admission generator). No harness mutates compiler source and asks whether the suite notices, beyond one comment-out example in docs/188. |
| A 4-way native/self-host x C/LLVM differential is needed | Partly exists | The binary evaluation-order observation runs all four legs (execution matrix, lines 50-57); nothing does so generally. |

### The review's proposed InstructionId counterexamples, checked against the owner

Each case below is decided by reading `MirProgramInstructionIdentityMark` and
the runtime `ToInt`; none has been executed.

| Input | What the code does | Pinned by a gate |
| --- | --- | --- |
| `id` absent | empty number fact, `n <= 0`, reject (line 18) | no |
| `-1`, `1.0` | non-digit byte, reject (lines 22-27) | no |
| `01` | leading zero, reject (line 18) | no |
| `"1"` (string) | depends on `MirObjectNumberFactAtBounds` returning empty for a quoted value; not verified | no |
| decimal far beyond `Int` | `ToInt` is `(int32_t)strtol(s, NULL, 10)` (`src/runtime/pgy_runtime_scalar_std_inline.h:95-97`): clamps, no crash path; the round-trip `ToString(value) != id` at line 30 rejects on both LLP64 and LP64 | no |
| duplicate in the same block | sorted adjacent compare, reject | no |
| duplicate across blocks | reject | **yes** |
| same ID in two routines | accept (per-routine reset) | no |
| sparse IDs | accept | **yes** (valid control) |
| very large documents | no input budget found in `mir_lower/json_fact_read.pgy`; cost is O(n log n) per routine in this owner | no |

So the implementation already closes most of the proposed cases; the gap is
that the gate pins two of them. That matters for the review's larger point:
the first external-MIR packet (`c268b695`) shipped an owner that demanded dense
IDs and a gate with no valid sparse control, and only CI on the native oracle
exposed it (`docs/current_work_handoff.md:116-119`). A gate that pins only
refusals admits an overstrict implementation.

## Live findings from this intake

### F1. Three oracle legs still delegate to the self-host driver

`src/pgy_driver.c:252-261` routes every compile to the installed self-host
driver unless `--native-pipeline` or `PGY_NATIVE_PIPELINE` is set. These legs
set neither, anywhere in their scripts:

- `tests/self_hosted/parity/mir_json_parity.sh:1019-1021` -- the "C oracle" is
  `"$PGY" <fixture> --backend=c`. Its pass line (`:1033`) reads
  `frozen native MIR | mir_lower | codegen == C oracle`.
- `tests/self_hosted/parity/mir_json_coverage_probe.sh:111` -- same shape.
- `tests/self_hosted/parity/one_mir_cfg_air_plan_projection.sh:225-231` -- its
  failure text says "native oracle compile failed" and "native oracle did not
  produce".

This is not literal self-grading: the candidate is `mir_lower` plus `codegen`,
the oracle is the installed driver. But the oracle is a second self-host route,
not the native reference the gates name, so a defect shared by both routes
passes. `docs/152_validation_isolation_policy.md:385-434` requires an oracle
leg to name its compiler. The consequence for an earlier claim is recorded
below (C2). Not repaired here: changing oracle identity changes these gates'
results, as `8df77c92` did, and has to land with a gate run.

### F2. The gate-subject meta-gate checks one direction only

`tests/gate_subject_declaration_smoke.sh` requires every script that declares
`PGY_NATIVE_PIPELINE` to state its subject, and forbids self-host-subject
scripts from declaring it. Nothing rejects the reverse -- an oracle leg that
omits the declaration and silently delegates -- which is how F1 survives.

### F3. Tests that no runner reaches

A reference scan over the 802 `*.sh` files directly under `tests/`,
`tests/self_hosted/`, `tests/self_hosted/parity/` and `tests/concept_semantics/`
found 60 whose name appears in no Makefile, workflow, script or test other than
possibly itself. The scan is a heuristic: a script sourced by a relative path it
does not spell out would be a false positive, so the number is a ceiling. Five
were checked by hand and are unreached:

- `tests/arena_ledger_smoke.sh` (the Makefile builds `arena_ledger_smoke.c`, not
  this script);
- `tests/self_hosted/parity/world_zone_admission_owner.sh`,
  `hashmap_runtime_owner.sh` and `one_mir_string_trim_projection.sh`, which are
  cited only from documentation, including
  `docs/semantics/sot_owner_spine_registry.md` -- registry evidence that nothing
  executes;
- the 14 `tests/concept_semantics/*.sh` probes, run by hand into `.tmp/` logs.

Manual probes are legitimate when declared as manual. What is missing is the
declaration and a gate that holds every executable test to "reached by a target,
or listed as manual".

### F4. `mir_json_parity.sh` keeps its work directory across runs

`$B` is `.tmp/self_hosted/mir_lower/parity` (line 96), created with `mkdir -p`
(line 106) and never cleared. `40fa0d34` fixed the one block that read a
reconstruction before writing it; the directory still supplies the previous
run's files to any read that precedes its write. Compare
`one_mir_scalar_cfg_graph_projection.sh`, which clears its work directory first.

### F5. A path is not a binary identity

The execution matrix records that `.tmp/ci-34251704201-native/pgy.exe` held two
different binaries during the census (lines 121-127); the three divergences
were re-checked against the census hash. Reports that name a path without a
hash cannot be tied to what ran.

### F6. A gate that ordinary push CI runs only for Markdown-only pushes

`docs/192_protocol_abi_api_registry.md` declared `runtime_call_abi_rows.txt#count=262`
from `419b1745` (2026-09-09) while the artifact carried 267, so
`protocol_registry_smoke.sh` failed at every HEAD until `13ecb4c6` repaired it
on 2026-09-13. In `.github/workflows/ci.yml` the gate is a step of the
Markdown-only branch (lines 65-74); the non-Markdown branch runs
`make ci-push-linux`, and `scripts/ci_push_linux_steps.sh` does not reach it
(checked in the working copy). A code push therefore reports green with this
gate red. It surfaced once, on the Markdown-only commit `23728a58`; the next
code push, `bde22943`, was green without running it.

### F7. A known heap-corruption exit outside the gate graph

The routine-index fixture's LLVM build exits `0xC0000374`, on this packet and on
an untouched `bf329e99` worktree (`docs/current_work_handoff.md:139-143`). It is
recorded as pre-existing and correctly not called green, but no gate carries it,
so it cannot turn red or green on its own.

## Corrections to earlier statements

- **C1.** The 2026-09-13 frontier audit said the nested-intent refusal came from
  a consumer re-deriving a fact and offered two repairs to the document format.
  The fixture was malformed; the check was correct. The audit now says so.
- **C2.** `mir_json_parity` was reported on 2026-09-13 as green over 118
  fixtures against a C oracle. The run happened, but by F1 its oracle leg was
  the installed self-host driver. It is evidence that reconstruction agrees
  with the installed driver, not with the native pipeline.
- **C3.** The six uncommitted `Slice<T>` edits (five semantic owners and
  `tests/self_host_pergyra_likeness_smoke.sh`) were described in session as
  measured with no effect. No measurement ran: there is no output under
  `.tmp/slice_probe/` for it. They remain unmeasured and are not evidence for
  anything. The decision criterion is whether
  `semantic call target rows are incomplete after body fixpoint` disappears for
  `tests/cases/backend_compare/slice_copy/main.pgy` under the driver built from
  them (`.tmp/slice_probe/pgy-self-driver-slice.exe`).

## Priority register

Nothing below has started. Each item names the evidence it would retire.

| Priority | Change | Retires |
| --- | --- | --- |
| P0 | Declare the native pipeline on the three oracle legs, then extend `gate_subject_declaration_smoke.sh` to reject an oracle leg that omits it | F1, F2, C2 |
| P0 | A reachability gate: every executable under `tests/` is reached by a target or workflow on each push branch that can break it, or listed as manual with its reason; start from the 60 candidates and `protocol_registry_smoke.sh` | F3, F6 |
| P0 | Fresh per-run work directories for parity gates that read what they write, starting with `mir_json_parity.sh` | F4 |
| P0 | Record binary and source SHA-256 in every parity report, as the runtime collector already does | F5 |
| P1 | Pin the accepted cases, not only refusals: same-routine and cross-routine IDs, `01`, `-1`, `1.0`, a quoted ID, an out-of-range decimal, and an explicit input budget | the InstructionId table |
| P1 | Compiler-source mutants for the critical semantic checks (duplicate ID, target-kind validation, no-artifact guard, oracle opt-out), with 100% kill as the bar | mutation-testing claim |
| P1 | Run gate orchestration to completion and report pass/fail/skip/unreached counts, keeping fail-fast inside a single gate only | fail-fast claim |
| P1 | Separate semantic gates, representation gates and governance ratchets so a ratchet change cannot be read as a semantic pass or failure | pin churn claim |
| P2 | Give the `0xC0000374` fixture a gate that is red until it is fixed | F7 |
