# 2026-11-16: a getter loan extends to a local that is used again

This answers [the OPEN question](2026-11-16_open_getter_loan_on_reused_local.md).
The user asked for the rule that is better in the long run. The Claude
root lane recommended it from a comparison of Rust (two-phase borrows,
NLL, Polonius), Swift (`_read`/`_modify` accessors and exclusivity), Hylo
(projections), Mojo (origins) and Vale (regions). That comparison is
section 5 of the [hand-off rule study](../../audits/unified_handoff_rule_study_2026-11-16.md);
the generic return study is
[here](../../audits/generic_array_return_study_2026-11-16.md).

## Why a loan

The languages that avoid both a copy and a runtime check all lend. To lend,
a compiler needs two facts:
- which argument the result borrows;
- how long the loan lives.

Rust, Mojo and Swift make the user declare these. This language already
infers both:
- the `Return` effect class of the one hand-off rule table
  ([decision](2026-11-16_one_handoff_rule_for_both_front_ends.md));
- the later-use predicate of that table.

## The rule

- A call whose effect class on an owner argument is only `Return` lends
  that owner, a formal or a local, when the caller uses the owner again.
  This widens decision 2026-10-19, which lent formals only.
- The loaned result is second class. It may be read, passed to a `Read`
  formal, or passed to a `Return` formal (a derived loan). Anything else is
  a loan escape: changing it, passing it to a moving formal or a
  constructor, storing it beyond the owner's scope, returning it, or
  capturing it in a detached context.
- The loan lives while the result has a later use. The predicate is
  defined once in the rule `.def`.
- While the loan lives, growing, handing off, constructing from,
  `inout`-passing, reassigning or dropping the owner conflicts with it.
- A conflict or an escape takes the existing copy ladder (decisions
  2026-10-20 and 2026-10-24). When no whole copy exists, the program is
  refused.
- The owner releases its storage as before. The result row has origin
  `lent` and releases nothing.
- No annotation is added. There is no per-path (Polonius-style)
  refinement.

## Order

This lands after the two front ends agree on the hand-off rule (slice S5
of the migration plan), as slice S5b:
- native's lent proof widens to local owners;
- the self-host implements the same rule from the generated table;
- MIR validation checks `lent` rows (S6).

Slice S0 counts the local getter sites and the copies landed today, to
measure the gain.
