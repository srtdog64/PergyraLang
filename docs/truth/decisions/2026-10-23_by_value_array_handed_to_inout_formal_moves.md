# 2026-10-23: a by-value Array formal handed to an `inout` formal that may not keep its buffer moves

Decided by: the Claude lane. It applies the
[default aggregate move](2026-10-18_default_aggregate_move.md) and its
[copy rule](2026-10-22_moved_argument_used_again_is_copied.md) to a case
the storage walk missed, as far as memory safety needs. Whether an element
write through such a formal is a move too is
[OPEN](2026-10-23_open_by_value_array_element_write_through_inout.md).
The user can overturn it.

## Rule

The storage walk now records for each Array formal whether its buffer may
be reallocated or freed through it (`array_buffer_unkept` in the formal's
summary entry): a stdlib call other than `ArrayLength`, `ArraySet` and
`ArrayPop` that takes the formal's storage in place (a push), or a hand-off
to a declared callee's formal that does not keep its buffer. The bit only
grows, so a cycle of hand-offs keeps the buffer unless a routine on it
pushes.

A by-value Array formal handed whole to an `inout` formal that does not
keep its buffer fails its descriptor proof: the routine moves its formal,
and a caller that uses its argument again takes a copy where it passes it,
or is refused when `Clone` does not copy the value whole
(`by_value_array_of_maps_grown_by_inout_callee_invalid`). An `inout` formal
of the routine handed on keeps the callee's verdict: it is its caller's
place, which the write-back reaches.

## Why it holds

A by-value Array formal shares its buffer with the caller's binding. An
`inout` callee's write-back updates only the routine's own descriptor; the
caller's still names the old buffer, which a push may have reallocated and
freed. A length read, an element set or a pop leaves the buffer where it
was (both backends: `pgy_array_set_*` writes the slot, `pgy_array_pop_*`
lowers the length), so the caller's descriptor stays valid.

## Why

On `b66b90a30` a by-value Array formal handed to an `inout` callee that
grows it was admitted without a copy, and the caller read freed storage
after the call (ASan heap-use-after-free:
`by_value_array_grown_by_inout_callee_landed_valid`, two calls deep, and on
a recursive cycle). A first version failed the proof for every such
`inout` hand-off. It added 65 refusals on the canonical driver input: the
self-hosted expression parser takes its cursor by value in
`ParseExprFact`, `ParseExprFactIn` and `ParseExpr`, hands it to `inout`
parsers that only set its element, and reads the advanced cursor after the
call (`by_value_cursor_set_through_recursive_parser_valid`).

Replaces: nothing.
