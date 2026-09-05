# CI semantic integration review

Status: AUDIT COMPLETE

Base: `f34355b37dbd9e86ef574399e895a78fd41dd0a3` plus the primary task's
uncommitted enum/receiver and reached CI-consumer repairs. This directive is
temporary coordination, not compiler semantics or completion evidence.

## Shared objective card

- Objective: verify the reached semantic-tool indexed-assignment repair and CI
  assertion repairs without opening another compiler implementation rung.
- Priority: existing fact ownership, exact negative rejection, accepted controls,
  inspectable failures, then bounded verification cost.
- Owners: `SemanticCollectionMutationError` owns collection-mutation policy;
  `SemanticTopLevelOperatorFactsFromExpression` owns operator positions;
  `SemanticVerdictPayloadFixtureFrontierCount` owns the declared fixture count.
  Native diagnostic owners and registered include faces remain unchanged.
- Last consumers: auxiliary `CheckBody`, the semantic parity orchestrator, and
  the relevant diagnostic/CI/include registry assertions.
- Forbidden fallback: fixture-name policy, a second mode table/count authority,
  suppressed diagnostics, accepting malformed transport, or a native retry.
- Integration owner: primary task alone owns implementation, shared gates,
  navigation/progress documents, and any candidate build.
- Integration gate: `tests/self_hosted/parity/semantic_parity.sh`, after the
  focused indexed-write probe and source-owner gate pass. A source-review result
  is not executable parity, installed-driver evidence, or remote CI green.

## Independent lanes

### Semantic mutation review

Read the changed `src/self_hosted/semantic/body_check_owner.pgy`,
`expression_operator_fact_owner.pgy`, the existing mutation policy, and
`tests/self_hosted/parity/fixture/semantic_index_mutation_owner_probe.pgy`.
Check indexed writes, equality/operators, comments, nesting, and scoped parameter
modes. Report concrete gaps, exact source anchors, and bounded falsifiers; do not
change implementation or shared tests. The only writable path is
`docs/audits/2026-09-05_semantic_index_mutation_review.md`.

### CI assertion review

Review the current workflow/package guard, JSON diagnostic assertion, semantic
frontier transport, and border include-checker repair against their existing
owners. Run only the existing lightweight focused checker/transport tests, with
60 seconds per invocation. Do not rebuild compilers, run full matrices, query
or mutate remote CI, or edit shared scripts. The only writable path is
`docs/audits/2026-09-05_ci_assertion_repair_review.md`.

## Execution and handoff

Read-only source/Git inspection and narrowly scoped local diagnostic checks are
allowed. Use `apply_patch` for each owned report. Do not stage, commit, push,
install binaries, spawn more agents, or edit other-session/protected paths.
Protected paths are `examples/raid_graph_fsm/results.txt`,
`docs/compiler_architectures/`, `pgy-80135c2c/`, and `pgy-91d769ec/`.
Reports distinguish observed facts, inferences, unrun cases, and proposals, and
record the exact base and commands/results. Agent count and fixture count do not
increase SoT/self-host progress. Primary runs the focused probe while reviews
proceed and integrates actionable findings before recording a verdict.

## Primary integration

Both independent reports are complete:

- `docs/audits/2026-09-05_semantic_index_mutation_review.md`
- `docs/audits/2026-09-05_ci_assertion_repair_review.md`

The primary reproduced the grouped-receiver rejection gap (focused session
`78674`, exit 1) and both comment variants (`--check` incorrectly returned
`Status: ok`). Existing operator positions, grouping normalization and mutation
policy now handle these targets; focused `run.QZi11k` passes, including exact
mutation code/receiver/function/mode and accepted comparison/inout/shadowing
controls. The semantic reviewer rechecked the correction without finding another
blocking issue in this top-level slice. Nested assignment expressions are not
claimed covered.

The primary also reproduced `accepted missing_inline`, then required the exact
canonical inline twin. The full border gate passes, and the CI reviewer
independently reran its six checker cases successfully. Existing no-Python JSON
validation and literal-line include inventory limitations remain explicit.

First complete semantic parity (`54964`) passed 115 fixtures on both C and LLVM
before the final trivia correction. Current-source session `63651` passed all
115 C verdicts and completed the LLVM executable build, but exited 124 at the
five-minute budget before the LLVM verdict loop completed. No matching compiler
process remained. Primary pinned the completed C/LLVM/comparator and actual gate
artifacts, then reused the actual manifest/verdict consumers without rebuilding.
Continuation `23707` passed all 115 LLVM verdicts and both comment refusals in
both binaries; the self-host source graph stayed byte-identical throughout.
This is completed resumed verification, not an uninterrupted PASS for `63651`.
Complete structural inventory `12826` passed in 252.418 seconds; its 60-second
latency target remains unmet. SoT, generated keyword inventory, documentation,
source-scan and normalization gates also pass. No driver installation, Git
publication, remote rerun or progress-percentage promotion occurred here.
