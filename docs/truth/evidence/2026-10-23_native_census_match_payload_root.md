# 2026-10-23: canonical census after match payload bindings root at their subject

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`a43f6ee26` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_match_payload_binding_rooted_at_its_subject.md).

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `a43f6ee26` | 217 | 175 |
| a payload binding roots at its match subject | 215 | 173 |

Removed, none added: the two refusals of `PublishMirCArtifact`
(`'machine_request' was moved into 'DriverRung2MirCProducePayloadAdmitted'`).
`DriverSourceCExecution.Compile` now fails at loan-proof code 1, an
assignment to `self.outcome` whose root the proof does not resolve, and
`EmitDirectMir` at code 9 (96:24), both after their payload bindings.

The same census run back to back took 948 s and 3978236 KiB at peak on
`a43f6ee26`, and 967 s and 3978536 KiB with the rule, within the 927 to
1036 s spread of this session's runs.

## Gates

`function-param-flow-summary` gate passed (202 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (775 compared against `a43f6ee26`)

Errors, landing analyses and `Clone` counts per routine changed only in
`loan_payload_binding_passed_to_reader_valid`, new: refused at 37:47, 39:32
and 42:64 on `a43f6ee26`, prints 3 and 1 on C and LLVM now. Two new
negatives are refused the same on both trees: a payload binding a callee
writes, and one bound from a match on the lending call's own result.

## Mutations (151 loan, getter, lend and payload fixtures)

| Mutation | Caught by |
|---|---|
| a payload binding has no root | `loan_payload_binding_passed_to_reader_valid` refused |
| a call's result as subject roots the binding at nothing | not caught |

The last stays as a guard: a binding rooted at nothing would be attributed
to no proof. In `loan_payload_binding_of_call_subject_written_invalid` the
loan frame refuses the write first, as a call escape of the loan the
payload carries.
