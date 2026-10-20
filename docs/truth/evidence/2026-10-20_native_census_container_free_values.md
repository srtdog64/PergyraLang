# 2026-10-20: canonical census after container-free values hold no storage

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a value whose parts are all Strings or copy-only
  values (an Option of one, a struct of such fields) holds no container
  storage, in the storage proof and for a loan's carriers
  ([decision](../decisions/2026-10-20_container_free_values_hold_no_storage.md)).
- **Wall:** 817 s for four analyses (3.98 GB peak).

| Run | Refusals |
|---|---|
| before the call-result rule (`c60564db0`) | 2532 |
| call-result rule (`37fa21435`) | 2479 |
| after | 2218 |

- Against `37fa21435`: 311 refusals cleared and 50 appeared. The rounds:
  3985, 2557, 2352, 2218.
- Against `c60564db0`: 436 cleared and 122 appeared. Of the 122, 51 are
  uses after `SemanticAstLocalBindingFactsFromArtifact`, 45 after the
  `TypedAstArena*Text` getters and 9 after `ParserExpressionBinary`; not
  yet classified.
