# 2026-10-20: a loan is copied where it is made when its exit cannot be

Decided by: the Claude lane. It extends the
[escaping loan rule](2026-10-20_escaping_loan_copies_where_it_fails.md)
(copy where the proof fails; the user asked for the long-term choice).
The user can overturn it.

## Rule

A getter loan whose proof fails is copied at the first of these places a
copy can stand in:

1. where it leaves, when it leaves and `Clone` copies the value that takes
   it out whole (unchanged);
2. otherwise where it is made: the getter call becomes `Clone(call)`, when
   `Clone` copies the call's result whole and the call changes no storage
   it shares with its caller.

The second place was used only when the loan's owner changed. It now also
serves a loan that leaves where no copy can stand in (a receiver, an
`inout` argument, or a value `Clone` copies only by value), and a loan
whose proof failed for any other reason. A copy of the whole result makes
the loan go away, so nothing of the owner is shared.

When neither place can take a copy, the argument is still moved.

## Why

- On the canonical driver input, 62 refused uses came from one loan of a
  local: the getter's facts left inside the routine's analysis result,
  which `Clone` copies only by value, so no copy could stand at that exit.
  The facts themselves are a struct of scalar and String Arrays, which
  `Clone` copies whole.
- Copying at creation costs the whole result once instead of only the part
  that leaves. That is the price of a loan no proof admits and no exit copy
  can fix; refusing would leave the unchanged canonical program refused.

## Cost on the record

As with the other landings, the copy stays allocated until the process
ends while emitted code frees no String.

Replaces: nothing. It widens step 2 of the escaping loan rule.
