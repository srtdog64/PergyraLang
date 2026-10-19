# 2026-10-19: a getter lends a local too, until a call hands it on

Decided by: the Claude lane, as the next step the
[by-value getter decision](2026-10-19_by_value_getter_lends_when_used_again.md)
named. The user can overturn it.

## Rule

A read-and-return call lends its by-value aggregate argument whether the
argument is a formal or a local of the calling routine. A later use is
admitted when the loan proof holds:

- a loan of a local stays in the routine; a loan of a formal stays in it
  or leaves only as the formal's own return;
- nothing changes the owner while the loan lives. Besides writes, moves
  and `inout` hand-offs, a read-and-return call on a **local** whose
  result leaves the routine hands the local on, so it changes the owner
  of every other loan of that local.

## Why

The earlier rule kept locals moving because a last read-and-return call
can hand a local to a result that outlives the routine while an earlier
loan of the same local is still read
(`by_value_getter_lend_local_transfer_invalid`). The owner scan now treats
that call as a change, so the earlier loan is refused and the transfer
stays sound. The canonical census had 647 move refusals of getters on
locals at `bc2aefb30`.

Replaces: the "Why a formal and not a local" part of
[2026-10-19_by_value_getter_lends_when_used_again](2026-10-19_by_value_getter_lends_when_used_again.md);
its other rules stand.
