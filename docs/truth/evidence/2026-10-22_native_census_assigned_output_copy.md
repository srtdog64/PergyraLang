# 2026-10-22: canonical census after an assigned output keeps a copy

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`f78289b73` plus the change measured (local, not pushed), on main
`7e7b63673`. Rule: [decision](../decisions/2026-10-22_formal_storage_assigned_into_output_is_copied.md).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `f78289b73` | 3463, 1184, 1059, 1050 | 744 |
| assigned output keeps a copy | 3463, 1169, 1044, 1035 | 740 |

Outside `direct_mir_*`, moves fell by 1 and `ref` escapes through a call by
3 (`SemanticFutureJoin`, `SemanticFutureLoop`, `SemanticFutureNode`,
`DebugSessionWalkNode`); the other 11 were in `direct_mir_*` routes. No
kind was added. No flow store failed.

## A first version added 13 refusals

Selecting every assigned value also selected a local written back to the
field it was taken from (`let names = projection.names; ArrayPush(names,
name); projection.names = names;` in
`MirIntentExecutionCarrierProjectionAppend` and three others): the
`Clone` ended the write-back, so the field read as moved into the local
("Field 'names' was moved into collection binding ... cannot be used
again"). A local assigned back now lands nothing; the store into the local
is where a copy belongs.

## Fixtures

- `output_field_store_of_ref_formal_landed_valid`: refused before; after
  one landing analysis it emits MIR and prints `x1` on C and LLVM.
- `array_result_owned_element_inout_escape_invalid` became
  `array_result_owned_element_inout_escape_landed_valid`: `output = rows[0]`
  of a `ref` Array into an `inout` String now lands a copy, so releasing
  the Array's Strings before printing the output is safe. C with ASan and
  UBSan runs it clean and prints `owned value`.
- `output_field_store_of_ref_formal_by_value_clone_invalid` and
  `array_element_inout_escape_by_value_clone_invalid`: an Array of structs,
  which `Clone` copies only by value, assigned into an output; still
  refused, with borrow-escape diagnostics only.
- Seven negatives assigned a formal's String or aggregate straight into an
  output, and the copy now admits them:
  `array_result_owned_defer_late_scalar_retention_invalid`,
  `callee_alias_return_inout_invalid`, `high_output65_dynamic_invalid`,
  `inout_owned_param_input_return_moves_invalid`,
  `local_read_payload_save_invalid`, `physical_return_inout_invalid`,
  `ref_view_beside_unit_variant_stored_invalid`. Each also falsifies
  another rule (the 65th output, an alias chain, a read payload, a physical
  return, a unit variant beside the escape), so each now puts the value in
  a local first and assigns the local, on the same line: that still keeps
  the formal's storage in the output, lands nothing, and gives the same
  refusals as before. Every other fixture's refusals are unchanged
  (706 compared).
- `inout_self_assignment_lands_nothing_valid`: `rows = Rows(rows.values,
  ...)` into the frame's own `inout` formal runs no landing analysis and
  prints `1`.

## Gates and mutations

`test-semantic` 3237, `test-transpile` 1014, `test-dir` 15, `test-air`
161, `test-rir` 44, `test-hir` 58 passed, 0 failed; the flow summary
target passed; `test-mir` units passed (the target exits 2 at its
self-host driver step, as on main).

| Mutation | Caught by |
|---|---|
| select nothing | flow gate (the two landed fixtures) |
| select a local assigned back too | flow gate (the routed negatives) |
| select in loan walks too | flow gate (`return_loan_inout_invalid`) |
| select a value without the frame's input | flow gate |
| select an assignment into any formal, not only `inout` | flow gate (`owned_param_field_store_ref_return_invalid`) |

A first version also skipped an assignment into the frame's own formal.
Dropping that condition changed nothing, `inout_self_assignment_lands_nothing_valid`
included: only a by-value or `ref` formal has a backing frame, and the
target must be an `inout` one. The condition was removed; the fixture stays
as the check that an `inout` self-update lands nothing.
