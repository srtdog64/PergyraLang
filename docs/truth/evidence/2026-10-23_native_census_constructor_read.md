# 2026-10-23: canonical census after a constructor of a loan owner's fields reads the owner

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`a3fd04ba1` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_constructor_of_loan_owner_fields_reads_the_owner.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `a3fd04ba1` | 247 | 205 |
| constructor read, lent arguments only (counterfactual) | 233 | 191 |
| constructor read, both proofs | 233 | 191 |

Removed, none added: the 14 refusals of
`SemanticAstExpressionTypedBindingContractReady`, `'base' was moved into
'SemanticAstExpressionSurfaceFactsFromTypedArtifact'`, at the base's paths
the two artifacts are built from and at its later uses.

The same census run back to back took 951 s and 3978004 KiB at peak on
`a3fd04ba1`, and 976 s and 3977608 KiB with the rule. The rule walks one
loan frame of the constructed value for each constructor argument a loan
proof reaches; the difference is within the 930 to 1036 s spread of this
session's runs.

The proof moved, unchanged but for the rule, from
`type_checker_ownership_param_summary.c` into
`function_param_flow_loan_owner_proof.c`, which keeps the first under its
600-line cap.

## Gates

`function-param-flow-summary` gate passed (179 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (760 compared against `a3fd04ba1`)

Errors, landing analyses and `Clone` counts per routine changed in two:

- `loan_owner_read_by_constructor_valid`, new: refused at 32:41, 32:52,
  32:64 and 34:81 on `a3fd04ba1`, prints 8 on C and LLVM now.
- `by_value_getter_lend_in_constructor_passed_grown_landed_valid`: two
  `Clone`s in two landing analyses before, one in one now. The new `Outer`
  built around the loan is still copied where it goes to `Fill`; once
  copied it stays, so building it reads the loan, which is no longer copied
  where it is made. It prints 10 on C and LLVM, as before.

Four new negatives are refused on both trees: a constructed value changed
under the loan (by the local copy rule only, now), one passed to a callee
that writes it, one returned, and an in-place `ArrayReverse` of the
owner's field.

## Mutations (134 loan, getter and lend fixtures)

| Mutation | Caught by |
|---|---|
| any call without a formal whose value stays is a read | `loan_owner_permuted_in_place_invalid` admitted |
| the constructed value need not stay | `loan_owner_constructor_copy_written_by_callee_invalid`, `..._returned_invalid` and `loan_read_in_the_statement_that_moves_its_owner_invalid` admitted |
| no constructor is a read | `loan_owner_read_by_constructor_valid` refused |
