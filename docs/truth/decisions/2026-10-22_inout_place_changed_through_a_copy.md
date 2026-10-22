# 2026-10-22: an inout place changed through a copy is refused at each exit

Decided by: the Claude lane. It extends
[the local copy rule](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md)
to the caller's side of an `inout` formal. The user can overturn it.

## Rule

An `inout` formal is the caller's own place, which the caller reads after
the call returns. When a copy of one of its paths changes in place, and
the path is not assigned again before an exit, that exit is refused: each
`return` and the end of the body. The next analysis copies the path where
it was copied (`Clone`) when `Clone` copies it whole. Otherwise the exit
stays refused until the copy is assigned back, as in
`facts.table = table`.

Not covered by this rule, and why:

- A by-value formal changed through a copy: the callee's summary already
  moves the caller's argument, so the caller copies it or passes a fresh
  value.
- An `own` formal: the caller does not read it again.
- A `ref` formal: a copy of its path into a binding is already refused.
- A `fail` inside a `transaction` ends the transaction, not the routine.

## Why

The branch at `eec39bb63` admits a routine that binds a local to an
`inout` formal's field, grows the local and returns. The caller then
reads its own argument's field, whose storage the growth reallocated: C
with ASan reports heap-use-after-free, for an `Array<Int>` and a
`HashMap<String, Int>` field alike. The local copy rule refused only a
later use inside the routine, and there was none.

Replaces: nothing.
