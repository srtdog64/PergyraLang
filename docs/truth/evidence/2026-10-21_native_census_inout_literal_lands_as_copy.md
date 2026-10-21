# 2026-10-21: canonical census after a literal pushed into an inout output lands as a copy

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`f7b42bff7` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** a String literal a store builtin puts as an
  `Array<String>` element into an `inout` output is replaced by `Clone` of
  it, as a formal's String already is there
  ([decision](../decisions/2026-10-19_string_landing_copy_then_move.md)).
  Strings are `char *` in the runtime and `ArrayDropOwnedStrings` frees each
  element, so a literal there is not an element the caller's Array can own.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `f7b42bff7` | 3396, 1260, 1130, 1121 | 315 |
| inout literals land as copies | 3387, 1251, 1121, 1112 | 315 |

No flow store failed. Outside `direct_mir_*` (806 before, 797 after):

| Kind | Before | After |
|---|---|---|
| an Array argument call requires preserved array storage | 49 | 43 |
| ArrayPushOwnedString requires preserved array storage | 8 | 5 |

The removed call refusals follow the intent emitters
(`CodegenIntentEmitActionStep` 2, `CodegenIntentPlanEmitTransition`,
`CodegenIntentEmitNestedCall`, `CodegenIntentEmitExpect`,
`CodegenIntentEmitCleanup`), which push C lines such as
`"    bool __intent_failed = false;\n"` into their `inout` body. None was added.
