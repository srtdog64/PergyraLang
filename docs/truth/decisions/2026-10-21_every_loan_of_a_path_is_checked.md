# 2026-10-21: every loan of a path is checked, and a loan handed to a grower leaves

Decided by: the Claude lane. It tightens the
[getter lend rule](2026-10-19_by_value_getter_lends_when_used_again.md) and
the loan proof behind it. The user can overturn it.

## Rule

1. **Each loan is its own row.** When a getter lends a path that an earlier
   getter already lent, the move owner records the second loan as well. A
   later use of the path needs every loan of it admitted, and each refused
   loan selects its copy in the same analysis. Before, the first loan's row
   stood for every later one, so the second loan was never checked.
2. **A loan handed to a grower leaves.** A loan that reaches a formal whose
   storage proof fails (the callee grows or changes what it receives)
   leaves the routine through that call, whether the loan is the argument
   itself or a value built from it. The loan proof fails and the copy lands
   where it leaves, or where it is made when no copy can stand there.
   Before, only a local holding the loan was checked against such a call;
   a loan passed straight on was not, because the move owner sees only
   access paths. Parameter-flow summaries are unchanged: a formal's storage
   handed to a grower is already its storage proof's to refuse.

## Why

On `main` (`7cd004fab`) the native checker admits all of these, and C with
ASan reports heap-use-after-free:

- `Fill(Part(outer))`, Fill pushing onto what it receives, then a use of
  `outer`;
- two `Part(outer)` loans, pushes onto the second, then uses of the first
  loan and of `outer`, for a local and for a by-value formal;
- a loan built into `Outer(part, 1)` that is handed to such a Fill, or that
  is pushed onto itself.

The fixtures named `by_value_getter_*second_lend*`, `*two_lends*`,
`*lend_result_passed*` and `*lend*constructor*` hold each shape, with a
copy landed where one can stand and a refusal where `Clone` copies only by
value.

## Not landed

Letting a local that holds a loan go into a constructor or `Clone` without
counting as a change (the walk follows the built value) cut the first
analysis's refusals on the canonical input from 3959 to 3625, but the
final one rose from 2074 to 2085: copies that landed early stopped
landing within the three landing analyses
([census record](../evidence/2026-10-21_native_census_every_loan_checked.md)).

Replaces: nothing.
