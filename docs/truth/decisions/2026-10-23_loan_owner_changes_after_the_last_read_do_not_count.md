# 2026-10-23: a change to a loan's owner after the loan's last read does not count

Decided by: the Claude lane. The user can overturn it.

## Rule

The proof that a lent argument's owner stays unchanged
([before the loan](2026-10-23_loan_owner_changes_before_the_loan_do_not_count.md),
[after its locals' statement](2026-10-23_loan_ends_with_the_statement_that_holds_its_locals.md))
now also skips a change that starts after the loan's last read. The
statement holding the lending call, and the statement holding each read of
a local the loan reached, must have finished when the change starts and
must not run again after it: the statement holds no part of the change,
the read starts first, and the statement and the change meet in a
statement that runs once. The reads are listed only when every row of the
routine runs where it is written (plain code, types, `while`); a routine
with a closure, `defer`, `async`, `parallel` or `for` body keeps every
change in the proof.

In the proofs of the locals that carry the loan, handing such a local on
through a read-and-return call whose result leaves the routine is no
change. That rule was written for an owner whose other loans the new
holder could change; a carrier owns no other loan, and its loan leaving
with it is the loan frame's to refuse unless the loan may leave.

## Why it holds

A read of a carrier yields a value that lives until its statement ends:
`Inspect(record.text, RetireThenGrow(rows))` reads `record` before the
argument that frees `rows`, but `Inspect` reads the text after
(`named_string_field_lineage_same_call_retire_grow_invalid`,
`loan_read_by_call_after_its_owner_changed_in_its_arguments_invalid`). A read
inside the change is the change's own: the target root of
`surfaces.graph.arena.texts[0] = "missing"` starts before the index node
that carries a position
(`loan_owner_written_by_indexed_assignment_after_loan_invalid`). A read in a
loop that also holds the change runs again after it
(`loan_read_in_loop_that_changes_owner_invalid`), and a `defer` reads at the
exit (`loan_read_by_defer_after_owner_changed_invalid`). A carrier handed
into an `inout` output takes the loan into it, and the owner's later change
is refused (`loan_carrier_handed_into_output_before_owner_changed_invalid`).

## Why

On the canonical driver input `SelfMirProgramFactsFromArtifact` makes
empty facts that lend its analysis's expression graph, reads them only in
two early error returns, and at the end hands the analysis to
`SelfMirProgramFactsFromReadyArtifact`, which keeps it. Its eight refusals
failed at that last call, after the facts' last read; once the owner's
proof held, the facts' own proof failed at the error returns, which hand
the facts into the returned result. `SemanticAstExpressionSurfaceFactsFromTypedArtifact`
hands the graph that lends its artifact into the returned
`SemanticAstExpressionSurfaceFactsWithGraph(...)`, its one refusal by the
second rule alone. Neither rule alone moved the census for the first.

Replaces: nothing.
