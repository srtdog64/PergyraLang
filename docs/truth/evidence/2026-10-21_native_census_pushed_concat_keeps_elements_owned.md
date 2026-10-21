# 2026-10-21: canonical census after a pushed Concat keeps elements owned

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`007649315` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** `ArrayPush(a, Concat(x, y))` keeps the elements of `a`
  owned, as a pushed builtin `Clone` does. Both backends emit a stdlib
  `Concat` call as `StringConcat`, which allocates the result; only the `+`
  operator can take the region route, at sites the region plan certified.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `007649315` | 3400, 1264, 1134, 1125 | 315 |
| pushed Concat keeps elements owned | 3396, 1260, 1130, 1121 | 315 |

No flow store failed. Outside `direct_mir_*` (810 before, 806 after) only
"an Array argument call requires preserved array storage" changed, 53 -> 49:
the 4 are calls after `SelfMirDeclarationAppendMethodContract`, whose
`SelfMirDeclarationCopyStringRange` pushes `Concat("", source[start + i])`.
