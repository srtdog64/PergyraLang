# 2026-10-23: a by-value Array formal handed to an `inout` formal that may reallocate its buffer

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`b66b90a30` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_by_value_array_handed_to_inout_formal_moves.md);
open question: [element writes](../decisions/2026-10-23_open_by_value_array_element_write_through_inout.md).

## The hole

On `b66b90a30`, `Pass(rows: Array<Int>)` handing `rows` to
`Grow(inout rows: Array<Int>)`, which pushes, was admitted with no copy,
and `Main` read `rows[0]` after `Pass(rows)`: ASan heap-use-after-free on
C. The push reallocated the buffer `Main`'s descriptor still named. The
same held two calls deep and on a recursive cycle back through the
by-value formal.

## Canonical census

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `b66b90a30` | 198 | 156 |
| every by-value-to-`inout` hand-off moves (first version) | 263 | 221 |
| only when the `inout` formal may not keep its buffer | 198 | 156 |

The first version's 65 added refusals were all `Array argument call
requires preserved array storage` in the self-hosted parser, whose
expression entry points take the cursor by value. The rule kept refuses
nothing more and removes nothing: the canonical input hands no by-value
Array to an `inout` formal that pushes. The census took 1056 s and 3978388
KiB at peak beside another lane's canonical run on the same machine.

## Gates

`function-param-flow-summary` gate passed (196 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units passed, 0 failed (the target exits 2 at its
self-host driver step, as on main).

## Fixtures (799 compared against `b66b90a30`)

Errors, landing analyses and `Clone` counts per routine changed only in new
fixtures. A copy now lands in `Main` where a grown Array was read freed
(`by_value_array_grown_by_inout_callee_landed_valid`, `..._two_calls_deep_landed_valid`,
`..._grown_through_recursive_hand_off_landed_valid`; ASan clean, outputs
2, 2 and 6 on C and LLVM), and an Array of maps that `Clone` cannot copy
is refused (`by_value_array_of_maps_grown_by_inout_callee_invalid`).
Unchanged and new: the parser's cursor shape
(`by_value_cursor_set_through_recursive_parser_valid`, `[a]3`), a pop
(`by_value_array_popped_by_inout_callee_valid`), a last use, a by-value and
a generic reader.

## Mutations (227 fixtures)

| Mutation | Caught by |
|---|---|
| no by-value formal moves through an `inout` hand-off | `..._grown_by_inout_callee_landed_valid` |
| an `inout` formal of the routine moves too | `inout_defers_handed_to_recursive_emitter_valid` refused |
| any callee formal that may not keep its buffer | `by_value_array_handed_to_generic_reader_valid` |
| every `inout` hand-off moves (first version) | `by_value_cursor_set_through_recursive_parser_valid` |
| a push does not mark the buffer | `..._grown_by_inout_callee_landed_valid` |
| a hand-off does not pass the mark on | `..._grown_two_calls_deep_landed_valid` |
| `ArraySet`, `ArrayLength` or `ArrayPop` does not keep the buffer | the cursor and pop fixtures |
| a missing callee formal is proved | not caught |
| an unsupported entry keeps its buffer | not caught |
| a changed buffer bit does not mark the entry's readers | not caught |

The last three stay as guards: each fails closed on a fact the walk could
not produce, and a reader must be walked again when the bit it read
changes. `ArraySort` and `ArrayReverse` are left out of the kept list: no
fixture can tell them apart, as a permutation through an `inout` callee
already moves the formal.
