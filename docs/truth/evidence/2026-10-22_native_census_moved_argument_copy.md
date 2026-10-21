# 2026-10-22: canonical census after moved arguments used again are copied

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`e248f67b9` plus the change measured (local, not pushed). Rules:
[moved argument](../decisions/2026-10-22_moved_argument_used_again_is_copied.md),
[borrowed storage](../decisions/2026-10-22_borrowed_storage_handed_on_is_copied.md).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `e248f67b9` | 3463, 1169, 1044, 1035 | 740 |
| moved arguments and borrowed storage copied | 3463, 1094, 967, 961 | 670 |

Outside `direct_mir_*`:

| Refusal | Before | After |
|---|---|---|
| `'_' was moved into '_' and cannot be used again` | 265 | 218 |
| `Borrowed parameter storage cannot be moved into '_'` | 24 | 2 |
| `ref` escape through a call | 147 | 146 |
| String element across an opaque boundary | 86 | 85 |
| `Value boundary arguments ... must use a named variable` | 4 | 5 |

The one value-boundary refusal added is a loan copy wrapped around a `ref`
argument (`CanonicalizeMirArtifactWithAdmittedTopology(..., admitted, ...)`),
not a move copy: a move copy only stands at a default-mode formal. Six
such loan copies were already refused before; their carrier rule is the
next fix. Most moves left are of values `Clone` copies only by value
(`AstTreeArtifact` and other structs holding Arrays of structs), so no
copy lands for them.

## Fixtures

Twenty-four negatives moved a value and then used it again; the copy now
admits each after one landing analysis, C and LLVM print the same output,
and C with ASan and UBSan runs each clean. Each became `*_landed_valid`
with that landing count and output checked, so a move the checker stops
seeing (no landing) or a copy that stops landing (a refusal) both fail:
`array_slice_view_returned`, `by_value_formal_array_sorted`,
`by_value_formal_rebound_to_result_grown`, `by_value_getter_lend_inout_owner`,
`by_value_getter_result_grown`, `by_value_getter_result_local_returned_grown`,
`by_value_getter_result_passed_grown`, `by_value_getter_result_reassigned_grown`,
`constructor_result_grown`, `default_aggregate_move_field_alias_passed_on`,
`default_aggregate_move_field_alias_store`, `default_aggregate_move_field_element_write`,
`default_aggregate_move_field_set`, `default_aggregate_move_field_store`,
`default_aggregate_move_option_field_alias_store`, `formal_array_store_field_take`,
`inout_owned_param_input_return_moves`, `intent_action_stores_by_value_moves`,
`local_copied_path_then_moved`, `local_copy_changed_then_moved`,
`optional_wrap_result_grown`, `option_payload_result_grown`,
`path_alias_returned_in_constructor_grown`, `unit_enum_constant_constructor_moves`.
Moves of values `Clone` copies only by value stay refused
(`aggregate_move_void_call_invalid` and the other remaining negatives).

New: `borrowed_inout_array_kept_in_result_landed_valid` (prints `23`: the
result holds the two ids it saw, the caller's ids grow to three) and
`moved_argument_two_branches_landed_valid` (prints `21`, one landing
analysis for both branches).

## Gates and mutations

`test-semantic` 3237, `test-transpile` 1014, `test-dir` 15, `test-air`
161, `test-rir` 44, `test-hir` 58 passed, 0 failed; the flow summary
target passed; `test-mir` units passed (the target exits 2 at its
self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| no copy where a moved path is used again | `by_value_getter_result_grown_landed_valid` ran 0 landing analyses |
| no copy at a borrowed parameter's move | `borrowed_inout_array_kept_in_result_landed_valid` ran 0 |
| moves by different arguments merged into one row | `moved_argument_two_branches_landed_valid` ran 2 |

## Not measured yet

The decision asks for the self-host compile time and the copies once the
copies land. The canonical program is still refused (961), so no
self-host compiler built from it with these copies exists yet; the
measurement waits for its admission.
