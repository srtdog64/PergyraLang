# 2026-10-23: a by-value argument beside an `inout` one naming the same variable

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`0f0ecbbe4` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_by_value_argument_beside_its_inout_is_copied.md).

## The hole

On `b66b90a30` and `0f0ecbbe4`, `Both(rows, rows)` with
`Both(inout a: Array<Int>, b: Array<Int>)` was admitted with no copy. A
push through `a` reallocated the buffer `b` still read (ASan
heap-use-after-free on C), and with the by-value formal first a set
through `a` changed what `b` read (printed 55). The type checker refused
the same variable at two `inout` formals, or at an `inout` and a `ref`
formal, but not at an `inout` and a by-value one.

## Canonical census

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `0f0ecbbe4` | 194 | 152 |
| a by-value argument beside its `inout` one is copied | 194 | 152 |

No refusal added or removed: the canonical input passes no variable to an
`inout` and a by-value formal of one call. The census took 1061 s and
3978340 KiB at peak beside other runs on the same machine.

## Gates

`function-param-flow-summary` gate passed (203 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

## Fixtures (802 compared against `0f0ecbbe4`)

Errors, landing analyses and `Clone` counts per routine changed only in new
fixtures: a copy lands in `Main` at the by-value argument, beside and
before its `inout` one (outputs 13 and 15 on C and LLVM, ASan clean), and
an Array of maps is refused (`by_value_maps_beside_their_inout_invalid`).
Unchanged and new: an Int and a String beside their `inout` arguments
(`54a!a`) and one Array to two reading by-value formals (`32`).

## Mutations (227 fixtures)

| Mutation | Caught by |
|---|---|
| no by-value argument beside an `inout` one is refused | `by_value_argument_beside_its_inout_landed_valid` |
| no copy is selected | the same, refused |
| any type, not only one that shares storage | `scalar_and_string_beside_their_inout_valid` |
| two by-value arguments count too | `by_value_array_to_two_reading_formals_valid` |
| a copy is selected whatever `Clone` copies | `by_value_maps_beside_their_inout_invalid` |
