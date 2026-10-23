# 2026-10-23: canonical census after changes before a loan stop counting

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`42b1c8a8c` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_loan_owner_changes_before_the_loan_do_not_count.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `42b1c8a8c` | 258 | 216 |
| changes before the lending call skipped | 255 | 213 |

Removed, none added: `'semantic_analysis' was moved into
'CodegenIntentPolicyViewFromDirFactsOrDie'` (70:57, 74:19) and
`'analysis' was moved into 'SelfMirProgramDomainFactsFromReadyArtifact'`
(177:19). An unsound bound (no assignment fails the proof) gave 249.

A first version ordered claims by their own source position. The parser
leaves every assignment at 0:0, so an indexed assignment made after the
loan sorted before it: that version gave 246, and the nine extra
removals were `SemanticAstAssignmentTypeFactsContractReady`'s writes
through a loan of `valid` into storage `valid` shares. A probe of that
shape (`loan_owner_written_by_indexed_assignment_after_loan_invalid`)
compiled and printed 7 instead of 1. The rule now places an assignment
at its target's root identifier and orders no node without a position.

`SemanticAstExpressionSurfaceFactsWithGraph`'s eight refusals now fail at
loan-proof code 6, the `SemanticAstExpressionSurfaceFacts` constructor
that returns the copy (463:12), instead of at its earlier assignments.

The same census run back to back took 1036 s and 3976656 KiB at peak on
`42b1c8a8c`, and 1018 s and 3978488 KiB with the rule, while another
lane's census and probes ran beside them. The rule builds one parent map
per claims generation and walks one enclosing chain per claim it orders;
the difference is within the 955 to 1036 s spread of this session's runs.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (749 compared against `42b1c8a8c`)

New: `loan_owner_assigned_before_loan_valid` (refused at 33:33 and 37:45
on `42b1c8a8c`, `9` on C and LLVM now) and four negatives refused the
same on both trees: an assignment after the view in the loop, one in the
loop before the view, one whose value takes the view, and the indexed
write above. `string_element_kept_by_callee_landed_valid` now lands one
Clone in `Lines`, at the return: the element passed to `Join` stays a
loan of `parts`, pushed only before the loop.

## Mutations

| Mutation | Caught by |
|---|---|
| the lent-argument proof passes no lending call | `loan_owner_assigned_before_loan_valid` refused |
| any enclosing node counts as straight-line | `loan_owner_assigned_in_loop_before_loan_invalid` admitted |
| a claim that contains the call is skipped | `loan_owner_assigned_from_its_own_loan_invalid` admitted |
| source order is not compared | `readfile_result_borrowed_identity_landed_valid` refused at a deep drop (14:27) |
| the assignment's target position is not used | `loan_owner_assigned_before_loan_valid` refused |
| the chain need not reach the routine body | not caught |
| a node without a position is ordered | not caught |

The last two survive: every claim the proof walks has a recorded chain
to the body, and every assignment's target root has a position. Both
stay as guards: an unrecorded enclosing loop, or a node a desugaring
made, has no order the rule can trust.
