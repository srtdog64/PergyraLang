# 2026-10-22: a Clone of a lent owner reads it and changes nothing

Decided by: the Claude lane. It narrows the owner walk of
[a by-value getter lends a formal used again](2026-10-19_by_value_getter_lends_when_used_again.md).
The user can overturn it.

## Rule

A loan holds only while nothing changes its owner. The owner walk counts
a call that receives the owner, or a path of it, as a change unless the
callee is known to leave it alone. A builtin `Clone` of the owner now
counts as a read: it copies its argument and keeps nothing of it. Where
`Clone` copies only by value, its result shares the owner's storage, and
a change through that result is still refused by the local copy rule
([decision](2026-10-22_copy_changed_under_a_loan_is_copied.md)).

## Why

The copies landed for a moved argument used again
([decision](2026-10-22_moved_argument_used_again_is_copied.md)) put
`Clone(env)` into routines that also lend `env` to a call before it, so
every such landing failed the earlier loan: on the canonical driver
input 19 failed loans, 7 in `EmitLet` and 4 in `EmitStmtList`, all with a
lent `env` handed to `CodegenTypeEnvStateAppendTypedValueBinding`.

## Measured and not taken

`CodegenTypeEnvStateAppendTypedValueBinding` lends its by-value `env`
although it returns nothing, because the syntactic summary marks `env`
RETURN and its backing frame cannot clear it: for a routine with an
`inout` or `own` formal, RETURN also stands for a String element that
leaves through the output, which the frame does not track. Clearing it
when the frame was complete and the result scalar admitted
`named_string_field_forward_project_drop_invalid`, a real use-after-free
(an owned element kept in an `inout` output, then dropped), so it was
reverted.

Fixtures: `by_value_lend_then_owner_cloned_valid` (refused before,
prints `a41` now), `by_value_lend_then_shallow_clone_grown_invalid`
(a by-value Clone of the lent owner grown through a path: refused at
26:40 before and after).

Replaces: nothing.
