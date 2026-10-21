# 2026-10-21: canonical census after ArraySort is modelled

Recorded by the Claude lane on branch `claude/p0-native-checker` (local
commits, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** ArraySort and ArrayReverse return their receiver and
  write it in place
  ([decision](../decisions/2026-10-21_array_sort_permutes_in_place.md)).
- **Wall:** 847 s for four analyses (3.98 GB peak), final run.

| Run | Refusals | Rounds |
|---|---|---|
| `c62df5aea` | 2055 | 3999, 2229, 2059, 2055 |
| result relation only | 2027 | 3977, 2207, 2034, 2027 |
| relation and the call as a write of copies (final) | 2027 | 3977, 2207, 2034, 2027 |

- 33 refusals cleared, 5 appeared. Cleared: 19 call and 7 channel escapes
  of borrowed `ref` values (`DriverRung2ExecuteInstalledRequest` and the
  routines whose frames the one unmodelled sort made unsupported), 6
  moves. New: 4 "value boundary arguments must use a named variable", 1
  `ref` escape.
- How the root was found: a census-mode run that records, for each formal
  given a conservative call escape, the last site that made its frame
  incomplete; 139 of 156 sites were an argument into a callee formal whose
  analysis was unsupported, and the only call that made a frame
  unsupported on its own was that sort.
