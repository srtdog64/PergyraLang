# Ownership checkpoint and green integration

> **OLD** (2026-10-08 표시): 연혁 기록이다. 지금의 작업 대기열이 아니며, 아래 원래 상태 줄과 내용은 바꾸지 않았다. 현재 협업 상태는 [claude_gpt_ownership_collaboration_2026-10-08.md](claude_gpt_ownership_collaboration_2026-10-08.md)와 인계 문서의 활성 카드를 본다.

Status: ACTIVE coordination; implementation candidates, not closure evidence.
Base: `1e2fd61d9ec45f902dc4ef805963e6ca0780e269` on `main`.
The starting checkout has 57 dirty paths; preserve existing work.

## Shared objective card

- Objective: preserve the pending compiler work in Git and make the exact
  published revision pass required CI without removing safety obligations.
- Priority: semantic identity and one owner, fail-closed boundaries, retired
  path removal, focused verification, publication, then exact-head CI.
- Fact owner: current collection ownership and mutation policy source owners;
  tests consume those owners and do not create language authority.
- Last consumer: the MIR-root self-host bootstrap and required CI jobs.
- Forbidden fallback: disabled gates, native semantic bypass, caller-name
  whitelists, guessed ownership, or claiming a narrow oracle is full parity.
- Integration gate: the required workflow checks on the exact pushed HEAD.
  The focused source-inventory gate is
  `tests/self_hosted_component_contract_smoke.sh`; it is structural only.
- Falsifying case: a relocated readonly policy guard or exact local identity
  check is absent from its named owner; a failing/omitted required CI check
  also falsifies the final green claim.

## Independent scopes

- Root: `tests/self_hosted_component_contract_smoke.sh`, this directive,
  integration review, Git staging/commit/push, exact-head CI verification.
- `generic_return_gate`: only
  `tests/self_hosted/parity/generic_return_probe_parity.sh` (completed), then
  `tests/self_hosted_component_checker_smoke.sh` for the isolated dry-run tests.
- `handoff_refresh`: only `docs/current_work_handoff.md` and
  `src/self_hosted/tools/generic_return_probe/intent.md`.
- `ci_failure_triage`: read-only workflow, exact-head run/job/log inspection;
  report findings to Root, no workflow changes or external writes.
- Main chat: compiler semantic source and the already active executable rung;
  no writes to the scopes above and no stage/commit/push during Root ownership.

No agent may edit another scope, reset/clean the checkout, alter registry
closure status, install dependencies, craft exploit reproductions, or start
another compiler implementation track. No new malformed-input experiments
are part of this directive. Preserve `deployment_optimization_guide.md` as a
user document. Keep `gmon.out` locally and exclude it from staging.

## Commands and budgets

Read-only `rg`, file/hash inspection, non-mutating Git commands and `gh api`
are allowed. File edits use `apply_patch`. Agents may run `bash -n` for their
shell file and `git diff --check`; no compiler/probe/profile runs or Git writes.
Root validates the structural gate within its 60-second budget. Existing
integration execution remains with Main; Root independently checks receipts
and CI. No timeout, memory, retry or matrix relaxation is a green mechanism.

Root is the integration and Git owner. Agent completion is supporting evidence
only; Root rechecks diffs, source ownership and observed gates before publishing.
The next source revision and terminal CI receipts, not this directive, determine
whether the user's request is complete.

## Reached structural-gate boundary

- Objective: make the source-only replacement-frontier inventory independent
  of a previous native build while preserving all three graph assertions.
- Priority: unchanged Makefile graph authority, explicit dry-run failure,
  isolated generated response files, focused positive/negative checks.
- Fact owner: Makefile installed, standalone, and admitted target graphs.
- Last consumer: the component inventory's frontier bootstrap-count assertion.
- Forbidden fallback: creating a fake compiler, skipping the graph assertion,
  modifying the production build directory, or executing build recipes.
- Gate: the checker exercises the inventory owner's actual graph checker on
  an unbuilt fixture, including unexpected bootstrap and failed Make cases.
- Falsifier: the dry-run depends on an existing build directory, changes caller
  artifacts, executes a recipe, or accepts an invalid target graph.

This is an inventory transport fix, not another compiler implementation rung.
The full component gate keeps its 60-second execution budget. Preparation for
an exported source snapshot is separate from evidence for that gate.
