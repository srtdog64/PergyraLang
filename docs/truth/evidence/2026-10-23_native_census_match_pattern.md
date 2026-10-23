# 2026-10-23: canonical census after match patterns stop counting as changes

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`0fa1e2346` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_match_pattern_is_no_change_of_a_loan_owner.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `0fa1e2346` | 221 | 179 |
| a variant call under a case is no claim (first version) | 217 | 175 |
| any call inside a case pattern is no claim | 217 | 175 |

Removed, none added: the two refusals of
`DriverRung2CliLogSourceCPayloadOrDie` (`'request' was moved into
'ProduceSourceCThroughPgyCompilerWorld'`) and the two of
`DriverRung2CliLogMirCPayloadOrDie` (`'machine_request' was moved into
'ProduceMirCThroughPgyCompilerWorld'`). `DriverSourceCExecution.Compile`
(114:21), `PublishMirCArtifact` (152:21) and `EmitDirectMir` (96:24) now
fail at loan-proof code 3, at a call that takes a pattern binding, instead
of code 2 at the pattern.

The same census run back to back took 971 s and 3977712 KiB at peak on
`0fa1e2346`, and 974 s and 3978496 KiB with the rule, which walks one
claim's parents per call a loan proof reaches.

The loan-order code moved from `function_param_flow_claim_publication.c`
(607 lines with the rule) into `function_param_flow_loan_order.c`, and its
declarations from `function_param_flow_summary_internal.h` (600 lines) into
`function_param_flow_loan_order.h`.

## Gates

`function-param-flow-summary` gate passed (198 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (772 compared against `0fa1e2346`)

The gate's first run refused the rule:
`enum_payload_constructor_ref_field_return_invalid` was admitted. Built on
`0fa1e2346`, the same loan returned as a struct result, or as a payload
read without a match, compiles and prints `owner`; the fixture was refused
only at its two patterns. It is renamed `..._return_valid` (prints `owner`
on C and LLVM), and `enum_payload_constructor_ref_field_owner_changed_invalid`,
new, replaces the input in a case body and is refused at the call (32:28)
on both trees.

Errors, landing analyses and `Clone` counts per routine changed in those
two and in `loan_payload_bound_by_match_valid`, new (refused at 33:47 and
35:64 on `0fa1e2346`, prints 3 and 1 on C and LLVM now).
`loan_payload_bound_by_match_written_invalid`, new, is refused the same on
both trees.

## Mutations (148 loan, getter, lend and payload fixtures)

| Mutation | Caught by |
|---|---|
| no pattern is skipped | `loan_payload_bound_by_match_valid` refused |
| any call inside a case, its guard and body too, is a pattern | `loan_payload_bound_by_match_written_invalid` admitted |

A first version stopped the walk at any parent that is not a call. No
fixture told it from the walk to the nearest case, which decides by the
case's own patterns; the condition was removed.
