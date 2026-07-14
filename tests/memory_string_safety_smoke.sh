#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[memory-string-safety] $*" >&2
    exit 1
}

require_literal() {
    local rel="$1"
    local term="$2"

    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing memory/string safety term: $term"
}

reject_literal() {
    local rel="$1"
    local term="$2"

    ! grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel still contains forbidden memory/string safety term: $term"
}

unsafe_calls="$(
    grep -RInE '\b(sprintf|vsprintf|strcpy|strncpy|strcat|strncat|gets)[[:space:]]*\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
unsafe_calls="$(
    printf '%s\n' "$unsafe_calls" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$unsafe_calls" ]; then
    printf '%s\n' "$unsafe_calls" >&2
    fail "production code must use project-owned bounded formatting/copy/append helpers, not unsafe C string APIs"
fi

strtok_calls="$(
    grep -RInE '\bstrtok[[:space:]]*\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
strtok_calls="$(
    printf '%s\n' "$strtok_calls" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$strtok_calls" ]; then
    printf '%s\n' "$strtok_calls" >&2
    fail "production code must not use process-global strtok tokenizer state"
fi

truncated_stack_copies="$(
    grep -RInE 'memcpy\([^;]*stack_buf,[[:space:]]*\(size_t\)len[[:space:]]*\+[[:space:]]*1\)' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
truncated_stack_copies="$(
    printf '%s\n' "$truncated_stack_copies" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$truncated_stack_copies" ]; then
    printf '%s\n' "$truncated_stack_copies" >&2
    fail "snprintf stack buffers must not be copied using the required length after truncation"
fi

unsafe_offset_accumulators="$(
    grep -RInE '\+=[[:space:]]*(\(size_t\))?snprintf\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
unsafe_offset_accumulators="$(
    printf '%s\n' "$unsafe_offset_accumulators" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$unsafe_offset_accumulators" ]; then
    printf '%s\n' "$unsafe_offset_accumulators" >&2
    fail "incremental string assembly must use pergyra_str_append/pergyra_str_appendf, not raw snprintf offset accumulation"
fi

grep -Fq "Memory/string safety audit remains open" "$ROOT_DIR/TODO.md" \
    || fail "TODO must keep the memory/string safety audit bucket visible"
grep -Fq "Buffer Overflow" "$ROOT_DIR/src/test_security_buffer_overflow.c" \
    || fail "historical buffer-overflow regression coverage is missing"
grep -Fq "snprintf" "$ROOT_DIR/src/test_security_comprehensive.c" \
    || fail "bounded formatting regression coverage is missing"

require_literal "src/runtime/pgy_runtime_intent_trace_inline.h" \
    "old_len > SIZE_MAX - 1 || add_len > SIZE_MAX - old_len - 1"
require_literal "src/runtime/pgy_runtime_lib_set_intent_trace_exports.c" \
    "old_len > SIZE_MAX - 1 || add_len > SIZE_MAX - old_len - 1"
require_literal "src/codegen/llvm_backend_type_render.c" \
    "arg_len > ((size_t)-1) - result_len - 4"
require_literal "src/codegen/llvm_backend_type_render.c" \
    "cur_len > ((size_t)-1) - 2"
require_literal "src/codegen/llvm_stmt_type_render.c" \
    "arg_len > ((size_t)-1) - cur_len - 4"
require_literal "src/codegen/llvm_stmt_type_render.c" \
    "cur_len > ((size_t)-1) - 2"
require_literal "src/codegen/llvm_domain_projection_value_helpers.c" \
    "nested_len > ((size_t)-1) - field_len - 2"
require_literal "src/codegen/llvm_domain_projection_value_helpers.c" \
    "llvm_domain_projection_join_path"
require_literal "src/codegen/llvm_domain_projection_value_helpers.c" \
    "written < 0 || (size_t)written >= path_len"
require_literal "src/codegen/llvm_expr_projection_path_helpers.c" \
    "nested_len > ((size_t)-1) - field_len - 2"
require_literal "src/codegen/llvm_expr_projection_path_helpers.c" \
    "llvm_expr_projection_join_path"
require_literal "src/codegen/llvm_expr_projection_path_helpers.c" \
    "written < 0 || (size_t)written >= path_len"
require_literal "src/codegen/llvm_member_call_support.c" \
    "method_len > ((size_t)-1) - class_len - 2"
require_literal "src/codegen/llvm_expr_scalar_core.c" \
    "type_len > ((size_t)-1) - prefix_len - suffix_len - 2"
require_literal "src/semantic/type_checker_class_decl.c" \
    "method_len > ((size_t)-1) - name_len - 2"
require_literal "src/semantic/type_system_slot.c" \
    "inner_len > ((size_t)-1) - prefix_len - 2"
require_literal "src/compiler/path_utils.c" \
    "ext_len > ((size_t)-1) - base_len - 1"
require_literal "src/codegen/transpiler_mir_resource_op_core.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/codegen_channel_runtime_abi.c" \
    "pgy_channel_runtime_name"
require_literal "src/codegen/codegen_channel_runtime_abi.c" \
    "pgy_lane_channel_runtime_name"
require_literal "src/codegen/codegen_channel_runtime_abi.c" \
    "return written >= 0 && (size_t)written < out_size"
reject_literal "src/codegen/llvm_expr_channel.c" \
    "llvm_channel_format_runtime_name"
reject_literal "src/codegen/llvm_expr_task_channel_calls.c" \
    "llvm_task_channel_format_runtime_name"
reject_literal "src/codegen/llvm_expr_task_channel_calls.c" \
    "llvm_task_channel_format_op_runtime_name"
require_literal "src/codegen/llvm_stmt_parallel_names.c" \
    "llvm_select_channel_runtime_name"
require_literal "src/codegen/llvm_stmt_parallel_names.c" \
    "pgy_lane_channel_runtime_name(out, out_size, op, inner)"
require_literal "src/runtime/slot_manager_security_stats.c" \
    "slot_security_localtime"
require_literal "src/runtime/slot_manager_security_stats.c" \
    "localtime_s(out, &now)"
require_literal "src/runtime/slot_manager_security_stats.c" \
    "localtime_r(&now, out)"
require_literal "src/codegen/llvm_expr_slot_device_calls.c" \
    "mir_abi_resource_runtime_row_by_kind"
require_literal "src/compiler/mir_abi_resource_runtime.c" \
    'ABI_RESOURCE_OP("DeviceSlot<Int>", "SubmitRead"'
require_literal "src/codegen/llvm_expr_array_calls.c" \
    "llvm_array_format_runtime_name"
require_literal "src/codegen/llvm_expr_array_calls.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_expr_call_methods_domain_slice.c" \
    "mir_abi_resource_runtime_fn_by_kind"
require_literal "src/codegen/llvm_expr_call_methods_domain_slice.c" \
    "MIR_RESOURCE_ABI_SECURE_SLOT"
require_literal "src/codegen/transpiler_mir_pin_emit.c" \
    "transpiler_mir_slot_address_local"
require_literal "src/codegen/transpiler_mir_pin_emit.c" \
    "mir_abi_resource_runtime_row_by_kind"
require_literal "src/codegen/transpiler_mir_pin_emit.c" \
    "\"PinRead\""
require_literal "src/codegen/transpiler_mir_pin_emit.c" \
    "\"Unpin\""
require_literal "src/codegen/transpiler_mir_pin_emit.c" \
    "return written >= 0 && (size_t)written < buf_size"
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "llvm_mir_pin_local_name"
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "llvm_mir_pin_token_name"
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "mir_abi_resource_runtime_row_by_kind"
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "\"PinReadInit\""
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "\"PinWriteInit\""
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "\"Unpin\""
require_literal "src/codegen/llvm_mir_pin_region.c" \
    "written >= 0 && (size_t)written < buf_size"
require_literal "src/codegen/transpiler_mir_resource_op_core.c" \
    "transpiler_mir_resource_format_addr"
require_literal "src/codegen/transpiler_mir_resource_op_core.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_method_helpers.c" \
    "llvm_domain_provenance_field_name"
require_literal "src/codegen/llvm_domain_method_helpers.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "llvm_projection_sync_field_name"
require_literal "src/codegen/llvm_domain_projection_sync_body_helpers.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_struct_fields.c" \
    "llvm_domain_struct_projection_field_name"
require_literal "src/codegen/llvm_domain_struct_fields.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_world_frontier.c" \
    "llvm_world_frontier_field_name"
require_literal "src/codegen/llvm_domain_world_frontier.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_world_sync.c" \
    "llvm_world_sync_field_name"
require_literal "src/codegen/llvm_domain_world_sync.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_zone_frontier_state.c" \
    "llvm_zone_frontier_field_name"
require_literal "src/codegen/llvm_domain_zone_frontier_state.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_struct_register_fields.c" \
    "llvm_domain_struct_register_field_name"
require_literal "src/codegen/llvm_domain_struct_register_fields.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_zone_sync.c" \
    "llvm_zone_sync_field_name"
require_literal "src/codegen/llvm_domain_zone_sync.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_zone_sync_relations.c" \
    "llvm_zone_relation_sync_field_name"
require_literal "src/codegen/llvm_domain_zone_sync_relations.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "llvm_zone_bind_projection_field_name"
require_literal "src/codegen/llvm_domain_zone_bind_lowering.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_world_sync_directives.c" \
    "llvm_world_sync_directive_field_name"
require_literal "src/codegen/llvm_domain_world_sync_directives.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_forward.c" \
    "llvm_domain_forward_suffix_name"
require_literal "src/codegen/llvm_domain_forward.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_role_method_symbol_name"
require_literal "src/codegen/llvm_domain_role_lookup.c" \
    "llvm_role_vtable_global_name"
require_literal "src/codegen/llvm_domain_role_lookup.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_domain_event.c" \
    "llvm_domain_event_helper_name"
require_literal "src/codegen/llvm_domain_event.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_expr_event_calls.c" \
    "llvm_event_call_helper_name"
require_literal "src/codegen/llvm_expr_event_calls.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_expr_rc_calls.c" \
    "llvm_rc_runtime_name"
require_literal "src/codegen/llvm_expr_rc_calls.c" \
    "written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_expr_call_projection_sync.c" \
    "llvm_projection_sync_call_field_name"
require_literal "src/codegen/llvm_expr_call_projection_sync.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "llvm_world_effect_sync_field_name"
require_literal "src/codegen/llvm_expr_call_methods_world_effect_sync.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_expr_domain_query_calls.c" \
    "llvm_domain_query_field_name"
require_literal "src/codegen/llvm_expr_domain_query_calls.c" \
    "written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_intent_effect.c" \
    "llvm_intent_effect_field_name"
require_literal "src/codegen/llvm_intent_effect.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_intent_zone.c" \
    "llvm_intent_zone_sync_name"
require_literal "src/codegen/llvm_intent_zone.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_intent_emit_support.c" \
    "llvm_intent_action_function_name"
require_literal "src/codegen/llvm_intent_emit_support.c" \
    "written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_runtime_channels.c" \
    "pgy_channel_runtime_name"
require_literal "src/codegen/llvm_runtime_channels.c" \
    "pgy_lane_channel_runtime_name"
reject_literal "src/codegen/llvm_runtime_channels.c" \
    "llvm_runtime_channel_name"
reject_literal "src/codegen/llvm_runtime_channels.c" \
    "llvm_runtime_lane_channel_name"
require_literal "src/codegen/llvm_runtime_secure_slot_decl.c" \
    "mir_abi_resource_runtime_row_by_type_name"
require_literal "src/codegen/llvm_runtime_secure_slot_decl.c" \
    "\"PinReadInit\""
require_literal "src/codegen/llvm_runtime.c" \
    "mir_abi_resource_runtime_row_by_type_name"
require_literal "src/codegen/llvm_runtime.c" \
    "llvm_runtime_export_name"
require_literal "src/codegen/llvm_runtime.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/llvm_type.c" \
    "pgy_result_join_names"
require_literal "src/codegen/llvm_type.c" \
    "return written >= 0 && (size_t)written < out_n"
require_literal "src/codegen/transpiler_type_mapping.c" \
    "transpiler_type_name_join"
require_literal "src/codegen/transpiler_type_mapping.c" \
    "return written >= 0 && (size_t)written < out_size"
require_literal "src/codegen/transpiler_decl_host_lookup.c" \
    "transpiler_cache_nominal_host_decl"
require_literal "src/codegen/transpiler_decl_host_lookup.c" \
    "len >= dst_size"
require_literal "src/codegen/transpiler_mir_local_type_lookup.c" \
    "transpiler_mir_arena_render_type_name"
require_literal "src/codegen/transpiler_mir_local_type_lookup.c" \
    "pgy_arena_fmt(&ctx->arena, \"%s<%s>\", prefix, inner)"
require_literal "src/codegen/transpiler_expr_type_infer.c" \
    "transpiler_infer_arena_format_type_name"
require_literal "src/codegen/transpiler_expr_type_infer.c" \
    "pgy_arena_fmt(&ctx->arena, \"%s<%s>\", prefix, inner)"
require_literal "src/codegen/transpiler_mir_destructure_emit.c" \
    "transpiler_mir_destructure_format_type"
require_literal "src/codegen/transpiler_mir_destructure_emit.c" \
    "transpiler_mir_destructure_ssa_local"
require_literal "src/codegen/transpiler_domain_role_ability_names.c" \
    "transpiler_role_ability_copy_name"
require_literal "src/codegen/transpiler_domain_role_ability_names.c" \
    "transpiler_role_ability_host_method_name"
require_literal "src/codegen/transpiler_domain_role_ability_names.c" \
    "transpiler_role_ability_vtable_typedef_name"
require_literal "src/codegen/transpiler_domain_role_ability_names.c" \
    "transpiler_role_operator_alias_name"
require_literal "src/codegen/transpiler_domain_role_ability_names.c" \
    "transpiler_role_ability_surface_desc"
require_literal "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_domain_nominal_surface_desc"
require_literal "src/codegen/transpiler_domain_nominal_emit.c" \
    "transpiler_domain_nominal_surface_desc_too_long"
require_literal "src/codegen/transpiler_relation_effect_emit.c" \
    "transpiler_relation_effect_surface_desc"
require_literal "src/codegen/transpiler_relation_effect_emit.c" \
    "transpiler_relation_effect_surface_desc_too_long"
require_literal "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_class_surface_desc"
require_literal "src/codegen/transpiler_class_decl_emit.c" \
    "transpiler_class_method_emit_name"
require_literal "src/codegen/transpiler_expr_stdlib_collection_support.c" \
    "transpiler_collection_copy_type_name"
require_literal "src/codegen/transpiler_expr_stdlib_collection_support.c" \
    "len >= out_size"
require_literal "src/codegen/transpiler_func_flow_policy.c" \
    "transpiler_func_copy_current_return_type"
require_literal "src/codegen/transpiler_func_flow_policy.c" \
    "transpiler_func_parameter_surface_desc"
require_literal "src/codegen/transpiler_mir_emit_state.c" \
    "transpiler_mir_emit_copy_return_type"
require_literal "src/codegen/transpiler_mir_emit_state.c" \
    "transpiler_mir_emit_return_type_too_long"
require_literal "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_zone_surface_desc"
require_literal "src/codegen/transpiler_zone_struct_emit.c" \
    "transpiler_zone_surface_desc_too_long"
require_literal "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "transpiler_intent_binding_surface_desc"
require_literal "src/codegen/transpiler_intent_zone_binding_emit.c" \
    "transpiler_intent_binding_surface_desc_too_long"
require_literal "src/codegen/transpiler_expr_stdlib_builtin.c" \
    "transpiler_stdlib_copy_type_name"
require_literal "src/codegen/transpiler_expr_stdlib_builtin.c" \
    "len >= out_size"
require_literal "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_enum_method_emit_name"
require_literal "src/codegen/transpiler_enum_decl_emit.c" \
    "transpiler_enum_method_surface_desc"
require_literal "src/codegen/transpiler_block_emit.c" \
    "transpiler_block_pin_address_too_long"
require_literal "src/codegen/transpiler_block_emit.c" \
    "mir_abi_resource_runtime_row_by_kind"
require_literal "src/codegen/transpiler_block_emit.c" \
    "\"UnpinCleanup\""
require_literal "src/codegen/transpiler_block_emit.c" \
    "\"Release\""
require_literal "src/codegen/transpiler_block_emit.c" \
    "C secure slot auto-release requires paired token binding"
require_literal "src/codegen/transpiler_block_emit.c" \
    "written >= 0 && (size_t)written < buf_size"
require_literal "src/codegen/transpiler_decl_lookup.c" \
    "transpiler_decl_lookup_cache_store"
require_literal "src/codegen/transpiler_decl_lookup.c" \
    "len >= sizeof(ctx->last_decl_lookup_name)"
require_literal "src/codegen/transpiler_intent_prologue_emit.c" \
    "transpiler_intent_prologue_surface_desc"
require_literal "src/codegen/transpiler_intent_prologue_emit.c" \
    "transpiler_intent_prologue_surface_desc_too_long"
require_literal "src/codegen/transpiler_let_slot_emit.c" \
    "transpiler_let_slot_constructed_type_name"
require_literal "src/codegen/transpiler_let_slot_emit.c" \
    "transpiler_let_slot_constructed_type_too_long"
require_literal "src/codegen/transpiler_event_builtin_emit.c" \
    "written < 0 || written != needed"
require_literal "src/codegen/transpiler_async_parallel_emit.c" \
    "transpiler_capture_surface_desc"
require_literal "src/codegen/transpiler_async_parallel_emit.c" \
    "transpiler_capture_surface_desc_too_long"
require_literal "src/codegen/transpiler_expr_call_member_emit.c" \
    "pergyra_str_copy(stable_type_name"
require_literal "src/codegen/transpiler_expr_call_member_emit.c" \
    "nominal receiver type name is too long"
require_literal "src/codegen/transpiler_generic_class_naming.c" \
    "transpiler_generic_class_method_name"
require_literal "src/codegen/transpiler_generic_class_naming.c" \
    "transpiler_generic_class_copy_name"
require_literal "src/codegen/transpiler_generic_class_naming.c" \
    "transpiler_generic_class_surface_desc"
require_literal "src/codegen/transpiler_generic_class_naming.c" \
    "transpiler_generic_class_format_too_long"
require_literal "src/codegen/transpiler_generic_class_naming.c" \
    "transpiler_generic_class_specialization_name"
require_literal "src/codegen/transpiler_generic_class_naming.c" \
    "append_mangled_type_name"
require_literal "src/codegen/transpiler_specialization_registry.c" \
    "transpiler_specialization_copy_spec_name"
require_literal "src/codegen/transpiler_specialization_registry.c" \
    "transpiler_specialization_append_spec_text"
require_literal "src/codegen/transpiler_specialization_registry.c" \
    "transpiler_specialization_spec_name_too_long"
require_literal "src/codegen/transpiler_generic_specialization_emit.c" \
    "transpiler_generic_specialization_copy_name"
require_literal "src/codegen/transpiler_generic_specialization_emit.c" \
    "transpiler_generic_specialization_name_too_long"
if grep -Fq "pergyra_str_copy(entry->specialized_name" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c"; then
    fail "generic function specialization names must reject overlong names instead of truncating"
fi
require_literal "src/codegen/transpiler_control_flow_emit.c" \
    "transpiler_loop_label_name"
require_literal "src/codegen/transpiler_mir_reason.h" \
    "transpiler_mir_reasonf"
require_literal "src/codegen/llvm_intent.c" \
    "llvm_intent_reason_name"
require_literal "src/codegen/llvm_intent_flow.c" \
    "llvm_intent_flow_reason_name"
require_literal "src/codegen/llvm_api.c" \
    "llvm_result_error_fmt_with_hints"
require_literal "src/codegen/llvm_api.c" \
    "llvm_result_error_fmt(\"LLVM verify failed: %s\""
require_literal "src/semantic/type_checker_ownership_diag.c" \
    "semantic_format_secure_token_name"
require_literal "src/semantic/type_checker_ownership_diag.c" \
    "truncating that token name would break the slot/token capability invariant"
require_literal "src/codegen/llvm_stmt_type_infer_helpers.c" \
    "llvm_stmt_format_host_method_name"
require_literal "src/codegen/llvm_register.c" \
    "llvm_register_join_name"
require_literal "src/codegen/llvm_register.c" \
    "llvm_register_payload_field_name"
require_literal "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_zone_action_field_name"
require_literal "src/codegen/llvm_stmt_zone_action.c" \
    "llvm_zone_action_sync_name"
require_literal "src/codegen/llvm_stmt_let_names.c" \
    "llvm_let_with_token_name"
require_literal "src/codegen/llvm_stmt_let_resources.c" \
    "mir_abi_resource_runtime_fn_by_kind"
require_literal "src/codegen/llvm_stmt_let_collections.c" \
    "llvm_stmt_collection_runtime_name"
require_literal "src/codegen/llvm_stmt_block.c" \
    "mir_abi_resource_runtime_fn_by_kind"
require_literal "src/codegen/llvm_stmt.c" \
    "llvm_stmt_format_bind_name"
require_literal "src/codegen/llvm_stmt_let_helpers.c" \
    "written < 0 || (size_t)written >= sizeof(buf)"
require_literal "src/codegen/llvm_stmt_let_helpers.c" \
    "pgy_arena_strdup(&ctx->scratch, actual_type)"
require_literal "src/codegen/llvm_stmt_with.c" \
    "llvm_with_token_name"
require_literal "src/codegen/llvm_stmt_with.c" \
    "mir_abi_resource_runtime_fn_by_kind"
require_literal "src/codegen/llvm_expr_emit_support.c" \
    "llvm_expr_runtime_name"
require_literal "src/codegen/llvm_expr_emit_support.c" \
    "llvm_expr_lambda_name"
require_literal "src/codegen/llvm_expr_spawn_names.c" \
    "llvm_spawn_copy_name"
require_literal "src/codegen/llvm_expr_spawn_names.c" \
    "llvm_spawn_format_name"
require_literal "src/codegen/llvm_expr_spawn_names.c" \
    "llvm_spawn_wrapper_name"
require_literal "src/compiler/air_names.c" \
    "air_vformat_owned"
require_literal "src/compiler/air_names.c" \
    "written < 0 || written != needed"
require_literal "src/compiler/air_verify.c" \
    "air_append_driftf"
require_literal "src/compiler/air_verify.c" \
    "air_format_authority_names_owned"
require_literal "src/compiler/air_verify.c" \
    "air_format_boundary_provenance_owned"
require_literal "src/compiler/air_validate.c" \
    "air_vformat_owned"
require_literal "src/compiler/mir_type_helpers.c" \
    "mir_type_append_owned"
require_literal "src/compiler/mir_type_helpers.c" \
    "written < 0 || written != length"
require_literal "src/compiler/mir_base_helpers.c" \
    "written < 0 || written != length"
require_literal "src/compiler/mir_lifecycle.c" \
    "mir_strdup_fmt(\"_pgy_mir_bb_%s_%zu\""
require_literal "src/compiler/mir_lifecycle.c" \
    "block source-location allocation failed"
require_literal "src/compiler/mir_decl_header_validate.c" \
    "written < 0 || written != length"
require_literal "src/compiler/mir_fact_validate.c" \
    "written < 0 || written != length"
require_literal "src/compiler/mir_intent_fact.c" \
    "written < 0 || written != length"
require_literal "src/compiler/mir_validation.c" \
    "written < 0 || written != length"
require_literal "src/compiler/mir_ssa_use_edges.c" \
    "mir_parse_versioned_name_owned"
require_literal "src/compiler/mir_ssa_use_edges.c" \
    "pergyra_strndup(versioned, len)"
require_literal "src/common/string_compat.h" \
    "written < 0 || written != needed"
require_literal "src/common/string_compat.h" \
    "length > SIZE_MAX - 1"
require_literal "src/common/string_compat.h" \
    "src == NULL && length > 0"
require_literal "src/common/string_compat.h" \
    "if (src == NULL)"

if grep -Fq "snprintf(" "$ROOT_DIR/src/compiler/air_verify.c"; then
    fail "AIR verifier drift diagnostics must use owned dynamic formatting, not fixed stack snprintf"
fi
if grep -Fq "vsnprintf(" "$ROOT_DIR/src/compiler/air_validate.c"; then
    fail "AIR invariant diagnostics must use owned dynamic formatting, not fixed stack vsnprintf"
fi

echo "[memory-string-safety] unsafe production string APIs are gated"
