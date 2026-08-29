# Markdown-only Linux contract split — 2026-08-29

Status: `ACTIVE — FULL PATH GREEN, MARKDOWN-ONLY REMOTE PROBE PENDING`
(implementation `16d491732d6b2fb682f199af13705131b2cf8a44` is on
`origin/main`; exact-head run `33231503191` completed GREEN 30/30)

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
  locally. The non-Markdown implementation run completed GREEN 30/30;
  `build-linux` passed in 22m05s and full self-host in 35m36s. This
  Markdown-only status commit is the decisive remote probe: dependency
  installation and `make ci-push-linux` must skip while the eight-script
  contract succeeds.
