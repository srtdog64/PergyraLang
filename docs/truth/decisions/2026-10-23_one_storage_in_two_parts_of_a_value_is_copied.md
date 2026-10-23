# 2026-10-23: one storage in two parts of a value is copied at the earlier part

Decided by: the Claude lane. It applies
[a moved argument used again is copied](2026-10-22_moved_argument_used_again_is_copied.md)
to the parts of one value. The user can overturn it.

## Rule

When two parts of a value a routine builds, the arguments of a nominal
constructor or the elements of an Array or tuple literal, name the same
storage, directly or through a local copy of it, the move walk refuses
the value and selects the earlier part for a copy where the value takes it
(`move_walk_copy_parts_distinct`, `default_aggregate_copy_owner.c`). The
next analysis passes `Clone` of that part when `Clone` copies it whole;
otherwise the refusal stays (`one_storage_in_two_map_parts_invalid`).
Parts that hold no storage (Ints, Strings) and parts built from distinct
storage are left as they are (`two_parts_of_distinct_storage_valid`).

## Why it holds

A value whose two parts share one buffer carries the sharing wherever it
goes, and no holder of the value can see it: a caller that grows one part
reallocates the buffer the other still names. Within the routine that
builds it, the copy rows can follow a change through either part; once the
value is returned or stored, nothing does. A copy at the earlier part
gives each part its own storage, and the later part keeps the original.

## Why

On the merge of main into the branch (`048633974`), and on main's own
build, `Make()` returning `Pair(rows, rows)` was admitted, and a caller
that pushed into `pair.first` and read `pair.second` read freed storage
(ASan heap-use-after-free:
`one_storage_in_two_constructor_parts_returned_landed_valid`; through a
local alias and in an Array literal too). The aggregate storage transfer
smoke names these shapes as unsafe negatives (`duplicate_array_negative`,
`array_alias_negative`, `map_alias_negative`), and main fails it there
today.

Replaces: nothing.
