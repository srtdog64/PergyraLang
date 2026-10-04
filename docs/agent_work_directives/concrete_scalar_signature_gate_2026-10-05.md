# Concrete scalar signature regression gate

Status: IMPLEMENTATION COMPLETE; source-placement evidence, not closure evidence.
Base: `9fd9ac06fa9521339ceef5c5f38572247a6b6cc2` on `main`.

## Shared objective card

- Objective: prevent the reached concrete-scalar signature consumers from
  restoring a direct indexed String borrow while Main validates the existing
  executable rung.
- Priority: exact callable identity, explicit missing-fact refusal, String
  materialization at its consumer, focused negative checks, then integration.
- Fact owner: `SemanticExpressionGraphConcreteScalarValueOwned`,
  `SemanticExpressionGraphContextualCallArgumentsOwned`, and
  `SemanticExpressionGraphResolvedCallArgumentFactsFromGraph` in
  `ast_expression_graph_concrete_scalar_verdict_owner.pgy`.
- Last consumer: their `SemanticSignatureRangeFactsFromSource(signature)` calls.
- Forbidden fallback: a raw `function_params` element assignment, a copy in
  another function masquerading as this consumer's copy, or treating this
  source inventory as a full-root formal-use proof.
- Integration gate: the actual component checker on focused fixture mutations,
  `bash -n`, and `git diff --check`, within 60 seconds. Compiler/bootstrap and
  exact-head CI remain separately required and are owned by Main and Root.
- Falsifier: the selected function loses the copy or a missing identity guard,
  restores the direct borrow, or passes only because a different function
  contains the expected text.

## Independent scopes

- Root: `tests/self_hosted_component_contract_smoke.sh`, this directive,
  the reached indexed-signature materialization in
  `SemanticExpressionGraphResolvedCallArgumentFactsFromGraph`, integration
  review, and sole Git publication.
- `generic_return_gate`: only
  `tests/self_hosted_component_checker_smoke.sh`, exercising Root's actual
  named checker rather than duplicating its predicates.
- `ci_failure_triage`: read-only exact-head workflow/job/log evidence.
- Main chat: existing executable verification of the reached compiler rung;
  no concurrent edits to this signature owner, either component checker, or
  Git publication. Preserve its preceding two consumer-copy changes.

Preserve the concurrent compiler edit and local `gmon.out`. No registry status,
production Makefile, workflow, dependency, or compiler safety rule changes.
Do not open another compiler implementation track.

## Commands and evidence boundary

Use `apply_patch` for edits. Read-only source/Git/CI inspection, shell syntax
checks, and focused checker fixtures are allowed. Agents do not run compilers,
probes, profiles, production Make graphs, or Git writes. Root independently
reruns the checker and reviews the diff before publication.

These tests falsify source placement and checker mechanics only. They do not
establish runtime ownership, formal-effect closure, installed-driver admission,
or caller release after `inout`. Required exact-head CI is the final shared
green gate; skipped jobs and a reduced graph are not successful substitution.

## Single checkout

The user's later consolidation request fixes implementation at
`D:\PergyraLang` only. Agents share this checkout and independent file scopes;
no new worktree is authorized. Root owns consolidation of existing worktree
changes after inspecting their actual commits, dirty state, and recoverable
artifacts. A read-only worktree audit does not authorize dropping local work.
