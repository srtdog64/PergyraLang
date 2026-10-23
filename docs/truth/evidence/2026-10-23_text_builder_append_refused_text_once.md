# 2026-10-23: a refused TextBuilderAppend text is reported once

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`610e86a1e` plus the change measured (local, not pushed).

## Change

`TextBuilderAppend` checked its text and, when the check returned no type,
reported `TextBuilderAppend text must be String, got '<unknown>'` at the
same node. When the text's own check had already reported a refusal (on the
canonical input, `String 'prologue' borrows an Array<String> element that
was released`), that was a second report of one failure. The append now
returns as `Cancel` does when its operand's check reported anything and
gave no type (`type_checker_builtins_text_builder.c`). A text of another
type is still refused as before.

## Canonical census

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `610e86a1e` | 192 | 150 |
| a refused text is reported once | 189 | 147 |

Removed: the three `TextBuilderAppend` reports in
`EmitFunctionWithSpecialization`, each behind a released-element refusal at
the same node, which stays. Added: none. The census took 933 s and 3978860
KiB at peak.

## Gates

`function-param-flow-summary` gate passed (197 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

## Fixtures (803 compared against `610e86a1e`)

Errors changed only in `text_builder_append_of_released_element_invalid`,
new: two errors at 10:31 on `610e86a1e`, the released-element refusal alone
now. Keeping the second report (the mutation) is caught by that fixture.
