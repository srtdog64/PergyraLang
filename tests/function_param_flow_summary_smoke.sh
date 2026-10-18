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
borrow_refusal = ("PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape")
released_refusal = ("PGY_SEM_MOVE_FROM_RELEASED", "semantic:move:from_released")
builtin_refusal = ("PGY_SEM_BUILTIN_ARGS_INVALID", "semantic:builtin:signature_mismatch")
# Exact emitted diagnostics for the reached release/retention cases. In
# particular, a stale read can also trigger a checked builtin-arity cascade;
# it must not be mistaken for a fresh BORROW-only refusal or a MIR artifact.
release_lifetime_refusals = {
    # A call that grows its default-mode aggregate argument in place moves it.
    "default_aggregate_move_reuse_invalid": [(*released_refusal, 21, 28), (*released_refusal, 22, 34)],
    "default_aggregate_move_loop_reuse_invalid": [(*released_refusal, 18, 32)],
    "default_aggregate_move_branch_use_invalid": [(*released_refusal, 20, 41)],
    # Returning a field's storage shares it with the result, which also moves.
    "default_aggregate_move_shared_field_return_invalid": [(*released_refusal, 17, 28)],
    # A field of a struct declared before the loop moves again next iteration.
    "collection_member_move_loop_outer_source_invalid": [(*released_refusal, 28, 41)],
    # An inout fill of a by-value parameter from a second input moves it.
    "inout_owned_param_input_return_moves_invalid": [(*released_refusal, 32, 38)],
    # The 8:6 return escape is the by-value parameter carrying the ref input's
    # stored field; without it only the assignment refusal would remain.
    "owned_param_field_store_ref_return_invalid": [
        (*borrow_refusal, 0, 0), (*borrow_refusal, 8, 6), (*borrow_refusal, 17, 44)],
    "array_local_owned_push_double_drop_invalid": [(*released_refusal, 5, 27)],
    "array_local_owned_push_use_after_drop_invalid": [
        (*released_refusal, 5, 21), (*builtin_refusal, 5, 21)],
    "array_result_owned_double_drop_invalid": [(*released_refusal, 10, 27)],
    "array_result_owned_use_after_drop_invalid": [
        (*released_refusal, 10, 21), (*builtin_refusal, 10, 21)],
    "array_result_owned_defer_drop_then_direct_drop_invalid": [(*borrow_refusal, 10, 27)],
    "array_result_owned_defer_drop_then_own_invalid": [(*borrow_refusal, 14, 17)],
    "array_result_owned_defer_late_array_alias_invalid": [(*borrow_refusal, 12, 16)],
    "array_result_owned_defer_late_scalar_retention_invalid": [(*borrow_refusal, 17, 9)],
    "array_result_owned_defer_named_array_retainer_invalid": [(*borrow_refusal, 19, 9)],
    "array_result_owned_element_array_assign_invalid": [(*borrow_refusal, 10, 28)],
    "array_result_owned_element_defer_after_drop_invalid": [(*borrow_refusal, 10, 27)],
    "array_result_owned_element_defer_lifo_drop_then_read_invalid": [(*borrow_refusal, 10, 35)],
    "array_result_owned_element_defer_mutable_reassign_invalid": [(*borrow_refusal, 12, 27)],
    "array_result_owned_element_device_write_invalid": [(*borrow_refusal, 10, 29)],
    "array_result_owned_element_host_method_retainer_invalid": [
        (*borrow_refusal, 20, 12), (*borrow_refusal, 21, 27)],
    "array_result_owned_element_inner_defer_then_drop_invalid": [(*borrow_refusal, 12, 27)],
    "array_result_owned_element_local_return_invalid": [(*borrow_refusal, 9, 16)],
    "array_result_owned_element_secure_slot_initializer_invalid": [(*borrow_refusal, 9, 42)],
    "array_result_owned_element_secure_slot_write_invalid": [(*borrow_refusal, 10, 23)],
    "array_result_owned_element_slot_initializer_invalid": [(*borrow_refusal, 9, 36)],
    "array_result_owned_element_spawn_invalid": [(*borrow_refusal, 13, 52)],
    "array_result_owned_element_write_view_invalid": [(*borrow_refusal, 11, 25)],
    "array_result_owned_defer_alias_retainer_invalid": [(*borrow_refusal, 17, 5)],
    "array_result_owned_defer_direct_retainer_invalid": [(*borrow_refusal, 16, 5)],
    "array_result_owned_defer_host_own_invalid": [(*borrow_refusal, 19, 28)],
    "array_result_owned_defer_return_invalid": [(*borrow_refusal, 10, 12)],
    "array_result_owned_defer_direct_opaque_invalid": [(*borrow_refusal, 11, 35)],
    "array_result_owned_defer_constructor_return_invalid": [(*borrow_refusal, 11, 35)],
    "array_release_host_own_reuse_invalid": [
        (*released_refusal, 19, 21), (*builtin_refusal, 19, 21)],
    "physical_return_opaque_invalid": [
        (*borrow_refusal, 4, 6), (*borrow_refusal, 5, 14),
        (*borrow_refusal, 10, 20)],
    "selector_method_retaining_receiver_invalid": [(*borrow_refusal, 19, 15)],
    "named_string_identity_loop_reassignment_invalid": [(*borrow_refusal, 17, 27)],
    "named_string_identity_owned_element_retain_invalid": [
        (*borrow_refusal, 17, 20), (*borrow_refusal, 19, 27)],
    "named_string_all_def0_branch_borrow_invalid": [(*borrow_refusal, 24, 22)],
    "named_string_field_make_nodes_drop_invalid": [
        (*borrow_refusal, 31, 34), (*borrow_refusal, 33, 27)],
    "named_string_field_forward_project_drop_invalid": [
        (*borrow_refusal, 34, 5), (*borrow_refusal, 36, 27)],
    "named_string_field_mixed_branch_drop_invalid": [
        (*borrow_refusal, 28, 5), (*borrow_refusal, 30, 27)],
    "named_string_field_alias_mutation_invalid": [(*borrow_refusal, 4, 6)],
    "named_string_same_empty_owned_then_borrowed_direct_invalid": [(*borrow_refusal, 7, 5)],
    "named_string_same_empty_owned_then_borrowed_named_invalid": [(*borrow_refusal, 12, 5)],
    "named_string_same_empty_borrowed_then_owned_direct_invalid": [(*borrow_refusal, 6, 5)],
    "named_string_same_empty_borrowed_then_owned_named_invalid": [(*borrow_refusal, 11, 5)],
    "option_payload_mixed_borrow_invalid": [(*borrow_refusal, 5, 24)],
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
    "readfile_result_borrowed_identity_invalid": [
        (*borrow_refusal, 10, 25), (*borrow_refusal, 11, 27)],
    "readfile_result_defer_released_path_invalid": [(*borrow_refusal, 12, 27)],
    "nonowner_risk_alias_owned_insert_invalid": [
        (*borrow_refusal, 16, 21), (*borrow_refusal, 18, 30)],
    "nonowner_risk_rebind_clone_invalid": [(*borrow_refusal, 16, 26)],
    "nonowner_risk_branch_owned_insert_invalid": [(*borrow_refusal, 12, 9)],
    "nonowner_risk_later_loop_owned_invalid": [(*borrow_refusal, 15, 13)],
    "nonowner_risk_later_actual_handoff_invalid": [(*borrow_refusal, 22, 13)],
    "nonowner_risk_local_carrier_invalid": [(*borrow_refusal, 18, 26)],
    "nonowner_risk_safe_then_owned_invalid": [(*borrow_refusal, 16, 13)],
    "nonowner_risk_owned_then_safe_invalid": [(*borrow_refusal, 15, 13)],
    "nonowner_risk_lambda_capture_invalid": [(*borrow_refusal, 16, 56)],
    # The String-field getter is not receiver-independent. The first error is
    # the summary's unproved backing relation; eight concrete borrow paths
    # must also refuse it. Do not treat this as a physical-storage grant.
    "receiver_result_string_field_escape_invalid": [
        (None, None, 9, 6),
        (*borrow_refusal, 9, 6), (*borrow_refusal, 9, 6),
        (*borrow_refusal, 9, 6),
        (*borrow_refusal, 13, 6), (*borrow_refusal, 13, 6),
        (*borrow_refusal, 13, 6),
        (*borrow_refusal, 14, 30), (*borrow_refusal, 20, 30)],
}
paired = (
    ("unit_enum_qualified_result_valid", True),
    ("option_payload_fresh_literal_none_valid", True),
    ("option_payload_local_alias_valid", True),
    ("option_payload_shared_carrier_valid", True),
    ("option_payload_mixed_borrow_invalid", False),
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
    ("readfile_result_borrowed_identity_invalid", False),
    ("readfile_result_defer_released_path_invalid", False),
    ("nonowner_risk_option_reader_valid", True),
    ("nonowner_risk_shallow_loop_valid", True),
    ("nonowner_risk_alias_owned_insert_invalid", False),
    ("nonowner_risk_rebind_clone_invalid", False),
    ("nonowner_risk_branch_owned_insert_invalid", False),
    ("nonowner_risk_later_loop_owned_invalid", False),
    ("nonowner_risk_later_actual_handoff_invalid", False),
    ("nonowner_risk_local_carrier_invalid", False),
    ("nonowner_risk_safe_then_owned_invalid", False),
    ("nonowner_risk_owned_then_safe_invalid", False),
    ("nonowner_risk_lambda_capture_invalid", False),
    ("secure_implicit_token_safe", True),
    ("secure_implicit_token_keep_invalid", False),
    ("result_builder_birth_valid", True),
    ("retaining_capacity_builder_birth_invalid", False),
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
    ("array_result_owned_retained_element_invalid", False),
    ("array_result_owned_retain_before_deepdrop_invalid", False),
    ("array_result_local_owned_element_external_keep_invalid", False),
    ("array_result_local_owned_element_constructor_invalid", False),
    ("array_result_owned_named_inout_shallow_push_invalid", False),
    ("array_result_owned_named_inout_shallow_set_invalid", False),
    ("array_result_owned_named_inout_index_assign_invalid", False),
    ("array_result_local_owned_element_container_retention_invalid", False),
    ("array_result_local_owned_element_index_assign_invalid", False),
    ("array_result_local_owned_element_field_retention_invalid", False),
    ("array_result_owned_element_inout_escape_invalid", False),
    ("array_result_owned_element_direct_return_invalid", False),
    ("array_result_owned_element_tuple_direct_invalid", False),
    ("array_result_owned_element_tuple_binding_invalid", False),
    ("array_result_owned_element_tuple_return_invalid", False),
    ("array_result_owned_element_option_some_invalid", False),
    ("array_result_owned_element_result_ok_invalid", False),
    ("array_result_owned_element_result_err_invalid", False),
    ("array_result_owned_element_slot_assign_invalid", False),
    ("array_result_owned_element_slot_write_invalid", False),
    ("array_result_owned_element_lambda_capture_invalid", False),
    ("array_result_owned_element_event_retaining_handler_invalid", False),
    # A released Array or its String payload cannot be reused, retained in a
    # deferred body, or stored shallowly before the final deep-drop.
    ("array_local_owned_push_double_drop_invalid", False),
    ("array_local_owned_push_use_after_drop_invalid", False),
    ("array_result_owned_double_drop_invalid", False),
    ("array_result_owned_use_after_drop_invalid", False),
    ("array_result_owned_defer_drop_then_direct_drop_invalid", False),
    ("array_result_owned_defer_drop_then_own_invalid", False),
    ("array_result_owned_defer_late_array_alias_invalid", False),
    ("array_result_owned_defer_late_scalar_retention_invalid", False),
    ("array_result_owned_defer_named_array_retainer_invalid", False),
    ("array_result_owned_element_array_assign_invalid", False),
    ("array_result_owned_element_defer_after_drop_invalid", False),
    ("array_result_owned_element_defer_lifo_drop_then_read_invalid", False),
    ("array_result_owned_element_defer_mutable_reassign_invalid", False),
    ("array_result_owned_element_device_write_invalid", False),
    ("array_result_owned_element_host_method_retainer_invalid", False),
    ("array_result_owned_element_local_return_invalid", False),
    ("array_result_owned_element_secure_slot_initializer_invalid", False),
    ("array_result_owned_element_secure_slot_write_invalid", False),
    ("array_result_owned_element_slot_initializer_invalid", False),
    ("array_result_owned_element_spawn_invalid", False),
    ("array_result_owned_element_write_view_invalid", False),
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
    ("mapnew_ref_string_value_copy_unready", False),
    ("mapnew_ref_box_value_storage_invalid", False),
    ("declared_static_array_valid", True),
    ("declared_array_borrow_invalid", False),
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
    ("result_frame_formal_return_invalid", False),
    ("result_frame_formal_some_invalid", False),
    ("result_frame_formal_nominal_invalid", False),
    ("result_frame_formal_array_push_invalid", False),
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
    ("private_array_push_nonempty_alias_escape_invalid", False),
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
    ("default_string_string_return_invalid", False),
    ("default_string_option_return_invalid", False),
    ("direct_ref_string_loan_valid", True),
    ("callee_alias_return_record_invalid", False),
    ("callee_alias_branch_return_record_invalid", False),
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
    ("return_loan_direct_slot_invalid", False),
    ("return_loan_staged_slot_invalid", False),
    ("return_loan_wrap_slot_invalid", False),
    ("return_loan_inout_invalid", False),
    ("return_loan_mutated_literal_opaque_slot_invalid", False),
    ("return_loan_mutated_literal_opaque_inout_invalid", False),
    ("return_loan_mutated_literal_assignment_invalid", False),
    ("return_loan_mutated_literal_bodyful_inout_invalid", False),
    ("return_loan_using_alias_opaque_inout_invalid", False),
    ("return_loan_mutated_literal_two_aliases_invalid", False),
    ("return_loan_site_two_aliases_invalid", False),
    ("return_loan_two_alias_assignment_invalid", False),
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
    ("named_string_identity_loop_reassignment_invalid", False),
    ("named_string_identity_owned_element_retain_invalid", False),
    ("named_string_all_def0_branch_valid", True),
    ("named_string_all_def0_loop_concat_valid", True),
    ("named_string_all_def0_known_zero_valid", True),
    ("named_string_all_def0_branch_borrow_invalid", False),
    ("named_string_field_empty_arena_valid", True),
    ("named_string_field_make_nodes_drop_invalid", False),
    ("named_string_field_forward_project_drop_invalid", False),
    ("named_string_field_mixed_branch_drop_invalid", False),
    ("named_string_field_alias_mutation_invalid", False),
    ("named_string_field_lineage_two_ref_control_valid", True),
    ("named_string_field_lineage_two_ref_empty_valid", True),
    ("named_string_field_lineage_named_order_valid", True),
    ("named_string_field_lineage_unrelated_match_valid", True),
    ("named_string_field_lineage_unrelated_with_valid", True),
    ("named_string_field_lineage_same_call_retire_grow_invalid", False),
    ("named_string_field_lineage_root_overwrite_invalid", False),
    ("named_string_field_lineage_named_mapping_invalid", False),
    ("named_string_field_lineage_mixed_return_invalid", False),
    ("named_string_field_lineage_recursive_forward_invalid", False),
    ("named_string_same_empty_direct_valid", True),
    ("named_string_same_empty_named_valid", True),
    ("named_string_same_empty_fresh_valid", True),
    ("named_string_same_empty_owned_then_borrowed_direct_invalid", False),
    ("named_string_same_empty_owned_then_borrowed_named_invalid", False),
    ("named_string_same_empty_borrowed_then_owned_direct_invalid", False),
    ("named_string_same_empty_borrowed_then_owned_named_invalid", False),
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
    ("string_element_tostring_opaque_call_invalid", False),
    ("string_param_field_opaque_call_valid", True),
    ("string_own_param_field_opaque_call_invalid", False),
    ("string_inout_helper_repeat_call_valid", True),
    ("string_inout_helper_then_own_release_invalid", False),
    ("string_inout_helper_then_escape_call_invalid", False),
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
    ("default_aggregate_move_shared_field_return_invalid", False),
    ("recursive_ref_read_only_walk_valid", True),
    ("recursive_ref_returned_input_invalid", False),
    ("collection_member_move_loop_local_source_valid", True),
    ("collection_member_move_loop_outer_source_invalid", False),
    ("inout_owned_param_self_flow_valid", True),
    ("inout_owned_param_input_return_moves_invalid", False),
    ("owned_param_field_store_ref_return_invalid", False),
)
for name, valid in paired:
    source = root / "tests/cases/function_param_flow_summary" / (name + ".pgy")
    checked = subprocess.run(
        [pgy, "--native-pipeline", "--mir-json", "--error-format=json", str(source)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        timeout=10.0, check=False)
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
        if name in direct_own_named_sites and (
                checked.stdout or len(errors) != 1 or errors[0].get("stage") != "semantic" or
                (errors[0].get("location", {}).get("line"),
                 errors[0].get("location", {}).get("column")) != direct_own_named_sites[name]):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"direct own actual {name} lost its named-source refusal site")
        retained_sites = {
            # These sites are the outer consumer of Make's retained result,
            # not evidence of a refusal directly on the MapSet statement.
            "mapnew_ref_string_value_copy_unready": [(14, 15), (20, 17)],
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
            # The returned formal is refused in the callee before any unsafe
            # caller artifact is emitted or executed.
            "result_frame_formal_return_invalid": [(7, 6), (13, 27)],
            "result_frame_formal_some_invalid": [(7, 6), (8, 12), (13, 23)],
            "result_frame_formal_nominal_invalid": [(8, 6), (9, 12), (14, 23)],
            "result_frame_formal_array_push_invalid": [(9, 6), (10, 12), (15, 23)],
            "physical_call_alias_nested_retain_invalid": [(16, 52), (22, 28)],
            "physical_call_alias_scalar_opaque_invalid": [(23, 28)],
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
            "array_result_owned_retained_element_invalid": (7, 14),
            "array_result_local_owned_element_external_keep_invalid": (7, 14),
            "array_result_local_owned_element_constructor_invalid": (7, 31),
            "array_result_local_owned_element_container_retention_invalid": (6, 27),
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
            "array_release_owned_strings_move_invalid": {(13, 5), (14, 17)},
            "array_release_inout_foreign_owned_strings_invalid": {(14, 22), (15, 26)},
            "array_release_prefix_foreign_inout_invalid": {(19, 5), (20, 9)},
            "array_release_factory_retaining_reader_invalid": {(25, 24), (27, 27)},
            "array_release_loop_second_iteration_escape_invalid": {(20, 30)},
            "array_release_inout_retaining_reader_invalid": {(12, 26), (21, 9), (22, 27)},
            "array_result_owned_retain_before_deepdrop_invalid": {(18, 24), (19, 27)},
            # The in-place helper call takes no element ownership; the deep
            # drop of the array it left with a borrowed String is the refusal.
            "array_result_owned_named_inout_shallow_push_invalid": {(17, 27)},
            "array_result_owned_named_inout_shallow_set_invalid": {(17, 27)},
            "array_result_owned_named_inout_index_assign_invalid": {(17, 27)},
            "array_field_detach_duplicate_alias_own_invalid": {(13, 20), (13, 35)},
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
            "array_result_nonown_ref_nested_projection_invalid": [(3, 5), (1, 6), (14, 36)],
            "array_result_nonown_ref_mapget_descriptor_invalid": [(1, 6), (1, 6), (1, 6)],
            "array_result_local_owned_element_index_assign_invalid": [(6, 27)],
            "array_result_local_owned_element_field_retention_invalid": [(8, 21)],
            "array_result_owned_element_inout_escape_invalid": [(10, 18), (8, 6), (17, 21)],
            "array_result_owned_element_direct_return_invalid": [(10, 16), (8, 6), (16, 40)],
            "array_result_owned_element_tuple_direct_invalid": [(9, 33)],
            "array_result_owned_element_tuple_binding_invalid": [(9, 21)],
            "array_result_owned_element_tuple_return_invalid": [(8, 17), (7, 6), (13, 33)],
            "array_result_owned_element_option_some_invalid": [(9, 44)],
            "array_result_owned_element_result_ok_invalid": [(9, 50)],
            "array_result_owned_element_result_err_invalid": [(9, 48)],
            "array_result_owned_element_slot_assign_invalid": [(10, 18)],
            "array_result_owned_element_slot_write_invalid": [(10, 23)],
            "array_result_owned_element_lambda_capture_invalid": [(10, 53)],
            "array_result_owned_element_event_retaining_handler_invalid": [(18, 14)],
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
    ("stdlib_tostring_string_forward_invalid", (10, 27)),
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
    "pool_builder_birth_invalid",
    "opaque_builder_birth_invalid",
    "rebound_pool_builder_birth_invalid",
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
