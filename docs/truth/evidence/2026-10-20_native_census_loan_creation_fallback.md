# 2026-10-20: canonical census after the loan copy at creation fallback

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a getter loan whose exit no copy can stand in is
  copied where it is made
  ([decision](../decisions/2026-10-20_loan_copied_where_made_when_its_exit_cannot_be.md)).
- **Wall:** 784 s for four analyses (3.98 GB peak).

| Run | Refusals | Moves | Loan proof failed |
|---|---|---|---|
| before (`e4a905042`) | 2637 | 1077 | 571 |
| after | 2532 | 972 | 466 |

- 110 refusals cleared and 5 appeared. The rounds: 3890, 2670, 2561, 2532.
- One loan of the local `signatures` in
  `SemanticAstArtifactAnalyzeFromExpressionSurfaces` accounted for 62 of
  the cleared refusals: the getter's facts left inside the analysis result,
  which `Clone` copies only by value.
- Of the 5 new refusals, three are later uses of `semantic_analysis` in
  `GenerateCUnitFromAdmittedSemanticArtifactObserved` that now reach the
  move check. Two are borrowed `ref` escapes. One of them is at a landed
  getter call: the `ref` argument's loan now flows into the `Clone` that
  copies the result, and the escape check counts that Clone as a call that
  could keep it.
