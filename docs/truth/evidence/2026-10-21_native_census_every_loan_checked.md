# 2026-10-21: canonical census after every loan of a path is checked

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** each loan of a path is its own move row and selects
  its copy in the same analysis, and a loan handed to a formal whose
  storage proof fails leaves through that call
  ([decision](../decisions/2026-10-21_every_loan_of_a_path_is_checked.md)).
- **Wall:** 819 s for four analyses (3.98 GB peak).

| Run | Refusals | Rounds |
|---|---|---|
| `370eca5a6` | 2182 | 3940, 2521, 2316, 2182 |
| both rules, the grower rule with extra unfalsified conditions | 2074 | 3959, 2266, 2104, 2074 |
| and a loan's local handed to a constructor is no change (not landed) | 2085 | 3625, 2243, 2115, 2085 |
| both rules (final) | 2066 | 3944, 2258, 2096, 2066 |

- Final run: 168 refusals cleared and 52 appeared, all outside
  `direct_mir_*` except 6 cleared there. The cleared ones are mostly uses
  after `SemanticAstLocalBindingFactsFromArtifact` (51) and the
  `TypedAstArena*Text` and `LookupKindType` getters: a refused loan now
  lands its copy in the analysis that refuses it, not one per analysis.
- The new ones are uses after `SemanticAstExpressionSurfaceFactsFromTypedArtifact`
  (30), `RewriteSemanticCallArgument`, `ParserExpressionArrayElement` and
  `ParserExpressionSetElement` (5 each): second loans that were never
  checked before and whose copy cannot land.
- The constructor relaxation let 334 more loans through the first analysis,
  but copies that used to land early stopped landing within the three
  landing analyses, so the final count rose.
