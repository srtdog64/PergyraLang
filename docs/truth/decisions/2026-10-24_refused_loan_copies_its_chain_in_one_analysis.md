# 2026-10-24: a refused loan copies its chain in one analysis

Decided by: the Claude lane, as piece 1 of the
[bounded landing planner experiment](../../agent_work_directives/bounded_landing_planner_experiment_2026-10-24.md),
which the user asked for on 2026-10-24. It refines where
[an escaping loan is copied](2026-10-20_escaping_loan_copies_where_it_fails.md);
the places a copy may stand are unchanged.

## Rule

When the lent proof (`function_param_flow_lent_argument_admitted`) finds
a loan leaving through an expression a Clone copies whole, it walks the
loan again with that expression read as a copy already made: the value
reads its source as a Clone would, holds no origin, and goes nowhere its
source goes. It repeats up to 16 times. A store this analysis already
selected to land as a copy (`semantic_formal_store_landing_selected`) is
read the same way, and that copy stays the store owner's.

- When the last walk finds the loan staying, the move owner selects every
  copy it read as made, and the analysis still refuses.
- When a walk leaves where no copy can stand, the loan is copied where it
  is made, as before, and the chain is dropped.
- When the loan stays but its owner still changes, the getter call is
  copied if a copy can stand there; otherwise the lent argument where the
  call takes it ([rule](2026-10-24_refused_loan_copies_its_argument_when_result_cannot_be.md));
  otherwise the chain is copied and the next analysis judges the owner
  with every copy in place.

## Why

The round driver found one place per analysis: a loan leaving through
three places took three reloads of the whole program. On the canonical
census input the rule leaves the verdict unchanged (116 final refusals,
75 outside `direct_mir_*`) with 4 analyses instead of 10 and 1156 s
instead of 2734 s ([record](../evidence/2026-10-24_landing_planner_chain_walk_census.md)).
The store case also removes a copy the round driver made twice: a loan
pushed into an inout Array landed as `ArrayPushOwnedString(out,
Clone(first))`.

## Falsifiers

- `by_value_getter_lend_kept_twice_landed_valid`: a loan pushed into two
  inout Arrays lands as two owned pushes and no Clone, in one landing
  analysis. Without the store case each push gets a Clone as well.
- `by_value_getter_lend_kept_then_owned_landed_valid`: a loan pushed and
  then handed to an `own` formal lands as one Clone of the getter call in
  one landing analysis. The round driver copied the push and then the
  getter, in two.
- `by_value_getter_lend_kept_landed_valid` now lands its push as the owned
  push alone.

## Not covered

The copy set is not minimal. A copy read as made is not yet the Clone the
next analysis sees, and one loan's walks do not read another loan's
copies, so on the census input 17 later links are copied that the round
driver did not need, while 9 of its copies are not made. The owner-change
case has no fixture of its own: on its own module the `ApplyPostfixFact`
loans need the whole program's summaries, so the census is its record
(119 final refusals without it, 116 with it). No fixture separates the
destination read of a copy either: dropping it changes no smoke result.
