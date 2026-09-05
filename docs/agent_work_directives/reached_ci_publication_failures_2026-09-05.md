# Reached publication CI failures

Status: second-round IMPLEMENTATION COMPLETE and published; remote results reconciled

Base: `918a7c98e541771fab542713262b643d375d51c0`, published. CI `33966684805`
completed FAILURE (29/30 passed); Platform full `33966692630` completed FAILURE
(8/13 passed). Repair `2c5d0ed0c8e1d936aa3f8ab932617c06de44156b` is published;
CI `33975994077` completed SUCCESS 30/30; Platform full `33976010056` completed
FAILURE 10/13 at that exact SHA. The second-round harness repair is published as
`da818c3df133c23572486872e4114d594f981890`, after local verification and
independent review. CI `33979208920` completed SUCCESS 30/30 and Platform full
`33979255563` completed FAILURE 10/13 on that exact revision. Linux/Windows
core passed; macOS checker and both ability-diagnostic driver checks failed.
The reached continuation is separately coordinated by
`reached_platform_portability_and_ability_diagnostics_2026-09-06.md`.
The user's existing explicit
integration/commit/push/CI authorization applies; no new compiler rung opens.

## Objective card

- Objective: repair the failures actually reached by the current enum/receiver
  integration, preserving the compiler owner and existing negative contracts.
- Priority: typed ownership and explicit failure, real cross-platform coverage,
  test falsifiers, then publication and exact-revision CI. Do not raise a metric
  ceiling or hide a failing backend to obtain green.
- Fact owner: MirMatchBindingOrigins owns exact instruction/ordinal-to-row
  lookup. Existing backend-profile inputs and checker responsibility own CI
  execution/structural checks; reports and literal file counts are not semantic
  authority.
- Last consumers: match local identity and Option/tagged-enum GraphPlan
  admission; existing preparation and macOS platform test parents.
- Forbidden fallback: raw -1 lookup failure, guessed row zero, missing-root
  checker success, ad-hoc backend skip, deleted negative case, cosmetic compiler
  splits or unexplained cap increases.
- Integration gate: focused ownership and checker regressions, then CI and
  Platform full at the next reviewed pushed revision. Let the current matrix
  surface other failures before publishing the combined repair.

## Independent scopes

- Primary owns all compiler source changes, binding-origin tests, builds,
  navigation, staging, commits/push, and workflow/result coordination.
- `ci_assertion_review` owns only the reached test-harness fixes in
  `tests/self_hosted_component_contract_smoke.sh`,
  `tests/self_hosted_component_checker_smoke.sh`,
  `tests/structured_spawn_lifecycle_smoke.sh`, and
  `tests/concurrency_examples_smoke.sh`, plus its separate report
  `docs/audits/2026-09-05_reached_platform_checker_review.md`.
  Diagnose macOS Bash 3.2 missing-placement-root acceptance, C-only backend
  profile propagation, and the missing Subject of this gate comment. Preserve
  Linux C+LLVM coverage and add focused mechanics negatives where needed.
  Do not edit Makefile/workflows or compiler sources; propose if required.
- `semantic_index_review` is read-only for the five owner-size failures and
  their existing gate/cap ownership. Only report at
  `docs/audits/2026-09-05_reached_owner_size_review.md`. Trace current
  `tests/test_inc_size_smoke.sh` and existing source-cap registries/consumers,
  distinguish pre-existing debt from this commit, and recommend an owner-
  consistent correction without an automatic cap increase or broad refactor.

## Reached enum-local admission sub-slice

- Production entry: public source LLVM -> the installed self-host driver ->
  shared scalar GraphPlan. CI Linux step 19 and the current local candidate
  both reject the SEA classifier at routine ordinal 4, `local_inventory`.
- Exact fact: `CaptureFactFromBoundaryFact` carries one materialized match
  subject of nominal type `BoundaryKind`. The sealed referenced-enum owner
  includes payload-free and payload-bearing enums; the local type/inventory
  consumers currently accept only the payload-bearing subset.
- Objective: preserve that nominal local identity through the existing owner,
  not reclassify arbitrary names as Int or bypass the self-host LLVM route.
- Last consumers: local value-type admission and source-local inventory;
  final typed-plan readiness must consume the same enum fact. The local
  emitters now admit an ordinal only for Int/Long or a sealed payload-free
  enum, and explicitly reject an unadmitted representation.
- The two local emitter caps move 135 -> 140 and 210 -> 215 for these new
  typed rejection branches within the existing emission responsibilities.
  This is explicit implementation growth, unlike the eight unchanged policy
  limits migrated from conflicting consumers. No generic cap is raised.
- The Option match-condition responsibility cap moves 110 -> 115: the typed
  origin lookup now requires an explicit presence guard and checked unwrap
  (111 current lines). The failed full inventory reported this exact delta.
  No function is split or proof branch removed to fit the former limit; the
  preparation sentinel ceiling remains 23.
- Falsifier: real enum-member match source and C/LLVM execution, then missing
  enum identity / mismatched local type refusal with no output artifact.
- Primary alone edits this reached compiler slice. This remains the active
  enum callable rung, not a parallel SEA/runtime implementation track.

## Budgets and evidence

Static/checker tests have 60-second budgets; one focused executable slice may
use 300 seconds. No agent rebuilds a compiler/driver, produces full MIR, deletes
existing artifacts, changes Git state, contacts other sessions, or spawns agents.
Primary owns one current-source candidate and one integration gate. D: has
less than 100 MiB free, so preserve existing evidence and report storage limits.
Use normal tools without sandbox_permissions; author files with apply_patch.
Record actual runs and failures, not inferred green. Reports do not advance
SoT or substitution counters.

## Integration evidence — 2026-09-06

Both bounded review lanes are AUDIT COMPLETE. Primary retains publication and
exact-revision CI ownership. Current candidate `1AAFC7D7` (source graph
`6bbeb7206ad331585d44fdc62b9be72a629a3a3ef3165cc2f9cd382a83659f2c`) passed
SEA C/LLVM 35/35 per backend and missing-term negatives, adjacent Option match
with seven mutation pairs, and the tightened mixed-enum gate (`85024`,
`run.qXP0DI`). That gate executes three sources over five projections plus two
roundtrip controls, probes the production origin lookup, and retains the
33 mixed-payload refusals. Six new local mutations reach `local_inventory` or
`admitted-type`; two declaration-deletion refusals occur earlier and do not
prove the local boundary. The independent reviewer confirmed the original
loss-of-reference confounder is removed.

Full component inventory (`22994`, 2,331 caps / 973 extractions / 677 reuses),
size, likeness (23/23 sentinel, 4,768 Option/Result uses), keyword, subject,
CI profile, documentation and SoT edge checks passed. Native structured-spawn
default C+LLVM passed (`94527`), in addition to the earlier C-only run. The
class/array parent passed both selected fixtures (`61006`). The Makefile spawn
profile propagation now has a focused assertion in the existing CI-profile
gate. Shared installed driver and old fixed-point artifacts are unchanged.
The later full-bootstrap and exact-revision results are recorded at the top
and end of this directive; the local gates above were not their substitutes.

Observed same-name enum reconstruction and preceding-if/exhaustive-return
limitations are OPEN in the separate counterexample audit. No compiler repair
of those limitations or SoT percentage increase is claimed by this CI slice.

## Bounded read-only follow-up while exact-revision CI runs

- Objective: identify the missing fact/consumer for the two already reached
  enum positive counterexamples, so they remain actionable instead of vague
  OPEN notes. This is diagnosis within the current enum rung, not permission
  to open a successor implementation track or cancel the running CI.
- Priority: exact nominal and CFG identity, current owner reuse, then a minimal
  falsifier. No name-based guess, first matching variant, duplicated semantic
  owner, general query framework, or new representation is proposed by default.
- Fact owners: the existing match-subject/enum declaration facts and CFG
  reachability/exhaustive-match facts; source output and reports do not own them.
- Last consumers: structured condition/ordered expression reconstruction for
  duplicate variant names; terminal-return/exhaustive fallthrough admission for
  an earlier returning `if` followed by an exhaustive enum match.
- Independent scopes: `ci_assertion_review` reads the duplicate-name route;
  `semantic_index_review` reads exhaustive-return code 103. Neither edits files,
  builds a compiler, runs full suites, changes Git/workflows, or delegates.
  Return exact source anchors, the missing boundary, and the smallest proposed
  falsifier in a message. Primary alone owns source and any documentation edit.
- Integration gate: primary reconciles the live CI runs before any publication
  or new implementation. Read-only proposals require an objective/card and
  executable success/refusal evidence before being counted as repairs.
- Budget: bounded source inspection and existing small retained artifacts;
  individual static commands at most 60 seconds. No new large disk artifacts.

## Reached second CI round — published `2c5d0ed0`

Initial observation: the two completed failing jobs were macOS full `101332706575` (steps 31/53)
and Linux driver parity `101334223211`. Other jobs, including full driver
bootstrap, remain live and must not be restarted or cancelled.

Subsequent observation: regular CI completed SUCCESS 30/30, including driver
fixed point (177,559 lines), codegen fixed point (76,777 lines), installed CLI
and three policy sources. Platform Linux core passed all 122 steps. Windows
driver `101335222877` reached the identical cash.23 assertion failure; only
Windows core remains live. Remote results do not refresh the local installed
binary or promote substitution percentages.

Final observation: Platform full completed FAILURE, 10/13 jobs passed. Windows
core also passed all 56 steps. The only failures are the macOS AWK parser and
the identical Linux/Windows collection-enum assertion. Both original watchers
completed without restarting or cancelling a live job.

- Objective: repair the actually observed AWK portability failure and determine
  whether the collection/enum loop's literal `cash.23` assertion names the
  correct owned SSA fact. Preserve all existing semantic and negative checks.
- Owners: the shared size-policy parser and the routine's exact phi/use/
  predecessor facts. A platform-specific line count or a fresh SSA number is
  not an alternate authority.
- Last consumers: component/general size gates on macOS; the collection-enum
  match-loop child in `driver_rung2_body_parity.sh`.
- Forbidden fallback: generic cap bypass, silent AWK dialect skip, replacing
  23 with another guessed SSA suffix, removing phi mutations, accepting arbitrary
  nonzero exit, or interpreting source/native parity as proof of phi identity.
- Independent edit scopes: `semantic_index_review` may change only
  `tests/self_hosted_owner_size_policy.awk` and
  `tests/self_hosted_owner_size_policy_smoke.sh`. Primary owns the collection/
  enum parity child and its focused fixtures/diagnostics, all compiler source,
  documentation, Git, and CI. No agent builds a compiler or changes workflows.
- Integration gate: focused size-policy mechanics (60 seconds) and the reached
  driver fixture (300 seconds), then existing parents and exact-revision CI.
  Review all remaining results before publishing a combined repair. Any newly
  discovered semantic defect must be distinguished from assertion drift.

Second-round implementation evidence: the AWK negated class changes only slash
escaping, with unchanged eight caps. Test-first ratchet failed before the edit;
GNU checker passed after it. Native macOS verification remains outstanding.
Primary observed the old collection/enum literal assertion fail (`49853`).
The replacement checks exact initialization/header/latch definitions and
preserves the deleted-backedge-input production refusal, now requiring exact exit 1
for all six mutations. DecideOf is evaluated once by match materialization;
the former three-call expectation was another legacy assertion, not a desired
semantic behavior. The full one-fixture parent passed (`22125`), as did the
renumber/reorder controls and seven in-memory checker negatives. Those seven
are checker mechanics, not seven additional compiler rejections.

Before publication, primary may run the bounded adjacent fixtures
class_result_chain_loop, class_method_result_loop, class_bump_option_match and
coalesce_accumulate_loop through the same existing parent (300-second budget,
one current candidate, no compiler rebuild). These falsify the same reached
match-materialization/loop assertion boundary. `semantic_index_review` now
reviews only the new checker and collection/enum child read-only; no further
edit scope is delegated. Primary owns any reached correction and integration.

Adjacent parent `50755` passed all four selected fixtures with the same
candidate. CI-profile/documentation gates passed (`9355`); primary independently
reran AWK mechanics successfully (`58881`). The broader local size scan
`85297` hit the 60-second budget (exit 124) after mechanics passed and did not
reach a completion verdict. It is recorded as incomplete, not green. No budget
increase or unrelated size-checker redesign is part of this repair; the next
exact-revision remote full gates remain the integration falsifier.

The independent read-only phi review is COMPLETE. It confirmed the retained
entry 0 -> header 1 <- latch 12 definition join, one materialized DecideOf
evaluation per match, and deletion of only the latch incoming use (not the CFG
edge). No material checker gap was found. This review ran no compiler or new
fixture and does not add executable evidence to the primary's observed runs.

Exact-revision evidence on `da818c3d`: job `101341267377` completed
codegen gen2 == gen3 (76,777 lines); job `101341267386` compiled/kernel-verified
49 proofs with two declared abstractions, no admits and no unsafe kernel
features. The planted-Admitted selftest also passed. Full driver job
`101341267420` completed gen2 == gen3 (177,559 lines), and regular CI finished
30/30 SUCCESS. Platform full finished 10/13 FAILURE. Its succeeding macOS
empty-array/read-error and ability-diagnostic failures belong to the new
directive; no full-platform green is inferred from the successful jobs.
