# Reached platform portability and ability diagnostics

Status: LOCAL CHECKER REPAIR VERIFIED — COMPILER INTEGRATION OPEN

Base: `da818c3df133c23572486872e4114d594f981890`, published. Regular CI
`33979208920` completed SUCCESS, 30/30, including full driver gen2 == gen3
(177,559 lines). Platform full `33979255563` reached macOS checker and matching
Linux/Windows ability-diagnostic failures and completed FAILURE, 10/13 passed.
Both Linux and Windows core passed. Watches `63073` / `36279` completed exit
0 / 1; no run or watcher remains live from this pair.

## Objective card

- Objective: repair only the next failures surfaced by the integrated enum
  rung. Its prior AWK regexp and collection/enum SSA fixes now pass their
  reached boundaries; they must not be reverted or replaced with a skip.
- Priority: exact failure identity and one fact owner, portable checker
  behavior, negative controls, focused execution, then exact-revision CI.
- Fact owners: existing component scan/directory inventory and shared size
  policy own checker decisions. The semantic member-call diagnostic owns the
  dynamic ability argument-type refusal; the shell expectation consumes it.
- Last consumers: macOS preparation/size parents and the driver ability-bind
  parity child. No compiler source or successor implementation rung is open.
- Forbidden fallback: accepting any nonzero exit, suppressing unreadable input,
  skipping a dialect/backend, weakening caps, deleting empty-array coverage,
  generic diagnostic alternation, or converting a valid-input refusal into a
  passing negative test.
- Integration gate: each checker mechanics gate <= 60 seconds and the exact
  ability fixture through the existing driver parent <= 300 seconds. Reconcile
  all remaining remote results before publishing the combined repair.

## Independent scopes

- `semantic_index_review` owns only
  `tests/self_hosted_component_contract_smoke.sh`,
  `tests/self_hosted_component_checker_smoke.sh`,
  `tests/self_hosted_owner_size_policy.awk`, and
  `tests/self_hosted_owner_size_policy_smoke.sh`.
  Reached macOS failures: `checked_dirs[@]: unbound variable` from the extracted
  regex scope checker under Bash 3.2; `unreadable-source returned exit 2` when
  BWK awk reads a directory, instead of the expected owned read-error exit 1.
  Preserve explicit distinction between policy rejection and interpreter/read
  failure. Prefer a small portable correction; do not invent unobserved native
  macOS success. Inspect adjacent empty arrays in the same reached functions.
- Primary owns the ability-bind parity child and focused execution, all
  compiler source, documentation, Git, and CI. The actual failed diagnostic is
  `member_call_arg_type_mismatch`, func `storage.buffer.Put`, expected Int,
  actual String, from Linux job `101342708201` and Windows job `101343906110`.
- `ci_assertion_review`, if its existing model session is available, has a
  read-only scope: independently check the retained ability MIR against the
  declaration producer and expression identity-epoch owner. Report whether the
  audit's missing-ID explanation is supported and whether its proposed join
  hides another owner conflict. No files, compiler execution, or wider search
  are delegated to this lane; return bounded findings in a message.

## Observed continuation, not completion

Both delegated lanes are complete and hold no edit leases. The four-file
checker repair preserves all eight caps. Empty-array and whitespace-path
controls passed; the AWK boundary safely quotes whole paths and distinguishes
normal refusal 1 from execution error 2. Directory source/manifest and injected
shell exit 7 at each source/manifest × lookup/scan boundary are checked.
Primary independently passed component mechanics (6.51s) and size mechanics
(29.41s) plus shell syntax in `67481`; both gates kept their 60-second budgets.
The agent's final size run was 27.01s. Local Bash 5.3.15 / GNU Awk 5.4.1 does
not prove native Bash 3.2 / BWK/macOS execution. The full size/structural matrix
was not rerun locally: its earlier budget miss and low disk space remain.

The diagnostic child now requires exact compiler exit 1, complete member-call
code and argument facts; CRLF is normalized without discarding raw evidence.
The real mutated source passes this child (`994181`). Its full focused parent
(`5107`) then fails canonical MIR expression-graph admission. A separate valid
source-C projection (`493594`) fails at the same boundary, while the existing
native oracle compiles and executes 12. Do not weaken graph admission or skip
this fixture to publish a green result.

The retained input and producer/consumer identity gap are recorded in
`docs/audits/2026-09-06_ability_declaration_identity_epoch_counterexample.md`.
This is a newly reached integration blocker, not an independent compiler track.
Compiler source remains unchanged pending a verifiable, storage-safe edit loop.
The independent read-only lane confirmed the identity loss and flagged two
integration constraints now included in the audit: deduplicate verified
declaration/routine identity and leave raw generic types with their existing
semantic-default owner. Documentation and CI-profile gates passed (`34020`).
Final pre-publication repeat `55844` passed both gates; strict UTF-8 validated
all 12 reviewed paths and `git diff --check` was clean. No compiler/cap TSV
diff exists. The root scratch probe and every original artifact stay excluded.

Publication: the reviewed 12-file set is committed/pushed as
`61f923165553d25af3d17cd9ba14ad30b486486c`. New regular CI `33982549234`
completed SUCCESS, 30/30 on that exact SHA; watch `73619` completed exit 0.
Full driver gen2 == gen3 (177,559 lines), installed-driver CLI/transaction
checks and all three policy-corpus sources passed. No new Platform full run was
dispatched before correcting the already-observed ability compiler failure.
The publication/CI-ID navigation refresh is a subsequent local-only delta.

## Reached fast-macOS coverage continuation

Base: published `61f923165553d25af3d17cd9ba14ad30b486486c`.
Its fast macOS job `101350275135` passed seven steps, but source and job logs
confirm that neither repaired checker mechanics gate ran. That job cannot
validate the Bash/BWK fixes. The existing full Platform workflow has no
per-platform dispatch input, and the ability compiler counterexample is RED.

- Objective: reach the existing two checker falsifiers on macOS during the
  fast push gate, before compiler work, without importing the full matrix.
- Priority: real host dialect coverage, exact existing refusal checks, bounded
  feedback cost, then publication after the live CI completes.
- Owners: the existing checker scripts retain all checker decisions;
  `scripts/ci_push_macos_steps.sh` owns their fast macOS scheduling.
- Last consumer: the existing `build-macos-c-only` push job.
- Forbidden fallback: duplicating whole preparation/bootstrap jobs, weakening
  checks, replacing native Bash/awk, adding a new workflow or changing timeouts.
- Primary-only edit scope: that fast step list, its existing CI-profile ratchet,
  and navigation/evidence docs. All sub-agent leases stay closed.
- Gate: observe the ratchet reject the old seven-step list, then pass after
  wiring both existing mechanics gates exactly once before native compilation;
  keep local checks <= 60 seconds and await native macOS job evidence.

Local result: old-list profile `19312` failed with the new owned message.
`47595` passed the corrected profile and exercised the actual two mechanics
scripts once through the step list, explicitly not executing compiler steps.
`40183` passed the final profile and six in-memory missing/duplicate/late/
earlier-native controls using the exact profile AWK program. These controls
require exit 1 and preserve first-compilation ordering. Native macOS remains
unverified; no compiler source or timeout changed. The current published CI
has completed SUCCESS, 30/30, so publication of this bounded repair is unblocked.

Publication: the six reviewed step/profile/navigation paths passed final
profile/docs/syntax/UTF-8/whitespace checks (`59711`) and were committed/pushed
as `5653165b7b372a84622ee5837e9375c0828fecd6`. Regular CI `33984709173`
completed FAILURE, 29/30 passed on that exact SHA; watch `71941` completed
exit 1. Linux passed all 23 push steps, full driver fixed point and installed
CLI/policy gates passed. Only native macOS job
`101356033822` failed 1/9 steps: the shared size-policy mechanics passed, but
the component checker reached a GNU-specific sed insertion in its final
transport mutation. Its earlier empty-array controls completed before that
construction error. All other eight steps passed. The scheduling edit lease
is complete; its reached mutation repair has a separate bounded directive:
`docs/agent_work_directives/macos_checker_mutation_portability_2026-09-06.md`.
No compiler source changed. No watcher remains live from this run.

The bounded mutation follow-up is published as `91119c45`. Native macOS job
`101362486942` in new regular CI `33987047080` passed both actual checker
mechanics and all nine push steps. That regular run completed SUCCESS, 30/30;
watch `87947` ended exit 0. The separate directive records publication evidence.
Neither native-checker success nor this report closes the ability-ID compiler gap.

## Constraints and handoff

Use apply_patch. No agent builds a compiler, installs tools, changes workflows
or Git, removes existing artifacts, changes IO permissions, creates worktrees,
or delegates. D: has about 19 MiB free; generate only small task-owned checker
fixtures. C: relocation approval is pending and is not supplied by automatic
goal continuation. Main owns one unchanged isolated candidate `1AAFC7D7`.
Report exact edits, commands, statuses, and platform limitations in a message.
Do not add numbered architecture documents or promote SoT/progress counters.
