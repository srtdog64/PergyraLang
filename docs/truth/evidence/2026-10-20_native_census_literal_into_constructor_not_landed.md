# 2026-10-20: census of a loan-proof relaxation that was not landed

Recorded by the Claude lane on branch `claude/p0-native-checker`. The
change measured here was reverted before commit; `cf8c11c9b` stays the
branch state.

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a local Array literal that holds parts of a loan
  may be handed to a constructor whose result keeps it.

| Run | Refusals | First analysis |
|---|---|---|
| `cf8c11c9b` | 2218 | 3969 |
| with the relaxation | 2251 | 3793 |

- 101 refusals cleared and 134 appeared.
- 111 of the new loan-proof failures follow
  `SemanticAstNominalConstructorFactsFromArtifact`. Without the relaxation
  its own loans fail and a copy lands inside it, so its result holds none
  of its input and its summary only reads the artifact. With the
  relaxation no copy lands there: its result holds the artifact's Strings,
  it becomes a getter, and the loans its callers take of the artifact fail
  in turn (71 in `SemanticAstArtifactAnalyzeFromExpressionSurfaces`).
- Lesson: a copy that lands deep inside a getter can admit more uses than
  the proof it replaces. Fewer refusals in the first analysis is not the
  measure; the final analysis is.
