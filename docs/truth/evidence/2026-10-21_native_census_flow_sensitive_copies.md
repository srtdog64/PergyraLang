# 2026-10-21: canonical census after flow-sensitive local copies

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** copy rows and their changes in the move owner, and
  owned String push landings
  ([decision](../decisions/2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)).
- **Wall:** 1658 s for four analyses (3.98 GB peak) on a loaded machine.
  One analysis of each tree run side by side: 309 s with the rule, 317 s
  without.

| Run | Refusals | Rounds |
|---|---|---|
| `ce13eda6d` | 2066 | 3944, 2258, 2096, 2066 |
| copy rows only | 45494 | 3996, 45618, 45494 |
| copy rows and owned String pushes (final) | 2055 | 3999, 2229, 2059, 2055 |

- Copy rows alone failed the flow store after the first landing: a copied
  `Array<String>` became an owned Array, the plain String pushes into it
  were refused, and the refusal returned before checking the pushed value,
  so `MirExpressionGraphSequenceAppend`'s claims went stale. Owned String
  push landings and the checked value fixed it.
- Final run: 125 refusals cleared (16 in `direct_mir_*`), 114 appeared.
  The new ones are 70 copies `Clone` cannot copy whole (most in
  `SemanticAstBodyTypeBundleFromAdmittedAnalysisObservedWithIdentityPolicy`,
  `SemanticAstCollectionExclusiveValueFactsFrom` and
  `SemanticAstMapStringPayloadAtCallCompletion`) and 35 moves, 9 others.
  The cleared ones are mostly uses after `SelfMirIntentExecutionCanonicalPlan`
  (22) and `DirectMirScalarCfgArrayIntCollectionFromOwners` (13).
