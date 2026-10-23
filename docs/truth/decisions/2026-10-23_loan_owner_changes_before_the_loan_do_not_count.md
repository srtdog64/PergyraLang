# 2026-10-23: a change to a loan's owner before the loan is made does not count

Decided by: the Claude lane. The user can overturn it.

## Rule

The proof that a lent argument's owner stays unchanged walked every
assignment and call of the routine, wherever it stood. It now skips one
that runs before the lending call every time the routine runs: it starts
earlier in the source, it does not contain the lending call, and the
chain of nodes enclosing it reaches the routine's body through blocks,
branches (`if`, `match`), `with` scopes and plain statements or
expressions only. A loop, a `defer`, an `async`, `parallel` or intent
body, a lambda, or a node the program-point index does not record keeps
the change in the proof. A returned loan's proof is unchanged.

Order is by source position. The parser leaves assignments without one,
so an assignment is placed at its target's root identifier, where it
starts; a node with no position at all, as a desugaring makes, is never
placed before the call. A first version compared the assignment's own
position, 0, and so skipped an indexed write made after the loan: it
admitted `loan_owner_written_by_indexed_assignment_after_loan_invalid`,
which then printed 7 (the owner's element read through the loan's write)
instead of 1.

## Why it holds

Pergyra has no jump other than loops, `break`, `continue` and `return`.
Code that starts before the lending call, under no enclosing loop,
deferred or concurrent body or closure, has finished when the call runs,
and does not run again in that invocation; the loan lives only from the
call on. A change inside a loop that also holds the call runs again under
the previous pass's loan (`loan_owner_assigned_in_loop_before_loan_invalid`),
and an assignment whose value makes the loan writes after it
(`loan_owner_assigned_from_its_own_loan_invalid`).

## Why

On the canonical driver input three moves were refused for loans whose
owner was written only before the lending call
(`GenerateCUnitFromAdmittedSemanticArtifactObserved`, `CompileSourceDirTextVerified`).
`SemanticAstExpressionSurfaceFactsWithGraph` copies its graph, assigns two
of its arena fields, then takes views of it in a loop; its loans now pass
those assignments, but still fail at the constructor that returns the
copy after the loop (the copy leaves while the proof cannot see that the
views are dead), so its eight refusals stay.

Replaces: nothing.
