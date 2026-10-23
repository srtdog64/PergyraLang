# 2026-10-23: a loan ends with the statement that holds every local it reached

Decided by: the Claude lane. The user can overturn it.

## Rule

The proof that a lent argument's owner stays unchanged
([before the loan](2026-10-23_loan_owner_changes_before_the_loan_do_not_count.md))
now also skips a change that starts after the statement that ends the
loan, outside that statement. That statement is the innermost one that

- is a statement of a block and runs once each time the routine runs
  (only blocks, branches, `with` scopes and plain statements enclose it);
- holds the lending call and the `let` of every local the loan frame
  marks as holding the loan, none of them the statement itself;
- holds only plain code, types, `while` loops, `break` and `continue`.

A loan with no such statement keeps every change in the proof: one held by
a formal, a `for` or `match` binding or a local declared outside, one made
in a `for` loop or under a `defer`, `async`, `parallel` or closure body.
An `if` statement has no source position from the parser, so a branch
does not end a loan; the statement around the `if` can.

## Why it holds

The loan frame marks every local the loan's storage reaches, and refuses
the loan when it leaves through a result, a formal output, a channel or a
call that keeps it. Those locals are declared inside the statement, so they
are out of scope once it ends; `break` and `continue` there reach only
loops inside it, and after a `return` nothing of the routine runs. A loan no
local holds lives until its statement ends, so an expression never ends a
loan (`loan_read_in_the_statement_that_moves_its_owner_invalid`). A view
kept past the loop in a local, an Array or through an `inout` callee
declared before it is still refused (`loan_kept_outside_its_loop_invalid`
and two more): the assignment, push or call that receives the view changes
that local after the loan, and its own proof counts it.

## Why

On the canonical driver input `SemanticAstExpressionSurfaceFactsWithGraph`
copies its graph, takes three views of the copy in each pass of a `while`
loop and returns a constructor that holds the copy after the loop. Its
eight refusals (`'sealed' was moved into 'SemanticExpressionGraphViewAt'`)
failed at that constructor, while every view was a local of the loop body
and out of scope there.

Replaces: nothing.
