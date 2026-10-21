# 2026-10-21: canonical census after a pushed Clone keeps an Array's elements owned

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`046712fa2` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** `ArrayPush(a, Clone(s))` counts as keeping the
  elements of `a` owned, as `ArrayPushOwnedString` does. A by-value or
  `ref` formal's String pushed into an `inout` output already lands as such a
  `Clone` ([decision](../decisions/2026-10-19_string_landing_copy_then_move.md)),
  but the callee's element verdict still said the push might leave a
  borrowed String, so a caller that later kept or released the Array was
  refused.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `046712fa2` | 3415, 1279, 1149, 1140 | 315 |
| pushed Clone keeps elements owned | 3400, 1264, 1134, 1125 | 315 |

No flow store failed in any analysis. Outside `direct_mir_*` (825 before,
810 after):

| Kind | Before | After |
|---|---|---|
| an Array argument call requires preserved array storage | 66 | 53 |
| ArrayPushOwnedString requires preserved array storage | 10 | 8 |

The removed refusals are at calls to `SelfMirRoutineLocalInventoryError`
(7, each after `SelfMirRoutineLocalInventoryAppend`, whose
`ArrayPush(names, name)` goes into an `inout` output),
`MirDomainTopologyOwnerGraphAddEdge` (4) and 4 others. None was added.
