# 2026-10-23: canonical census after a callee reads of a copy's other side only the paths it names

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`824cf8bdd` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-23_local_copy_other_side_read_by_callee_paths.md);
plan: [directive](../../agent_work_directives/field_level_loan_and_copy_facts_2026-10-23.md), slice 1.

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `824cf8bdd` | 215 | 173 |
| read paths for any other side (first version) | 226 | 184 |
| read paths for an `inout` formal's other side | 209 | 167 |

The first version removed 14 local-copy refusals and added 22 moves of
`facts` in `SelfMirCanonicalInstructionIds`: `SelfMirIntentExecutionCanonicalPlan`
copies `facts.intent_execution.steps` and sets rows of the copy, and the
copy that its refusal landed had kept the routine from changing its
by-value `facts`.

The version kept removed nine local-copy refusals and added none:
`SemanticAstAnalysisResolveExpressionIdentities`,
`...ResolveExpressionPlacesFromAdmittedBody` and
`...ResolveRoleOperatorTargetsFromAdmittedBody` two each,
`...ResolveCallReturnTypes`, `SemanticAstIterationSyntheticGraphRootsAttach`
and `SemanticAstMatchMaterializationFactsAttach` one each. In the last three
a `Borrowed parameter storage cannot be moved into
'SemanticAstExpressionSurfaceFactsWithGraph'` refusal now shows at the same
node (214:36, 169:32, 179:32), which the copy refusal hid there, as each
node is reported once.

The same census run back to back took 971 s and 3978916 KiB at peak on
`824cf8bdd`, and 996 s and 3978872 KiB with the rule. A read-path query
builds the callee's claims and walks its rows, only for an argument below
which an `inout` formal's copy changed; the difference is within the 927
to 1036 s spread of this session's runs.

## Gates

`function-param-flow-summary` gate passed (187 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 161, `test-rir` 44,
`test-hir` 58, `test-mir` units 691 passed, 0 failed (the target exits 2
at its self-host driver step, as on main).

## Fixtures (780 compared against `824cf8bdd`)

Errors, landing analyses and `Clone` counts per routine changed only in
`local_copy_other_side_passed_to_reader_of_other_rows_valid`, new: refused
at 36:41 on `824cf8bdd`, prints 7 on C and LLVM now. Four new negatives
are refused the same on both trees: a callee reading the changed rows, one
handing them to a reader, one copying the value whole into a local, and
the valid shape under a by-value formal.

## Mutations (164 loan, getter, lend, payload and local-copy fixtures)

| Mutation | Caught by |
|---|---|
| no argument is narrowed by read paths | `local_copy_other_side_passed_to_reader_of_other_rows_valid` refused |
| a path handed to a callee reads nothing | `local_copy_other_side_passed_through_to_reader_of_changed_rows_invalid` admitted |
| a plain appearance reads nothing | `..._passed_to_reader_of_changed_rows_invalid` and `..._passed_through_..._invalid` admitted |
| any other side is narrowed, not only an `inout` formal | `local_copy_of_by_value_formal_passed_to_reader_of_other_rows_invalid` admitted |
| a routine with a deferred or closure body names its reads | not caught |
| a path walked inside an argument is narrowed like the argument | not caught |

The last two stay as guards: a deferred or closure body reads the formal
after code written after it, and the only paths walked inside an argument
besides the argument itself are its index expressions, which the call
does not receive.
