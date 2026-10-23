# 2026-10-23: canonical census after a loan ends with the statement holding its locals

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`2e068ebdd` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_loan_ends_with_the_statement_that_holds_its_locals.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `2e068ebdd` | 255 | 213 |
| changes after the statement that ends the loan skipped | 247 | 205 |

Removed, none added: the eight refusals of
`SemanticAstExpressionSurfaceFactsWithGraph`, `'sealed' was moved into
'SemanticExpressionGraphViewAt'` at 386:17, 390:17 and 394:17 (the views),
400:21, 421:21 and 440:21 (spelling checks in the loop), 459:54 (the
arena count after it) and 469:65 (the constructor that returns the copy).

With the span taken from the proofs of the locals the loan reached (the
owner's proof keeps it), the census gave 248: `'analysis' was moved into
'SelfMirProgramDomainFactsFromReadyArtifact'` in
`CompileSourceDirTextVerified` (177:19) came back, the refusal `2e068ebdd`
removed through those proofs. The analyzing call at 166:9 passes an enum
case, whose root names no local, and the projection's proof stopped there
(loan-proof code 3). No fixture held that shape:
`loan_carrier_unchanged_by_call_before_loan_valid` now does.

The same census run back to back took 937 s and 3977824 KiB at peak on
`2e068ebdd`, and 930 s and 3977952 KiB with the rule. The rule walks the
lending call's enclosing statements once per loan and each claim's chain
once; the difference is within the 930 to 1036 s spread of this
session's runs.

## Gates

`function-param-flow-summary` gate passed (191 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (749 compared against `2e068ebdd`, none changed)

New: `loan_scope_ended_before_owner_moved_valid` (refused at 36:33 and
40:26 on `2e068ebdd`; prints 6 on C and LLVM now) and four negatives
refused the same on both trees. Three keep the view past the loop in a
local, an Array, or an Array an `inout` callee pushes into; the carrier's
own proof refuses them, since the assignment, push or call that receives
the view changes it after the loan. The fourth reads a view no local holds
after its owner moved in the same statement. A view read by a detached
`async` body or a `defer` in the loop was refused before the loan proof
(capture of a local, a `defer` in dynamic `while` control) on both trees.
`loan_carrier_unchanged_by_call_before_loan_valid` lands no copy on both
trees and one `Clone` in `Compile` on `42b1c8a8c`.

## Mutations (129 loan, getter and lend fixtures)

| Mutation | Caught by |
|---|---|
| the lent-argument proof gets no span | `loan_owner_assigned_before_loan_valid` refused |
| the proofs of the loan's locals get no span | `loan_carrier_unchanged_by_call_before_loan_valid` lands a `Clone` |
| any enclosing node counts as straight-line | `loan_owner_assigned_in_loop_before_loan_invalid` admitted |
| a claim that contains the call runs before it | `loan_owner_assigned_from_its_own_loan_invalid` admitted |
| source order before the call is not compared | `readfile_result_borrowed_identity_landed_valid` refused (14:27) |
| the assignment's target position is not used | `loan_owner_assigned_before_loan_valid` refused |
| a local the loan reached need not be inside the statement | `loan_owner_written_by_indexed_assignment_after_loan_invalid` admitted |
| the statement may be an expression | `loan_read_in_the_statement_that_moves_its_owner_invalid` admitted |
| a claim inside the statement counts as after it | `loan_owner_assigned_after_loan_invalid` admitted |
| no `while` inside the statement | `loan_scope_ended_before_owner_moved_valid` refused |
| the statement may be a local's own `let` | `loan_owner_written_by_indexed_assignment_after_loan_invalid` admitted |
| the chain need not reach the routine body | not caught |
| a node without a position is ordered | not caught |
| the statement need not run once | not caught |
| any code may stand inside the statement | not caught |
| the statement's position is not compared | not caught |

The last five stay as guards. Every claim the proof walks has a recorded
chain to the body, and every assignment's target root has a position. A
statement under a loop ends its loan before code after it in that pass,
but one in a concurrent body does not. A detached `async` capture and a
`defer` in a dynamic loop are refused by other rules today, and a claim
before the statement that is not in it finished before it unless a closure
runs it later.
