# macOS checker mutation portability

Status: REPAIR PUBLISHED / NATIVE MACOS AND REGULAR CI VERIFIED

Base: `5653165b7b372a84622ee5837e9375c0828fecd6`, main and origin/main.
Regular CI `33984709173` completed FAILURE, 29/30 passed; watch `71941`
completed exit 1. macOS job
`101356033822` has already failed. Its newly reached step 3 rejects the
single-line `sed` insertion used to manufacture the early-success mutation.
Step 4, the shared size-policy mechanics, passed on the actual macOS host.
Do not confuse that test-construction error with a successful owner refusal.

## Objective card

- Objective: construct the same forbidden early-success transport mutation on
  GNU and macOS tools so the existing ownership checker can reject it.
- Priority: byte-identical mutation, exact exit/diagnostic preservation, native
  portability, then bounded CI feedback. This is not compiler substitution.
- Fact owner: `check_artifact_comparison_transport_placement` in the existing
  component contract retains the ownership verdict. The mechanics smoke owns
  only its synthetic input construction.
- Last consumer: the actual component-checker mechanics gate, now reached by
  fast macOS step 3. No cap, timeout, full-matrix or compiler change is open.
- Forbidden fallback: removing the mutant, accepting arbitrary nonzero exit,
  skipping a platform, replacing its host tools, or permitting shell success
  without the Pergyra comparator.
- Integration gate: compare the old GNU mutation and portable construction
  byte-for-byte, then run the real component mechanics gate within 60 seconds,
  plus the existing profile/docs/syntax checks. Native success must come from
  an exact-revision macOS run; do not cancel the current integration run.

## Independent scopes

- Primary owns the single mutation line in
  `tests/self_hosted_component_checker_smoke.sh`, all evidence/navigation
  updates, local execution, Git and CI. No compiler implementation lease opens.
- `semantic_index_review` is read-only: review this mechanics smoke and the
  functions it extracts from `tests/self_hosted_component_contract_smoke.sh`
  for adjacent Bash 3.2 / BSD tool assumptions reached by the same execution.
  Validate whether the portable insertion preserves the intended mutant and
  the existing exact refusal. Return bounded findings in a message; no edits,
  builds, tool installation, remote writes, deletion or broader audit.
- Other agent leases stay closed. The integration owner is primary. Do not
  claim native success from local GNU execution or publish while watch `71941`
  still owns a live run.

## Observed remote evidence

Job `101356033822`, 2026-09-05 UTC:

- 18:39:26: step 3 failed with the insertion-command syntax error, exit 1.
- 18:39:28: `[owner-size-policy-checker]` PASS on the native host.
- 18:41:17: fast macOS reported 1/9 steps failed; no full green is claimed.

## Local result

Primary replaced the single-line sed insertion with a portable AWK operation.
Execution `82951` compared the old GNU mutation and new construction byte for
byte using the retained baseline, verified one inserted row and five total
rows, and passed the actual full component mechanics, CI profile, syntax and
documentation gates. The unchanged checker requires exact exit 1 and the
`retired term: return 0` diagnostic; interpreter failure is not accepted.

The read-only lane completed its review, found no additional adjacent defect,
and confirmed that both required transport rows remain in the mutant. It
performed no edits or execution. All agent leases are closed. Native recheck
is still pending. The preceding CI is now terminal and reconciled: Linux
passed all 23 push steps, full driver gen2 == gen3 (177,559 lines), installed
CLI/transaction gates and all three policy-corpus sources passed. No watcher
remains live from it; the reviewed mutation repair can now be published.

Publication/recheck: final local component/profile/docs/syntax checks passed
(`35121`), as did exact staged-path, UTF-8 and whitespace validation. The six
reviewed paths were committed/pushed as
`91119c45683c10efdccaad18c95fdc2a9ebf0d41`. New regular CI `33987047080`
targets that SHA and completed SUCCESS, 30/30; watch `87947` ended exit 0.
Native macOS job `101362486942`
passed component mechanics at 19:25:46 UTC, size-policy mechanics at 19:25:48,
and all nine push steps at 19:27:15. This closes the reached native mutation
failure, not the separate ability compiler bug or whole integration objective.

Original compiler outputs and IO policy were unchanged by this checker scope.
The later user-authorized bounded cleanup resolved storage separately; see the
active handoff. Do not revive this directive's original pending-storage state.
