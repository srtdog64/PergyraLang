# Bootstrap artifact file-comparison gate review — 2026-09-05

Status: AUDIT COMPLETE; final focused gate independently PASS. The initial
top-level JSON assertion precision recommendation is resolved in the final
follow-up below.

Base: `b4d22cf2c6e68fcbd42a1ce4a44444de0e7899fa` plus the primary task's
uncommitted work. This report follows
[`bootstrap_artifact_comparison_review_2026-09-05.md`](../agent_work_directives/bootstrap_artifact_comparison_review_2026-09-05.md).
It is a bounded gate review, not compiler semantics, a new implementation
rung, a bootstrap PASS, or self-host/SoT progress evidence.

## Scope and independent observations

Only this report may be edited by this lane. Compiler sources, shared tests,
the handoff, other reports, installed binaries, and Git state are read-only.
No compiler build, full MIR production/comparison, bootstrap, full test matrix,
old-artifact deletion, or remote publication is authorized for this lane.

Read-only inspection covered the active collaboration objective,
`llvm_leg_helpers.sh`, the comparator's `main.pgy` and `intent.md`, and
`backend_output_comparator_parity.sh`. `git rev-parse HEAD` matched the base.
The existing comparator at
`.tmp/self_hosted/driver/enum_receiver_integration_20260905/backend_output_comparator_38116.exe`
was independently SHA-256 verified as
`8F11050ADBDB56C9E14552369617E7BFDFB50A47ED4FD89231E4E29B2DDB21FC`.
D: had 97,947,648 free bytes at inspection; only a small focused run is allowed.

## Contracts the gate must falsify

The existing shell normalizer owns removal of CR bytes, repository-prefix
normalization specifically in `source_module_path`, and removal of trailing
empty lines. Raw byte identity can avoid both normalized copies because these
identical inputs receive the same deterministic normalization. It cannot own
the final parity verdict.

The Pergyra comparator independently admits artifact kind and harness/
subprocess facts, reads the supplied files, and compares their lines. Its
success exit and emitted schema are executable evidence that shell transport
actually reached the owner. The existing parent gate also checks exact clean,
argv, mismatch, and missing-input JSON and covers C/LLVM comparator execution
when LLVM is available. This lane will not rebuild or rerun that full parent.

The following falsifiers were sent to primary before its implementation patch:

- Identical bytes plus an invalid artifact kind must reach the real Pergyra
  owner and fail with its owner error. A `cmp`-only success must not satisfy
  this case.
- Valid identical files must yield the real comparator JSON with original
  paths, while producing no normalized artifact copies.
- Nonidentical raw files differing only in admitted CR/provenance/trailing-
  empty-line normalization must compare successfully through normalized paths.
- Real line changes, interior blank lines, and differences in JSON path fields
  other than the owned provenance field must remain mismatches.
- `cmp` status 2 must be distinguished from status 1; it must not enter the
  normalization branch as an ordinary content difference.
- Expected-side and actual-side normalization failures should be independently
  injected and must stop before the comparator runs, including when the
  function is called from a shell conditional that suppresses `errexit`.
- Missing/unreadable inputs should fail explicitly. A small filename containing
  spaces is useful evidence that original and normalized path transport stays
  quoted.

## Execution record

Primary reported the test-first RED at `run.FEbn0y`: the old file-comparison
function materialized normalized copies for `identical-lf`. That RED was not
independently rerun by this reviewer. Primary retains the full-MIR zero-copy
integration check; no result for that larger operation is asserted here.

After primary announced the candidate, this reviewer independently ran
`bash -n` on the focused gate, `llvm_leg_helpers.sh`, and the parent parity
script individually, then ran:

```sh
timeout 300s bash tests/self_hosted/parity/text_artifact_file_comparison_owner.sh \
  /d/PergyraLang/.tmp/self_hosted/driver/enum_receiver_integration_20260905/backend_output_comparator_38116.exe
```

Execution used the existing MSYS2 Bash and runtime paths. Observed session
`78114` completed with exit 0 and the terminal message:
`PASS (real Pergyra verdict, no-copy identity, normalization and explicit refusals)`.
Artifacts are retained under
`.tmp/self_hosted/text_artifact_file_comparison/run.348vaY/`.
All 19 named cases printed PASS; none reported a skip.

| Boundary | Independently observed focused cases |
| --- | --- |
| Identical inputs use originals and no normalized copies | LF, CRLF, no final LF, empty input, trailing blank lines |
| Pergyra remains the verdict owner even for identical bytes | Invalid artifact kind returns exit 1 and `BACKEND COMPARATOR OWNER ERROR` |
| Input/byte-comparison errors stop before comparator | Missing expected, missing actual, injected `cmp` status 7 |
| Existing normalization still reaches owner and accepts equivalent contents | CRLF plus trailing blanks, absent final LF, `source_module_path` provenance |
| Semantic text differences remain refusals | Changed line, interior blank line, root-path difference in `other_path` |
| Normalization failures stop before comparator | Expected-side failure, actual-side failure, root-helper failure, path-conversion failure |

The gate creates a byte-equal copy of the supplied real comparator under its
own run directory; it does not build a comparator or substitute a shell verdict.
The bound `backend_output_comparator_45484.exe` is 217,088 bytes and was
independently hash-verified against the supplied executable after the run.
The complete run directory contained 238,620 file bytes at inspection. No old
artifacts were deleted. The normalized-route cases intentionally leave their
small normalized files behind; zero copies is the assertion for the preceding
identical-input cases, not a claim that the completed mixed-case directory has
no normalized files at all.

Executed source snapshot hashes:

| File under repository root | SHA-256 |
| --- | --- |
| `tests/self_hosted/parity/text_artifact_file_comparison_owner.sh` | `B04DF5A3D0B62828BBF69863799138900F5DE6612ABF73F0C588DA851987B456` |
| `tests/self_hosted/parity/llvm_leg_helpers.sh` | `C40E707CFB0CEF9497EDF4FB817F2DC277D4C1B81316A929EED596DE2E9B9447` |
| `tests/self_hosted/parity/backend_output_comparator_parity.sh` | `8AB92E87384BDB7BCB5289D903AB5D2FF5CF87A2B01A6F9B5DF6702EB1193E66` |

## Gate strength and source review

The gate reuses the real Pergyra executable and pins its copied bytes at lines
17–20. Cases exercise the actual sourced file-comparison function rather than
an extracted imitation. The normalizer/cmp overrides are scoped fault
injections, not replacements for the verdict owner.

`check_pair` deliberately invokes the subject from a conditional at lines
32–35. This suppresses implicit shell `errexit`, so the subject's explicit
normalization guards are necessary for the fault cases to pass. The injected
normalizers emit plausible equal output before returning failure; this makes
ignored failure capable of producing a misleading green comparator result.
The original `comparator not reached` sentinel must remain in the comparator
output on transport failures. Exact nonzero status and the required boundary
diagnostic are checked as well.

Positive and ordinary mismatch routes assert real comparator schema, verdict,
expected/actual transport paths, artifact kind, and `c_oracle`/`self_hosted`
projection identities. Input filenames contain spaces. Raw cases run before
any materialized case, making their no-copy assertion unambiguous. The invalid
kind case specifically prevents raw identity from returning shell-only green.

The helper delta preserves the existing normalization operations. `cmp` status
0 selects original transport, 1 selects guarded normalization, and other values
fail before comparator execution. Root/path conversion failures now propagate
explicitly through their existing owners. The final comparator invocation is
shared by the successful raw and normalized transport branches. The source
documents completed immutable inputs; this gate does not claim concurrent
writer correctness.

Parent integration was independently inspected at
`backend_output_comparator_parity.sh:217`: the focused gate receives the
parent's already-built `ARG_BIN`. No separate comparator build is introduced
there. The reviewer did not execute that complete parent because this lane
forbids builds and full matrices.

## Initial assertion precision recommendation and remaining limits

The first executed candidate's positive verdict assertion used a whole-output search for
`"ok":true`. The actual report also contains `subprocess_plan.ok:true`, so that
search is not, by itself, an assertion of the top-level verdict. The exact exit
status, real comparator binding, owner-refusal case, and semantic negative
cases provide independent protection for the current transport change; no
false PASS was observed. Nevertheless, matching the comparator's owned schema
and adjacent top-level `ok` emission together would tighten this assertion
without introducing a new JSON dependency. This recommendation was sent to
primary immediately after the independent PASS.

Actual unreadable-file permissions, a disk-full event, concurrent mutation,
and the large full-MIR pair were not reproduced by this lane. Missing inputs
and explicit nonzero fault injection demonstrate the affected control-flow
refusals, not every operating-system failure mechanism. The full comparator
C/LLVM parent, full bootstrap, installed-driver receipt, remote CI, and
publication remain outside this report's execution claims. No closure metric
is increased by this review or its case count.

## Final independent execution after assertion strengthening

Primary corrected `check_pair` to match the comparator emission's schema and
adjacent top-level `ok` field together, rather than any nested `ok` field. The
inspected prefix is
`{"schema":"pgy.selfhost.backend-output-comparator.v1","ok":...`.
This resolves the assertion-precision recommendation above using the existing
owned JSON emission shape, with no new parser dependency or verdict authority.

Primary also added one `emitted_c` identical-input transport case and two
path-conversion refusal cases. The latter use existing files spelled relative
to repository cwd: byte comparison can succeed, but conversion rejects that
spelling when the final raw transport requires absolute repository-root input
paths. Both expected-side and actual-side failures must leave the comparator
sentinel untouched. These are path-transport checks, not missing-file cases.
The `emitted_c` case checks known artifact-kind admission and raw transport;
it does not compile or prove the fixture text to be valid C.

After re-reading the complete final gate and verifying the supplied comparator
hash again, this reviewer independently ran shell syntax validation and the
same focused command with its 300-second outer budget. Observed session
`75625` completed with exit 0, all 22 named cases printed PASS, and the terminal
focused PASS was present. The new artifacts remain under
`.tmp/self_hosted/text_artifact_file_comparison/run.MbqSpI/`.
No build, full-MIR producer/comparison, deletion, or Git write was performed.

Final gate SHA-256:
`375C49877585EE22D89C143B3E95F85B4BECF1087BB0AF3743D2AC3F7D6FC736`.
The helper hash remains
`C40E707CFB0CEF9497EDF4FB817F2DC277D4C1B81316A929EED596DE2E9B9447`,
the parent hash remains
`8AB92E87384BDB7BCB5289D903AB5D2FF5CF87A2B01A6F9B5DF6702EB1193E66`,
and the supplied comparator still matches the directive's
`8F11050ADBDB56C9E14552369617E7BFDFB50A47ED4FD89231E4E29B2DDB21FC`.

### Separately attributed primary integration evidence

Primary reported that the preserved 285,190,841-byte MIR pair passed through
the actual file-comparison function in `full-mir.cee6AL`, with zero normalized
copies and identical original-file SHA values before and after the operation.
This reviewer did not inspect or execute that large-pair check; it is recorded
only as primary-reported integration evidence. It is not a full bootstrap
rerun or an independent bootstrap PASS from this lane.
