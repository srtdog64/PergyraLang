# 2026-10-20: a call result that holds a formal's storage is a path of it

Decided by: the Claude lane, as a safety repair. The refusals it adds are
not open to choice; the precision it keeps can be overturned by the user.

## What was unsound

Main (`7cd004fab`) and this branch admitted programs that free storage a
caller still names (ASan heap-use-after-free on C):

- a by-value formal's getter result grown in the callee:
  `let part = Part(outer); ArrayPush(part.items, 9)`;
- the same result handed to a routine that grows it (`Fill(Part(outer))`),
  assigned to an existing local (`part = Part(outer)`), or returned from a
  local by another getter that a caller then grows;
- an Option payload of a formal, unwrapped and grown
  (`UnwrapOption(box.inner)`), and a formal's field returned in `Some` and
  grown after the caller unwraps it;
- a local's loan grown while the local is used again:
  `let part = Part(outer); ArrayPush(part.items, 9)`, then `outer.inner`.

Two proofs missed these:

1. The storage proof of a formal followed its paths and the `let` aliases
   of paths. A call result that holds the formal's storage was neither, so
   a store through it was not seen.
2. The loan proof checked that nothing changes the loan's owner, not that
   nothing changes the loan. The loan shares the owner's storage.

## Rule

1. **Storage proof.** A call's result is a path of the formal when the
   callee may return an argument that is a path of it: a routine whose
   formal's summary has the RETURN bit, or a builtin or constructor whose
   result keeps or depends on the argument. `Clone`'s result is new
   storage, and a String, a copy-only value or an Option of one holds
   none, as a result or as an argument. The
   result is proved as an alias is: it may be read, handed to a routine
   that only reads or returns it, or returned when it holds the formal
   itself, which the summary's RETURN bit carries as for a returned path.
   Because the result is followed, a constructor's argument and a
   depending builtin's argument are proved where the result goes instead
   of being refused at the call. A local that takes such a result, as in
   `let part = Part(outer)`, is that result, so returning it or a
   constructor of it is a getter's return; a local that takes a path is
   still an alias, which the summary does not follow.
2. **Loan proof.** Every local the loan's backing reached must stay as
   unchanged as the owner must. A String or copy-only local holds none of
   the owner's storage and is not checked.

When the loan proof fails, the existing landing copies the loan where it
is made if `Clone` copies the result whole
([decision](2026-10-20_loan_copied_where_made_when_its_exit_cannot_be.md)).

## Fixtures

`default_aggregate_move_shared_field_return_invalid` was refused because
its callee returned a constructor of the argument's field. That callee now
lends; the caller grows the loan, so the loan is copied where it is made
(`default_aggregate_move_shared_field_return_landed_valid`). The negative
keeps a nested Array field, which `Clone` copies only by value.

Replaces: nothing.
