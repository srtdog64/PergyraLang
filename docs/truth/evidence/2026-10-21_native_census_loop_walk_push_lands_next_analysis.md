# 2026-10-21: canonical census after a push a later loop walk copies lands in the next analysis

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`89193c667` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Cause:** a loop body is walked again when its back edge changes resource
  state. On the first walk a String push qualified as a private push (the
  call's builtin kind is marked only after its arguments are checked), so it
  was sealed as a plain shallow push. On the next walk the call was marked,
  another `Array<String>` of the routine had a history by then, and the same
  call wanted an owned copy: it was rewritten to `ArrayPushOwnedString` in
  place, and its write-once effect receipt (shallow) no longer matched
  ("Could not seal Array<String> ownership transition receipt").
- **Change measured:** such a call is refused in that analysis and selected
  for an owned landing; the next analysis checks it as an owned push from its
  first walk.

| Run | Refusals by analysis | Final, in `direct_mir_*` |
|---|---|---|
| `89193c667` | 3387, 1251, 1121, 1112 | 315 |
| lands in the next analysis | 3483, 1204, 1079, 1070 | 306 |

No flow store failed. Outside `direct_mir_*` (797 before, 764 after) only
the receipt refusal changed, 33 -> 0; 9 more were in `direct_mir_*` routes.

A first attempt withdrew the shallow receipt at the rewrite instead. It gave
the same census, but in the reproducer the first walk had also recorded the
receiver as a non-owning String payload owner, so the rewritten push was
then refused as "An excluded String payload owner has no current
non-owning history proof".
