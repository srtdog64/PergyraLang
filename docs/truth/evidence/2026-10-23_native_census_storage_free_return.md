# 2026-10-23: canonical census after a complete frame decides RETURN of a storage-free routine

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`293d3996b` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_complete_frame_decides_return_of_storage_free_routine.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `293d3996b` | 3388, 362, 271, 269 | 227 |
| complete frame decides RETURN | 3349, 342, 267, 265 | 223 |

Removed, none added: the callers of `SemanticFutureJoin`
(`SemanticFutureNode` 130:17, `SemanticFutureExpression` 193:9), of
`SemanticAstNumericStringAllocationRecordResolvedCall` (438:38) and of
`SemanticAstSignatureCanonicalNameIndexFill` (441:5), each refused for a
borrowed `ref` value passed to a routine with an `inout` output.

The same census run back to back on an otherwise idle machine took
998 s and 3977452 KiB at peak on `293d3996b`, and 993 s and 3977152 KiB
with the rule: no cost added.

## How it was found

A census that printed which return of the summary demand answered, with
each entry's flags, showed these routines' summaries complete and
proved, with RETURN on the borrowed input and no call escape. A copy of
`SemanticFutureJoin` reduced statement by statement kept the RETURN only
for `target.verdict = Clone(SemanticAstBodyVerdictError(...))`: the
syntactic summary marks `branch` RETURN because it reaches the call's
result, and nothing replaced that for a routine with an `inout` formal.

## Gates

`function-param-flow-summary` gate passed. `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (741 compared against `293d3996b`)

Three changed. New: `ref_input_through_call_result_into_inout_output_landed_valid`
(refused at 37:5 on `293d3996b`; one Clone in each of `JoinCopied` and
`JoinLanded`, `f26` on C and LLVM). `named_string_field_forward_project_drop_invalid`
and `named_string_field_mixed_branch_drop_invalid` are now `..._landed_valid`:
the node `Fill` keeps is a Clone, `owned value` on C and LLVM, and C with
ASan runs both clean while `ArrayDropOwnedStrings` frees `rows`. With the
store landing turned off in a scratch build, both are still refused at
the drop. New negative: `named_string_field_shallow_node_drop_invalid`
(a node Clone copies only by value: refused at 41:27 on both trees).

## Mutations

| Mutation | Caught by |
|---|---|
| the frame never decides RETURN for a storage-free routine | `named_string_field_forward_project_drop_landed_valid` refused at the drop |
| the frame decides it also when incomplete | not caught |

The second survives: for a routine with an `inout` or `own` formal an
incomplete frame adds a call escape anyway, and no fixture leaves the
frame of a routine with only `ref` and by-value formals incomplete
without marking it unsupported. The guard stays, as in the scalar-return
case it extends: an incomplete frame's effects are not an over-approximation.
