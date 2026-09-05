# Reached publication CI failures

Status: ACTIVE

Base: `918a7c98e541771fab542713262b643d375d51c0`, published. CI `33966684805`
completed FAILURE (29/30 passed); Platform full `33966692630` completed FAILURE
(8/13 passed). The current dirty repair is not covered by these runs. The user's existing explicit
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
New full bootstrap and repair-revision remote CI remain outstanding.

Observed same-name enum reconstruction and preceding-if/exhaustive-return
limitations are OPEN in the separate counterexample audit. No compiler repair
of those limitations or SoT percentage increase is claimed by this CI slice.
