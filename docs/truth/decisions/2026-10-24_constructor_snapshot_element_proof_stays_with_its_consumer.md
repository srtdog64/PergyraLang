# 2026-10-24: a constructor snapshot's String element proof stays with its consumer

Decided by: the Claude lane, repairing the integration of main's
`abedba79e` (GPT root lane) on the merged branch.

## Rule

`semantic_nominal_result_snapshot_current` checks a constructor result's
declaration, field types and receipt identity. It does not replay a
field's owned String element receipt; that proof is replayed only by its
consumer, the owned-element member requirement
(`collection_owned_element_member_requirement_owner.c`), which already
checks it at its own phase.

## Why

The same snapshot answers `publication_result_current`, which the flow
summaries read for a constructor call's backing relation. They run before
the deferred Array preservations are finalized, and an owned function
result (`owned_function_result_replay`) cannot replay before then. On the
canonical input every such constructor read failed, so its arguments had
no backing relation, `CodegenCallableReceiverFactsFromAdmittedRowsOrDie`'s
required return relation stayed unproved, and the flow summary store
failed closed for the whole program: 35977 census refusals, most of them
"cannot escape through channel send" from routines with no channel
([record](../evidence/2026-10-24_native_census_after_main_merge_flow_repairs.md)).

The element proof keeps its strength: its only consumer still refuses a
field whose owned receipt does not replay.

## Falsifier

`constructor_string_field_snapshot_before_finalize_invalid` keeps exactly
its callee's refusal; with the replay in the snapshot it also reports the
failed-closed summary and three escapes of `rows`.
