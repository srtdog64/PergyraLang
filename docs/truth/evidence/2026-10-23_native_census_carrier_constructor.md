# 2026-10-23: canonical census after a carrier's fields read into any constructor

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`4659e3682` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_carrier_fields_read_into_any_constructor.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `4659e3682` | 224 | 182 |
| a carrier's constructor reads wherever its value goes | 221 | 179 |

Removed: the two refusals of `SelfMirProgramDomainFactsFromReadyArtifact`
(`'analysis.expression_surfaces.expression_graph' was moved into
'SelfMirProgramFactsEmpty'`) and the two of
`CodegenAdmittedCViewFromFactsOrDie` (`'analysis.expression_surfaces' was
moved into 'DirectMirIntentExecutionViewFromAdmitted'`).

Added: `'artifact' was moved into 'SemanticAstExpressionSurfaceRowsFromArtifact'`
in `SemanticAstArtifactAnalyzeWithExpressionGraph` (528:9). At `4659e3682`
the census walked no loan for that call: the callee's summary did not
return `artifact`. Now it does, with this rule run in the callee; the loan reaches
`expression_surfaces` and goes to `SemanticAstArtifactAnalyzeFromExpressionSurfaces`,
whose formal keeps it (call escape at 527:5), and `Clone` copies the artifact
only by value, so no copy lands at the caller either. A rule taking every
constructor argument as a read added the same refusal on 2026-10-23.

The same census run back to back took 927 s and 3977468 KiB at peak on
`4659e3682`, and 964 s and 3978068 KiB with the rule. The rule only skips
a loan frame walk for a carrier's constructor; the difference is within the
927 to 1036 s spread of this session's runs.

## Gates

`function-param-flow-summary` gate passed (204 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (769 compared against `4659e3682`)

Errors, landing analyses and `Clone` counts per routine changed in two:

- `loan_carrier_read_into_returned_constructor_valid`, new: refused at
  39:49 on `4659e3682`, prints 3 and 1 on C and LLVM now.
- `constructor_of_getter_result_returned_valid`: one `Clone` in `Wrap` in
  one landing analysis before, none now. `Wrap`'s `Holder` holds the loan
  of its `outer`, which `Wrap`'s summary returns, as the fixture's comment
  says; it prints 7 on C and LLVM, as before.

`loan_carrier_read_into_constructor_a_callee_writes_invalid`, new, is
refused the same on both trees.

## Mutations (144 loan, getter and lend fixtures)

| Mutation | Caught by |
|---|---|
| a carrier's constructor value must stay | `loan_carrier_read_into_returned_constructor_valid` refused (41:49) |
| any call without a formal whose value stays is a read | `readfile_result_borrowed_identity_landed_valid` refused (14:27) |
| an owner's constructor value need not stay | `loan_read_in_the_statement_that_moves_its_owner_invalid`, `loan_owner_constructor_copy_written_by_callee_invalid` and `..._returned_invalid` admitted |
| no constructor is a read | `loan_owner_read_by_constructor_valid` refused |
| a carrier handed on is still a change | `loan_carrier_handed_on_by_early_return_valid` refused |
