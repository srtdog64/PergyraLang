# 2026-11-16: getter loan correction; element ownership in the hand-off table

Decided by the Claude root lane from the S0 attribution
([evidence](../evidence/2026-11-16_handoff_disagreement_attribution.md)).
The user can overturn it.

## 1. Correction to the getter loan decision

The [getter loan decision](2026-11-16_getter_loan_extends_to_reused_locals.md)
says it widens 2026-10-19 "which lent formals only". That is wrong.

- [2026-10-19_local_getter_lends_until_handed_on](2026-10-19_local_getter_lends_until_handed_on.md)
  already lends a local.
- Native applies it: 50 of the 105 getter-on-local sites of the canonical
  compiler input are loans.

What the decision adds is:
- the self-host implementing the same loan rule from the generated rule
  table (slice S5b);
- the loan conditions stated once in that table.

The rule itself stands as written. A conflict or an escape takes the copy
ladder.

## 2. Element ownership is an axis of the constructor hand-off

S0 found 24 hand-off rows where native places an owned-element `Clone` at
a constructor input (`aggregate_constructor_proves`), and the decision
table had no cell for them.

The table gains one axis, the element ownership a constructor input must
give its field:
- the field must own its elements (a deep-drop or owned-push field), and
  the argument's elements are borrowed or unknown: the input takes an
  owned-element `Clone`;
- otherwise the hand-off rules apply unchanged: a single use hands off;
  a later use copies (decision 2026-10-22) or is refused.

This follows from decision 2026-10-22, which copies only where a copy is
needed, and from fail-closed release: a record may deep-drop only
elements it owns.

## Consequences for the migration plan

The S0 evidence names the front end that changes for each cause:
- self-host:
  - owned-element constructor inputs it leaves `retired` (33 rows);
  - one binding fed to two inputs (6 rows);
  - a handed-off local that is read later (5 rows);
  - an unnamed struct literal input (1 row);
- both: the element-ownership axis (24 rows).

Two caveats:
- The self-host MIR carries no compiler `Clone`; native places 449 in 274
  programs. So slice S2 compares the copy plan as well as the hand-off
  rows.
- 9 of the 24 rows come from a callee-side constructor `Clone` that turns
  the callee formal from Return into Read on the next analysis. Slice S4,
  as first written, does not change them.
