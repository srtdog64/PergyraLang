# Markdown-only Linux contract split — 2026-08-29

Status: `ACTIVE — IMPLEMENTED LOCALLY, PUBLICATION PENDING` (exact base
`244efe54367ed42a419a38f3a6aba00a8ba3f680`)

This directive coordinates one CI feedback repair. It is not compiler semantic
authority or substitution progress.

## Objective card

- Objective: keep `build-linux` as the mandatory required check while making a
  classifier-proven Markdown-only push run only documentation, SoT, protocol,
  and progress static contracts instead of the compiler/LLVM/Coq integration
  target.
- Priority: exact changed-path classification, fail-closed unknown base and
  empty diff, required-check identity, Markdown contract coverage, then wall
  time and dependency download avoidance.
- Fact owner: `scripts/ci_change_scope_owner.sh` owns `run_full` and
  `markdown_only`; no workflow glob may recreate that decision.
- Last consumer: `.github/workflows/ci.yml` job `build-linux` and its mutually
  exclusive heavy/Markdown steps.
- Forbidden fallback: skip all Markdown validation, infer scope independently
  in YAML, weaken non-Markdown coverage, treat unavailable base as docs-only,
  or rename the required job.
- Verification gate: `tests/self_host_ci_profile_smoke.sh` must prove exclusive
  step selection and the exact Markdown target inventory. The eight Markdown
  scripts must pass locally. Remote falsifier is a Markdown-only push where
  dependency installation and `make ci-push-linux` are skipped and the
  Markdown contract step succeeds.

## Observed trigger and local evidence

- Docs-only commit `244efe54` was classified correctly: nine full-only jobs
  skipped, but unconditional `build-linux` began the full dependency install
  and `make ci-push-linux` path.
- The workflow now selects the heavy steps only when `markdown_only != true`
  and selects the eight-script Markdown contract only when
  `markdown_only == true`.
- The CI-profile gate and all eight Markdown contract scripts are GREEN
  locally. Publication remains pending until the workflow/test repair is
  committed, pushed, and exact-head CI is GREEN.
