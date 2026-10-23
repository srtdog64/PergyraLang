# 2026-10-23: canonical census after an Array handed to two keeping formals of one call is copied

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`f1151f1cf` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_array_handed_to_two_keeping_formals_of_one_call_is_copied.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `f1151f1cf` | 194 | 152 |
| earlier same-call arguments are copied | 192 | 150 |

Removed: `Array argument call requires preserved array storage` at the two
`SemanticAstGenericSpecializationError` calls that pass one empty Array for
four formals. Added: none. The census took 1080 s and 3978796 KiB at peak
beside other runs on the same machine.

## Gates

`function-param-flow-summary` gate passed (206 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

## Fixtures (802 compared against `f1151f1cf`)

Errors, landing analyses and `Clone` counts per routine changed only in two
new fixtures, refused on `f1151f1cf` as needing preserved storage: one
Array to two keeping formals (a copy in `Build`, prints 1) and to three
(two copies in `Main`, prints 102), ASan clean on C. An Array an earlier
call keeps as well is refused the same on both trees, with no copy
selected.

## Mutations (230 fixtures)

| Mutation | Caught by |
|---|---|
| no earlier argument is selected | `array_handed_to_two_keeping_formals_landed_valid` refused |
| an earlier handoff of another call is copied too | `array_descriptor_kept_then_read_invalid` and `array_kept_by_earlier_call_and_two_formals_invalid` admitted |
| an earlier `inout` argument is copied too | a copy at an `inout` argument is refused as not a variable |
| an earlier result with no proof does not stop the selection | not caught |

The last stays: with such a result the refusal stands whatever is copied,
so selecting copies would only add a landing analysis that clears nothing.
