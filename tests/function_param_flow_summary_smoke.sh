#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_BIN="$(pgy_select_optional_exe_binary "$PGY_BIN")"
pgy_require_runnable_binary_here "function-param-flow-summary" "$PGY_BIN"

PYTHON_BIN="${PYTHON_BIN:-python}"
FIXTURE="$ROOT_DIR/tests/cases/function_param_flow_summary/main.pgy"

"$PYTHON_BIN" - "$ROOT_DIR" "$PGY_BIN" "$FIXTURE" <<'PY'
import os
import json
import pathlib
import re
import subprocess
import sys
import tempfile
from collections import Counter

root = pathlib.Path(sys.argv[1])
pgy = sys.argv[2]
fixture = sys.argv[3]

# Verify the owner's admitted artifacts and refusal behavior below. Source
# spelling is not an oracle for summary identity or bounded evaluation.

env = os.environ.copy()
env["PGY_DEBUG_FUNCTION_PARAM_FLOW"] = "1"
env["PGY_DEBUG_RESOURCE_FLOW_FACTS"] = "1"
# Runtime call-pattern typed/owner coverage lives in the phase unit. Even the
# --hir CLI currently reaches an unsupported MIR artifact route for its ref
# counterpart, so it cannot certify that positive here. The owning case below
# must fail at the borrow boundary, never at artifact OOM.
# Reuse the admitted worker/capture inputs at their existing owners. A
# conservative unknown-capture result must not replace their checked facts.
for relative in (
    "backend_compare/closure_copy_capture/main.pgy",
    "backend_compare/event_lambda_handler/main.pgy",
    "backend_compare/async_block_runtime/main.pgy",
    "backend_compare/async_func_decl/main.pgy",
    "backend_compare/async_spawn_await/main.pgy",
    "backend_compare/await_inline_spawn/main.pgy",
    "backend_compare/generic_spawn/main.pgy",
    "slot_contract/positive/device_slot_async_read/main.pgy",
    "structured_spawn_lifecycle/positive_joined.pgy",
    "structured_spawn_lifecycle/positive_own_transfer.pgy",
    "structured_spawn_lifecycle/positive_immediate_await.pgy",
    "structured_spawn_lifecycle/positive_parallel_single_join.pgy",
):
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(root / "tests/cases" / relative)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15.0, check=False)
    if checked.returncode != 0 or b'"pgy.mir.v1"' not in checked.stdout:
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"existing worker/capture positive lost MIR admission: {relative}")
# Semantic-only paired controls: no unsafe negative is emitted or executed.
selected_call_targets = {
    "source_callable_class_tostring_constructor_valid": ("ToString", "class", "ToString", "none"),
    "source_callable_enum_tostring_payload_valid": ("ToString", "enum", "Conversion", "none"),
    "source_callable_class_method_alias_valid": ("Box_Value", "method", "Box", "declared_callable"),
    "source_callable_enum_method_alias_valid": ("Route_Value", "method", "Route", "declared_callable"),
    "source_callable_enum_payload_method_alias_valid": ("Packet_Value", "method", "Packet", "declared_callable"),
}
# Positive fixtures whose String push lands as an owned copy: routine and the
# number of ArrayPushOwnedString calls it must reach MIR with.
owned_push_landings = {
    "owned_copy_string_push_landed_valid": ("Main", 1),
    "formal_string_push_same_routine_batch_valid": ("FormalStringPushBatchApply", 3),
    "owned_string_push_multi_receiver_batch_valid": ("OwnedStringPushBatchApply", 3),
    "string_element_push_copy_landing_valid": ("Selected", 1),
    # The recursive chain itself remains scoped by push/pop; only the result
    # stored in out_blocks lands as one owned String.
    "inout_string_chain_popped_by_recursive_parser_valid": ("Decls", 1),
    "array_result_local_owned_element_container_copy_landing_valid": ("Main", 2),
    "named_string_all_def0_branch_copy_landing_valid": ("Observe", 1),
    "formal_string_store_landing_local_receiver_valid": ("Gather", 1),
    "formal_string_store_landing_private_local_valid": ("Count", 1),
    "formal_store_landing_alias_output_valid": ("Append", 1),
    "formal_string_store_landing_field_take_valid": ("Append", 1),
    "formal_string_store_landing_value_source_valid": ("Append", 1),
    "named_string_same_empty_borrowed_then_owned_direct_landed_valid": ("Project", 2),
    "named_string_same_empty_owned_then_borrowed_direct_landed_valid": ("Project", 2),
    "named_string_same_empty_owned_then_borrowed_named_landed_valid": ("Project", 2),
    "named_string_same_empty_borrowed_then_owned_named_landed_valid": ("Project", 2),
    "string_inout_returned_literal_then_escape_call_landed_valid": ("Mark", 1),
    "string_inout_unwrapped_literal_then_escape_call_landed_valid": ("Mark", 1),
    "string_inout_returned_literal_then_own_release_landed_valid": ("Mark", 1),
}
# Positive fixtures whose landing reaches MIR as Clone calls (a formal's
# String store, a copied getter loan): each routine and its Clone count.
landed_clones = {
    "string_element_tostring_opaque_call_landed_valid": {"Main": 1},
    "string_argument_kept_copy_landed_valid": {"Block": 1},
    "string_copy_landing_same_function_batch_valid": {"Build": 2},
    "string_copy_landing_function_boundary_valid": {
        "BuildFirst": 2, "BuildSecond": 2},
    "formal_string_store_same_routine_batch_valid": {
        "FormalStringStoreBatchApply": 3},
    "formal_string_store_multiple_formals_batch_valid": {
        "FormalStringStoreMultiApply": 4},
    "formal_string_store_cross_routine_batch_valid": {
        "FormalStringStoreCrossRoutineFirst": 1,
        "FormalStringStoreCrossRoutineSecond": 1},
    "formal_string_store_batch_boundary_valid": {
        "FormalStringStoreBoundaryApply": 2},
    "formal_string_store_landing_ref_push_valid": {"Collect": 1},
    "formal_string_store_landing_map_array_set_valid": {"Label": 1, "Put": 1},
    "borrowed_owner_loan_grown_landed_valid": {"Nest": 1},
    "string_crossing_window_pair_landed_valid": {"Wrap": 1},
    "formal_string_store_landing_value_set_valid": {"Rename": 1},
    "formal_string_store_landing_value_map_valid": {"Label": 1},
    "mapnew_ref_string_value_copy_valid": {"Make": 1},
    "formal_string_store_landing_shadowed_function_valid": {"Collect": 1},
    "formal_string_store_landing_shadowed_struct_valid": {"Collect": 1},
    "formal_string_store_landing_shadowed_method_valid": {"Label": 1},
    "private_array_push_nonempty_alias_landed_valid": {"Build": 1},
    "by_value_getter_lend_owner_written_landed_valid": {"Rename": 1},
    "by_value_getter_lend_mutated_landed_valid": {"Touch": 1},
    "by_value_getter_lend_local_transfer_landed_valid": {"Make": 1},
    "by_value_getter_lend_owner_written_shadowed_landed_valid": {"Rename": 1},
    "by_value_getter_lend_kept_landed_valid": {"Collect": 1},
    "by_value_getter_lend_returned_landed_valid": {"Make": 1},
    "by_value_getter_lend_passed_kept_landed_valid": {"Collect": 1},
    "by_value_getter_lend_assigned_landed_valid": {"Collect": 1},
    "by_value_getter_lend_owner_written_after_lowering_landed_valid": {"Rename": 1},
    "formal_store_landing_struct_output_valid": {"Reserve": 1},
    "array_push_loan_inout_landed_valid": {"Collect": 1},
    "aggregate_move_void_call_landed_valid": {"Report": 1},
    "by_value_getter_lend_inout_passed_landed_valid": {"Use": 1},
    "by_value_getter_lend_inout_then_returned_landed_valid": {"Use": 1},
    "nonowner_risk_rebind_clone_landed_valid": {"Main": 2},
    "nonowner_risk_local_carrier_landed_valid": {"Main": 1},
    "array_result_owned_element_direct_return_landed_valid": {
        "CaptureDirect": 1, "Main": 1},
    "named_string_identity_loop_reassignment_landed_valid": {"Main": 1},
    "named_string_field_lineage_same_call_retire_grow_landed_valid": {"Main": 2},
    "named_string_field_lineage_named_mapping_landed_valid": {"Main": 2},
    "named_string_field_lineage_mixed_return_landed_valid": {"Main": 2},
    "named_string_field_lineage_recursive_forward_landed_valid": {"Main": 2},
    "string_inout_returned_literal_then_escape_call_landed_valid": {"Stash": 1},
    "string_inout_unwrapped_literal_then_escape_call_landed_valid": {"Stash": 1},
    "by_value_getter_lend_result_grown_landed_valid": {"Main": 1},
    "default_aggregate_move_shared_field_return_landed_valid": {"Main": 1},
    "by_value_getter_lend_result_path_grown_landed_valid": {"Main": 1},
    "by_value_getter_lend_result_passed_grown_landed_valid": {"Main": 1},
    "by_value_getter_lend_in_constructor_passed_grown_landed_valid": {"Main": 1},
    "by_value_getter_second_lend_grown_landed_valid": {"Main": 1},
    "by_value_getter_second_lend_formal_grown_landed_valid": {"Grow": 1},
    "by_value_getter_lend_in_constructor_grown_landed_valid": {"Main": 1},
    "by_value_getter_lend_owner_in_constructor_grown_landed_valid": {"Main": 1},
    "by_value_getter_two_lends_grown_landed_valid": {"Main": 2},
    "local_let_alias_grown_landed_valid": {"Main": 1},
    "local_let_alias_source_grown_landed_valid": {"Main": 1},
    "local_assigned_alias_grown_landed_valid": {"Main": 1},
    "local_constructor_alias_grown_landed_valid": {"Main": 1},
    "local_array_literal_alias_grown_landed_valid": {"Main": 1},
    "local_pushed_alias_grown_landed_valid": {"Main": 1},
    "formal_let_alias_grown_landed_valid": {"Grow": 1},
    "local_copy_of_copy_grown_landed_valid": {"Main": 1},
    "local_copy_source_element_set_landed_valid": {"Main": 1},
    "local_copy_source_inout_grown_landed_valid": {"Main": 1},
    "local_copy_source_sorted_landed_valid": {"Main": 1},
    "collection_value_store_grown_landed_valid": {
        "MapStore": 1, "SetStore": 1, "ListPushStore": 1, "ListSetStore": 1, "QueueStore": 1},
    "formal_field_copy_changed_at_exit_landed_valid": {"Grow": 1},
    # The result returned. The element passed to Join stays a loan of parts,
    # which no change reaches after the loan is made.
    "string_element_kept_by_callee_landed_valid": {"Lines": 1},
    "ref_loan_field_written_back_valid": {"Plan": 1},
    "ref_loan_stored_into_inout_output_landed_valid": {"Fill": 1},
    # The borrowed String each push into an Array taken from a field keeps.
    "string_local_pushed_into_field_array_landed_valid": {"Build": 1, "Plan": 1},
    # Among borrowed literals a plain push keeps a Clone; an owned push has none.
    "string_element_push_copy_landing_mixed_landed_valid": {"Selected": 1},
    # The node Fill keeps holds a copy of the Array's String, so the drop is safe.
    "named_string_field_forward_project_drop_landed_valid": {"Fill": 1},
    "named_string_field_mixed_branch_drop_landed_valid": {"Fill": 1},
    # The verdict each Void routine stores into its inout output, as a copy.
    "ref_input_through_call_result_into_inout_output_landed_valid": {"JoinCopied": 1, "JoinLanded": 1},
    # The String returned from the ref input is copied exactly where ArraySet
    # stores it; the following inout reader keeps nothing.
    "ref_string_through_inout_reader_shallow_store_landed_valid": {"Names": 1},
    # The box Touch changes through an inout callee is copied where Main passes it.
    "by_value_field_alias_written_by_inout_callee_landed_valid": {"Main": 1},
    # The by-value Array Pass hands to an inout writer is copied where Main passes it.
    "by_value_array_grown_by_inout_callee_landed_valid": {"Main": 1},
    "by_value_array_grown_two_calls_deep_landed_valid": {"Main": 1},
    "by_value_array_grown_through_recursive_hand_off_landed_valid": {"Primary": 1, "Main": 1},
    # A by-value argument beside an inout one naming the same Array.
    "by_value_argument_beside_its_inout_landed_valid": {"Main": 1},
    "by_value_argument_before_its_inout_landed_valid": {"Main": 1},
    # One Array to several keeping formals of a call: each earlier argument is copied.
    "array_handed_to_two_keeping_formals_landed_valid": {"Build": 1},
    "array_handed_to_three_keeping_formals_landed_valid": {"Main": 2},
    # Detaching and retiring an inout aggregate's fields moves the caller's
    # original value; its later-used shallow alias becomes independent.
    "array_field_detach_caller_alias_landed_valid": {"Main": 1},
    # One storage in two parts of a value: the earlier part is copied.
    "one_storage_in_two_constructor_parts_returned_landed_valid": {"Make": 1},
    "one_storage_through_alias_in_two_parts_landed_valid": {"Make": 1},
    "one_storage_in_two_literal_elements_landed_valid": {"Grid": 1},
    "one_storage_in_two_map_parts_landed_valid": {"Make": 1},
    # The two recursive results are copied; Fresh's new String is not, as the
    # flow summaries stay open across the mutually recursive demand.
    "mutual_recursion_declared_result_stored_valid": {"Main": 2},
    # Maps Clone copies slot by slot: each copy the rules select lands.
    "default_aggregate_move_map_field_set_landed_valid": {"Main": 1},
    "formal_field_map_copy_changed_at_exit_landed_valid": {"GrowAndReturn": 1, "GrowAndFallOff": 1},
    "inout_callee_same_map_field_changed_landed_valid": {
        "AfterMark": 1, "AfterAlias": 1, "AfterNested": 1, "AfterReset": 1, "AfterReplace": 1},
    "local_copy_map_changed_landed_valid": {"MapSetCopy": 1, "MapRemoveCopy": 1},
    "local_copy_map_held_at_field_changed_landed_valid": {
        "AssignedValueChanged": 1, "FieldCopyChanged": 2, "HolderChanged": 1, "InnerPathChanged": 1},
    "local_copy_map_inout_callee_changes_landed_valid": {"Probe": 1, "Record": 1},
    "two_parts_of_distinct_storage_valid": {"Make": 1},
    "string_element_returned_in_result_loop_kept_landed_valid": {"Collect": 1},
    "string_element_returned_by_named_call_past_owner_landed_valid": {"Pick": 1},
    "string_element_returned_by_named_call_owner_changed_landed_valid": {"Main": 1},
    "string_element_inout_owner_named_call_landed_valid": {"Replaced": 1, "Main": 1},
    "readfile_result_borrowed_identity_landed_valid": {"Main": 1},
    "named_string_identity_owned_element_retain_landed_valid": {"Main": 1},
    "named_string_field_make_nodes_drop_landed_valid": {"Main": 1},
    "intent_subject_keeps_string_landed_valid": {"Main": 1},
    "array_result_owned_element_host_method_retainer_landed_valid": {"Main": 1},
    "array_result_owned_defer_late_scalar_retention_landed_valid": {"Main": 1},
}
# Positive fixtures whose loan is copied where it leaves: the routine and
# the value its Clone must wrap, not the call that made the loan.
landed_at_exit = {
    "by_value_getter_lend_kept_landed_valid": ("Collect", "first"),
}
# The analyses a compile runs after the first to land what it selected: an
# `inout` output lands in the first, a String store or getter loan selection
# in one more, and a store or loan no copy can land selects none.
landing_rounds = {
    "string_push_after_loop_walk_landed_valid": 1,
    "string_copy_landing_same_function_batch_valid": 1,
    "string_copy_landing_function_boundary_valid": 2,
    "formal_string_store_same_routine_batch_valid": 1,
    "formal_string_store_multiple_formals_batch_valid": 1,
    "formal_string_push_same_routine_batch_valid": 1,
    "owned_string_push_multi_receiver_batch_valid": 1,
    "formal_string_store_cross_routine_batch_valid": 1,
    "formal_string_store_batch_boundary_valid": 2,
    "output_field_store_of_ref_formal_landed_valid": 1,
    "inout_self_assignment_lands_nothing_valid": 0,
    "nonowner_risk_rebind_clone_landed_valid": 1,
    "nonowner_risk_local_carrier_landed_valid": 1,
    "array_result_owned_element_direct_return_landed_valid": 1,
    "named_string_identity_loop_reassignment_landed_valid": 1,
    "named_string_field_lineage_same_call_retire_grow_landed_valid": 2,
    "named_string_field_lineage_named_mapping_landed_valid": 2,
    "named_string_field_lineage_mixed_return_landed_valid": 2,
    "named_string_field_lineage_recursive_forward_landed_valid": 2,
    "named_string_same_empty_owned_then_borrowed_named_landed_valid": 1,
    "named_string_same_empty_borrowed_then_owned_named_landed_valid": 1,
    "string_inout_returned_literal_then_escape_call_landed_valid": 1,
    "string_inout_unwrapped_literal_then_escape_call_landed_valid": 1,
    "string_inout_returned_literal_then_own_release_landed_valid": 1,
    "borrowed_inout_array_kept_in_result_landed_valid": 1,
    "moved_argument_two_branches_landed_valid": 1,
    "moved_argument_chain_batch_landed_valid": 1,
    "by_value_getter_lend_into_own_formal_landed_valid": 1,
    "returned_alias_lends_formal_valid": 0,
    "loan_carrier_unchanged_by_call_before_loan_valid": 0,
    "loan_owner_read_by_constructor_valid": 0,
    "loan_read_last_before_owner_changed_valid": 0,
    "loan_carrier_handed_on_by_early_return_valid": 0,
    "loan_carrier_read_into_returned_constructor_valid": 0,
    "loan_payload_bound_by_match_valid": 0,
    "loan_payload_binding_passed_to_reader_valid": 0,
    "local_copy_other_side_passed_to_reader_of_other_rows_valid": 0,
    "inout_defers_handed_to_recursive_emitter_valid": 0,
    "by_value_array_grown_by_inout_callee_landed_valid": 1,
    "by_value_array_grown_by_inout_callee_last_use_valid": 0,
    "by_value_array_handed_to_by_value_reader_valid": 0,
    "by_value_array_grown_two_calls_deep_landed_valid": 1,
    "by_value_array_grown_through_recursive_hand_off_landed_valid": 1,
    "by_value_cursor_set_through_recursive_parser_valid": 0,
    "by_value_array_popped_by_inout_callee_valid": 0,
    "by_value_array_handed_to_generic_reader_valid": 0,
    "inout_string_chain_popped_by_recursive_parser_valid": 1,
    "by_value_argument_beside_its_inout_landed_valid": 1,
    "by_value_argument_before_its_inout_landed_valid": 1,
    "scalar_and_string_beside_their_inout_valid": 0,
    "by_value_array_to_two_reading_formals_valid": 0,
    "array_handed_to_two_keeping_formals_landed_valid": 1,
    "array_handed_to_three_keeping_formals_landed_valid": 1,
    "array_field_detach_two_own_valid": 0,
    "array_field_detach_caller_alias_landed_valid": 1,
    "array_field_detach_later_declared_own_valid": 0,
    "one_storage_in_two_constructor_parts_returned_landed_valid": 1,
    "one_storage_through_alias_in_two_parts_landed_valid": 1,
    "one_storage_in_two_literal_elements_landed_valid": 1,
    "one_storage_in_two_map_parts_landed_valid": 1,
    "local_copy_map_changed_landed_valid": 1,
    "two_parts_of_distinct_storage_valid": 0,
    "distinct_fields_of_a_local_copy_in_two_parts_valid": 0,
    "lent_path_copy_grown_landed_valid": 1,
    "borrowed_owner_loan_grown_landed_valid": 1,
    "string_crossing_window_pair_landed_valid": 1,
    "by_value_lend_then_owner_cloned_valid": 1,
    "by_value_lend_then_shallow_clone_grown_invalid": 1,
    "array_result_local_owned_element_constructor_landed_valid": 1,
    "array_result_local_owned_element_external_keep_landed_valid": 1,
    "array_result_local_owned_element_field_retention_landed_valid": 1,
    "array_result_owned_element_array_assign_landed_valid": 1,
    "array_result_owned_element_device_write_landed_valid": 1,
    "array_result_owned_element_event_retaining_handler_landed_valid": 1,
    "array_result_owned_element_local_return_landed_valid": 1,
    "array_result_owned_element_result_err_landed_valid": 1,
    "array_result_owned_element_result_ok_landed_valid": 1,
    "array_result_owned_element_secure_slot_initializer_landed_valid": 1,
    "array_result_owned_element_secure_slot_write_landed_valid": 1,
    "array_result_owned_element_slot_assign_landed_valid": 1,
    "array_result_owned_element_slot_initializer_landed_valid": 1,
    "array_result_owned_element_slot_write_landed_valid": 1,
    "array_result_owned_element_spawn_landed_valid": 1,
    "array_result_owned_element_tuple_binding_landed_valid": 1,
    "array_result_owned_element_tuple_direct_landed_valid": 1,
    "array_result_owned_element_tuple_return_landed_valid": 1,
    "array_result_owned_element_write_view_landed_valid": 1,
    "array_result_owned_retained_element_landed_valid": 1,
    "named_string_field_lineage_root_overwrite_landed_valid": 1,
    "option_payload_mixed_borrow_landed_valid": 1,
    "string_element_tostring_returned_landed_valid": 1,
    "string_join_shadowed_returns_part_landed_valid": 1,
    "aggregate_string_loan_local_owner_landed_valid": 1,
    "aggregate_string_loan_owner_kept_landed_valid": 1,
    "aggregate_string_loan_owner_moved_landed_valid": 1,
    "aggregate_string_loan_owner_mutated_landed_valid": 1,
    "aggregate_string_loan_struct_escape_landed_valid": 1,
    "callee_alias_branch_return_record_landed_valid": 1,
    "callee_alias_return_record_landed_valid": 1,
    "declared_array_borrow_landed_valid": 1,
    "default_string_option_return_landed_valid": 1,
    "default_string_string_return_landed_valid": 1,
    "result_frame_formal_array_push_landed_valid": 1,
    "result_frame_formal_nominal_landed_valid": 1,
    "result_frame_formal_return_landed_valid": 1,
    "result_frame_formal_some_landed_valid": 1,
    "return_loan_direct_slot_landed_valid": 1,
    "return_loan_inout_landed_valid": 1,
    "return_loan_mutated_literal_assignment_landed_valid": 1,
    "return_loan_mutated_literal_bodyful_inout_landed_valid": 1,
    "return_loan_mutated_literal_opaque_inout_landed_valid": 1,
    "return_loan_mutated_literal_opaque_slot_landed_valid": 1,
    "return_loan_mutated_literal_two_aliases_landed_valid": 1,
    "return_loan_site_two_aliases_landed_valid": 1,
    "return_loan_staged_slot_landed_valid": 1,
    "return_loan_two_alias_assignment_landed_valid": 1,
    "return_loan_using_alias_opaque_inout_landed_valid": 1,
    "return_loan_wrap_slot_landed_valid": 1,
    "stdlib_tostring_string_forward_landed_valid": 1,
    "match_payload_return_moves_landed_valid": 1,
    "default_aggregate_move_field_alias_returned_landed_valid": 1,
    "array_slice_view_returned_landed_valid": 1,
    "by_value_formal_rebound_to_result_grown_landed_valid": 1,
    "by_value_getter_lend_inout_owner_landed_valid": 1,
    "by_value_getter_result_grown_landed_valid": 1,
    "by_value_getter_result_local_returned_grown_landed_valid": 1,
    "by_value_getter_result_passed_grown_landed_valid": 1,
    "by_value_getter_result_reassigned_grown_landed_valid": 1,
    "constructor_result_grown_landed_valid": 1,
    "default_aggregate_move_field_alias_passed_on_landed_valid": 1,
    "default_aggregate_move_field_alias_store_landed_valid": 1,
    "default_aggregate_move_field_element_write_landed_valid": 1,
    "default_aggregate_move_field_set_landed_valid": 1,
    "default_aggregate_move_field_store_landed_valid": 1,
    "default_aggregate_move_option_field_alias_store_landed_valid": 1,
    "inout_owned_param_input_return_moves_landed_valid": 1,
    "intent_action_stores_by_value_moves_landed_valid": 1,
    "optional_wrap_result_grown_landed_valid": 1,
    "option_payload_result_grown_landed_valid": 1,
    "path_alias_returned_in_constructor_grown_landed_valid": 1,
    "unit_enum_constant_constructor_moves_landed_valid": 1,
    "array_result_owned_element_inout_escape_landed_valid": 1,
    "string_element_tostring_opaque_call_landed_valid": 1,
    "string_argument_kept_copy_landed_valid": 1,
    "formal_string_store_landing_ref_push_valid": 0,
    "formal_string_store_landing_local_receiver_valid": 1,
    "formal_string_store_landing_value_map_valid": 1,
    "formal_array_store_local_receiver_invalid": 0,
    "formal_array_store_field_take_landed_valid": 1,
    "formal_string_store_local_alias_set_invalid": 0,
    "by_value_getter_lend_owner_written_landed_valid": 1,
    "by_value_getter_lend_shared_result_owner_changed_invalid": 0,
    "by_value_getter_lend_returned_landed_valid": 1,
    "by_value_getter_lend_shared_result_returned_invalid": 0,
    "by_value_getter_lend_inout_passed_invalid": 0,
    "by_value_getter_lend_inout_passed_landed_valid": 1,
    "by_value_getter_lend_inout_then_returned_invalid": 0,
    "by_value_getter_lend_inout_then_returned_landed_valid": 1,
    "default_aggregate_move_reuse_invalid": 0,
    "default_aggregate_move_cloned_growth_invalid": 0,
    "formal_array_store_inout_output_invalid": 0,
    "formal_store_landing_struct_output_valid": 1,
    "formal_store_landing_alias_output_valid": 1,
    "formal_store_landing_struct_output_shallow_invalid": 0,
    "array_push_loan_inout_landed_valid": 1,
    "array_push_loan_inout_nested_clone_landed_valid": 1,
    "aggregate_move_void_call_landed_valid": 1,
    "aggregate_move_void_call_nested_clone_landed_valid": 1,
    "by_value_getter_lend_result_grown_landed_valid": 1,
    "default_aggregate_move_shared_field_return_landed_valid": 1,
    "default_aggregate_move_shared_field_return_nested_clone_landed_valid": 1,
    "by_value_getter_lend_result_lent_on_valid": 0,
    "by_value_getter_lend_string_reassigned_valid": 0,
    "by_value_getter_lend_option_string_unwrapped_valid": 0,
    "by_value_getter_lend_string_collected_valid": 0,
    "option_string_result_reassigned_valid": 0,
    "string_struct_result_reassigned_valid": 0,
    "by_value_getter_lend_result_path_grown_landed_valid": 1,
    "by_value_getter_lend_result_passed_grown_landed_valid": 1,
    "by_value_getter_lend_in_constructor_passed_grown_landed_valid": 1,
    "by_value_getter_second_lend_grown_landed_valid": 1,
    "by_value_getter_second_lend_formal_grown_landed_valid": 1,
    "by_value_getter_lend_in_constructor_grown_landed_valid": 1,
    "by_value_getter_lend_owner_in_constructor_grown_landed_valid": 1,
    "by_value_getter_two_lends_grown_landed_valid": 1,
    "local_let_alias_grown_landed_valid": 1,
    "local_let_alias_source_grown_landed_valid": 1,
    "local_assigned_alias_grown_landed_valid": 1,
    "local_constructor_alias_grown_landed_valid": 1,
    "local_array_literal_alias_grown_landed_valid": 1,
    "local_pushed_alias_grown_landed_valid": 1,
    "formal_let_alias_grown_landed_valid": 1,
    "local_copy_of_copy_grown_landed_valid": 1,
    "local_copy_source_element_set_landed_valid": 1,
    "local_copy_source_inout_grown_landed_valid": 1,
    "local_let_alias_read_valid": 0,
    "local_source_grown_before_copy_valid": 0,
    "local_copy_grown_source_unused_valid": 0,
    "local_copy_in_loop_valid": 0,
    "local_copy_rebound_valid": 0,
    "local_copy_inout_read_only_callee_valid": 0,
    "local_let_alias_grown_invalid": 0,
    "local_copied_path_then_moved_landed_valid": 1,
    "local_copy_changed_then_moved_landed_valid": 1,
    "owned_copy_string_push_landed_valid": 1,
    "local_copy_source_sorted_landed_valid": 1,
    "array_sort_after_handoff_ref_reader_valid": 0,
    "by_value_formal_array_sorted_landed_valid": 1,
    "by_value_getter_lend_result_passed_grown_invalid": 0,
    "by_value_getter_second_lend_grown_invalid": 0,
}
borrow_refusal = ("PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape")
released_refusal = ("PGY_SEM_MOVE_FROM_RELEASED", "semantic:move:from_released")
builtin_refusal = ("PGY_SEM_BUILTIN_ARGS_INVALID", "semantic:builtin:signature_mismatch")
# Exact emitted diagnostics for the reached release/retention cases. In
# particular, a stale read can also trigger a checked builtin-arity cascade;
# it must not be mistaken for a fresh BORROW-only refusal or a MIR artifact.
release_lifetime_refusals = {
    # A call that grows its default-mode aggregate argument in place moves it.
    "default_aggregate_move_reuse_invalid": [(*released_refusal, 21, 28), (*released_refusal, 22, 34)],
    "returned_alias_lends_formal_grown_invalid": [(*released_refusal, 29, 15)],
    # A loan of a borrowed owner that leaves stays refused where Clone copies
    # its result only by value.
    "borrowed_owner_loan_grown_by_value_clone_invalid": [(*borrow_refusal, 24, 5)],
    # A window pair whose text passes through a routine stays unproved.
    # A change through a by-value Clone of a lent owner reaches the loan.
    "by_value_lend_then_shallow_clone_grown_invalid": [(*released_refusal, 26, 40)],
    "string_window_pair_other_call_invalid": [
        ("PGY_SEM_STRING_WINDOW_EXTENT_UNPROVEN", "semantic:string_window:extent_unproven", 14, 12)],
    # An inout callee that changes the map of Arrays a caller's local copy
    # shares refuses the copy's later uses, as a MapSet in the routine does.
    "local_copy_inout_callee_changes_invalid": [
        (*released_refusal, 13, 19), (*released_refusal, 19, 12), (*released_refusal, 20, 19)],
    # A map of Arrays, Set, List or Queue set, removal or push through a local
    # copy refuses the source's next use, one site per change.
    "local_copy_collection_changed_invalid": [
        (*released_refusal, 9, 19), (*released_refusal, 17, 19), (*released_refusal, 25, 19),
        (*released_refusal, 33, 19), (*released_refusal, 41, 20), (*released_refusal, 49, 20),
        (*released_refusal, 58, 20), (*released_refusal, 66, 21)],
    # A value stored into a map that Clone copies only by value stays shared.
    "collection_value_store_shallow_grown_invalid": [(*released_refusal, 14, 35)],
    # A whole Array read after a call kept its descriptor: no element copy.
    "array_descriptor_kept_then_read_invalid": [(*borrow_refusal, 22, 16)],
    # An inout callee that writes the field a caller copied, five ways.
    "inout_callee_same_field_changed_invalid": [
        (*released_refusal, 38, 20), (*released_refusal, 44, 20), (*released_refusal, 50, 20),
        (*released_refusal, 56, 20), (*released_refusal, 62, 20)],
    # A lent local stored into an inout output that did not hold it escapes
    # the ref index: Fill's summary and its caller both refuse.
    "ref_loan_local_stored_into_inout_output_invalid": [(*borrow_refusal, 22, 6), (*borrow_refusal, 29, 10)],
    # A copy of an inout formal's field changed and not assigned back
    # refuses each exit: two returns and the end of a body.
    "formal_field_copy_changed_at_exit_invalid": [
        (*released_refusal, 14, 29), (*released_refusal, 15, 5), (*released_refusal, 18, 6)],
    # A value assigned into a field shares that field: a change through
    # either side refuses the other.
    "local_copy_held_at_field_changed_invalid": [
        (*released_refusal, 30, 26), (*released_refusal, 30, 5), (*released_refusal, 38, 20),
        (*released_refusal, 38, 5), (*released_refusal, 48, 20), (*released_refusal, 56, 24)],
    "default_aggregate_move_loop_reuse_invalid": [(*released_refusal, 18, 32)],
    "default_aggregate_move_branch_use_invalid": [(*released_refusal, 20, 41)],
    # A call result that holds a formal's storage is a path of that formal:
    # growing it changes the storage the caller's copy shares.
    "enum_formal_matched_payload_grown_invalid": [(*released_refusal, 27, 11)],
    "by_value_getter_lend_result_passed_grown_invalid": [(*released_refusal, 33, 48), (*released_refusal, 33, 83)],
    "by_value_getter_second_lend_grown_invalid": [(*released_refusal, 30, 101)],
    "local_let_alias_grown_invalid": [(*released_refusal, 25, 73)],
    # A field of a struct declared before the loop moves again next iteration.
    "collection_member_move_loop_outer_source_invalid": [(*released_refusal, 28, 41)],
    # An inout fill of a by-value parameter from a second input moves it.
    # Take's result carries the ref input's stored field, a loan Main then
    # grows (17:23); without that relation only the assignment refusal
    # would remain.
    "owned_param_field_store_ref_return_invalid": [
        (*borrow_refusal, 0, 0), (*borrow_refusal, 17, 23)],
    # Route's enum payload lends the ref input, and a case body replaces
    # Main's admission while the payload lives; Main's call is refused for
    # that loan (32:28). An enum constructor without its result relation
    # would lend nothing and lose the refusal.
    "enum_payload_constructor_ref_field_owner_changed_invalid": [(*borrow_refusal, 32, 28)],
    "array_local_owned_push_double_drop_invalid": [(*released_refusal, 5, 27)],
    "array_local_owned_push_use_after_drop_invalid": [
        (*released_refusal, 5, 21), (*builtin_refusal, 5, 21)],
    "array_result_owned_double_drop_invalid": [(*released_refusal, 10, 27)],
    "array_result_owned_use_after_drop_invalid": [
        (*released_refusal, 10, 21), (*builtin_refusal, 10, 21)],
    "array_result_owned_defer_drop_then_direct_drop_invalid": [(*borrow_refusal, 10, 27)],
    "array_result_owned_defer_drop_then_own_invalid": [(*borrow_refusal, 14, 17)],
    "array_result_owned_defer_late_array_alias_invalid": [(*borrow_refusal, 12, 16)],
    "array_result_owned_defer_named_array_retainer_invalid": [(*borrow_refusal, 18, 39)],
    "array_result_owned_element_defer_after_drop_invalid": [(*borrow_refusal, 10, 27)],
    "array_result_owned_element_defer_lifo_drop_then_read_invalid": [(*borrow_refusal, 10, 35)],
    "array_result_owned_element_defer_mutable_reassign_invalid": [(*borrow_refusal, 12, 27)],
    "array_result_owned_element_inner_defer_then_drop_invalid": [(*borrow_refusal, 12, 27)],
    # A defer's release is admitted again where the routine exits: the
    # alias moved rows (13:6), KeepRows may keep it (9:6), the return hands
    # it on (10:5), the constructor holds it (12:5), or RetainRows, judged
    # after every body, does not preserve it (15:35).
    "array_result_owned_defer_alias_retainer_invalid": [(*borrow_refusal, 13, 6)],
    "array_result_owned_defer_direct_retainer_invalid": [(*borrow_refusal, 15, 35)],
    "array_result_owned_defer_host_own_invalid": [(*borrow_refusal, 19, 28)],
    "array_result_owned_defer_return_invalid": [(*borrow_refusal, 10, 5)],
    "array_result_owned_defer_direct_opaque_invalid": [(*borrow_refusal, 9, 6)],
    "array_result_owned_defer_constructor_return_invalid": [(*borrow_refusal, 12, 5)],
    "array_release_host_own_reuse_invalid": [
        (*released_refusal, 19, 21), (*builtin_refusal, 19, 21)],
    "physical_return_opaque_invalid": [
        (*borrow_refusal, 4, 6), (*borrow_refusal, 5, 14),
        (*borrow_refusal, 10, 20)],
    "selector_method_retaining_receiver_invalid": [(*borrow_refusal, 19, 15)],
    "named_string_identity_loop_reassignment_invalid": [(*borrow_refusal, 17, 27)],
    # A call keeps the Array's String in a node Clone copies only by value;
    # the deep drop after it is refused for it.
    "named_string_field_shallow_node_drop_invalid": [(*borrow_refusal, 41, 27)],
    # The corresponding read-only inout case is a landed positive below: its
    # String store receives one independent Clone and the call adds no escape.
    # One that hands the local to an own formal keeps it: every caller refuses.
    "ref_string_through_inout_keeper_invalid": [
        (*borrow_refusal, 43, 6), (*borrow_refusal, 59, 6), (*borrow_refusal, 60, 28),
        (*borrow_refusal, 66, 26)],
    # An owner assigned after its view is made, in the loop before the view,
    # or from a view of itself, changes under a loan: each later use refuses.
    "loan_owner_assigned_after_loan_invalid": [
        (*released_refusal, 32, 33), (*released_refusal, 33, 22), (*released_refusal, 37, 45)],
    "loan_owner_assigned_in_loop_before_loan_invalid": [
        (*released_refusal, 32, 22), (*released_refusal, 33, 33), (*released_refusal, 37, 45)],
    "loan_owner_assigned_from_its_own_loan_invalid": [
        (*released_refusal, 28, 22), (*released_refusal, 33, 33), (*released_refusal, 37, 45)],
    # An indexed assignment after the loan writes through it into the owner's
    # storage; the parser leaves the assignment without a source position.
    "loan_owner_written_by_indexed_assignment_after_loan_invalid": [(*released_refusal, 29, 42)],
    # A view kept past its loop in a local, an Array or an inout callee's
    # Array declared before the loop, or read in the statement that moves its
    # owner, still lends the owner when it moves: each later use refuses.
    "loan_kept_outside_its_loop_invalid": [
        (*released_refusal, 35, 33), (*released_refusal, 39, 34), (*released_refusal, 40, 26)],
    "loan_pushed_outside_its_loop_invalid": [
        (*released_refusal, 34, 33), (*released_refusal, 38, 34), (*released_refusal, 39, 26)],
    "loan_kept_by_inout_callee_outside_its_loop_invalid": [
        (*released_refusal, 33, 33), (*released_refusal, 37, 34), (*released_refusal, 38, 26)],
    "loan_read_in_the_statement_that_moves_its_owner_invalid": [(*released_refusal, 36, 47)],
    # A constructor reads a loan's owner, but a value it builds from the
    # owner's fields shares their storage: changed under the loan, written by
    # a callee or returned, it refuses. An in-place permutation is no constructor.
    "loan_owner_constructor_copy_changed_invalid": [(*released_refusal, 31, 21)],
    "loan_owner_constructor_copy_written_by_callee_invalid": [
        (*released_refusal, 35, 44), (*released_refusal, 35, 55), (*released_refusal, 35, 67),
        (*released_refusal, 36, 51)],
    "loan_owner_constructor_copy_returned_invalid": [
        (*released_refusal, 34, 31), (*released_refusal, 34, 42), (*released_refusal, 34, 54),
        (*released_refusal, 34, 91)],
    "loan_owner_permuted_in_place_invalid": [(*released_refusal, 30, 57), (*released_refusal, 31, 65)],
    # A loan read after its owner changed, in a loop that also changes it, or
    # by a defer that runs at the exit is read after the change: each refuses.
    "loan_read_after_owner_changed_invalid": [(*released_refusal, 31, 29)],
    "loan_read_in_loop_that_changes_owner_invalid": [(*released_refusal, 35, 33)],
    "loan_read_by_defer_after_owner_changed_invalid": [(*released_refusal, 31, 29), (*released_refusal, 32, 35)],
    # A read lasts until its statement ends: the call it is an argument of
    # reads it after a later argument changed the owner.
    "loan_read_by_call_after_its_owner_changed_in_its_arguments_invalid": [(*released_refusal, 41, 37)],
    # Facts that carry a loan handed into an inout output take the loan out
    # with them: the owner's later change refuses.
    "loan_carrier_handed_into_output_before_owner_changed_invalid": [(*released_refusal, 33, 29)],
    # A value built from a carrier holds the loan: given to a callee that
    # writes it, the loan leaves into a writer.
    "loan_carrier_read_into_constructor_a_callee_writes_invalid": [(*released_refusal, 38, 39)],
    # A payload a match binds carries the subject's loan: written, it changes
    # the owner the subject lends.
    "loan_payload_bound_by_match_written_invalid": [(*released_refusal, 32, 47), (*released_refusal, 35, 36)],
    # A payload binding's root is its match subject's: a callee that writes
    # it changes the subject that carries the loan, and a call's result as
    # subject roots nothing the proof follows.
    "loan_payload_binding_written_by_callee_invalid": [(*released_refusal, 38, 47), (*released_refusal, 43, 36)],
    "loan_payload_binding_of_call_subject_written_invalid": [(*released_refusal, 37, 47), (*released_refusal, 42, 36)],
    # A copy changed below a value a call takes is used there when the callee
    # reads the changed rows, directly or through a callee of its own, or
    # copies the value whole into a local whose reads are not named.
    "local_copy_other_side_passed_to_reader_of_changed_rows_invalid": [(*released_refusal, 35, 41)],
    "local_copy_other_side_passed_through_to_reader_of_changed_rows_invalid": [(*released_refusal, 39, 41)],
    "local_copy_other_side_aliased_whole_by_callee_invalid": [(*released_refusal, 37, 41)],
    # Under a by-value formal the change reaches the caller's value through
    # the storage the formal shares: the copy must land, and none can here.
    "local_copy_of_by_value_formal_passed_to_reader_of_other_rows_invalid": [(*released_refusal, 36, 41)],
    # An inout Array handed on whole as an inout argument keeps what the
    # callee keeps: an alias of it written by an inout callee, or a callee
    # that keeps its descriptor, still refuses the next call that takes it.
    "inout_defers_alias_written_by_inout_callee_invalid": [(*borrow_refusal, 12, 30)],
    "inout_defers_kept_by_callee_before_nested_call_invalid": [(*borrow_refusal, 13, 25), (*borrow_refusal, 14, 30)],
    # A by-value Array formal handed to an inout callee moves; a later use
    # of maps Clone cannot copy whole is refused.
    "by_value_array_of_maps_grown_by_inout_callee_invalid": [(*released_refusal, 18, 32)],
    # ArrayPop keeps nothing of an inout chain; a callee that keeps the chain
    # before popping it still refuses the nested call that takes it.
    "inout_string_chain_kept_before_pop_invalid": [(*borrow_refusal, 13, 9)],
    # A by-value argument beside an inout one naming the same variable is
    # copied; an Array of maps Clone cannot copy whole is refused.
    "by_value_maps_beside_their_inout_invalid": [(*borrow_refusal, 13, 31)],
    # An Array an earlier call keeps too is not copied at the later call.
    "array_kept_by_earlier_call_and_two_formals_invalid": [(*borrow_refusal, 22, 24)],
    # A refused text is not reported again as a TextBuilderAppend type mismatch.
    "text_builder_append_of_released_element_invalid": [(*borrow_refusal, 10, 31)],
    # One map of Arrays in two parts of a value: no copy can stand in.
    "one_storage_in_two_array_map_parts_invalid": [(*released_refusal, 11, 26)],
    # Snapshot currentness must not replay ownership early. The actual owned
    # element consumer still replays the receipt and emits this refusal.
    "constructor_string_field_snapshot_before_finalize_invalid": [(*borrow_refusal, 14, 6)],
    "named_string_field_alias_mutation_invalid": [(*borrow_refusal, 4, 6)],
    "formal_string_store_local_alias_set_invalid": [(*borrow_refusal, 16, 24)],
    "struct_clone_generic_ref_returned_invalid": [(*borrow_refusal, 13, 25)],
    "map_clone_array_value_ref_returned_invalid": [(*borrow_refusal, 10, 42)],
    "struct_clone_shared_field_ref_returned_invalid": [(*borrow_refusal, 18, 20)],
    "struct_clone_subject_field_ref_returned_invalid": [(*borrow_refusal, 19, 21)],
    "by_value_getter_lend_shared_result_owner_changed_invalid": [(*released_refusal, 23, 20)],
    "default_aggregate_move_cloned_growth_invalid": [(*released_refusal, 18, 34), (*released_refusal, 19, 34)],
    "option_payload_unwrap_no_deep_drop_invalid": [(*borrow_refusal, 11, 31)],
    "option_payload_literal_array_mixed_returns_invalid": [(*borrow_refusal, 12, 22)],
    "option_payload_literal_array_set_invalid": [(*borrow_refusal, 9, 22)],
    "option_payload_literal_array_push_invalid": [(*borrow_refusal, 9, 22)],
    "option_payload_literal_array_owned_push_invalid": [(*borrow_refusal, 3, 26)],
    "option_payload_literal_array_alias_invalid": [(*borrow_refusal, 9, 22)],
    "option_payload_literal_array_rebind_invalid": [(*borrow_refusal, 8, 32)],
    # The read-only helper takes no element ownership; the Some payload is
    # the retention that lacks a permanent-literal proof after the handoff.
    "option_payload_literal_array_helper_handoff_invalid": [(*borrow_refusal, 14, 26)],
    "option_payload_mixed_array_return_invalid": [(*borrow_refusal, 11, 30)],
    "readfile_result_defer_released_path_invalid": [(*borrow_refusal, 12, 27)],
    "nonowner_risk_alias_owned_insert_invalid": [
        (*borrow_refusal, 16, 21), (*borrow_refusal, 18, 30)],
    "nonowner_risk_later_actual_handoff_invalid": [(*borrow_refusal, 22, 13)],
    "nonowner_risk_lambda_capture_invalid": [(*borrow_refusal, 16, 56)],
    # The String-field getter returns its receiver's storage. EchoWrapper
    # and Inspect lend it on; MakeInspected returns that loan past the
    # holder it owns (24:5). Do not treat this as a physical-storage grant.
    "receiver_result_string_field_escape_invalid": [(*borrow_refusal, 24, 5)],
    # A host field is `self` storage; these returns of it are refused as
    # `self` escapes, not by an incomplete frame.
    "world_ref_self_member_return_invalid": [(*borrow_refusal, 19, 10)],
    "method_self_array_field_return_invalid": [(*borrow_refusal, 7, 10)],
    # A match binding carries the matched enum's storage; returning it moves
    # the by-value enum argument.
    # A receiver the action changes follows its self formal's summary: Get
    # lends its world's subject storage, and the action writes that world,
    # so the loan's owner changes under it (31:24).
    "subject_action_returns_self_ref_invalid": [(*borrow_refusal, 31, 24)],
    # Split is modelled (every part copied), so the action's self summary is
    # exact: Get lends its world's subject storage, and the action writes
    # that world, so the loan's owner changes under it (32:24).
    "receiver_unsupported_ref_return_invalid": [(*borrow_refusal, 32, 24)],
    # A unit enum constant carries no storage; the frame completes and the
    # returned Array still moves its argument.
    # A Slice view is its receiver's storage, so Window lends its Array;
    # MakeWindow returns that loan past the Array it owns.
    "slice_view_ref_return_invalid": [(*borrow_refusal, 9, 5)],
    # The local declared after the output call is a local, so its loan of
    # graph escapes only where it really does: into the channel.
    # A release on a path that reaches the read stays after the if.
    "string_released_in_then_falling_invalid": [(*borrow_refusal, 13, 25)],
    "string_released_in_else_falling_invalid": [(*borrow_refusal, 15, 25)],
    "string_released_before_returning_if_invalid": [(*borrow_refusal, 15, 25)],
    "ref_loan_local_after_output_call_channel_invalid": [
        (*borrow_refusal, 11, 6), (*borrow_refusal, 28, 25)],
    # No caller proves a loan of a receiver, so a `ref self` method's
    # return of its own storage stays refused where it returns.
    "receiver_ref_self_local_return_invalid": [(*borrow_refusal, 19, 10)],
    "receiver_ref_self_string_return_invalid": [(*borrow_refusal, 6, 10)],
    # A push stores its value in the Array it names: a local Array carries
    # the loan where it goes (here past its owner, 27:5).
    "array_push_loan_past_owner_invalid": [(*borrow_refusal, 27, 5)],
    # A stored struct Clone copies only by value lands nothing, so the
    # caller's later use of its context reads a moved value.
    "formal_store_landing_struct_output_shallow_invalid": [(*released_refusal, 26, 28)],
    # A handoff a return value makes ends the path: the next iteration's
    # Contains is not refused for it, and the handoff is not judged against
    # its own call. A call before it that may rebind the Array still refuses
    # it (21:23), and two calls in one return value still run in order (24:45).
    "array_handoff_after_rebinding_call_invalid": [(*borrow_refusal, 21, 23)],
    "array_handoff_same_return_value_invalid": [(*borrow_refusal, 24, 45)],
    # A defer releases its Array when the routine exits, so at each exit the
    # Array must still own its storage: an own handoff (14:5), returning the
    # binding (7:5), a fallthrough after an extern keeps it (5:6), or a
    # constructor (13:5) refuses that exit. A call judged after every body
    # (a retaining Keep, a rebinding Swap) refuses the defer's release,
    # ordered after it at the exit (17:23, 20:23, 15:23).
    "defer_array_drop_return_keep_invalid": [(*borrow_refusal, 17, 23)],
    "defer_array_drop_reader_then_keep_invalid": [(*borrow_refusal, 20, 23)],
    "defer_array_drop_own_handoff_invalid": [(*borrow_refusal, 14, 5)],
    "defer_array_drop_return_binding_invalid": [(*borrow_refusal, 7, 5)],
    "defer_array_drop_fallthrough_kept_invalid": [(*borrow_refusal, 5, 6)],
    "defer_owned_strings_return_constructor_invalid": [(*borrow_refusal, 13, 5)],
    "defer_array_drop_after_rebinding_call_invalid": [(*borrow_refusal, 15, 23)],
    # The block ends rows while the facts Keep returned still hold it (18:27).
    "defer_array_drop_block_kept_invalid": [(*borrow_refusal, 18, 27)],
    # An own handoff in the cleanup takes rows at exit like a release (21:17).
    "defer_own_handoff_cleanup_kept_invalid": [(*borrow_refusal, 21, 17)],
    # A lambda falls through to its own defer (9:27).
    "defer_array_drop_lambda_kept_invalid": [(*borrow_refusal, 9, 27)],
    # Storing the ref view into the inout output escapes it (25:6, 33:16).
    "ref_view_beside_unit_variant_stored_invalid": [
        (*borrow_refusal, 25, 6), (*borrow_refusal, 33, 16)],
    # A by-value formal lends what a read-and-return call takes from it when
    # the routine uses it again, and only while the loan stays here and
    # nothing changes it: a push of the loan into an inout Array (14:30), a
    # replaced element (13:20) or a Grow that pushes onto the arena (19:27)
    # makes the call a move again, as does a loan returned past a local (14:38).
    "by_value_getter_lend_shared_result_returned_invalid": [(*released_refusal, 24, 38)],
    "by_value_getter_lend_inout_passed_invalid": [(*released_refusal, 28, 38)],
    "by_value_getter_lend_inout_then_returned_invalid": [(*released_refusal, 28, 46)],
    # A store through an access path of a by-value aggregate changes storage
    # the caller's aggregate shares, so the call moves the argument.
    "default_aggregate_move_nested_field_store_invalid": [(*released_refusal, 21, 39)],
    "default_aggregate_move_map_field_set_invalid": [(*released_refusal, 17, 41)],
    # A local that starts at None() and is then bound to a ref input's field
    # lends that input; Make returns the loan past its plan (29:5).
    "option_none_local_argument_kept_invalid": [(*borrow_refusal, 29, 5)],
    # A Slice view of a by-value Array is its storage: handed to a routine
    # that returns it and returned, the call moves the Array (15:15).
    # A local or loop variable that takes a path of the arena is an alias of
    # its storage: returned (22:35), handed to a routine that returns it
    # (27:35), or iterated and pushed onto (23:39, 19:33), the call moves.
    "default_aggregate_move_loop_element_store_invalid": [(*released_refusal, 23, 39)],
    "default_aggregate_move_array_loop_element_store_invalid": [(*released_refusal, 19, 33)],
    # Text finished with a non-result allocator lives in it. A pool's text
    # read after AllocatorDestroy (16:11, 17:11) or returned past the pool
    # (13:27) is refused; so is a builder finished with a pool and returned
    # after the destroy (5:18 and 7:12, 6:18 and 8:12 after a rebind).
    "allocator_pool_read_after_destroy_invalid": [(*borrow_refusal, 16, 11)],
    "allocator_pool_loan_returned_invalid": [(*borrow_refusal, 13, 27)],
    "allocator_pool_loan_kept_outside_invalid": [(*borrow_refusal, 17, 11)],
    "pool_builder_birth_invalid": [(*borrow_refusal, 5, 18), (*borrow_refusal, 7, 12)],
    "rebound_pool_builder_birth_invalid": [(*borrow_refusal, 6, 18), (*borrow_refusal, 8, 12)],
    "default_aggregate_move_nested_field_write_invalid": [(*released_refusal, 20, 27)],
    # An element of a by-value Array formal is such a path too.
    "default_aggregate_move_array_element_alias_store_invalid": [(*released_refusal, 17, 33)],
    # TakeFirst returns a String of its arena but also pushes onto it, which
    # the loan proof sees at the lending call itself (16:53). Make hands its
    # local arena on while the first loan lives (15:35).
    "by_value_getter_lend_growing_invalid": [(*released_refusal, 16, 53)],
    # An inout formal may change while the loan lives, so it is not lent.
}
paired = (
    ("unit_enum_qualified_result_valid", True),
    ("option_payload_fresh_literal_none_valid", True),
    ("option_payload_local_alias_valid", True),
    ("option_payload_shared_carrier_valid", True),
    ("option_payload_mixed_borrow_landed_valid", True),
    ("option_payload_unwrap_no_deep_drop_invalid", False),
    ("option_payload_literal_array_return_valid", True),
    ("option_payload_mixed_array_return_invalid", False),
    ("option_payload_literal_array_set_invalid", False),
    ("option_payload_literal_array_push_invalid", False),
    ("option_payload_literal_array_owned_push_invalid", False),
    ("option_payload_literal_array_alias_invalid", False),
    ("option_payload_literal_array_rebind_invalid", False),
    ("option_payload_literal_array_mixed_returns_invalid", False),
    ("option_payload_literal_array_helper_handoff_invalid", False),
    ("readfile_result_direct_valid", True),
    ("readfile_result_borrowed_path_valid", True),
    ("readfile_result_named_return_valid", True),
    ("readfile_result_defer_valid", True),
    ("readfile_result_defer_late_valid", True),
    ("readfile_result_distinct_loop_valid", True),
    ("readfile_result_multipart_defer_valid", True),
    ("readfile_result_control_flow_valid", True),
    ("readfile_result_borrowed_identity_landed_valid", True),
    ("readfile_result_defer_released_path_invalid", False),
    ("nonowner_risk_option_reader_valid", True),
    ("nonowner_risk_shallow_loop_valid", True),
    ("nonowner_risk_alias_owned_insert_invalid", False),
    ("nonowner_risk_rebind_clone_landed_valid", True),
    ("nonowner_risk_branch_owned_insert_landed_valid", True),
    ("nonowner_risk_later_loop_owned_landed_valid", True),
    ("nonowner_risk_later_actual_handoff_invalid", False),
    ("nonowner_risk_local_carrier_landed_valid", True),
    ("nonowner_risk_safe_then_owned_landed_valid", True),
    ("nonowner_risk_owned_then_safe_landed_valid", True),
    ("nonowner_risk_lambda_capture_invalid", False),
    ("secure_implicit_token_safe", True),
    ("secure_implicit_token_keep_invalid", False),
    ("result_builder_birth_valid", True),
    ("retaining_capacity_builder_birth_invalid", False),
    ("pool_builder_birth_invalid", False),
    ("rebound_pool_builder_birth_invalid", False),
    ("wrapper_pool_builder_birth_invalid", False),
    ("wrapper_opaque_builder_birth_invalid", False),
    ("wrapper_rebound_builder_birth_invalid", False),
    ("wrapper_branch_builder_birth_invalid", False),
    # The bounded Finish proof does not follow safe Result-only helpers or
    # recover after any allocator reassignment: these are precision losses.
    ("wrapper_result_builder_birth_unready", False),
    ("rebound_result_builder_birth_unready", False),
    ("declared_empty_scope_valid", True),
    ("zeroarg_private_int_nested_valid", True),
    ("early_return_array_drop_valid", True),
    ("early_return_array_drop_mixed_inputs_valid", True),
    ("early_return_private_array_push_nested_valid", True),
    ("early_return_array_drop_retained_invalid", False),
    ("early_return_array_drop_condition_escape_invalid", False),
    ("array_release_foreign_default_invalid", False),
    ("array_release_foreign_ref_invalid", False),
    ("array_release_forward_foreign_invalid", False),
    ("array_release_owned_strings_foreign_invalid", False),
    ("array_release_named_reader_forward_valid", True),
    ("array_release_owned_strings_named_reader_valid", True),
    ("array_release_forward_own_unseeded_invalid", False),
    ("array_release_owned_strings_move_invalid", False),
    ("array_result_owned_element_inner_defer_then_drop_invalid", False),
    ("defer_multipart_mutable_exit_valid", True),
    ("receiver_result_readonly_scalar_valid", True),
    ("receiver_result_string_field_escape_invalid", False),
    ("array_release_named_reader_own_unseeded_valid", True),
    ("array_release_owned_strings_move_valid", True),
    ("array_release_recursive_inout_owned_strings_valid", True),
    ("array_release_reader_record_owned_strings_valid", True),
    ("array_release_inout_foreign_owned_strings_invalid", False),
    ("array_release_prefix_foreign_inout_invalid", False),
    ("array_release_prefix_reader_inout_valid", True),
    ("array_release_loop_outer_recheck_valid", True),
    ("array_release_loop_local_recheck_valid", True),
    ("array_release_loop_second_iteration_escape_invalid", False),
    ("array_release_inout_reader_then_push_valid", True),
    ("array_release_inout_retaining_reader_invalid", False),
    ("array_release_factory_reader_deepdrop_valid", True),
    ("array_release_factory_retaining_reader_invalid", False),
    ("array_release_alias_before_pending_own_invalid", False),
    ("array_release_alias_after_pending_own_invalid", False),
    ("array_alias_tuple_destructure_own_invalid", False),
    ("array_alias_array_element_destructure_own_invalid", False),
    ("array_alias_slice_element_destructure_own_invalid", False),
    ("array_alias_for_payload_own_invalid", False),
    ("array_alias_for_payload_once_own_invalid", False),
    ("array_alias_match_payload_own_invalid", False),
    ("array_alias_tuple_container_source_own_invalid", False),
    ("array_alias_field_read_own_invalid", False),
    ("array_alias_index_read_own_invalid", False),
    ("array_alias_two_hop_own_invalid", False),
    ("array_field_detach_wrong_field_own_invalid", False),
    ("array_field_detach_duplicate_alias_own_invalid", False),
    ("array_field_detach_partial_branch_own_invalid", False),
    ("array_field_detach_opaque_hold_own_invalid", False),
    ("array_field_detach_live_root_alias_own_invalid", False),
    ("array_field_detach_later_declared_nonrelease_invalid", False),
    ("array_field_detach_two_own_valid", True),
    ("array_field_detach_caller_alias_landed_valid", True),
    ("array_field_detach_later_declared_own_valid", True),
    ("array_alias_then_numeric_cast_invalid", False),
    ("array_string_member_live_root_own_invalid", False),
    ("array_string_member_cross_field_own_invalid", False),
    ("array_string_member_live_root_growth_own_invalid", False),
    ("array_result_nonown_default_field_own_invalid", False),
    ("array_result_nonown_default_field_growth_invalid", False),
    ("array_result_nonown_default_forward_own_invalid", False),
    ("array_result_nonown_default_local_alias_own_invalid", False),
    ("array_result_nonown_default_mixed_return_own_invalid", False),
    ("array_result_nonown_inout_field_own_invalid", False),
    ("array_result_nonown_ref_nested_projection_invalid", False),
    ("array_result_nonown_ref_mapget_descriptor_invalid", False),
    ("array_result_nonown_fresh_scalar_valid", True),
    ("array_result_nonown_own_relay_valid", True),
    ("array_result_nonown_ref_scalar_projection_valid", True),
    ("array_result_unclosed_retained_array_invalid", False),
    ("array_result_unclosed_opaque_array_invalid", False),
    ("array_result_unclosed_recursive_array_invalid", False),
    ("array_result_unclosed_indirect_array_invalid", False),
    # Direct CALL-to-own remains a named-source boundary, independently of
    # whether the returned Array's elements are owned.
    ("array_result_borrowed_direct_own_deepdrop_invalid", False),
    ("array_result_owned_direct_own_named_required_invalid", False),
    ("array_result_owned_factory_deepdrop_valid", True),
    ("array_result_owned_clone_deepdrop_valid", True),
    ("array_result_owned_mapkeys_deepdrop_valid", True),
    ("array_result_local_owned_element_reader_valid", True),
    ("array_result_borrowed_element_move_deepdrop_invalid", False),
    ("array_result_owned_shallow_push_deepdrop_invalid", False),
    ("array_result_owned_shallow_set_deepdrop_invalid", False),
    ("array_result_owned_retained_element_landed_valid", True),
    ("array_result_owned_retain_before_deepdrop_invalid", False),
    ("array_result_local_owned_element_external_keep_landed_valid", True),
    ("array_result_local_owned_element_constructor_landed_valid", True),
    ("array_result_owned_named_inout_shallow_push_landed_valid", True),
    ("array_result_owned_named_inout_shallow_set_invalid", False),
    ("array_result_owned_named_inout_index_assign_invalid", False),
    ("array_result_local_owned_element_container_copy_landing_valid", True),
    ("array_result_local_owned_element_index_assign_invalid", False),
    ("array_result_local_owned_element_field_retention_landed_valid", True),
    ("array_result_owned_element_inout_escape_landed_valid", True),
    ("array_element_inout_escape_by_value_clone_invalid", False),
    ("array_result_owned_element_direct_return_landed_valid", True),
    ("array_result_owned_element_tuple_direct_landed_valid", True),
    ("array_result_owned_element_tuple_binding_landed_valid", True),
    ("array_result_owned_element_tuple_return_landed_valid", True),
    ("array_result_owned_element_option_some_invalid", False),
    ("array_result_owned_element_result_ok_landed_valid", True),
    ("array_result_owned_element_result_err_landed_valid", True),
    ("array_result_owned_element_slot_assign_landed_valid", True),
    ("array_result_owned_element_slot_write_landed_valid", True),
    ("array_result_owned_element_lambda_capture_invalid", False),
    ("array_result_owned_element_event_retaining_handler_landed_valid", True),
    # A released Array or its String payload cannot be reused, retained in a
    # deferred body, or stored shallowly before the final deep-drop.
    ("array_local_owned_push_double_drop_invalid", False),
    ("array_local_owned_push_use_after_drop_invalid", False),
    ("array_result_owned_double_drop_invalid", False),
    ("array_result_owned_use_after_drop_invalid", False),
    ("array_result_owned_defer_drop_then_direct_drop_invalid", False),
    ("array_result_owned_defer_drop_then_own_invalid", False),
    ("array_result_owned_defer_late_array_alias_invalid", False),
    ("array_result_owned_defer_late_scalar_retention_landed_valid", True),
    ("array_result_owned_defer_named_array_retainer_invalid", False),
    ("array_result_owned_element_array_assign_landed_valid", True),
    ("array_result_owned_element_defer_after_drop_invalid", False),
    ("array_result_owned_element_defer_lifo_drop_then_read_invalid", False),
    ("array_result_owned_element_defer_mutable_reassign_invalid", False),
    ("array_result_owned_element_device_write_landed_valid", True),
    ("array_result_owned_element_host_method_retainer_landed_valid", True),
    ("array_result_owned_element_local_return_landed_valid", True),
    ("array_result_owned_element_secure_slot_initializer_landed_valid", True),
    ("array_result_owned_element_secure_slot_write_landed_valid", True),
    ("array_result_owned_element_slot_initializer_landed_valid", True),
    ("array_result_owned_element_spawn_landed_valid", True),
    ("array_result_owned_element_write_view_landed_valid", True),
    # The two return/own call paths were unsafe admissions in native 0b39.
    # Keep these analyze-only until the existing last consumers reject them.
    ("array_result_owned_defer_alias_retainer_invalid", False),
    ("array_result_owned_defer_direct_retainer_invalid", False),
    ("array_result_owned_defer_direct_opaque_invalid", False),
    ("array_result_owned_defer_constructor_return_invalid", False),
    ("array_result_owned_defer_return_invalid", False),
    ("array_result_owned_defer_host_own_invalid", False),
    ("array_release_host_own_reuse_invalid", False),
    ("selector_method_retaining_receiver_invalid", False),
    ("array_result_owned_element_inout_concat_copy_valid", True),
    ("array_result_owned_element_tuple_concat_copy_valid", True),
    ("array_result_owned_element_tuple_return_concat_copy_valid", True),
    ("array_result_owned_element_option_concat_copy_valid", True),
    ("array_result_owned_element_result_ok_concat_copy_valid", True),
    ("array_result_owned_element_result_err_concat_copy_valid", True),
    ("array_result_owned_element_slot_assign_concat_copy_valid", True),
    ("array_result_owned_element_slot_write_concat_copy_valid", True),
    ("array_result_owned_element_lambda_concat_copy_valid", True),
    ("array_result_owned_element_event_concat_copy_valid", True),
    ("array_result_owned_element_channel_send_copy_valid", True),
    ("array_result_owned_element_list_push_copy_valid", True),
    ("array_result_owned_element_list_set_copy_valid", True),
    ("array_result_owned_element_queue_push_copy_valid", True),
    ("array_result_owned_element_set_add_copy_valid", True),
    ("array_result_owned_element_map_key_copy_valid", True),
    ("array_result_owned_element_map_value_copy_valid", True),
    # These are independent copies/readers, not String ownership grants from
    # the source element. Their C/LLVM outputs are checked below.
    ("array_result_owned_element_array_assign_concat_copy_valid", True),
    ("array_result_owned_element_defer_concat_copy_valid", True),
    ("array_result_owned_element_defer_lifo_read_then_drop_valid", True),
    ("array_result_owned_element_defer_mutable_concat_copy_valid", True),
    ("array_result_owned_element_defer_same_body_read_then_drop_valid", True),
    ("array_result_owned_element_defer_without_drop_valid", True),
    ("array_result_owned_element_device_write_concat_copy_valid", True),
    ("array_result_owned_element_host_method_reader_valid", True),
    ("array_result_owned_element_local_return_concat_copy_valid", True),
    ("array_result_owned_element_secure_slot_initializer_concat_copy_valid", True),
    ("array_result_owned_element_slot_initializer_concat_copy_valid", True),
    ("array_result_owned_element_string_plus_copy_probe", True),
    ("array_result_owned_element_substring_copy_valid", True),
    ("array_result_owned_defer_named_reader_valid", True),
    ("array_result_owned_defer_named_array_reader_valid", True),
    # Semantic MIR admission only: native C still reports "Unknown destructuring
    # initializer" for this fresh tuple. It is not a C/LLVM runtime control.
    ("array_literal_tuple_destructure_own_valid", True),
    ("array_release_extern_own_call_valid", True),
    ("array_release_class_method_own_call_valid", True),
    ("array_release_generic_own_call_valid", True),
    ("selector_member_array_nested_valid", True),
    ("selector_member_effectful_receiver_invalid", False),
    ("selector_array_effectful_index_invalid", False),
    ("declared_empty_map_scope_valid", True),
    ("mapnew_ref_key_copy_valid", True),
    # The runtime copies String values, but source-retention ordinal 2 is not
    # proved here. This is a conservative source refusal, not runtime UB.
    ("mapnew_ref_string_value_copy_valid", True),
    ("mapnew_ref_box_value_storage_invalid", False),
    ("declared_static_array_valid", True),
    ("declared_array_borrow_landed_valid", True),
    ("declared_empty_forward_valid", True),
    ("declared_empty_scope_keep_invalid", False),
    ("stdlib_input_retention_valid", True),
    ("stdlib_prefix_factory_valid", True),
    ("stdlib_replace_option_result_valid", True),
    ("stdlib_substring_option_result_valid", True),
    ("stdlib_tostring_scalar_option_result_valid", True),
    ("source_callable_intent_tostring_valid", True),
    ("source_callable_event_tostring_handler_valid", True),
    ("source_callable_class_tostring_constructor_valid", True),
    ("source_callable_enum_tostring_payload_valid", True),
    ("source_callable_class_method_alias_valid", True),
    ("source_callable_enum_method_alias_valid", True),
    ("source_callable_enum_payload_method_alias_valid", True),
    ("source_callable_namespace_valid", True),
    ("result_frame_option_multi_exit_valid", True),
    ("result_frame_formal_return_landed_valid", True),
    ("result_frame_formal_some_landed_valid", True),
    ("result_frame_formal_nominal_landed_valid", True),
    ("result_frame_formal_array_push_landed_valid", True),
    ("physical_call_alias_inout_valid", True),
    ("physical_call_alias_nested_retain_invalid", False),
    ("physical_call_alias_scalar_opaque_invalid", False),
    ("physical_call_alias_descriptor_retain_invalid", False),
    ("physical_array_element_loop_valid", True),
    ("physical_array_element_effectful_index_invalid", False),
    ("physical_array_element_effectful_receiver_invalid", False),
    ("control_navigation_nested_loop_valid", True),
    ("control_navigation_dead_ancestor_valid", True),
    ("control_navigation_nested_retain_invalid", False),
    ("consumer_empty_nested_compare_valid", True),
    ("consumer_empty_nested_retain_invalid", False),
    ("stdlib_prefix_retaining_user_invalid", False),
    ("stdlib_prefix_opaque_invalid", False),
    ("stdlib_prefix_shadow_invalid", False),
    ("stdlib_concat_nested_retaining_invalid", False),
    ("stdlib_trim_result_return_valid", True),
    ("stdlib_trim_result_retain_invalid", False),
    ("stdlib_trim_result_copy_retain_valid", True),
    ("stdlib_trim_opaque_invalid", False),
    ("private_array_push_local_read_valid", True),
    ("private_array_push_nonempty_mixed_valid", True),
    ("private_array_push_return_escape_invalid", False),
    ("private_array_push_wrapper_escape_invalid", False),
    ("private_array_push_nonempty_ref_initial_invalid", False),
    ("private_array_push_nonempty_ref_inserted_invalid", False),
    ("private_array_push_nonempty_nested_retain_invalid", False),
    ("private_array_push_nonempty_alias_landed_valid", True),
    ("private_array_push_nonempty_branch_escape_invalid", False),
    ("physical_return_default_string_valid", True),
    ("physical_return_default_record_valid", True),
    ("physical_return_opaque_invalid", False),
    ("physical_return_slot_invalid", False),
    ("physical_return_inout_invalid", False),
    ("high_output65_dynamic_invalid", False),
    ("high_output65_literal_valid", True),
    ("backward_loop_output_dynamic_invalid", False),
    ("backward_loop_output_literal_valid", True),
    ("default_string_string_return_landed_valid", True),
    ("default_string_option_return_landed_valid", True),
    ("direct_ref_string_loan_valid", True),
    ("callee_alias_return_record_landed_valid", True),
    ("callee_alias_branch_return_record_landed_valid", True),
    ("callee_alias_return_keep_invalid", False),
    ("callee_alias_return_inout_invalid", False),
    ("callee_alias_independent_literal_valid", True),
    ("callee_alias_scalar_read_valid", True),
    ("builtin_string_add_record_return_valid", True),
    ("builtin_string_add_retaining_operand_invalid", False),
    ("call_result_field_read_valid", True),
    ("call_result_field_inout_invalid", False),
    ("slot_read_payload_log_valid", True),
    ("slot_read_payload_keep_invalid", False),
    ("slot_read_payload_same_write_valid", True),
    ("slot_read_payload_other_write_invalid", False),
    ("read_view_payload_log_valid", True),
    ("read_view_payload_keep_invalid", False),
    ("local_read_payload_save_invalid", False),
    ("local_slot_read_keep_invalid", False),
    ("local_view_read_nested_keep_invalid", False),
    ("default_view_read_return_invalid", False),
    ("local_read_payload_log_valid", True),
    ("implicit_main_string_read_valid", True),
    ("implicit_main_int_read_valid", True),
    ("return_loan_direct_slot_landed_valid", True),
    ("return_loan_staged_slot_landed_valid", True),
    ("return_loan_wrap_slot_landed_valid", True),
    ("return_loan_inout_landed_valid", True),
    ("return_loan_mutated_literal_opaque_slot_landed_valid", True),
    ("return_loan_mutated_literal_opaque_inout_landed_valid", True),
    ("return_loan_mutated_literal_assignment_landed_valid", True),
    ("return_loan_mutated_literal_bodyful_inout_landed_valid", True),
    ("return_loan_using_alias_opaque_inout_landed_valid", True),
    ("return_loan_mutated_literal_two_aliases_landed_valid", True),
    ("return_loan_site_two_aliases_landed_valid", True),
    ("return_loan_two_alias_assignment_landed_valid", True),
    ("return_loan_ordinary_copy_opaque_inout_valid", True),
    ("return_loan_literal_slot_valid", True),
    ("return_loan_read_static_local_valid", True),
    ("direct_ref_string_retaining_literal_valid", True),
    ("direct_ref_string_retaining_concat_invalid", False),
    ("direct_ref_string_local_retain_invalid", False),
    ("direct_ref_string_slot_invalid", False),
    ("direct_ref_string_channel_invalid", False),
    ("direct_ref_string_opaque_literal_invalid", False),
    ("direct_ref_string_opaque_concat_invalid", False),
    ("direct_ref_string_alias_literal_invalid", False),
    ("direct_ref_string_alias_concat_invalid", False),
    ("direct_ref_string_alias_named_invalid", False),
    ("recursive_record_wrapper_control_invalid", False),
    ("nested_call_retaining_input_invalid", False),
    ("nested_call_independent_result_valid", True),
    ("named_string_identity_substring_push_valid", True),
    ("named_string_identity_loop_reassignment_landed_valid", True),
    ("named_string_identity_owned_element_retain_landed_valid", True),
    ("named_string_all_def0_branch_valid", True),
    ("named_string_all_def0_loop_concat_valid", True),
    ("named_string_all_def0_known_zero_valid", True),
    ("named_string_all_def0_branch_copy_landing_valid", True),
    ("named_string_field_empty_arena_valid", True),
    ("named_string_field_make_nodes_drop_landed_valid", True),
    ("named_string_field_forward_project_drop_landed_valid", True),
    ("named_string_field_mixed_branch_drop_landed_valid", True),
    ("named_string_field_shallow_node_drop_invalid", False),
    ("ref_input_through_call_result_into_inout_output_landed_valid", True),
    ("ref_string_through_inout_reader_shallow_store_landed_valid", True),
    ("ref_string_through_inout_keeper_invalid", False),
    ("by_value_field_alias_written_by_inout_callee_landed_valid", True),
    ("loan_owner_assigned_before_loan_valid", True),
    ("loan_owner_assigned_after_loan_invalid", False),
    ("loan_owner_assigned_in_loop_before_loan_invalid", False),
    ("loan_owner_assigned_from_its_own_loan_invalid", False),
    ("loan_owner_written_by_indexed_assignment_after_loan_invalid", False),
    ("loan_carrier_unchanged_by_call_before_loan_valid", True),
    ("loan_scope_ended_before_owner_moved_valid", True),
    ("loan_kept_outside_its_loop_invalid", False),
    ("loan_pushed_outside_its_loop_invalid", False),
    ("loan_kept_by_inout_callee_outside_its_loop_invalid", False),
    ("loan_read_in_the_statement_that_moves_its_owner_invalid", False),
    ("loan_owner_read_by_constructor_valid", True),
    ("loan_owner_constructor_copy_changed_invalid", False),
    ("loan_owner_constructor_copy_written_by_callee_invalid", False),
    ("loan_owner_constructor_copy_returned_invalid", False),
    ("loan_owner_permuted_in_place_invalid", False),
    ("loan_read_last_before_owner_changed_valid", True),
    ("loan_read_after_owner_changed_invalid", False),
    ("loan_read_in_loop_that_changes_owner_invalid", False),
    ("loan_read_by_defer_after_owner_changed_invalid", False),
    ("loan_read_by_call_after_its_owner_changed_in_its_arguments_invalid", False),
    ("loan_carrier_handed_on_by_early_return_valid", True),
    ("loan_carrier_handed_into_output_before_owner_changed_invalid", False),
    ("loan_carrier_read_into_returned_constructor_valid", True),
    ("loan_carrier_read_into_constructor_a_callee_writes_invalid", False),
    ("loan_payload_bound_by_match_valid", True),
    ("loan_payload_bound_by_match_written_invalid", False),
    ("loan_payload_binding_passed_to_reader_valid", True),
    ("loan_payload_binding_written_by_callee_invalid", False),
    ("loan_payload_binding_of_call_subject_written_invalid", False),
    ("local_copy_other_side_passed_to_reader_of_other_rows_valid", True),
    ("local_copy_other_side_passed_to_reader_of_changed_rows_invalid", False),
    ("local_copy_other_side_passed_through_to_reader_of_changed_rows_invalid", False),
    ("local_copy_other_side_aliased_whole_by_callee_invalid", False),
    ("local_copy_of_by_value_formal_passed_to_reader_of_other_rows_invalid", False),
    ("inout_defers_handed_to_recursive_emitter_valid", True),
    ("inout_defers_alias_written_by_inout_callee_invalid", False),
    ("inout_defers_kept_by_callee_before_nested_call_invalid", False),
    ("by_value_array_grown_by_inout_callee_landed_valid", True),
    ("by_value_array_grown_by_inout_callee_last_use_valid", True),
    ("by_value_array_of_maps_grown_by_inout_callee_invalid", False),
    ("by_value_array_handed_to_by_value_reader_valid", True),
    ("by_value_array_grown_two_calls_deep_landed_valid", True),
    ("by_value_array_grown_through_recursive_hand_off_landed_valid", True),
    ("by_value_cursor_set_through_recursive_parser_valid", True),
    ("by_value_array_popped_by_inout_callee_valid", True),
    ("by_value_array_handed_to_generic_reader_valid", True),
    ("inout_string_chain_popped_by_recursive_parser_valid", True),
    ("inout_string_chain_kept_before_pop_invalid", False),
    ("by_value_argument_beside_its_inout_landed_valid", True),
    ("by_value_argument_before_its_inout_landed_valid", True),
    ("scalar_and_string_beside_their_inout_valid", True),
    ("by_value_array_to_two_reading_formals_valid", True),
    ("by_value_maps_beside_their_inout_invalid", False),
    ("array_handed_to_two_keeping_formals_landed_valid", True),
    ("array_handed_to_three_keeping_formals_landed_valid", True),
    ("array_kept_by_earlier_call_and_two_formals_invalid", False),
    ("text_builder_append_of_released_element_invalid", False),
    ("one_storage_in_two_constructor_parts_returned_landed_valid", True),
    ("one_storage_through_alias_in_two_parts_landed_valid", True),
    ("one_storage_in_two_literal_elements_landed_valid", True),
    ("two_parts_of_distinct_storage_valid", True),
    ("distinct_fields_of_a_local_copy_in_two_parts_valid", True),
    ("one_storage_in_two_map_parts_landed_valid", True),
    ("one_storage_in_two_array_map_parts_invalid", False),
    ("constructor_string_field_snapshot_before_finalize_invalid", False),
    ("declared_result_member_chain_qualifier_valid", True),
    ("mutual_recursion_declared_result_stored_valid", True),
    ("named_string_field_alias_mutation_invalid", False),
    ("named_string_field_lineage_two_ref_control_valid", True),
    ("named_string_field_lineage_two_ref_empty_valid", True),
    ("named_string_field_lineage_named_order_valid", True),
    ("named_string_field_lineage_unrelated_match_valid", True),
    ("named_string_field_lineage_unrelated_with_valid", True),
    ("named_string_field_lineage_same_call_retire_grow_landed_valid", True),
    ("named_string_field_lineage_root_overwrite_landed_valid", True),
    ("named_string_field_lineage_named_mapping_landed_valid", True),
    ("named_string_field_lineage_mixed_return_landed_valid", True),
    ("named_string_field_lineage_recursive_forward_landed_valid", True),
    ("named_string_same_empty_direct_valid", True),
    ("named_string_same_empty_named_valid", True),
    ("named_string_same_empty_fresh_valid", True),
    ("named_string_same_empty_owned_then_borrowed_direct_landed_valid", True),
    ("named_string_same_empty_owned_then_borrowed_named_landed_valid", True),
    ("named_string_same_empty_borrowed_then_owned_direct_landed_valid", True),
    ("named_string_same_empty_borrowed_then_owned_named_landed_valid", True),
    ("slot_wrapper_direct_own_invalid", False),
    ("slot_wrapper_direct_ref_valid", True),
    ("slot_wrapper_binary_equal_own_invalid", False),
    ("slot_wrapper_binary_equal_ref_valid", True),
    ("slot_wrapper_binary_rhs_own_invalid", False),
    ("slot_wrapper_binary_rhs_ref_valid", True),
    ("slot_wrapper_unary_own_invalid", False),
    ("slot_wrapper_unary_ref_valid", True),
    ("slot_wrapper_return_own_invalid", False),
    ("slot_wrapper_return_ref_valid", True),
    ("slot_wrapper_let_own_invalid", False),
    ("slot_wrapper_let_ref_valid", True),
    ("slot_wrapper_call_argument_own_invalid", False),
    ("slot_wrapper_call_argument_ref_valid", True),
    ("slot_wrapper_array_own_invalid", False),
    ("slot_wrapper_array_ref_valid", True),
    ("slot_wrapper_tuple_own_invalid", False),
    ("slot_wrapper_tuple_ref_valid", True),
    ("slot_wrapper_index_base_own_invalid", False),
    ("slot_wrapper_index_base_ref_valid", True),
    ("slot_wrapper_index_index_own_invalid", False),
    ("slot_wrapper_index_index_ref_valid", True),
    ("slot_wrapper_match_tag_binding_ref_valid", True),
    ("slot_wrapper_runtime_pattern_own_invalid", False),
    ("slot_wrapper_transaction_compensation_own_invalid", False),
    ("slot_wrapper_transaction_compensation_ref_valid", True),
    ("slot_wrapper_capture_slot_map_invalid", False),
    ("slot_wrapper_capture_string_map_invalid", False),
    ("slot_wrapper_capture_slot_cast_invalid", False),
    ("slot_wrapper_capture_string_cast_invalid", False),
    ("slot_wrapper_capture_slot_type_test_invalid", False),
    ("slot_wrapper_capture_string_type_test_invalid", False),
    ("slot_wrapper_capture_string_closure_return_invalid", False),
    ("slot_wrapper_capture_slot_direct_invalid", False),
    ("slot_wrapper_capture_string_direct_return_invalid", False),
    # Aggregate-carrier controls certify MIR admission/refusal only, not field invocation.
    ("callable_holder_string_relay_invalid", False),
    ("callable_holder_named_relay_valid", True),
    ("callable_holder_int_copy_return_valid", True),
    ("slot_wrapper_assignment_own_invalid", False),
    ("slot_wrapper_assignment_ref_valid", True),
    ("slot_wrapper_cast_own_invalid", False),
    ("slot_wrapper_cast_ref_valid", True),
    ("slot_wrapper_type_test_own_invalid", False),
    ("slot_wrapper_type_test_ref_valid", True),
    ("slot_wrapper_member_own_invalid", False),
    ("slot_wrapper_member_ref_valid", True),
    ("slot_wrapper_map_key_own_invalid", False),
    ("slot_wrapper_map_key_ref_valid", True),
    ("slot_wrapper_map_value_own_invalid", False),
    ("slot_wrapper_map_value_ref_valid", True),
    ("slot_wrapper_match_subject_own_invalid", False),
    ("slot_wrapper_match_subject_ref_valid", True),
    ("slot_wrapper_match_guard_own_invalid", False),
    ("slot_wrapper_match_guard_ref_valid", True),
    ("slot_wrapper_destructure_own_invalid", False),
    ("slot_wrapper_destructure_ref_valid", True),
    ("string_scalar_tostring_opaque_call_valid", True),
    ("string_element_tostring_opaque_call_landed_valid", True),
    ("string_param_field_opaque_call_valid", True),
    ("string_own_param_field_opaque_call_invalid", False),
    ("string_inout_helper_repeat_call_valid", True),
    ("string_inout_helper_then_own_release_landed_valid", True),
    ("string_inout_helper_then_escape_call_landed_valid", True),
    ("string_inout_literal_then_escape_call_landed_valid", True),
    ("string_inout_concat_copy_then_escape_call_valid", True),
    ("string_push_after_loop_walk_landed_valid", True),
    ("ref_view_beside_unit_variant_valid", True),
    ("ref_view_beside_unit_variant_stored_invalid", False),
    ("string_inout_returned_literal_then_escape_call_landed_valid", True),
    ("string_inout_unwrapped_literal_then_escape_call_landed_valid", True),
    ("string_inout_returned_literal_then_own_release_landed_valid", True),
    ("default_aggregate_move_reassign_valid", True),
    ("default_aggregate_move_reuse_invalid", False),
    ("default_aggregate_move_loop_reuse_invalid", False),
    ("default_aggregate_move_branch_use_invalid", False),
    ("default_aggregate_move_ref_param_invalid", False),
    ("own_rebind_scalar_frame_valid", True),
    ("own_rebind_ref_input_return_invalid", False),
    ("clone_string_array_ref_input_result_valid", True),
    ("clone_struct_array_ref_input_result_invalid", False),
    ("default_aggregate_move_read_only_struct_valid", True),
    ("default_aggregate_move_shared_field_return_nested_clone_landed_valid", True),
    ("default_aggregate_move_shared_field_return_landed_valid", True),
    ("by_value_getter_result_grown_landed_valid", True),
    ("by_value_getter_result_passed_grown_landed_valid", True),
    ("by_value_getter_result_reassigned_grown_landed_valid", True),
    ("constructor_result_grown_landed_valid", True),
    ("by_value_getter_result_read_valid", True),
    ("constructor_result_read_valid", True),
    ("by_value_getter_lend_result_grown_landed_valid", True),
    ("option_payload_result_grown_landed_valid", True),
    ("option_payload_result_read_valid", True),
    ("optional_wrap_result_grown_landed_valid", True),
    ("optional_wrap_result_read_valid", True),
    ("by_value_getter_lend_result_lent_on_valid", True),
    ("by_value_getter_lend_string_reassigned_valid", True),
    ("by_value_getter_result_local_returned_valid", True),
    ("constructor_of_getter_result_returned_valid", True),
    ("by_value_getter_result_local_returned_grown_landed_valid", True),
    ("constructor_of_formal_values_stored_valid", True),
    ("path_alias_lent_to_string_getter_valid", True),
    ("option_string_result_reassigned_valid", True),
    ("string_struct_result_reassigned_valid", True),
    ("by_value_getter_lend_option_string_unwrapped_valid", True),
    ("by_value_getter_lend_string_collected_valid", True),
    ("by_value_getter_lend_result_path_grown_landed_valid", True),
    ("by_value_formal_rebound_to_result_valid", True),
    ("by_value_formal_rebound_to_result_grown_landed_valid", True),
    ("alias_rebound_to_result_valid", True),
    ("path_alias_returned_in_constructor_valid", True),
    ("path_alias_returned_in_constructor_grown_landed_valid", True),
    ("enum_formal_matched_payload_read_valid", True),
    ("enum_formal_matched_payload_grown_invalid", False),
    ("by_value_getter_lend_result_passed_grown_landed_valid", True),
    ("by_value_getter_lend_in_constructor_passed_grown_landed_valid", True),
    ("by_value_getter_second_lend_grown_landed_valid", True),
    ("by_value_getter_second_lend_formal_grown_landed_valid", True),
    ("by_value_getter_lend_in_constructor_grown_landed_valid", True),
    ("by_value_getter_lend_owner_in_constructor_grown_landed_valid", True),
    ("by_value_getter_two_lends_grown_landed_valid", True),
    ("local_let_alias_grown_landed_valid", True),
    ("local_let_alias_source_grown_landed_valid", True),
    ("local_assigned_alias_grown_landed_valid", True),
    ("local_constructor_alias_grown_landed_valid", True),
    ("local_array_literal_alias_grown_landed_valid", True),
    ("local_pushed_alias_grown_landed_valid", True),
    ("formal_let_alias_grown_landed_valid", True),
    ("local_copy_of_copy_grown_landed_valid", True),
    ("local_copy_source_element_set_landed_valid", True),
    ("local_copy_source_inout_grown_landed_valid", True),
    ("local_let_alias_read_valid", True),
    ("local_source_grown_before_copy_valid", True),
    ("local_copy_grown_source_unused_valid", True),
    ("local_copy_in_loop_valid", True),
    ("local_copy_rebound_valid", True),
    ("local_copy_inout_read_only_callee_valid", True),
    ("local_copy_inout_callee_changes_invalid", False),
    ("local_copy_collection_changed_invalid", False),
    ("local_copy_map_changed_landed_valid", True),
    ("local_copy_map_held_at_field_changed_landed_valid", True),
    ("local_copy_map_inout_callee_changes_landed_valid", True),
    ("inout_callee_same_map_field_changed_landed_valid", True),
    ("formal_field_map_copy_changed_at_exit_landed_valid", True),
    ("default_aggregate_move_map_field_set_landed_valid", True),
    ("local_copy_collection_changed_side_used_valid", True),
    ("collection_value_store_grown_landed_valid", True),
    ("collection_value_store_shallow_grown_invalid", False),
    ("local_copy_held_at_other_field_valid", True),
    ("local_copy_held_at_field_changed_invalid", False),
    ("formal_field_copy_changed_at_exit_invalid", False),
    ("formal_field_copy_changed_at_exit_landed_valid", True),
    ("formal_field_copy_stored_back_valid", True),
    ("string_element_kept_by_callee_landed_valid", True),
    ("array_descriptor_kept_then_read_invalid", False),
    ("ref_loan_through_popping_helper_valid", True),
    ("inout_callee_other_field_changed_valid", True),
    ("inout_callee_same_field_changed_invalid", False),
    ("ref_loan_field_written_back_valid", True),
    ("ref_loan_stored_into_inout_output_landed_valid", True),
    ("ref_loan_local_stored_into_inout_output_invalid", False),
    ("string_local_pushed_into_field_array_landed_valid", True),
    ("string_copy_landing_same_function_batch_valid", True),
    ("string_copy_landing_function_boundary_valid", True),
    ("formal_string_store_same_routine_batch_valid", True),
    ("formal_string_store_multiple_formals_batch_valid", True),
    ("formal_string_push_same_routine_batch_valid", True),
    ("owned_string_push_multi_receiver_batch_valid", True),
    ("formal_string_store_cross_routine_batch_valid", True),
    ("formal_string_store_batch_boundary_valid", True),
    ("local_let_alias_grown_invalid", False),
    ("local_copied_path_then_moved_landed_valid", True),
    ("local_copy_changed_then_moved_landed_valid", True),
    ("owned_copy_string_push_landed_valid", True),
    ("local_copy_source_sorted_landed_valid", True),
    ("array_sort_after_handoff_ref_reader_valid", True),
    ("by_value_formal_array_sorted_landed_valid", True),
    ("by_value_getter_lend_result_passed_grown_invalid", False),
    ("by_value_getter_second_lend_grown_invalid", False),
    ("recursive_ref_read_only_walk_valid", True),
    ("recursive_ref_returned_input_invalid", False),
    ("collection_member_move_loop_local_source_valid", True),
    ("collection_member_move_loop_outer_source_invalid", False),
    ("inout_owned_param_self_flow_valid", True),
    ("inout_owned_param_input_return_moves_landed_valid", True),
    ("owned_param_field_store_ref_return_invalid", False),
    ("string_element_read_only_named_call_valid", True),
    ("string_element_returned_by_named_call_valid", True),
    ("string_element_returned_by_named_call_past_owner_landed_valid", True),
    ("string_element_returned_by_named_call_owner_changed_landed_valid", True),
    ("string_element_returned_in_result_loop_valid", True),
    ("string_element_returned_in_result_loop_kept_landed_valid", True),
    ("string_element_returned_from_dir_walk_rows_valid", True),
    ("allocator_result_lane_wrapper_valid", True),
    ("allocator_pool_used_before_destroy_valid", True),
    ("allocator_formal_finish_forwarded_valid", True),
    ("allocator_pool_read_after_destroy_invalid", False),
    ("allocator_pool_loan_returned_invalid", False),
    ("allocator_pool_loan_kept_outside_invalid", False),
    ("private_array_sort_ref_reader_valid", True),
    ("private_array_sort_alias_invalid", False),
    ("default_aggregate_move_map_field_read_valid", True),
    ("default_aggregate_move_map_field_set_invalid", False),
    ("option_none_ref_reader_valid", True),
    ("option_none_ref_loan_returned_invalid", False),
    ("option_none_local_argument_valid", True),
    ("option_none_local_argument_kept_invalid", False),
    ("array_slice_view_reader_valid", True),
    ("array_slice_view_returned_landed_valid", True),
    ("string_element_clone_after_drop_valid", True),
    ("string_element_clone_returned_valid", True),
    ("string_element_clone_getter_owner_changed_valid", True),
    ("string_element_clone_ref_output_valid", True),
    ("string_element_tostring_returned_landed_valid", True),
    ("by_value_getter_lend_value_read_valid", True),
    ("formal_string_store_landing_ref_push_valid", True),
    ("formal_string_store_landing_map_array_set_valid", True),
    ("formal_string_store_landing_local_receiver_valid", True),
    ("formal_string_store_landing_private_local_valid", True),
    ("formal_string_store_landing_field_take_valid", True),
    ("formal_string_store_landing_value_source_valid", True),
    ("formal_string_store_landing_value_set_valid", True),
    ("formal_string_store_landing_value_map_valid", True),
    ("formal_array_store_local_receiver_invalid", False),
    ("formal_array_store_field_take_landed_valid", True),
    ("formal_string_store_local_alias_set_invalid", False),
    ("formal_string_store_landing_shadowed_function_valid", True),
    ("formal_string_store_landing_shadowed_struct_valid", True),
    ("formal_string_store_landing_shadowed_method_valid", True),
    ("by_value_getter_lend_shared_result_owner_changed_invalid", False),
    ("default_aggregate_move_cloned_growth_invalid", False),
    ("struct_clone_array_field_copied_valid", True),
    ("struct_clone_option_payload_copied_valid", True),
    ("struct_clone_nested_field_copied_valid", True),
    ("struct_clone_ref_returned_valid", True),
    ("struct_clone_option_field_ref_returned_valid", True),
    ("struct_clone_generic_ref_returned_invalid", False),
    ("struct_clone_map_field_ref_returned_valid", True),
    ("map_clone_ref_returned_valid", True),
    ("map_clone_scalar_keys_ref_returned_valid", True),
    ("map_clone_array_value_ref_returned_invalid", False),
    ("struct_clone_subject_field_ref_returned_invalid", False),
    ("struct_clone_subject_copied_valid", True),
    ("struct_clone_shared_field_ref_returned_invalid", False),
    ("formal_array_store_inout_output_invalid", False),
    ("default_aggregate_move_option_field_read_valid", True),
    ("default_aggregate_move_option_field_alias_store_landed_valid", True),
    ("default_aggregate_move_field_alias_read_valid", True),
    ("default_aggregate_move_field_alias_rebound_valid", True),
    ("default_aggregate_move_field_alias_returned_landed_valid", True),
    ("default_aggregate_move_field_alias_passed_on_landed_valid", True),
    ("default_aggregate_move_loop_element_read_valid", True),
    ("default_aggregate_move_loop_element_store_invalid", False),
    ("default_aggregate_move_array_loop_element_store_invalid", False),
    ("string_element_inout_owner_named_call_landed_valid", True),
    ("enum_payload_constructor_value_result_valid", True),
    ("string_element_push_copy_landing_valid", True),
    ("string_element_push_copy_landing_mixed_landed_valid", True),
    ("world_ref_self_scalar_result_valid", True),
    ("world_ref_self_member_return_invalid", False),
    ("method_self_array_field_length_valid", True),
    ("method_self_array_field_return_invalid", False),
    ("world_member_host_field_clone_valid", True),
    ("intent_step_action_route_valid", True),
    ("intent_action_stores_by_value_moves_landed_valid", True),
    ("intent_aggregate_readonly_boundary_valid", True),
    ("intent_aggregate_owned_field_requirement_invalid", False),
    ("intent_subject_keeps_string_landed_valid", True),
    ("match_payload_return_valid", True),
    ("match_payload_return_moves_landed_valid", True),
    ("world_member_subject_action_route_valid", True),
    ("subject_action_returns_self_ref_invalid", False),
    ("receiver_unsupported_ref_return_invalid", False),
    ("unit_enum_constant_constructor_valid", True),
    ("unit_enum_constant_constructor_moves_landed_valid", True),
    ("intent_step_sorted_copy_route_valid", True),
    ("slice_view_scalar_read_valid", True),
    ("slice_view_ref_return_invalid", False),
    ("string_join_borrowed_parts_valid", True),
    ("string_join_shadowed_returns_part_landed_valid", True),
    ("ref_loan_local_after_output_call_valid", True),
    ("string_argument_kept_copy_landed_valid", True),
    ("string_released_in_returning_branch_valid", True),
    ("string_released_in_then_falling_invalid", False),
    ("string_released_in_else_falling_invalid", False),
    ("string_released_before_returning_if_invalid", False),
    ("ref_loan_local_after_output_call_channel_invalid", False),
    ("aggregate_string_loan_getter_valid", True),
    ("aggregate_string_loan_struct_escape_landed_valid", True),
    ("aggregate_string_loan_local_owner_landed_valid", True),
    ("aggregate_string_loan_owner_mutated_landed_valid", True),
    ("aggregate_string_loan_owner_moved_landed_valid", True),
    ("aggregate_string_loan_owner_kept_landed_valid", True),
    ("aggregate_string_loan_by_value_owner_valid", True),
    ("ref_string_option_loan_forward_valid", True),
    ("tostring_interpolation_ref_valid", True),
    ("results_lend_struct_view_valid", True),
    ("results_lend_slice_view_valid", True),
    ("results_lend_array_result_valid", True),
    ("receiver_ref_self_local_return_invalid", False),
    ("receiver_ref_self_string_return_invalid", False),
    ("array_push_loan_local_view_valid", True),
    ("array_push_loan_past_owner_invalid", False),
    ("array_push_loan_inout_nested_clone_landed_valid", True),
    ("aggregate_move_never_call_valid", True),
    ("aggregate_move_void_call_nested_clone_landed_valid", True),
    ("aggregate_move_void_call_landed_valid", True),
    ("array_handoff_return_value_loop_valid", True),
    ("inout_call_after_inner_loop_valid", True),
    ("output_field_store_of_ref_formal_landed_valid", True),
    ("inout_self_assignment_lands_nothing_valid", True),
    ("borrowed_inout_array_kept_in_result_landed_valid", True),
    ("moved_argument_two_branches_landed_valid", True),
    ("moved_argument_chain_batch_landed_valid", True),
    ("by_value_getter_lend_into_own_formal_landed_valid", True),
    ("returned_alias_lends_formal_valid", True),
    ("lent_path_copy_grown_landed_valid", True),
    ("returned_alias_lends_formal_grown_invalid", False),
    ("borrowed_owner_loan_grown_landed_valid", True),
    ("string_crossing_window_pair_landed_valid", True),
    ("string_window_pair_other_call_invalid", False),
    ("by_value_lend_then_owner_cloned_valid", True),
    ("by_value_lend_then_shallow_clone_grown_invalid", False),
    ("stdlib_tostring_string_forward_landed_valid", True),
    ("borrowed_owner_loan_grown_by_value_clone_invalid", False),
    ("output_field_store_of_ref_formal_by_value_clone_invalid", False),
    ("array_handoff_error_return_after_append_valid", True),
    ("array_handoff_after_rebinding_call_invalid", False),
    ("array_handoff_same_return_value_invalid", False),
    ("defer_array_drop_reads_valid", True),
    ("defer_array_drop_fallthrough_valid", True),
    ("defer_owned_strings_read_valid", True),
    ("defer_array_drop_return_keep_invalid", False),
    ("defer_array_drop_reader_then_keep_invalid", False),
    ("defer_array_drop_own_handoff_invalid", False),
    ("defer_array_drop_return_binding_invalid", False),
    ("defer_array_drop_fallthrough_kept_invalid", False),
    ("defer_owned_strings_return_constructor_invalid", False),
    ("defer_array_drop_after_rebinding_call_invalid", False),
    ("defer_array_drop_block_kept_invalid", False),
    ("defer_own_handoff_cleanup_reads_valid", True),
    ("defer_own_handoff_cleanup_kept_invalid", False),
    ("defer_array_drop_lambda_reads_valid", True),
    ("defer_array_drop_struct_reader_valid", True),
    ("defer_array_drop_lambda_kept_invalid", False),
    ("enum_payload_constructor_ref_field_return_valid", True),
    ("enum_payload_constructor_ref_field_owner_changed_invalid", False),
    ("by_value_getter_lend_valid", True),
    ("by_value_getter_lend_local_valid", True),
    ("by_value_getter_lend_forward_valid", True),
    ("by_value_getter_lend_returned_landed_valid", True),
    ("by_value_getter_lend_mutated_landed_valid", True),
    ("by_value_getter_lend_kept_landed_valid", True),
    ("by_value_getter_lend_passed_kept_landed_valid", True),
    ("by_value_getter_lend_assigned_landed_valid", True),
    ("by_value_getter_lend_owner_written_after_lowering_landed_valid", True),
    ("by_value_getter_lend_shared_result_returned_invalid", False),
    ("by_value_getter_lend_inout_passed_invalid", False),
    ("by_value_getter_lend_inout_passed_landed_valid", True),
    ("by_value_getter_lend_inout_then_returned_invalid", False),
    ("by_value_getter_lend_inout_then_returned_landed_valid", True),
    ("by_value_getter_lend_owner_written_landed_valid", True),
    ("by_value_getter_lend_owner_written_shadowed_landed_valid", True),
    ("default_aggregate_move_field_read_valid", True),
    ("default_aggregate_move_field_store_landed_valid", True),
    ("default_aggregate_move_field_alias_store_landed_valid", True),
    ("default_aggregate_move_field_set_landed_valid", True),
    ("default_aggregate_move_nested_field_store_invalid", False),
    ("default_aggregate_move_field_element_write_landed_valid", True),
    ("default_aggregate_move_nested_field_write_invalid", False),
    ("default_aggregate_move_array_element_alias_store_invalid", False),
    ("default_aggregate_move_array_element_read_call_valid", True),
    ("by_value_getter_lend_growing_invalid", False),
    ("by_value_getter_lend_local_transfer_landed_valid", True),
    ("by_value_getter_lend_inout_owner_landed_valid", True),
    ("formal_store_landing_struct_output_valid", True),
    ("formal_store_landing_alias_output_valid", True),
    ("formal_store_landing_struct_output_shallow_invalid", False),
    ("array_push_loan_inout_landed_valid", True),
)
for name, valid in paired:
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=10.0, check=False)
    if name in landing_rounds:
        staged = subprocess.run(
            [pgy, "--native-pipeline", "--mir-json", str(source)],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            env=dict(os.environ, PGY_DEBUG_PIPELINE_STAGE="1"), timeout=10.0, check=False)
        rounds = staged.stderr.decode(errors="replace").splitlines().count("[driver stage] semantic_landing")
        if rounds != landing_rounds[name]:
            raise SystemExit(f"landing {name} ran {rounds} landing analyses")
    if valid:
        if checked.returncode != 0 or '"pgy.mir.v1"' not in checked.stdout.decode(errors="replace"):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"physical-return positive {name} failed")
        if name in selected_call_targets:
            target_name, owner_kind, owner_name, binding_kind = selected_call_targets[name]
            document = json.loads(checked.stdout)
            if owner_kind == "method":
                owners = [row for row in document.get("routines", []) if
                          row.get("kind") == "method" and row.get("owner") == owner_name
                          and row.get("name") == "Value"]
            else:
                owners = [row for row in document.get("decls", []) if
                          row.get("kind") == owner_kind and row.get("name") == owner_name]
            if len(owners) != 1 or not isinstance(owners[0].get("source_syntax_id"), int) or owners[0]["source_syntax_id"] <= 0:
                raise SystemExit(f"selected-callable {name} lost its unique declaration owner")
            target_id = owners[0]["source_syntax_id"]
            mains = [row for row in document.get("routines", []) if row.get("name") == "Main"]
            if len(mains) != 1:
                raise SystemExit(f"selected-callable {name} lost its Main routine")
            calls = []
            for block in mains[0].get("blocks", []):
                for instruction in block.get("instructions", []):
                    for lane in ("expr0_graph", "expr1_graph"):
                        graph = instruction.get(lane)
                        if not isinstance(graph, dict):
                            continue
                        nodes = graph.get("nodes", [])
                        for node in nodes:
                            if node.get("kind") == "call" and node.get("call_target_name") == target_name:
                                calls.append((node, nodes))
            if len(calls) != 1:
                raise SystemExit(f"selected-callable {name} lost its one direct call graph")
            call, nodes = calls[0]
            callee_index = call.get("left")
            callee = nodes[callee_index] if isinstance(callee_index, int) and 0 <= callee_index < len(nodes) else None
            if (call.get("call_target_kind") != "direct" or
                    call.get("call_target_syntax_id") != target_id or
                    not isinstance(callee, dict) or callee.get("kind") != "leaf" or
                    callee.get("text") != target_name or
                    callee.get("binding_kind") != binding_kind or
                    callee.get("binding_syntax_id") != (target_id if binding_kind == "declared_callable" else 0)):
                raise SystemExit(f"selected-callable {name} lost its exact target/leaf identity")
        if name in landed_clones:
            # The landed store must reach MIR with its Clone of the String.
            document = json.loads(checked.stdout)
            for routine_name, landed in landed_clones[name].items():
                calls = [node.get("call_target_name") for row in document.get("routines", [])
                         if row.get("name") == routine_name for block in row.get("blocks", [])
                         for instruction in block.get("instructions", [])
                         for lane in ("expr0_graph", "expr1_graph")
                         if isinstance(instruction.get(lane), dict)
                         for node in instruction[lane].get("nodes", []) if node.get("kind") == "call"]
                if calls.count("Clone") != landed:
                    raise SystemExit(f"landing {name} did not reach MIR with Clone in {routine_name}: {calls}")
        if name in landed_at_exit:
            routine_name, carrier = landed_at_exit[name]
            document = json.loads(checked.stdout)
            wrapped = []
            for row in document.get("routines", []):
                if row.get("name") != routine_name:
                    continue
                for block in row.get("blocks", []):
                    for instruction in block.get("instructions", []):
                        for lane in ("expr0_graph", "expr1_graph"):
                            graph = instruction.get(lane)
                            if not isinstance(graph, dict):
                                continue
                            nodes = graph.get("nodes", [])
                            for node in nodes:
                                left, right = node.get("left"), node.get("right")
                                if (node.get("kind") == "call_argument" and isinstance(left, int) and
                                        isinstance(right, int) and 0 <= left < len(nodes) and
                                        0 <= right < len(nodes) and nodes[left].get("kind") == "call" and
                                        nodes[left].get("call_target_name") == "Clone"):
                                    wrapped.append((nodes[right].get("kind"), nodes[right].get("text")))
            if wrapped != [("leaf", carrier)]:
                raise SystemExit(f"landing {name} did not copy where the loan leaves: {wrapped}")
        if name in owned_push_landings:
            # The landed push must reach MIR as the owned-copy operation.
            routine_name, landed = owned_push_landings[name]
            document = json.loads(checked.stdout)
            calls = [node.get("call_target_name") for row in document.get("routines", [])
                     if row.get("name") == routine_name for block in row.get("blocks", [])
                     for instruction in block.get("instructions", [])
                     for lane in ("expr0_graph", "expr1_graph")
                     if isinstance(instruction.get(lane), dict)
                     for node in instruction[lane].get("nodes", []) if node.get("kind") == "call"]
            if calls.count("ArrayPushOwnedString") != landed or "ArrayPush" in calls:
                raise SystemExit(f"owned push landing {name} did not reach MIR as ArrayPushOwnedString: {calls}")
    else:
        output = (checked.stdout + checked.stderr).decode(errors="replace")
        try:
            diagnostics = json.loads(output)
        except json.JSONDecodeError:
            raise SystemExit(f"physical-return negative {name} lost structured diagnostics: {output}")
        errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
        if any(not isinstance(row.get("location"), dict) for row in errors):
            raise SystemExit(f"physical-return negative {name} emitted an error without a source location")
        if name in release_lifetime_refusals:
            actual = Counter((row.get("code"), row.get("cause_ir"),
                              row.get("location", {}).get("line"),
                              row.get("location", {}).get("column")) for row in errors)
            if (checked.returncode != 1 or checked.stdout or
                    '"pgy.mir.v1"' in output or
                    any(row.get("stage") != "semantic" for row in errors) or
                    actual != Counter(release_lifetime_refusals[name])):
                sys.stderr.buffer.write(checked.stderr)
                raise SystemExit(f"release-lifetime negative {name} lost its exact refusal")
            continue
        direct_own_named_sites = {
            "array_result_borrowed_direct_own_deepdrop_invalid": (13, 13),
            "array_result_owned_direct_own_named_required_invalid": (15, 13),
        }
        expected_code = "PGY_SEM_TYPE_MISMATCH" if name in direct_own_named_sites else "PGY_SEM_BORROW_ESCAPE"
        expected_cause = "semantic:move:source_not_named" if name in direct_own_named_sites else "semantic:borrow_escape"
        if checked.returncode != 1 or '"pgy.mir.v1"' in output or not errors or any(
                row.get("code") != expected_code or row.get("cause_ir") != expected_cause
                for row in errors):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"physical-return negative {name} lost its exact refusal")
        if name == "intent_aggregate_owned_field_requirement_invalid" and (
                checked.stdout or len(errors) != 1 or
                (errors[0].get("location", {}).get("line"),
                 errors[0].get("location", {}).get("column")) != (29, 43) or
                not errors[0].get("message", "").startswith(
                    "Intent aggregate argument 1 cannot satisfy an owned String-array field requirement")):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit("intent aggregate boundary lost its fail-closed source proof refusal")
        if name in direct_own_named_sites and (
                checked.stdout or len(errors) != 1 or errors[0].get("stage") != "semantic" or
                (errors[0].get("location", {}).get("line"),
                 errors[0].get("location", {}).get("column")) != direct_own_named_sites[name]):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"direct own actual {name} lost its named-source refusal site")
        retained_sites = {
            # These sites are the outer consumer of Make's retained result,
            # not evidence of a refusal directly on the MapSet statement.
            "mapnew_ref_box_value_storage_invalid": [(18, 15), (24, 17)],
            "direct_ref_string_local_retain_invalid": [(8, 20)],
            "direct_ref_string_slot_invalid": [(7, 30)],
            "direct_ref_string_channel_invalid": [(9, 25)],
            "direct_ref_string_retaining_concat_invalid": [(4, 16)],
            "direct_ref_string_opaque_literal_invalid": [(3, 5)],
            "direct_ref_string_opaque_concat_invalid": [(3, 14)],
            "direct_ref_string_alias_literal_invalid": [(4, 5)],
            "direct_ref_string_alias_concat_invalid": [(4, 14)],
            "direct_ref_string_alias_named_invalid": [(5, 14)],
            "recursive_record_wrapper_control_invalid": [(11, 6), (17, 23)],
            "nested_call_retaining_input_invalid": [(7, 6), (8, 24), (13, 17)],
            "physical_call_alias_nested_retain_invalid": [(16, 52), (22, 28)],
            # Build finishes text with a foreign allocator and returns it after
            # the destroy: refused there (11:18, 13:12), not at Probe's call.
            "physical_call_alias_scalar_opaque_invalid": [(11, 18), (13, 12)],
            "physical_call_alias_descriptor_retain_invalid": [(11, 27), (17, 28)],
        }
        if name in retained_sites and (checked.stdout or
                Counter((row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                        for row in errors) != Counter(retained_sites[name])):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"ref-retaining negative {name} lost its owned refusal site")
        array_path_state_sites = {
            "early_return_array_drop_retained_invalid": (11, 15),
            "early_return_array_drop_condition_escape_invalid": (10, 15),
            "array_release_foreign_default_invalid": (6, 15),
            "array_release_foreign_ref_invalid": (6, 15),
            "array_release_forward_foreign_invalid": (10, 15),
            "array_release_owned_strings_foreign_invalid": (8, 27),
            "array_release_forward_own_unseeded_invalid": (17, 17),
            "array_release_alias_before_pending_own_invalid": (21, 17),
            "array_release_alias_after_pending_own_invalid": (21, 17),
            "array_alias_tuple_destructure_own_invalid": (10, 17),
            "array_alias_array_element_destructure_own_invalid": (10, 17),
            "array_alias_slice_element_destructure_own_invalid": (11, 17),
            "array_alias_for_payload_own_invalid": (11, 21),
            "array_alias_for_payload_once_own_invalid": (11, 21),
            "array_alias_match_payload_own_invalid": (11, 39),
            "array_alias_tuple_container_source_own_invalid": (10, 17),
            "array_alias_field_read_own_invalid": (12, 17),
            "array_alias_index_read_own_invalid": (10, 17),
            "array_alias_two_hop_own_invalid": (11, 17),
            "array_field_detach_wrong_field_own_invalid": (12, 20),
            "array_field_detach_partial_branch_own_invalid": (14, 20),
            "array_field_detach_opaque_hold_own_invalid": (15, 20),
            "array_field_detach_live_root_alias_own_invalid": (13, 20),
            "array_field_detach_later_declared_nonrelease_invalid": (9, 13),
            "array_alias_then_numeric_cast_invalid": (12, 17),
            "array_string_member_live_root_own_invalid": (13, 17),
            "array_string_member_cross_field_own_invalid": (13, 17),
            "array_string_member_live_root_growth_own_invalid": (18, 17),
            "array_result_unclosed_retained_array_invalid": (12, 16),
            "array_result_unclosed_opaque_array_invalid": (10, 16),
            "array_result_unclosed_recursive_array_invalid": (13, 16),
            "array_result_unclosed_indirect_array_invalid": (15, 16),
            "array_result_owned_shallow_push_deepdrop_invalid": (12, 27),
            "array_result_owned_shallow_set_deepdrop_invalid": (12, 27),
            "selector_member_effectful_receiver_invalid": (21, 15),
            "selector_array_effectful_index_invalid": (16, 15),
        }
        if name in array_path_state_sites and (
                checked.stdout or len(errors) != 1 or
                errors[0].get("stage") != "semantic" or
                (errors[0].get("location", {}).get("line"),
                 errors[0].get("location", {}).get("column")) != array_path_state_sites[name]):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"array path-state negative {name} lost its exact refusal")
        array_release_multi_sites = {
            # A call that keeps the Array is not refused for its own handoff;
            # what uses the Array after it is (the move, push or drop).
            "array_release_owned_strings_move_invalid": {(14, 17)},
            "array_release_inout_foreign_owned_strings_invalid": {(15, 26)},
            "array_release_prefix_foreign_inout_invalid": {(20, 9)},
            "array_release_factory_retaining_reader_invalid": {(27, 27)},
            "array_release_loop_second_iteration_escape_invalid": {(20, 30)},
            "array_release_inout_retaining_reader_invalid": {(12, 26), (22, 27)},
            "array_result_owned_retain_before_deepdrop_invalid": {(19, 27)},
            # The in-place helper call takes no element ownership; the deep
            # drop of the array it left with a borrowed String is the refusal.
            "array_result_owned_named_inout_shallow_set_invalid": {(17, 27)},
            "array_result_owned_named_inout_index_assign_invalid": {(17, 27)},
            # The earlier extraction is independently cloned when the field is
            # read again.  The final shallow carrier still lacks a release
            # boundary and must remain refused at the second own handoff.
            "array_field_detach_duplicate_alias_own_invalid": {(13, 35)},
            "array_result_borrowed_element_move_deepdrop_invalid": {(9, 32), (10, 27)},
        }
        if name in array_release_multi_sites and (
                checked.stdout or len(errors) != len(array_release_multi_sites[name]) or
                any(row.get("stage") != "semantic" for row in errors) or
                {(row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                 for row in errors} != array_release_multi_sites[name]):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"array release negative {name} lost its exact owned refusal sites")
        array_result_refusal_sites = {
            "array_result_nonown_default_field_own_invalid": [(3, 6)],
            "array_result_nonown_default_field_growth_invalid": [(3, 6)],
            "array_result_nonown_default_forward_own_invalid": [(3, 6), (8, 6)],
            "array_result_nonown_default_local_alias_own_invalid": [(3, 6)],
            "array_result_nonown_default_mixed_return_own_invalid": [(3, 6)],
            "array_result_nonown_inout_field_own_invalid": [(3, 6)],
            "array_result_nonown_ref_nested_projection_invalid": [(3, 5), (14, 28)],
            "array_result_nonown_ref_mapget_descriptor_invalid": [(1, 6), (1, 6)],
            "array_result_local_owned_element_index_assign_invalid": [(6, 27)],
            "array_result_owned_element_option_some_invalid": [(9, 44)],
            "array_result_owned_element_lambda_capture_invalid": [(10, 53)],
        }
        if name in array_result_refusal_sites and (
                checked.stdout or len(errors) != len(array_result_refusal_sites[name]) or
                any(row.get("stage") != "semantic" for row in errors) or
                sorted((row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                       for row in errors) != sorted(array_result_refusal_sites[name])):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"Array result negative {name} lost its exact refusal sites")
        array_element_effect_sites = {
            "physical_array_element_effectful_index_invalid": {(14, 33), (23, 28)},
            "physical_array_element_effectful_receiver_invalid": {(13, 27), (22, 28)},
        }
        if name in array_element_effect_sites and (
                checked.stdout or len(errors) != 2 or
                any(row.get("stage") != "semantic" for row in errors) or
                {(row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                 for row in errors} != array_element_effect_sites[name]):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"physical Array element negative {name} lost its two call effects")
        if name == "control_navigation_nested_retain_invalid" and (
                checked.stdout or len(errors) != 2 or
                any(row.get("stage") != "semantic" for row in errors) or
                {(row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                 for row in errors} != {(21, 19), (26, 15)}):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit("nested control retention lost its two owned refusal sites")
        if name == "consumer_empty_nested_retain_invalid" and (
                checked.stdout or len(errors) != 2 or
                any(row.get("stage") != "semantic" for row in errors) or
                {(row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                 for row in errors} != {(9, 57), (15, 15)}):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit("nested C consumer retention lost its two owned refusal sites")
print("[function-param-flow-summary] physical-return input/store pairs: PASS")

# These calls must still refuse at the unsafe nested actual or alias-forwarding
# site. Only compile and inspect diagnostics; the external Keep is never run.
for name, site in (
    ("stdlib_replace_nested_retaining_invalid", (11, 45)),
    ("stdlib_substring_nested_retaining_invalid", (11, 41)),
):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    try:
        diagnostics = json.loads(output)
    except json.JSONDecodeError:
        raise SystemExit(f"typed String result negative {name} lost structured diagnostics: {output}")
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
    if checked.returncode != 1 or '"pgy.mir.v1"' in output or not any(
            row.get("code") == "PGY_SEM_BORROW_ESCAPE" and
            row.get("cause_ir") == "semantic:borrow_escape" and
            (row.get("location", {}).get("line"), row.get("location", {}).get("column")) == site
            for row in errors):
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"typed String result negative {name} lost its owned refusal at {site}")
print("[function-param-flow-summary] typed String result alias/retention refusals: PASS")

# Wrong arity must fail at the checked builtin signature, not inherit a
# same-name retention row or reach a MIR artifact.
arity_source = root / "tests/cases/function_param_flow_summary/stdlib_prefix_arity_invalid.pgy"
checked = subprocess.run(
    [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(arity_source)],
    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
try:
    diagnostics = json.loads((checked.stdout + checked.stderr).decode(errors="replace"))
except json.JSONDecodeError:
    raise SystemExit("prefix arity negative lost structured diagnostics")
errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
if checked.returncode != 1 or checked.stdout or not errors or (
        errors[0].get("code") != "PGY_SEM_BUILTIN_ARGS_INVALID" or
        errors[0].get("cause_ir") != "semantic:builtin:signature_mismatch"):
    sys.stderr.buffer.write(checked.stderr)
    raise SystemExit("prefix arity negative lost its exact signature refusal")
print("[function-param-flow-summary] prefix signature arity refusal: PASS")

# A selected Intent/Event owns its callable identity even when its spelling
# matches an allocating String builtin. Wrong String bindings must fail at
# assignability before MIR; none of these rejected programs is executed.
for name, site in (
    ("source_callable_intent_tostring_string_invalid", (7, 28)),
    ("source_callable_event_tostring_string_invalid", (4, 28)),
    ("source_callable_intent_stringreplace_string_invalid", (7, 28)),
    ("source_callable_event_substring_string_invalid", (4, 28)),
):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    try:
        diagnostics = json.loads(output)
    except json.JSONDecodeError:
        raise SystemExit(f"selected-callable negative {name} lost structured diagnostics: {output}")
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
    first = errors[0] if errors else None
    if (checked.returncode != 1 or checked.stdout or not isinstance(first, dict)
            or first.get("code") != "PGY_SEM_TYPE_MISMATCH"
            or first.get("cause_ir") != "semantic:assignability_check"
            or (first.get("location", {}).get("line"), first.get("location", {}).get("column")) != site):
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"selected-callable negative {name} lost its exact type refusal at {site}")
print("[function-param-flow-summary] selected source callable String-spoof refusals: PASS")

# Reuse the CFG owner's source cases here without requiring the not-yet-ready
# installed driver. A non-Void routine that can fall through has no completed
# normal-exit receipt for a result frame.
for shape in ("if", "else_arm", "empty_body", "empty_body_called"):
    source = root / "tests/self_hosted/fixtures" / f"missing_return_{shape}_negative.pgy"
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    try:
        diagnostics = json.loads((checked.stdout + checked.stderr).decode(errors="replace"))
    except json.JSONDecodeError:
        raise SystemExit(f"missing-return {shape} lost structured diagnostics")
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
    if checked.returncode != 1 or checked.stdout or not errors or (
            errors[0].get("code") != "PGY_SEM_MISSING_RETURN" or
            errors[0].get("cause_ir") != "semantic:cfg:missing_return_path"):
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"missing-return {shape} lost its exact CFG refusal")
print("[function-param-flow-summary] native missing-return CFG refusals: PASS")

# These constructors were already unready before the Result-birth premise.
# Preserve the existing required-backing refusal rather than recategorising
# it as a completed backing relation or a borrow-only refusal.
required_backing_message = (
    "Function parameter flow summary failed closed: "
    "required return backing relation is incomplete or stale"
)
for name in (
    "opaque_builder_birth_invalid",
):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    try:
        diagnostics = json.loads(output)
    except json.JSONDecodeError:
        raise SystemExit(f"resource-birth unready {name} lost structured diagnostics: {output}")
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
    owner_errors = [row for row in errors if (
        row.get("message") == required_backing_message and
        row.get("stage") == "semantic" and "code" not in row and "cause_ir" not in row)]
    if checked.returncode != 1 or checked.stdout or '"pgy.mir.v1"' in output or not owner_errors or any(
            row not in owner_errors and (
                row.get("stage") != "semantic" or
                row.get("code") != "PGY_SEM_BORROW_ESCAPE" or
                row.get("cause_ir") != "semantic:borrow_escape")
            for row in errors):
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"resource-birth unready {name} lost its exact required-backing refusal")
print("[function-param-flow-summary] resource-birth unready constructor controls: PASS")

# A private Int push may justify a local store, but an external call before or
# after it still cannot make the returned nested rows input-independent. The
# old compiler also reports an incomplete result relation; once the local
# store premise lands, the exact borrow refusal must remain at the consumer.
for name in (
    "zeroarg_private_int_keep_before_invalid",
    "zeroarg_private_int_keep_after_invalid",
):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    try:
        diagnostics = json.loads(output)
    except json.JSONDecodeError:
        raise SystemExit(f"private Int store negative {name} lost structured diagnostics: {output}")
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"] if isinstance(diagnostics, list) else []
    def required_relation(row):
        return (row.get("stage") == "semantic" and row.get("message") == required_backing_message and
                "code" not in row and "cause_ir" not in row and
                (row.get("location", {}).get("line"), row.get("location", {}).get("column")) == (35, 6))
    def borrowed_at_consumer(row):
        return (row.get("stage") == "semantic" and row.get("code") == "PGY_SEM_BORROW_ESCAPE" and
                row.get("cause_ir") == "semantic:borrow_escape" and
                (row.get("location", {}).get("line"), row.get("location", {}).get("column")) == (47, 28))
    if checked.returncode != 1 or checked.stdout or not errors or not any(map(borrowed_at_consumer, errors)) or any(
            not required_relation(row) and not borrowed_at_consumer(row) for row in errors):
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"private Int store negative {name} lost its exact borrow refusal")
print("[function-param-flow-summary] private Int store retention refusals: PASS")

# This original full-program falsifier intentionally keeps ClaimSlot, Release
# and invocation after the rejected Make definition. Native error recovery can
# leave those later locals unbound. Only this independently observed case uses
# its FIRST diagnostic as the safety oracle; later diagnostics are recorded,
# not mistaken for either the original cause or additional borrow coverage.
def check_full_capture_refusal(name, expected_site):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    try:
        diagnostics = json.loads(output)
    except json.JSONDecodeError:
        raise SystemExit(f"full capture negative {name} lost structured diagnostics: {output}")
    first = diagnostics[0] if isinstance(diagnostics, list) and diagnostics else None
    if (checked.returncode != 1 or '"pgy.mir.v1"' in output or not isinstance(first, dict)
            or first.get("severity") != "error" or first.get("code") != "PGY_SEM_BORROW_ESCAPE"
            or first.get("cause_ir") != "semantic:borrow_escape"
            or (first.get("location", {}).get("line"), first.get("location", {}).get("column")) != expected_site):
        sys.stderr.buffer.write(checked.stderr)
        raise SystemExit(f"full capture negative {name} lost its primary owned borrow refusal")
    secondary = [{key: row.get(key) for key in ("severity", "code", "cause_ir", "location")}
                 for row in diagnostics[1:] if isinstance(row, dict)]
    print(f"[function-param-flow-summary] {name} secondary diagnostics: " + json.dumps(secondary, sort_keys=True))

check_full_capture_refusal("slot_wrapper_capture_slot_closure_return_invalid", (2, 49))

for name, code, cause in (
    ("stdlib_trim_arity_invalid", "PGY_SEM_BUILTIN_ARGS_INVALID", "semantic:builtin:signature_mismatch"),
    ("stdlib_toint_arity_invalid", "PGY_SEM_BUILTIN_ARGS_INVALID", "semantic:builtin:signature_mismatch"),
    ("stdlib_trim_shadow_invalid", "PGY_SEM_REDECLARATION", "semantic:function:builtin_name_reserved"),
    ("stdlib_toint_shadow_invalid", "PGY_SEM_REDECLARATION", "semantic:function:builtin_name_reserved"),
):
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10.0, check=False)
    output = (checked.stdout + checked.stderr).decode(errors="replace")
    diagnostics = json.loads(output)
    errors = [row for row in diagnostics if isinstance(row, dict) and row.get("severity") == "error"]
    if checked.returncode != 1 or '"pgy.mir.v1"' in output or not any(
            row.get("code") == code and row.get("cause_ir") == cause for row in errors):
        raise SystemExit(f"stdlib identity/arity negative {name} lost {code}/{cause}")
runtime_dir = pathlib.Path(tempfile.mkdtemp(prefix="stdlib-input-retention."))
# ToString(Bool) output parity is behavioral only: LLVM may return static text.
# These cases do not assert a fresh allocation or an exit-drop right.
for name, expected in (
    ("stdlib_input_retention_valid", "12\n2\n0\n-2147483648\n\n"),
    ("stdlib_replace_option_result_valid", "6\n"),
    ("stdlib_substring_option_result_valid", "bc\nabcde\n"),
    ("stdlib_tostring_scalar_option_result_valid", "5\ntrue\n12345\n7\nabcde\n"),
    ("source_callable_intent_tostring_valid", "true\n"),
    ("source_callable_event_tostring_handler_valid", "handled=7\nafter event\n"),
    ("source_callable_class_tostring_constructor_valid", "7\n"),
    ("source_callable_enum_tostring_payload_valid", "9\n"),
    ("source_callable_class_method_alias_valid", "31\n"),
    ("source_callable_enum_method_alias_valid", "19\n"),
    ("source_callable_enum_payload_method_alias_valid", "23\n"),
    ("source_callable_namespace_valid", "7\n"),
    ("stdlib_trim_result_return_valid", "heap text\n"),
    ("result_frame_option_multi_exit_valid", "heap text!\none!\nfalse\n13\n"),
    ("physical_call_alias_inout_valid", "7\n"),
    ("physical_array_element_loop_valid", "7\n"),
    ("control_navigation_nested_loop_valid", "6\n"),
    ("control_navigation_dead_ancestor_valid", "4\n"),
    ("consumer_empty_nested_compare_valid", "7\n"),
    ("zeroarg_private_int_nested_valid", "1\n4\n"),
    ("array_result_nonown_fresh_scalar_valid", "1\n"),
    ("array_result_nonown_own_relay_valid", "1\n"),
    ("array_result_nonown_ref_scalar_projection_valid", "7\n7\n"),
    ("array_result_owned_factory_deepdrop_valid", "1\n"),
    ("array_result_owned_clone_deepdrop_valid", "1\n"),
    ("array_result_owned_mapkeys_deepdrop_valid", "1\n"),
    ("array_result_local_owned_element_reader_valid", "owned value\n11\n1\n"),
    ("array_result_owned_element_inout_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_tuple_concat_copy_valid", "owned value\nowned value\n"),
    ("array_result_owned_element_tuple_return_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_option_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_result_ok_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_result_err_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_slot_assign_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_slot_write_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_lambda_concat_copy_valid", "11\n"),
    ("array_result_owned_element_event_concat_copy_valid", "owned value\nafter event\n"),
    ("array_result_owned_element_channel_send_copy_valid", "owned value\n"),
    ("array_result_owned_element_list_push_copy_valid", "owned value\n"),
    ("array_result_owned_element_list_set_copy_valid", "owned value\n"),
    ("array_result_owned_element_queue_push_copy_valid", "owned value\n"),
    ("array_result_owned_element_set_add_copy_valid", "true\n"),
    ("array_result_owned_element_map_key_copy_valid", "7\n"),
    ("array_result_owned_element_map_value_copy_valid", "owned value\n"),
    ("array_result_owned_element_array_assign_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_defer_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_defer_lifo_read_then_drop_valid", "owned value\n"),
    ("array_result_owned_element_defer_mutable_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_defer_same_body_read_then_drop_valid", "owned value\n"),
    ("array_result_owned_element_defer_without_drop_valid", "owned value\n"),
    ("defer_multipart_mutable_exit_valid", "2\n3\n7\n"),
    ("receiver_result_readonly_scalar_valid", "3\n"),
    ("array_result_owned_element_device_write_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_host_method_reader_valid", "11\n"),
    ("array_result_owned_element_local_return_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_secure_slot_initializer_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_slot_initializer_concat_copy_valid", "owned value\n"),
    ("array_result_owned_element_string_plus_copy_probe", "owned value\n"),
    ("array_result_owned_element_substring_copy_valid", "owned\n"),
    ("array_result_owned_defer_named_reader_valid", "11\n"),
    ("array_result_owned_defer_named_array_reader_valid", "1\n"),
    ("early_return_array_drop_valid", "0\n1\n"),
    ("early_return_array_drop_mixed_inputs_valid", "6\n6\n"),
    ("early_return_private_array_push_nested_valid", "4\n"),
    ("array_release_named_reader_forward_valid", "1\n"),
    ("array_release_owned_strings_named_reader_valid", "1\n"),
    ("array_release_named_reader_own_unseeded_valid", "1\n1\n"),
    ("array_release_owned_strings_move_valid", "1\n"),
    ("array_release_recursive_inout_owned_strings_valid", "2\n"),
    ("array_release_reader_record_owned_strings_valid", "1\n"),
    ("array_release_prefix_reader_inout_valid", "2\n"),
    ("array_release_loop_outer_recheck_valid", "2\n"),
    ("array_release_loop_local_recheck_valid", "1\n1\n"),
    ("array_release_inout_reader_then_push_valid", "2\n"),
    ("array_release_factory_reader_deepdrop_valid", "1\n"),
    ("array_release_class_method_own_call_valid", "1\n"),
    ("array_release_generic_own_call_valid", "1\n"),
    ("selector_member_array_nested_valid", "-9\n"),
    ("backward_loop_output_literal_valid", "literal output\n"),
    ("nested_call_independent_result_valid", "independent result\ntrue\n"),
    ("named_string_identity_substring_push_valid", "1\nowned\n"),
    ("named_string_all_def0_branch_valid", "ready\nready\nready\nready\n"),
    ("named_string_all_def0_loop_concat_valid", "a\nax\naxx\n"),
    ("named_string_all_def0_known_zero_valid", "seed\nnext\n"),
    ("named_string_field_empty_arena_valid", "TypeName\nTypeName\n"),
    ("named_string_field_lineage_two_ref_control_valid", "9\n"),
    ("named_string_field_lineage_two_ref_empty_valid", "9\n"),
    ("named_string_field_lineage_named_order_valid", "9\nx\n"),
    ("named_string_field_lineage_unrelated_match_valid", "9\n"),
    ("named_string_field_lineage_unrelated_with_valid", "9\n"),
    ("named_string_same_empty_direct_valid", "TypeName\nTypeName\n"),
    ("named_string_same_empty_named_valid", "TypeName\nTypeName\n"),
    ("named_string_same_empty_fresh_valid", "fresh value\n"),
    ("option_payload_fresh_literal_none_valid", "seed!\nliteral!\nfalse\nfalse\n"),
    ("option_payload_local_alias_valid", "ready\nheap\n"),
    ("option_payload_shared_carrier_valid", "shared\nshared\nheap\n"),
    ("option_payload_literal_array_return_valid", "alpha\nfalse\nheap\n"),
    ("readfile_result_direct_valid", "9\n0\nheap\n"),
    ("readfile_result_borrowed_path_valid", "9\nheap\n"),
    ("readfile_result_named_return_valid", "9\nheap\n"),
    ("readfile_result_defer_valid", "heap\n9\n"),
    ("readfile_result_defer_late_valid", "heap\n2\n"),
    ("readfile_result_distinct_loop_valid", "9\n9\n0\n9\n9\n9\nheap\n"),
    ("readfile_result_multipart_defer_valid", "heap\n9\n0\n"),
    ("readfile_result_control_flow_valid", "while\n9\nfor-start\n3\nfor-end\n9\nnested-if\nmatched\nshort-circuit\ntaken\nheap\n"),
    ("nonowner_risk_option_reader_valid", "9\n0\n"),
    ("nonowner_risk_shallow_loop_valid", "8\n8\n2\n"),
    ("private_array_push_local_read_valid", "1\n"),
    ("return_loan_literal_slot_valid", "true\nstatic literal\n"),
    ("return_loan_read_static_local_valid", "14\n14\nstatic literal\n"),
    ("slot_read_payload_same_write_valid", "2\nkept\n"),
    ("string_element_clone_after_drop_valid", "owned value"),
    ("string_element_clone_returned_valid", "abefef"),
    ("string_element_clone_getter_owner_changed_valid", "abcd"),
    ("string_element_clone_ref_output_valid", "ababab1"),
    ("string_copy_landing_same_function_batch_valid", "LR"),
    ("string_copy_landing_function_boundary_valid", "ABCD"),
    ("formal_string_store_same_routine_batch_valid", "a1b2c3"),
    ("formal_string_store_multiple_formals_batch_valid", "domainTypeOpSymbol"),
    ("formal_string_push_same_routine_batch_valid", "a1b2c3"),
    ("owned_string_push_multi_receiver_batch_valid", "a1b2c3"),
    ("formal_string_store_cross_routine_batch_valid", "firstsecond"),
    ("formal_string_store_batch_boundary_valid", "1\nab"),
    ("formal_string_store_landing_ref_push_valid", "2ba"),
    ("formal_string_store_landing_map_array_set_valid", "2tt"),
    ("formal_string_store_landing_local_receiver_valid", "aa"),
    ("formal_string_store_landing_private_local_valid", "1a"),
    ("formal_string_store_landing_field_take_valid", "tt"),
    ("formal_store_landing_struct_output_valid", "src.pgysrc.pgy1"),
    ("formal_store_landing_alias_output_valid", "src.pgysrc.pgy3"),
    ("array_push_loan_inout_landed_valid", "ab7"),
    ("aggregate_move_void_call_landed_valid", "3"),
    ("by_value_getter_lend_inout_passed_landed_valid", "2"),
    ("by_value_getter_lend_inout_then_returned_landed_valid", "21"),
    ("formal_string_store_landing_value_source_valid", "tt1"),
    ("formal_string_store_landing_value_set_valid", "tt1"),
    ("formal_string_store_landing_value_map_valid", "tt1"),
    ("named_string_same_empty_borrowed_then_owned_direct_landed_valid", "TypeName\n"),
    ("named_string_same_empty_owned_then_borrowed_direct_landed_valid", "TypeName\n"),
    ("named_string_same_empty_owned_then_borrowed_named_landed_valid", "TypeName\n"),
    ("named_string_same_empty_borrowed_then_owned_named_landed_valid", "TypeName\n"),
    ("string_inout_returned_literal_then_escape_call_landed_valid", "1"),
    ("string_inout_unwrapped_literal_then_escape_call_landed_valid", "1"),
    ("string_inout_returned_literal_then_own_release_landed_valid", ""),
    ("struct_clone_array_field_copied_valid", "11a"),
    ("struct_clone_option_payload_copied_valid", "1"),
    ("struct_clone_nested_field_copied_valid", "1t"),
    ("struct_clone_ref_returned_valid", "1a"),
    ("struct_clone_map_field_ref_returned_valid", "11a"),
    ("map_clone_ref_returned_valid", "one"),
    ("map_clone_scalar_keys_ref_returned_valid", "305false"),
    ("struct_clone_option_field_ref_returned_valid", "l1"),
    ("struct_clone_subject_copied_valid", "13"),
    ("formal_string_store_landing_shadowed_function_valid", "2ba"),
    ("formal_string_store_landing_shadowed_struct_valid", "1a7"),
    ("formal_string_store_landing_shadowed_method_valid", "tt"),
    ("by_value_getter_lend_owner_written_landed_valid", "2"),
    ("by_value_getter_lend_mutated_landed_valid", "4"),
    ("by_value_getter_lend_local_transfer_landed_valid", "abcd"),
    ("by_value_getter_lend_owner_written_shadowed_landed_valid", "2"),
    ("by_value_getter_lend_kept_landed_valid", "1"),
    ("by_value_getter_lend_returned_landed_valid", "1ab"),
    ("by_value_getter_lend_passed_kept_landed_valid", "2ab"),
    ("by_value_getter_lend_assigned_landed_valid", "1ab"),
    ("by_value_getter_lend_owner_written_after_lowering_landed_valid", "22"),
    ("by_value_getter_lend_result_grown_landed_valid", "10"),
    ("default_aggregate_move_shared_field_return_landed_valid", "1"),
    ("by_value_getter_result_read_valid", "6"),
    ("constructor_result_read_valid", "9"),
    ("option_payload_result_read_valid", "7"),
    ("optional_wrap_result_read_valid", "6"),
    ("by_value_getter_lend_result_lent_on_valid", "ab2ab"),
    ("by_value_getter_lend_string_reassigned_valid", "ab!1ab"),
    ("by_value_getter_result_local_returned_valid", "6"),
    ("constructor_of_getter_result_returned_valid", "7"),
    ("enum_payload_constructor_ref_field_return_valid", "owner"),
    ("inout_defers_handed_to_recursive_emitter_valid", "dd"),
    ("by_value_array_grown_by_inout_callee_landed_valid", "2"),
    ("by_value_array_grown_by_inout_callee_last_use_valid", "2"),
    ("by_value_array_handed_to_by_value_reader_valid", "6"),
    ("by_value_array_grown_two_calls_deep_landed_valid", "2"),
    ("by_value_array_grown_through_recursive_hand_off_landed_valid", "6"),
    ("by_value_cursor_set_through_recursive_parser_valid", "[a]3"),
    ("by_value_array_popped_by_inout_callee_valid", "22"),
    ("by_value_array_handed_to_generic_reader_valid", "3"),
    ("inout_string_chain_popped_by_recursive_parser_valid", "00000101101011110"),
    ("by_value_argument_beside_its_inout_landed_valid", "13"),
    ("by_value_argument_before_its_inout_landed_valid", "15"),
    ("scalar_and_string_beside_their_inout_valid", "54a!a"),
    ("by_value_array_to_two_reading_formals_valid", "32"),
    ("array_handed_to_two_keeping_formals_landed_valid", "1"),
    ("array_handed_to_three_keeping_formals_landed_valid", "102"),
    ("array_field_detach_two_own_valid", "1\n1\n0\n0\n"),
    ("array_field_detach_caller_alias_landed_valid", "1110"),
    ("array_field_detach_later_declared_own_valid", "2\n0\n"),
    ("one_storage_in_two_constructor_parts_returned_landed_valid", "211"),
    ("one_storage_through_alias_in_two_parts_landed_valid", "211"),
    ("one_storage_in_two_literal_elements_landed_valid", "22"),
    ("two_parts_of_distinct_storage_valid", "22"),
    ("one_storage_in_two_map_parts_landed_valid", "0"),
    ("declared_result_member_chain_qualifier_valid", "no names"),
    ("mutual_recursion_declared_result_stored_valid", "Array<Int>ap<String>x!"),
    ("default_aggregate_move_map_field_set_landed_valid", "1"),
    ("formal_field_map_copy_changed_at_exit_landed_valid", "0\n0\n"),
    ("inout_callee_same_map_field_changed_landed_valid", "3\n"),
    ("local_copy_map_changed_landed_valid", "11\ntrue\n"),
    ("local_copy_map_held_at_field_changed_landed_valid", "0\n0\n1\n0\n"),
    ("local_copy_map_inout_callee_changes_landed_valid", "10"),
    ("distinct_fields_of_a_local_copy_in_two_parts_valid", "21"),
    ("constructor_of_formal_values_stored_valid", "3ab"),
    ("path_alias_lent_to_string_getter_valid", "abab"),
    ("option_string_result_reassigned_valid", "3ab"),
    ("string_struct_result_reassigned_valid", "3ab"),
    ("by_value_getter_lend_option_string_unwrapped_valid", "3ab"),
    ("by_value_getter_lend_string_collected_valid", "3ab"),
    ("by_value_getter_lend_result_path_grown_landed_valid", "10"),
    ("by_value_formal_rebound_to_result_valid", "7"),
    ("alias_rebound_to_result_valid", "8"),
    ("path_alias_returned_in_constructor_valid", "7"),
    ("enum_formal_matched_payload_read_valid", "4"),
    ("by_value_getter_lend_result_passed_grown_landed_valid", "10"),
    ("by_value_getter_lend_in_constructor_passed_grown_landed_valid", "10"),
    ("by_value_getter_second_lend_grown_landed_valid", "12"),
    ("by_value_getter_second_lend_formal_grown_landed_valid", "14"),
    ("by_value_getter_lend_in_constructor_grown_landed_valid", "10"),
    ("by_value_getter_lend_owner_in_constructor_grown_landed_valid", "8"),
    ("by_value_getter_two_lends_grown_landed_valid", "14"),
    ("local_let_alias_grown_landed_valid", "10"),
    ("local_let_alias_source_grown_landed_valid", "10"),
    ("local_assigned_alias_grown_landed_valid", "10"),
    ("local_constructor_alias_grown_landed_valid", "10"),
    ("local_array_literal_alias_grown_landed_valid", "10"),
    ("local_pushed_alias_grown_landed_valid", "10"),
    ("formal_let_alias_grown_landed_valid", "14"),
    ("local_copy_of_copy_grown_landed_valid", "10"),
    ("local_copy_source_element_set_landed_valid", "15"),
    ("local_copy_source_inout_grown_landed_valid", "10"),
    ("local_let_alias_read_valid", "7"),
    ("local_source_grown_before_copy_valid", "17"),
    ("local_copy_grown_source_unused_valid", "18"),
    ("local_copy_in_loop_valid", "9"),
    ("local_copy_rebound_valid", "9"),
    ("owned_copy_string_push_landed_valid", "5c"),
    ("local_copy_source_sorted_landed_valid", "31"),
    ("array_sort_after_handoff_ref_reader_valid", "4"),
    ("array_handoff_return_value_loop_valid", "3"),
    ("inout_call_after_inner_loop_valid", "3"),
    ("output_field_store_of_ref_formal_landed_valid", "x1"),
    ("inout_self_assignment_lands_nothing_valid", "1"),
    ("borrowed_inout_array_kept_in_result_landed_valid", "23"),
    ("moved_argument_two_branches_landed_valid", "21"),
    ("moved_argument_chain_batch_landed_valid", "2221"),
    ("by_value_getter_lend_into_own_formal_landed_valid", "1"),
    ("returned_alias_lends_formal_valid", "22"),
    ("lent_path_copy_grown_landed_valid", "75"),
    ("borrowed_owner_loan_grown_landed_valid", "31"),
    ("string_crossing_window_pair_landed_valid", "row"),
    ("by_value_lend_then_owner_cloned_valid", "a41"),
    ("local_copy_inout_read_only_callee_valid", "10"),
    ("array_result_local_owned_element_constructor_landed_valid", "owned value\n"),
    ("array_result_local_owned_element_field_retention_landed_valid", "owned value\n"),
    ("array_result_owned_element_array_assign_landed_valid", "owned value\n"),
    ("array_result_owned_element_device_write_landed_valid", "owned value\n"),
    ("array_result_owned_element_local_return_landed_valid", "owned value\n"),
    ("array_result_owned_element_result_err_landed_valid", "owned value\n"),
    ("array_result_owned_element_result_ok_landed_valid", "owned value\n"),
    ("array_result_owned_element_secure_slot_initializer_landed_valid", "owned value\n"),
    ("array_result_owned_element_secure_slot_write_landed_valid", ""),
    ("array_result_owned_element_slot_assign_landed_valid", "owned value\n"),
    ("array_result_owned_element_slot_initializer_landed_valid", "owned value\n"),
    ("array_result_owned_element_slot_write_landed_valid", "owned value\n"),
    ("array_result_owned_element_spawn_landed_valid", "11\n"),
    ("array_result_owned_element_tuple_binding_landed_valid", "owned value\n"),
    ("array_result_owned_element_tuple_return_landed_valid", "owned value\n"),
    ("array_result_owned_element_write_view_landed_valid", ""),
    ("named_string_field_lineage_root_overwrite_landed_valid", "14\n"),
    ("option_payload_mixed_borrow_landed_valid", ""),
    ("string_element_tostring_returned_landed_valid", "ab"),
    ("string_join_shadowed_returns_part_landed_valid", "red"),
    ("aggregate_string_loan_local_owner_landed_valid", "ab7"),
    ("aggregate_string_loan_owner_kept_landed_valid", "ab7ab7"),
    ("aggregate_string_loan_owner_moved_landed_valid", "ab71"),
    ("aggregate_string_loan_owner_mutated_landed_valid", "ab7"),
    ("aggregate_string_loan_struct_escape_landed_valid", "abc"),
    ("callee_alias_branch_return_record_landed_valid", "heap text\n"),
    ("callee_alias_return_record_landed_valid", "heap text\n"),
    ("declared_array_borrow_landed_valid", "2\n"),
    ("default_string_option_return_landed_valid", "9\n"),
    ("default_string_string_return_landed_valid", "heap text\n"),
    ("result_frame_formal_array_push_landed_valid", "1\n"),
    ("result_frame_formal_nominal_landed_valid", "9\n"),
    ("result_frame_formal_return_landed_valid", "9\n"),
    ("result_frame_formal_some_landed_valid", "9\n"),
    ("return_loan_direct_slot_landed_valid", "true\nheap text\n"),
    ("return_loan_inout_landed_valid", "heap text\n"),
    ("return_loan_mutated_literal_assignment_landed_valid", "heap text\n"),
    ("return_loan_mutated_literal_bodyful_inout_landed_valid", "heap text\n"),
    ("return_loan_staged_slot_landed_valid", "true\nheap text\n"),
    ("return_loan_wrap_slot_landed_valid", "true\nheap text\n"),
    ("stdlib_tostring_string_forward_landed_valid", "9\n"),
    ("match_payload_return_moves_landed_valid", "a"),
    ("default_aggregate_move_field_alias_returned_landed_valid", "4"),
    ("unit_enum_constant_constructor_moves_landed_valid", "a"),
    ("path_alias_returned_in_constructor_grown_landed_valid", "10"),
    ("option_payload_result_grown_landed_valid", "10"),
    ("optional_wrap_result_grown_landed_valid", "10"),
    ("local_copy_changed_then_moved_landed_valid", "11"),
    ("local_copied_path_then_moved_landed_valid", "12"),
    ("intent_action_stores_by_value_moves_landed_valid", "a"),
    ("intent_aggregate_readonly_boundary_valid", ""),
    ("nonowner_risk_branch_owned_insert_landed_valid", "8\n"),
    ("nonowner_risk_later_loop_owned_landed_valid", "8\n8\n"),
    ("nonowner_risk_safe_then_owned_landed_valid", "8\n0\n"),
    ("nonowner_risk_owned_then_safe_landed_valid", "8\n0\n"),
    ("inout_owned_param_input_return_moves_landed_valid", "1"),
    ("formal_array_store_field_take_landed_valid", "11"),
    ("default_aggregate_move_option_field_alias_store_landed_valid", "3"),
    ("default_aggregate_move_field_store_landed_valid", "ab"),
    ("default_aggregate_move_field_set_landed_valid", "ab"),
    ("default_aggregate_move_field_element_write_landed_valid", "ab"),
    ("default_aggregate_move_field_alias_store_landed_valid", "ab"),
    ("default_aggregate_move_field_alias_passed_on_landed_valid", "4"),
    ("constructor_result_grown_landed_valid", "13"),
    ("by_value_getter_result_reassigned_grown_landed_valid", "10"),
    ("by_value_getter_result_passed_grown_landed_valid", "10"),
    ("by_value_getter_result_local_returned_grown_landed_valid", "10"),
    ("by_value_getter_result_grown_landed_valid", "10"),
    ("by_value_getter_lend_inout_owner_landed_valid", "abzz"),
    ("by_value_formal_rebound_to_result_grown_landed_valid", "11"),
    ("by_value_formal_array_sorted_landed_valid", "13"),
    ("array_slice_view_returned_landed_valid", "ab"),
    ("array_result_owned_element_inout_escape_landed_valid", "owned value"),
    ("array_handoff_error_return_after_append_valid", "10x"),
    ("defer_array_drop_reads_valid", "42"),
    ("defer_array_drop_fallthrough_valid", "8"),
    ("defer_owned_strings_read_valid", "ab!"),
    ("defer_own_handoff_cleanup_reads_valid", "42"),
    ("defer_array_drop_lambda_reads_valid", "7"),
    ("defer_array_drop_struct_reader_valid", "42"),
    ("string_inout_helper_then_own_release_landed_valid", ""),
    ("string_inout_helper_then_escape_call_landed_valid", "1"),
    ("string_inout_concat_copy_then_escape_call_valid", "ab"),
    ("string_push_after_loop_walk_landed_valid", "cd"),
    ("ref_view_beside_unit_variant_valid", "1"),
    ("string_inout_literal_then_escape_call_landed_valid", "1"),
    ("array_result_owned_named_inout_shallow_push_landed_valid", ""),
    ("string_local_pushed_into_field_array_landed_valid", "yx"),
    ("string_element_push_copy_landing_mixed_landed_valid", "header023"),
    ("ref_input_through_call_result_into_inout_output_landed_valid", "f26"),
    ("ref_string_through_inout_reader_shallow_store_landed_valid", "2\n2c"),
):
    for backend in ("c", "llvm"):
        executable = runtime_dir / (name + "-" + backend + ".exe")
        source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
        compiled = subprocess.run(
            [pgy, "--native-pipeline", "--backend=" + backend, "--opt=dev", str(source), "-o", str(executable)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=60.0, check=False)
        if compiled.returncode != 0:
            sys.stderr.buffer.write(compiled.stderr)
            raise SystemExit(f"stdlib input reader {name} failed to compile for {backend}")
        case_cwd = root if name.startswith("readfile_result_") else None
        if name == "readfile_result_defer_late_valid":
            case_cwd = runtime_dir / (name + "-" + backend + "-cwd")
            case_cwd.mkdir()
        observed = subprocess.run([str(executable)], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=10.0, check=False,
                                  cwd=case_cwd)
        if observed.returncode != 0 or observed.stderr or observed.stdout.decode().replace("\r\n", "\n") != expected:
            raise SystemExit(f"stdlib input reader {name} runtime mismatch for {backend}: {observed.stdout!r}")
print("[function-param-flow-summary] stdlib input retention C/LLVM and identity/arity controls: PASS")
try:
    run = subprocess.run(
        [pgy, "--native-pipeline", "--hir", fixture],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        timeout=10.0,
        check=False,
    )
except subprocess.TimeoutExpired:
    raise SystemExit("recursive summary fixture exceeded 10 seconds")

if run.returncode != 0:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit(f"recursive summary fixture failed with {run.returncode}")

resource_fixture = root / "tests/cases/slot_contract/positive/plain_read_write_release/main.pgy"
resource_run = subprocess.run(
    [pgy, "--native-pipeline", "--hir", str(resource_fixture)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=env,
    timeout=10.0,
    check=False,
)
if resource_run.returncode != 0:
    sys.stderr.buffer.write(resource_run.stderr)
    raise SystemExit(
        f"resource-flow HIR fixture failed with {resource_run.returncode}"
    )
hir_symbols = re.findall(
    rb"resource-flow-symbols=(\d+)", resource_run.stdout
)
if not hir_symbols or max(map(int, hir_symbols)) < 1:
    sys.stderr.buffer.write(resource_run.stdout)
    raise SystemExit("HIR did not carry ResourceFlowUniverse symbols")

match = re.search(
    rb"pgy: function-param-flow entries=(\d+) body_evaluations=(\d+) "
    rb"cache_hits=(\d+) recursion_hits=(\d+) fixed_point_passes=(\d+)",
    run.stderr,
)
if match is None:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit("function parameter flow summary telemetry is missing")

entries, evaluations, cache_hits, recursion_hits, passes = map(
    int, match.groups()
)
if entries < 1:
    raise SystemExit("expected at least one demanded summary")
if recursion_hits < 1:
    raise SystemExit("mutual recursion was not detected")
if evaluations <= entries:
    raise SystemExit("recursive component did not perform a fixed-point revisit")
if evaluations > 8 or passes > 6:
    raise SystemExit(
        "recursive component exceeded its bounded fixture budget: "
        f"evaluations={evaluations} passes={passes}"
    )
sparse = re.search(
    rb"pgy: function-param-flow-sparse functions=(\d+) "
    rb"statement_visits=(\d+) program_points=(\d+)",
    run.stderr,
)
if sparse is None:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit("function parameter flow sparse telemetry is missing")
indexed_functions, statement_visits, program_points = map(int, sparse.groups())
if indexed_functions < 1 or statement_visits <= 0:
    raise SystemExit("sparse owner did not index the demanded function")
if program_points >= statement_visits:
    raise SystemExit(
        "summary evaluator did not reduce the demanded program points: "
        f"statement_visits={statement_visits} program_points={program_points}"
    )
print(
    "[function-param-flow-summary] "
    f"entries={entries} evaluations={evaluations} cache_hits={cache_hits} "
    f"recursion_hits={recursion_hits} passes={passes} "
    f"statement_visits={statement_visits} program_points={program_points}"
)

# The work cap belongs to one demanded closure, not the entire compilation.
# Independent fresh queries may exceed it in aggregate; one oversized closure
# must still fail. Neither case has a deep stack or recursive call edges.
(root / ".tmp").mkdir(exist_ok=True)
pressure_dir = pathlib.Path(tempfile.mkdtemp(
    prefix="function-param-flow-budget.", dir=root / ".tmp"
))
pressure_env = os.environ.copy()
pressure_env["PGY_DEBUG_FUNCTION_PARAM_FLOW"] = "1"
pressure_env.pop("PGY_DEBUG_RESOURCE_FLOW_FACTS", None)
for shape, count in (("independent", 4097), ("single-closure", 4096)):
    leaves = "\n".join(
        f"func Leaf{i}(ref values: Array<Int>) -> Int {{ return values[0]; }}"
        for i in range(count)
    )
    calls = "\n".join(f"    Leaf{i}(values);" for i in range(count))
    if shape == "independent":
        source_text = leaves + (
            "\nfunc Main() -> Void {\n    let values: Array<Int> = [7];\n"
            + calls + "\n}\n"
        )
    else:
        source_text = (
            "func Entry(ref values: Array<Int>) -> Int { return FanOut(values); }\n"
            "func FanOut(ref values: Array<Int>) -> Int {\n" + calls
            + "\n    return 0;\n}\n" + leaves
            + "\nfunc Main() -> Void { let values: Array<Int> = [7]; Entry(values); }\n"
        )
    source_path = pressure_dir / f"{shape}.pgy"
    source_path.write_text(source_text, encoding="utf-8")
    try:
        pressure = subprocess.run(
            [pgy, "--native-pipeline", "--hir", str(source_path)],
            stdout=subprocess.PIPE if shape == "single-closure" else subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            env=pressure_env, timeout=30.0, check=False,
        )
    except subprocess.TimeoutExpired:
        raise SystemExit(f"{shape} demand-budget control exceeded 30 seconds")
    (pressure_dir / f"{shape}.err").write_bytes(pressure.stderr)
    telemetry = re.search(
        rb"function-param-flow entries=(\d+) body_evaluations=(\d+) "
        rb"cache_hits=(\d+) recursion_hits=(\d+)", pressure.stderr,
    )
    if telemetry is None or int(telemetry[4]) != 0:
        raise SystemExit(f"{shape} control lost its nonrecursive telemetry")
    work = re.search(rb"function-param-flow-work work_units=(\d+) work_budget=(\d+)",
                     pressure.stderr)
    if work is None or int(work[2]) != 4096 or int(work[1]) > int(work[2]):
        raise SystemExit(f"{shape} control lost its actual demand-episode work boundary")
    if shape == "independent":
        if pressure.returncode != 0 or tuple(map(int, telemetry.groups()[:2])) != (count, count):
            raise SystemExit("independent demands consumed an unrelated episode's budget")
    elif pressure.returncode != 1 or pressure.stdout or int(work[1]) != 4096 or (
        b"recursive summary work budget exceeded" not in pressure.stderr
    ):
        raise SystemExit("one oversized demand closure escaped the unchanged work cap")
# The cap counts a closure's entry evaluations, not its frame node visits: a
# recursive pair with six Array inputs and a long body is a small closure
# whose frames walk more nodes than the cap, and it must still be admitted.
wide_inputs = 6
wide_formals = ", ".join(f"ref a{i}: Array<Int>" for i in range(wide_inputs)) + ", n: Int"
wide_actuals = ", ".join(f"a{i}" for i in range(wide_inputs))
def wide_body(other):
    rows = ["    let out: Array<Int> = [];", "    let s: Int = 0;"]
    rows += ["    s = s + " + " + ".join(f"a{(i + k) % wide_inputs}[0]" for i in range(wide_inputs)) + ";"
             for k in range(100)]
    rows += ["    if n > 0 {", f"        let rest: Array<Int> = {other}({wide_actuals}, n - 1);",
             "        s = s + rest[0];", "    }", "    ArrayPush(out, s);", "    return out;"]
    return "\n".join(rows)
wide_path = pressure_dir / "wide-frames.pgy"
wide_path.write_text(
    f"func WideA({wide_formals}) -> Array<Int> {{\n{wide_body('WideB')}\n}}\n"
    f"func WideB({wide_formals}) -> Array<Int> {{\n{wide_body('WideA')}\n}}\n"
    "func Main() -> Void {\n    let v: Array<Int> = [1];\n"
    f"    let r: Array<Int> = WideA({', '.join('v' for _ in range(wide_inputs))}, 2);\n"
    "    Print(ToString(r[0]));\n}\n", encoding="utf-8")
wide = subprocess.run([pgy, "--native-pipeline", "--hir", str(wide_path)],
                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                      env=pressure_env, timeout=30.0, check=False)
(pressure_dir / "wide-frames.err").write_bytes(wide.stderr)
wide_work = re.search(rb"function-param-flow-work work_units=(\d+) work_budget=(\d+)"
                      rb" max_closure_frame_node_visits=(\d+)", wide.stderr)
if (wide.returncode != 0 or wide_work is None or int(wide_work[2]) != 4096 or
        int(wide_work[1]) > int(wide_work[2]) or int(wide_work[3]) <= int(wide_work[2])):
    sys.stderr.buffer.write(wide.stderr)
    raise SystemExit("a small closure with wide frames was charged for its frame node visits")
# After the first pass, a pass evaluates only the entries whose routine read an
# entry that changed after the read. Walk reads its own slot summary before its
# write is seen, so it needs a second pass; its 1400 leaves read nothing that
# changes. Evaluating every entry again would spend past the unchanged cap.
tip_count = 1400
tips = "\n".join(f"func Tip{i}(ref values: Array<Int>) -> Int {{ return values[0]; }}"
                 for i in range(tip_count))
tip_calls = "\n".join(f"    Tip{i}(values);" for i in range(tip_count))
walk_path = pressure_dir / "recursive-fan-out.pgy"
walk_path.write_text(
    "subject Vec2 {\n    let x: Int;\n    let y: Int;\n}\n"
    "func Walk(ref slot: Slot<Vec2>, ref values: Array<Int>, n: Int) -> Void {\n"
    "    if n > 0 { Walk(slot, values, n - 1); }\n" + tip_calls + "\n"
    "    Write(slot, Vec2(1, 2));\n}\n" + tips + "\n"
    "func Main() -> Void {\n    let slot: Slot<Vec2> = Vec2(0, 0);\n"
    "    let values: Array<Int> = [7];\n    Walk(slot, values, 3);\n    Release(slot);\n}\n",
    encoding="utf-8")
walk = subprocess.run([pgy, "--native-pipeline", "--hir", str(walk_path)],
                      stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                      env=pressure_env, timeout=30.0, check=False)
(pressure_dir / "recursive-fan-out.err").write_bytes(walk.stderr)
walk_flow = re.search(rb"function-param-flow entries=(\d+) body_evaluations=(\d+) "
                      rb"cache_hits=(\d+) recursion_hits=(\d+)", walk.stderr)
walk_work = re.search(rb"function-param-flow-work work_units=(\d+) work_budget=(\d+)"
                      rb" max_closure_frame_node_visits=(\d+) skipped_evaluations=(\d+)", walk.stderr)
if (walk.returncode != 0 or walk_flow is None or walk_work is None or int(walk_work[2]) != 4096 or
        int(walk_flow[4]) < 1 or int(walk_work[4]) < tip_count or
        int(walk_flow[2]) >= int(walk_flow[1]) + tip_count):
    sys.stderr.buffer.write(walk.stderr)
    raise SystemExit("a recursive closure evaluated again entries whose reads did not change")
print(f"[function-param-flow-summary] independent/oversized demand controls: PASS; {pressure_dir}")
PY

ESCAPE_FIXTURE="$ROOT_DIR/tests/cases/function_param_flow_summary/escape_negative.pgy"
ESCAPE_ERR="${TMPDIR:-$ROOT_DIR/.tmp}/function_param_flow_escape_negative.err"
mkdir -p "$(dirname "$ESCAPE_ERR")"
if "$PGY_BIN" --native-pipeline --hir "$ESCAPE_FIXTURE" >/dev/null 2>"$ESCAPE_ERR"; then
    echo "escape-negative fixture unexpectedly passed" >&2
    cat "$ESCAPE_ERR" >&2
    exit 1
fi
for expected in \
    "Borrowed ref slot handle (anchored) 'slot' cannot escape through return" \
    "Borrowed ref slot handle (anchored) 'slot' cannot escape through channel send" \
    "Borrowed ref slot handle (anchored) 'slot' cannot escape through helper/function call"; do
    if ! grep -Fq "$expected" "$ESCAPE_ERR"; then
        echo "escape-negative fixture missed diagnostic: $expected" >&2
        cat "$ESCAPE_ERR" >&2
        exit 1
    fi
done

echo "[function-param-flow-summary] recursive fixed point, refusal and demand-budget controls passed"
