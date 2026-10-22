# 2026-10-23: canonical census after an inout argument is no escape of a ref input by itself

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`7d6e8504c` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_inout_callee_keeping_nothing_leaves_ref_input.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `7d6e8504c` | 3349, 342, 267, 265 | 223 |
| inout argument of a ref input | 3349, 335, 260, 258 | 216 |

Removed, none added: borrowed `ref` values `routines` and `admitted`
escaping through a call, 25 -> 18, along
`MirExpressionIdentityEpochRebindGraphForArtifact` (434:6),
`MirExpressionGraphFactsForArtifact[Observed]` (19:6, 262:27, 271:6,
278:9), `CanonicalizeMirJsonVerified` (61:13) and
`CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved` (285:22).
An unsound bound (no call escape for an `inout` formal whose summary
keeps nothing, any input) gave 260: two more moves and the same seven.

The same census run back to back took 977 s and 3977164 KiB at peak on
`7d6e8504c`, and 1012 s and 3977184 KiB with the rule, while another
lane's census ran beside them. The rule adds one parameter-mode test per
`inout` argument; the same census of one tree ran in 955 to 1016 s across
this session's runs, so the difference is within that spread.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (744 compared against `7d6e8504c`)

One changed: `ref_string_through_inout_reader_shallow_store_invalid` (new;
refused for `index` at 35:6, 51:6, 52:28 and 58:26 on `7d6e8504c`; now
only the shallow String store at 44:28). New and unchanged on both trees:
`ref_string_through_inout_keeper_invalid` (the `inout` callee hands the
Array to an `own` formal: the four escapes stay) and
`by_value_field_alias_written_by_inout_callee_landed_valid` (a Clone in
`Main`, `0 1` on C and LLVM).

## Mutations

| Mutation | Caught by |
|---|---|
| an `inout` argument escapes a `ref` input again | `ref_string_through_inout_reader_shallow_store_invalid` |
| every input but `own` is treated as `ref` | `by_value_field_alias_written_by_inout_callee_landed_valid` loses its Clone and prints `0 9` |

A first version also required the callee's summary for the formal to
have no release, call or channel escape, and the argument not to alias
the input's own destination. Turning off either changed no fixture: the
call already adds the formal's call and channel escape, an `inout`
formal cannot be released without an `own` boundary, and a local cannot
alias a `ref` input's storage without a refusal at the binding. Both
were removed.
