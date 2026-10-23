# 2026-10-23: canonical census after changes past a loan's last read stop counting

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`143ea2dbf` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_loan_owner_changes_after_the_last_read_do_not_count.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `143ea2dbf` | 233 | 191 |
| changes after the last read skipped | 233 | 191 |
| and a carrier handed on is no change | 224 | 182 |

Removed, none added: the eight refusals of `SelfMirProgramFactsFromArtifact`
(`'analysis.expression_surfaces.expression_graph' was moved into
'SelfMirProgramFactsEmpty'`) and the one of
`SemanticAstExpressionSurfaceFactsFromTypedArtifact` (`'artifact.expression_graphs'
was moved into 'SemanticExpressionGraphFactsFromAstRows'`). With the first
rule alone the owner's proof of `SelfMirProgramFactsFromArtifact` passed
and its facts' proof failed at the error return at 541:16 (loan-proof code
9); the second routine failed at code 9 at 104:12 on both trees until the
second rule. `SelfMirProgramDomainFactsFromReadyArtifact` now fails at the
`SelfMirProgramFacts` constructor at 70:9 (code 6) instead of at its error
return (code 9).

The same census run back to back took 1035 s and 3971816 KiB at peak on
`143ea2dbf`, and 1013 s and 3975752 KiB with the rules. A lent-argument
proof now lists the identifier rows of the loan's locals once per loan;
the difference is within the 930 to 1036 s spread of this session's runs.

## Two holes the gate found first

A first version listed reads by their own position. The flow gate refused
it: `loan_owner_written_by_indexed_assignment_after_loan_invalid` was
admitted (the write's target root, a read of the carrier, sat inside the
write and looked earlier than it), and
`named_string_field_lineage_same_call_retire_grow_invalid` reached a later
refusal (a read in the first argument of `Inspect` before the argument
that frees its owner). The rule now asks each read's whole statement to
finish before the change.

## Gates

`function-param-flow-summary` gate passed (197 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (767 compared against `143ea2dbf`)

Errors, landing analyses and `Clone` counts per routine changed only in the
two new positives: `loan_read_last_before_owner_changed_valid` and
`loan_carrier_handed_on_by_early_return_valid`, refused on `143ea2dbf` and
printing 9 and 2 on C and LLVM now. Five new negatives are refused the same
on both trees: a read after the change, a read in a loop that also changes
the owner, a read by a `defer`, a read in an argument before the argument
that changes the owner, and a carrier handed into an `inout` output.

## Mutations (142 loan, getter and lend fixtures)

| Mutation | Caught by |
|---|---|
| the lending call's statement need not finish first | `loan_owner_assigned_from_its_own_loan_invalid` and `loan_read_in_the_statement_that_moves_its_owner_invalid` admitted |
| a read need not start first | `readfile_result_borrowed_identity_landed_valid` refused (14:27) |
| a read and the change may meet in a loop | `loan_read_in_loop_that_changes_owner_invalid` admitted |
| reads are listed in a routine with a `defer` | `loan_read_by_defer_after_owner_changed_invalid` admitted |
| no change after the last read is skipped | `loan_read_last_before_owner_changed_valid` refused |
| a read, not its statement, must finish first | `loan_owner_written_by_indexed_assignment_after_loan_invalid`, `loan_read_in_the_statement_that_moves_its_owner_invalid` and `loan_read_by_call_after_its_owner_changed_in_its_arguments_invalid` admitted |
| a statement holding the change may hold the read | the same three admitted |
| a carrier handed on is still a change | `loan_carrier_handed_on_by_early_return_valid` refused |
| the span's claims generation is not compared | not caught |

The last stays as a guard: no fixture rebuilds the claims index between
the span and the proof, and if one did, a stale row would order the wrong
node.
