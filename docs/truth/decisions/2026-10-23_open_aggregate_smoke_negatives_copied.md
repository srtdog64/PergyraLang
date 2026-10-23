# 2026-10-23 (OPEN): the aggregate storage smoke's negatives that now land a copy

Raised by: the Claude lane. Needs the user's decision, or the GPT root
lane's, as the smoke's owner.

## Question

`tests/aggregate_storage_transfer_smoke.sh` compiles six unsafe programs
and requires the native pipeline to refuse each. Four of them are now
admitted with a compiler-inserted `Clone`, under the copy rules decided
since the smoke was written
([moved argument used again](2026-10-22_moved_argument_used_again_is_copied.md),
[local copies](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md),
[one storage in two parts](2026-10-23_one_storage_in_two_parts_of_a_value_is_copied.md)):

| Program | Now |
|---|---|
| `duplicate_array_negative` | copy at the first part; runs, ASan clean |
| `array_alias_negative` | copy at the first part; runs, ASan clean |
| `push_pop_duplicate_negative` | copy at the first part; runs, ASan clean |
| `post_field_push_negative` | copy where the Pair takes `first`; runs, ASan clean |
| `map_alias_negative` | refused: `Clone` does not copy maps whole |
| `post_field_return_negative` | refused, but at the caller's result line, not the lines the smoke names |

## Options

1. Keep the copy rules and turn the four into positives with their
   outputs (the smoke's stated hazard, shared storage, no longer reaches
   run time).
2. Keep them negatives: a copy the source did not write is refused for
   aggregates, and the copy rules change.

Main fails the same smoke today on its own build (12 of its checks); the
branch fails 10 after the rule above.
