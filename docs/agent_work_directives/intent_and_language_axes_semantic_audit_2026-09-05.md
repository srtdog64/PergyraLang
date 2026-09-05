# Intent and language-axis semantic audit — 2026-09-05

Status: AUDIT COMPLETE

Base revision: `bf8b33d078b27c41cc6cdb7ffed2e8fa5c62ef22`

The user explicitly reopened concept testing after the first deletion audit.
This directive coordinates tests and evidence; it owns no language semantics,
SoT status, progress percentage, or successor implementation rung.

## Objective card

- Objective: test what compiler-visible Intent graphs actually guarantee and
  apply deletion/substitution and strengthening tests to the remaining
  language axes. Distinguish deleting a spelling, deleting a fact, and
  reimplementing that same fact under another name.
- Priority: preserve the full stated semantic obligation; find a distinguishing
  accepted/rejected pair; execute a bounded substitution experiment; verify
  source owner and last consumer; assess conceptual cost; then presentation.
- Fact owner: current canonical language contracts, their implementation
  owners, and observed executable gates. A design document or a passing model
  theorem does not prove implementation coverage.
- Last legitimate consumer: the primary task's integrated audit and an
  explicitly justified later implementation decision.
- Forbidden fallback: action/step counts as Intent admission, a required DELETE
  quota, concept-usage frequency as quality, invented syntax, stronger claims
  than observed tests, or relabelling an internal equivalent as deletion.
- Integration gate: primary review of all reports and test diffs; execute each
  new retained semantic runner; check cited paths and scoped `git diff --check`.
  No source semantic policy is changed by this audit.

## Independent scopes

1. Intent graph: participant/predecessor/typed outcome/authority/Zone/effect/
   compensation/terminal attribution; one-action and multi-action controls.
   Own `docs/audits/2026-09-05_intent_graph_semantic_audit.md` and optional
   `tests/concept_semantics/intent/` only.
2. Authority and Effect: compare Capability versus Authority and Effect versus
   Capability; distinguish instance/delegation history, effect classification,
   operation grants, and their existing checked joins.
   Own `docs/audits/2026-09-05_authority_effect_deletion_audit.md` and optional
   `tests/concept_semantics/authority_effect/` only.
3. Ability, World, Role, Action, within/where: test the interface/trait,
   Zone-graph, nominal/interface, and function-plus-contract substitutions.
   Own `docs/audits/2026-09-05_domain_axes_deletion_audit.md` and optional
   `tests/concept_semantics/domain_axes/` only.

Read the canonical owner documents, not only the prior report. The prior audit
contains wording that may overemphasize cross-step obligations; action count
is neither necessary nor sufficient. A shared purpose-bound checked bundle is
the object under test. Do not promote purpose labels or runtime trace alone
into an independent static theorem.

## Experiments and reporting

- Each axis gets a provisional KEEP-CORE, KEEP-FACT / NO NEW KEYWORD,
  CONDITIONAL, or DELETE verdict plus the evidence that would reverse it.
  Unknown or untested obligations remain explicit.
- Report separately: existing gate execution, runnable source-level rewrite,
  static owner inspection, model-level theorem, and proposed strengthening.
  Do not say the compiler concept was removed unless it actually was.
- At least one bounded source-level substitution experiment per lane should
  compare the same intended behavior and state which rejection or guarantee
  survives. If exact equivalence cannot be encoded, show the missing fact and
  stop that experiment without inventing a compiler feature.
- Retain only deterministic tests of current promises. A discovered incomplete
  promise goes into the report; do not freeze faulty behavior as expected
  success or create a new implementation track. Use valid programs and static
  semantic rejection checks, not runtime corruption or exploit probes.
- Three unrelated workloads is a sampling heuristic, not a semantic law.
  Existing compiler fixtures are not evidence of broad external adoption.

## Commands, budgets, and shared state

- Read-only source/Git inspection and bounded compiler/test commands are
  allowed. Use `apply_patch` for retained or scratch test source edits.
- Scratch is `.tmp/self_hosted/concept_semantics_20260905/<lane>/`.
- Use installed binaries; do not rebuild, install, delete shared scratch,
  mutate registries, stage, commit, or push. Use Git Bash explicitly on this
  Windows host. Existing focused runners must use distinct scratch locations.
- Each focused gate has a five-minute budget; each audit has thirty minutes.
  No full matrix. Report skips, build/tool failures, and timeouts accurately.
- DRV-2 SHA-256 at start is
  `FB37EA36D92E9C28B6BB7162F87BA00E733255AD5E46B24A166578713DF75847`.
  Native local SHA-256 is
  `0F9F4F30255D6850B5A773E21D5815F776B305E5C01A7A2C3DF6D373BB15A29E`;
  another session rebuilt it, so record its scope as local evidence.
- The active enum implementation owners and other-session native/compiler/
  parser/runtime/Makefile work are read-only. Never inspect or edit
  `examples/raid_graph_fsm/results.txt`, `docs/compiler_architectures/`,
  `pgy-80135c2c/`, or `pgy-91d769ec/`.

Primary integration owner: `/root`. Agents do not spawn further agents.

## Observed integration result

The three agent reports and primary's nominal experiment are integrated in
[the audit result](../audits/2026-09-05_language_axes_semantic_integration.md).
Primary reran all four retained regression lanes and the aggregate: PASS.
Documentation quality, keyword registry, local links, shell syntax, UTF-8 and
scoped whitespace checks also passed. The separate required
source-admission gate is RED: nine claims, nine failures, including eight
invalid sources published as MIR and one valid typed Intent missing its plan.
No expected-failure allowance or semantic implementation change was added.
The directive is complete as an audit, not as compiler closure.
