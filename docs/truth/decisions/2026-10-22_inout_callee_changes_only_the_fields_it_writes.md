# 2026-10-22: an inout callee changes only the fields it writes

Decided by: the Claude lane. It narrows
[an unwritten inout formal preserves the caller's storage](2026-10-22_unwritten_inout_formal_preserves_storage.md)
from the whole formal to each of its top-level fields. The user can
overturn it.

## Rule

An `inout` struct argument changes, for the local copy rule, only in the
top-level fields whose paths the callee's formal does not preserve. The
storage walk that proves a formal proves one field at a time over the
formal's settled program points: it ignores the paths of every other
field, and follows the aliases taken from the field's paths. Where the
whole formal is handed to another call, the field keeps whatever that
callee's formal does for the same field. A field verdict demanded again
while it is being computed (recursion) is unproved. Any argument that is
not a struct, and any callee whose formal is not proved through paths,
changes whole, as before.

## Why

The local copy rule counted every `inout` call as a change of the whole
argument, because no fact said which fields an `inout` callee writes. A
caller that copied one field of its state and then called a helper that
appends to another field was refused at the copy's next use. A census
that let no `inout` call change any copy (unsound, measurement only) took
the canonical driver input from 350 to 317 refusals, which bounds what
this rule can admit.

Replaces: nothing.
