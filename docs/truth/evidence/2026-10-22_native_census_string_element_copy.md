# 2026-10-22: canonical census after a kept String element is copied where passed

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`5c4dd0fd4` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-22_string_element_kept_by_callee_is_copied.md).

| Run | Refusals by analysis | Final, outside `direct_mir_*` |
|---|---|---|
| `5c4dd0fd4` | 3420, 478, 387, 385 | 343 |
| element copies land | 3420, 454, 363, 361 | 319 |

Outside `direct_mir_*`: "Array argument call requires preserved array
storage" 41 -> 20 and "ArrayPushOwnedString requires preserved array
storage" 5 -> 3, among them four `AppendLine` calls in the parser. No
refusal was added.

## What the preserved-Array refusals were

A census that printed, for each refusal "requires preserved array
storage", the earlier pending calls on the same binding that did not
preserve it (scratch instrumentation, not committed) sorted the 47 sites
(41 outside `direct_mir_*`) by their failing earlier calls:

| Failing earlier calls | Sites |
|---|---|
| a String element passed to a callee that may keep or return it | 20 |
| an `inout` Array whose callee writes its elements | 16 |
| a call that may keep or rebind the whole Array's descriptor | 6 |
| none (the consumer's own proof) | 5 |

This change lands a copy for the first kind only.

## Fixtures (732 compared against `5c4dd0fd4`)

New: `string_element_kept_by_callee_landed_valid` (two Clones in `Lines`:
the element passed to `Join` and the result returned; `a1|b2`, ASan
clean) and `array_descriptor_kept_then_read_invalid` (22:16, refused on
both trees). Ten negatives are admitted with one Clone each where the
element is passed, two in `string_element_inout_owner_named_call` (now
`*_landed_valid`): `array_result_owned_defer_late_scalar_retention`,
`array_result_owned_element_host_method_retainer`,
`intent_subject_keeps_string`, `named_string_field_make_nodes_drop`,
`named_string_identity_owned_element_retain`,
`readfile_result_borrowed_identity`, `string_element_inout_owner_named_call`,
`string_element_returned_by_named_call_owner_changed`,
`string_element_returned_by_named_call_past_owner` and
`string_element_returned_in_result_loop_kept`. The eight that link give
the same output on C and LLVM with ASan clean; two call an undefined
`extern "C"` function. Four `named_string_field_lineage_*_invalid`
fixtures stay refused, now by "A String field has no current
independent-source recipe".

## Mutations

| Mutation | Caught by |
|---|---|
| a copy lands for a whole-Array consumer too | `array_descriptor_kept_then_read_invalid` admitted |
| no copy lands | `string_element_kept_by_callee_landed_valid` refused at 17:17 |

## Gates

`function-param-flow-summary` gate passed (174 s); `test-semantic` 3240
passed, 0 failed.
