# OPEN 2026-11-16: a getter loan on a local that is used again

Status: OPEN. The user asked for the better rule, drawn from how other
languages handle it, before deciding.

## Question

A local is passed to a getter or a member-returning accessor, and the
local is used again after the call.

Today this is a move (decision 2026-10-19, where present, and 2026-10-22),
so a whole `Clone` lands at the call or the program is refused. The
alternative is a loan of the local that lasts while the result lives, with
no move during the loan.

## Options so far

- **(a)** Keep the move: a `Clone` lands or the program is refused. This is
  simple, but it copies.
- **(b)** Extend the loan to locals, with no transfer while a loan is live.
  There are fewer copies, but the rule grows.

The Claude root lane is studying Rust two-phase borrows and Polonius,
Swift `_read`/`_modify` accessors and exclusivity, Hylo projections, Mojo
origins and Vale regions. The study is section 5 of research proposal A.
It will propose a rule that needs no new annotation, fails closed, and is
checkable by both front ends from the one hand-off rule table
([decision](2026-11-16_one_handoff_rule_for_both_front_ends.md)).
