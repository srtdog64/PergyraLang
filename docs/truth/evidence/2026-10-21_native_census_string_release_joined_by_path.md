# 2026-10-21: canonical census after String releases join by path

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`395d83190` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** after an `if`, a String borrowed from an
  `Array<String>` counts as released only when a path that reaches the join
  released it, and the `else` branch starts from the state before the `if`.

  Before, the release flag a deep drop (`ArrayDropOwnedStrings`, or an
  `own` handoff of the Array) set inside a branch stayed set after the `if`,
  even when that branch returned. Moves are joined by path through the same
  resource snapshot; the String flag was captured only for cleanup bodies.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `395d83190` | 3606, 1447, 1306, 1297 | 315 |
| releases joined by path | 3431, 1298, 1168, 1159 | 315 |

No flow store failed in any analysis. Outside `direct_mir_*`:

| Kind | Before | After |
|---|---|---|
| a String borrows an Array<String> element that was released | 131 | 8 |
| a borrowed parameter's storage moved into a callee | 31 | 24 |
| every other kind | 820 | 812 |

Most of the 131 were in three type-fact owners
(`SemanticAstStatementTypeFactsFromAdmittedArtifact`, the initializer and
assignment ones). Each clears its working environment with
`SemanticAstExpressionEnvironmentClear` (three `own` Arrays) in branches
that return an error.
