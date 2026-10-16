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
paired = (
    ("unit_enum_qualified_result_valid", True),
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
        if checked.returncode != 1 or '"pgy.mir.v1"' in output or not errors or any(
                row.get("code") != "PGY_SEM_BORROW_ESCAPE" or row.get("cause_ir") != "semantic:borrow_escape"
                for row in errors):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"physical-return negative {name} lost its exact borrow refusal")
        retained_sites = {
            # These sites are the outer consumer of Make's retained result,
            # not evidence of a refusal directly on the MapSet statement.
            "mapnew_ref_string_value_copy_unready": (14, 15),
            "mapnew_ref_box_value_storage_invalid": (18, 15),
            "direct_ref_string_local_retain_invalid": (8, 20),
            "direct_ref_string_slot_invalid": (7, 30),
            "direct_ref_string_channel_invalid": (9, 25),
            "direct_ref_string_retaining_concat_invalid": (4, 16),
            "direct_ref_string_opaque_literal_invalid": (3, 5),
            "direct_ref_string_opaque_concat_invalid": (3, 14),
            "direct_ref_string_alias_literal_invalid": (4, 5),
            "direct_ref_string_alias_concat_invalid": (4, 14),
            "direct_ref_string_alias_named_invalid": (5, 14),
            "recursive_record_wrapper_control_invalid": (11, 6),
            "nested_call_retaining_input_invalid": (8, 24),
            # The returned formal is refused in the callee before any unsafe
            # caller artifact is emitted or executed.
            "result_frame_formal_return_invalid": (7, 6),
            "result_frame_formal_some_invalid": (7, 6),
            "result_frame_formal_nominal_invalid": (8, 6),
            "result_frame_formal_array_push_invalid": (9, 6),
        }
        if name in retained_sites and not any(
                (row.get("location", {}).get("line"), row.get("location", {}).get("column"))
                == retained_sites[name] for row in errors):
            sys.stderr.buffer.write(checked.stderr)
            raise SystemExit(f"ref-retaining negative {name} lost its owned refusal site")
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
    ("backward_loop_output_literal_valid", "literal output\n"),
    ("nested_call_independent_result_valid", "independent result\ntrue\n"),
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
        observed = subprocess.run([str(executable)], stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, timeout=10.0, check=False)
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
