# 2026-10-24: census with the lent-argument copy

Recorded by the Claude lane on branch `claude/p0-native-checker`, on the
tree of `f6de1d1b7` plus the lent-argument copy
([decision](../decisions/2026-10-24_refused_loan_copies_its_argument_when_result_cannot_be.md)),
local and not pushed. Census tool and input as in the
[chain walk record](2026-10-24_landing_planner_chain_walk_census.md)
(plan6 there is `f6de1d1b7`).

## Result

| Run | Analyses | Wall | Copies | Final (outside `direct_mir_*`) | Moves (final) |
|---|---|---|---|---|---|
| plan6 (`f6de1d1b7`) | 4 | 1156 s | 3390 | 116 (75) | 34 |
| plan7 (lent-argument copy) | 4 | 2088 s | 3391 | 91 (51) | 10 |

Peak memory 3.98 GB in both. plan7's wall time ran beside two flow smokes
on the same machine, so it is not comparable. The kinds diff shows only
moves changing (34 to 10); no kind is added.

plan7 copies 14 places plan6 does not, all lent arguments copied where the
call takes them (among them `valid` and `artifact` into
`SemanticAstExpressionSurfaceFactsFromTypedArtifact`, `build.cfg` and
`build.cfg.local_refs` into the `SelfMirCfgAttachLast*` and
`SelfMirInstructionLocalRefRows*` calls, `target_projection`,
`graph.arena.topology`). It leaves 13 of plan6's: the ten chain copies of
`ApplyPostfixFact`, which now copies only its two lent `base` arguments
(`expr_postfix_owner.pgy:160:48, 229:46`, in plan6's set as well), two of
`SelfMirRoutineAttachLastExpressionGraph` and one getter copy in
`CanonicalMirIdentityEpochRebindProgramFacts`.

On its module alone (`ast_assignment_type_fact_owner.pgy`) the nine `valid`
refusals go, and the module lands 213 copies instead of 216.

## Left outside `direct_mir_*` (51)

| Group | Count |
|---|---|
| `admitted` handed into `direct_mir_*` routes (`driver_rung2_owner.pgy`, scalar C substitution) | 8 |
| `compatibility_view` escapes | 5 |
| local copies of `analysis.expression_surfaces.expression_graph` whose other side changed | 4 |
| moves of arena rows (`graph.arena.identities` 3, `sequence.arena.identities` 1) and `sequence.arena` local copies 2 | 6 |
| `ref` escapes of MIR lowering `input` | 3 |
| borrowed parameter storage moved into `SemanticAstExpressionSurfaceFactsWithGraph` | 3 |
| preserved Array storage (`SemanticExpressionGraphDomainQuery*` 3, others 3) | 6 |
| Array call results with no ownership transfer proof (`DirectMirScalarProgram*`) | 2 |
| `request`, `codegen_analysis` moves | 4 |
| a landed Clone at an `inout` argument (`CanonicalMirIdentityEpochArtifactFromTreeText`) | 2 |
| others, one each | 8 |
