#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"

fail() {
    echo "[abi-ownership-shape] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing ownership ABI file: $rel"
}

require_term() {
    local rel="$1"
    local term="$2"
    local path="$ROOT_DIR/$rel"
    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        pgy_beta_checklist_contains "$term" ||
            fail "$rel shards missing term: $term"
        return 0
    fi
    grep -Fq "$term" "$path" || fail "$rel missing term: $term"
}

reject_term() {
    local rel="$1"
    local term="$2"
    local path="$ROOT_DIR/$rel"
    ! grep -Fq "$term" "$path" || fail "$rel still contains forbidden term: $term"
}

for rel in \
    "src/runtime/pgy_abi_spec.h" \
    "src/runtime/slot_manager.h" \
    "src/runtime/slot_manager_pin.c" \
    "src/runtime/pgy_runtime_plain_slot_inline.h" \
    "src/runtime/pgy_runtime_slot_macros.h" \
    "src/runtime/pgy_runtime_result_option_inline.h" \
    "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" \
    "src/runtime/pgy_runtime_lib_slot_array_io_string_exports.h" \
    "src/runtime/pgy_runtime_lib_secure_slot_exports.h" \
    "src/runtime/pgy_runtime_allocator_inline.h" \
    "src/runtime/pgy_runtime_lib_allocator_exports.h" \
    "src/test_abi_spec.c" \
    "src/test_memory_layout.c" \
    "src/test_security.c" \
    "src/codegen/transpiler_mir_pin_emit.h" \
    "src/codegen/transpiler_mir_resource_hook_emit.c" \
    "src/codegen/transpiler_block_emit.h" \
    "src/compiler/mir_types.h" \
    "src/compiler/mir_lower_population.c" \
    "src/compiler/mir_resource_runtime_population.c" \
    "src/compiler/mir_resource_runtime_population.h" \
    "src/codegen/llvm_runtime.c" \
    "src/codegen/llvm_runtime_row.c" \
    "src/compiler/mir_abi_resource_runtime_constructed.c" \
    "src/codegen/llvm_expr_allocator_calls.c" \
    "src/codegen/llvm_internal_api.h" \
    "src/codegen/llvm_mir_block_emit.c" \
    "src/codegen/llvm_mir_resource_view.c" \
    "src/codegen/llvm_runtime_secure_slot_decl.c" \
    "src/codegen/llvm_mir_block_emit.h" \
    "src/compiler/mir_cfg_contract_pin.h" \
    "src/compiler/mir_cfg_contract_validate.h" \
    "src/compiler/mir_cfg_contract_validate_cleanup.h" \
    "src/compiler/mir_cfg_contract_validate_cleanup.c" \
    "tests/cfg_body_dataflow_smoke.sh" \
    "tests/compare_backends.sh" \
    "docs/74_slot_pinning_caching.md" \
    "docs/100_beta_readiness_checklist.md" \
    "docs/107_beta_stable_subset.md" \
    "docs/125_source_of_truth_spine.md" \
    "docs/118_slot_model_rigor_audit.md" \
    "docs/136_abi_niche_and_explicit_layout.md" \
    "docs/145_bit_layout_boundary_matrix.md" \
    "docs/semantics/13_slot_abi_single_owner.md"; do
    require_file "$rel"
done

require_term "docs/semantics/proofs/SoTAuthority.v" "SFAbiRuntimeCallRows"
require_term "docs/semantics/proofs/SoTAuthority.v" "SFAbiRuntimeCallRows => SOMirAbi"
require_term "docs/semantics/sot_owner_spine_registry.md" "abi.runtime_call_rows | abi | RuntimeCallAbiId | SFAbiRuntimeCallRows | SOMirAbi"
require_term "docs/semantics/sot_owner_spine_registry.md" "resource_runtime_abi_fact_owner.pgy | MirResourceRuntimeRowFactReady | abi.runtime_call_rows | local_view"
require_term "docs/192_protocol_abi_api_registry.md" "registry:abi.runtime_call_rows"
reject_term "docs/192_protocol_abi_api_registry.md" "UNREGISTERED:runtime-call-abi-row-authority"
require_term "src/self_hosted/compiler/compatibility_evolution_owner.pgy" "func CompilerRuntimeCallAbiCompatibilityPolicy"
require_term "src/self_hosted/compiler/compatibility_evolution_owner.pgy" "func CompilerRuntimeCallAbiCompatibilityReady"
require_term "src/self_hosted/compiler/compatibility_evolution_owner.pgy" "same_major_reject_unknown_fields_fail_closed"
require_term "src/self_hosted/compiler/compatibility_evolution_manifest.pgy" "runtime_call_abi_policy="

require_term "src/runtime/pgy_abi_spec.h" "typedef struct { uint64_t id; bool can_write; bool can_read; } pgy_abi_token_int;"
require_term "src/runtime/pgy_abi_spec.h" "Debug/release mode is a build policy, not an ABI type-name dimension."
reject_term "src/runtime/pgy_abi_spec.h" "pgy_abi_token_int_rel"
reject_term "src/runtime/pgy_abi_spec.h" "pgy_abi_token_int_dbg"
require_term "src/runtime/pgy_abi_spec.h" "typedef struct { int32_t  value; bool occupied; } pgy_abi_slot_int;"
reject_term "src/runtime/pgy_abi_spec.h" "pgy_abi_slot_int_rel"
reject_term "src/runtime/pgy_abi_spec.h" "pgy_abi_slot_int_dbg"
reject_term "src/runtime/pgy_abi_spec.h" "PGY_RAW_SLOTS"
require_term "src/runtime/pgy_abi_spec.h" "typedef struct { int32_t value; bool occupied; uint64_t token; } pgy_abi_secure_slot_int;"
reject_term "src/runtime/pgy_abi_spec.h" "pgy_abi_secure_slot_int_rel"
reject_term "src/runtime/pgy_abi_spec.h" "pgy_abi_secure_slot_int_dbg"
require_term "src/runtime/pgy_runtime_panic_checked_inline.h" "PGY_WITH_SLOT_CHECKS remains defined"
reject_term "src/runtime/pgy_runtime_panic_checked_inline.h" "PGY_RAW_SLOTS"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "Canonical Slot<T> runtime shape"
reject_term "src/runtime/pgy_runtime_plain_slot_inline.h" "PGY_SLOT_DEFINE_CHECKED"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "Raw/value-only storage must use a"
reject_term "src/runtime/pgy_runtime_plain_slot_inline.h" "PGY_SLOT_DEFINE_RAW"
reject_term "src/runtime/pgy_runtime_plain_slot_inline.h" "#if PGY_WITH_SLOT_CHECKS"
reject_term "src/runtime/pgy_runtime_slot_macros.h" "PGY_SECURE_SLOT_DEFINE_CHECKED"
reject_term "src/runtime/pgy_runtime_slot_macros.h" "PGY_SECURE_SLOT_DEFINE_DEBUG"
require_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("occupied", pgy_abi_slot_int, occupied)'
require_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("token", pgy_abi_secure_slot_int, token)'
reject_term "src/compiler/mir_abi_resource_runtime.c" "Slot<Int>_rel"
reject_term "src/compiler/mir_abi_resource_runtime.c" 'mir_abi_format_owned("%s_rel"'
reject_term "src/compiler/mir_abi_resource_runtime.c" "mir_abi_lookup_runtime_fmt"
reject_term "src/compiler/mir_abi_resource_runtime.c" "mir_extract_inner_type_suffix_owned"
reject_term "src/compiler/mir_abi_resource_runtime.c" "runtime function name pattern"
require_term "src/compiler/mir_abi_resource_runtime.c" "Runtime function spelling is payload carried"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_fn"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_fn_by_type_name"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_count"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_domain"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_symbol"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_target_kind"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_materialization"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_call_shape"
require_term "src/compiler/mir_abi_layout.h" "typedef struct MIRAbiTargetPolicy"
require_term "src/compiler/mir_abi_layout.h" "const MIRAbiTargetPolicy *mir_abi_target_policy"
require_term "src/compiler/mir_abi_layout.c" "static const MIRAbiTargetPolicy k_abi_target_policy_table[]"
require_term "src/compiler/mir_abi_layout.c" '"selfhost-c", "cpu-c,self-hosted"'
require_term "src/compiler/mir_abi_layout.c" '"layout_shape,materialization_reason"'
require_term "src/compiler/mir_abi_layout.c" '"unsupported_shape,forbidden_loss_budget"'
require_term "src/compiler/mir_abi_resource_runtime.c" "native-resource"
require_term "src/compiler/mir_abi_resource_runtime.c" "mir_abi_resource_row"
require_term "src/compiler/mir_abi_resource_runtime.c" "MIRResourceRuntimeRow"
require_term "src/runtime/pgy_abi_spec.h" "allocator provenance as a fourth field"
require_term "src/compiler/mir_abi_layout.c" 'ABI_TYPE("Array<Long>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("allocator", pgy_abi_array_int, allocator)'
reject_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("len", pgy_abi_array_int, len)'
reject_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("cap", pgy_abi_array_int, cap)'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_PLAIN_RESOURCE_OPS("Slot<Int>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_SECURE_RESOURCE_OPS("SecureSlot<Int>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_SECURE_RESOURCE_OPS("SecureSlot<Long>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_PLAIN_PIN_OPS("Slot<Int>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_SECURE_PIN_OPS("SecureSlot<Int>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Int>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_PLAIN_RESOURCE_OPS("DeviceSlot<Long>"'
require_term "src/compiler/mir_abi_resource_runtime.c" 'ABI_RESOURCE_OP("DeviceSlot<Int>", "SubmitRead"'
require_term "src/codegen/transpiler_mir_resource_op_core.c" "mir_abi_resource_runtime_row_by_type_name("
require_term "src/codegen/transpiler_mir_resource_op_core.c" "mir_abi_resource_runtime_row_by_kind("
require_term "src/codegen/transpiler_mir_resource_op_core.c" "runtime_row->call_shape"
reject_term "src/codegen/transpiler_mir_resource_op_core.c" "transpiler_format_slot_runtime_fn"
require_term "src/codegen/llvm_runtime_row.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_runtime_row.c" "MIR-only LLVM path missing active routine for runtime-call ABI row"
require_term "src/codegen/llvm_runtime_row.c" "row->call_shape"
require_term "src/codegen/llvm_runtime_row.c" '"returns_container"'
require_term "src/codegen/llvm_runtime_row.c" '"container_ptr_to_value"'
require_term "src/codegen/llvm_runtime_row.c" '"container_ptr_value_to_void"'
require_term "src/codegen/llvm_runtime_row.c" '"container_ptr_to_void"'
require_term "src/codegen/llvm_runtime_row.c" '"PinRead"'
require_term "src/codegen/llvm_runtime_row.c" '"PinWrite"'
require_term "src/codegen/llvm_runtime_row.c" '"PinReadInit"'
require_term "src/codegen/llvm_runtime_row.c" '"PinWriteInit"'
require_term "src/codegen/llvm_runtime_row.c" '"Unpin"'
require_term "src/compiler/mir_abi_resource_runtime.c" '"UnpinCleanup"'
reject_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name("
reject_term "src/codegen/llvm_runtime.c" "llvm_runtime_slot_name"
reject_term "src/codegen/llvm_runtime_row.c" "mir_abi_resource_runtime_fn_by_type_name("
reject_term "src/codegen/llvm_runtime_row.c" "llvm_runtime_slot_name"
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "read", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "write", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release", suffix)'
require_term "src/codegen/llvm_runtime.c" "device_abi_type_name"
require_term "src/codegen/llvm_runtime.c" '"SubmitRead"'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim_device", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_read", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_write", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release_device", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "submit_device_read", suffix)'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "row->call_shape"
require_term "src/codegen/llvm_runtime_row.c" '"token_ptr_to_container"'
require_term "src/codegen/llvm_runtime_row.c" '"container_ptr_token_ptr_to_value"'
require_term "src/codegen/llvm_runtime_row.c" '"container_ptr_value_token_ptr_to_void"'
require_term "src/codegen/llvm_runtime_row.c" '"container_ptr_token_ptr_to_void"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinRead"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinWrite"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinReadInit"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinWriteInit"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"Unpin"'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" "mir_abi_resource_runtime_fn_by_type_name("
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" "llvm_runtime_secure_slot_name"
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "claim_secure", suf)'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_read", suf)'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_write", suf)'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_release", suf)'
require_term "src/compiler/mir_abi_layout.h" "MIRResourceAbiKind"
require_term "src/compiler/mir_abi.h" "typedef struct MIRResourceRuntimeRow"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_for_type_name"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_by_kind"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_fn_by_kind"
require_term "src/codegen/llvm_expr_slot_device_calls.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_expr_slot_device_calls.c" "row->call_shape"
require_term "src/codegen/llvm_expr_slot_device_calls.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_expr_slot_device_calls.c" "MIR_RESOURCE_ABI_DEVICE_SLOT"
require_term "src/codegen/llvm_expr_identifier_slot_helpers.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_expr_identifier_slot_helpers.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_expr_call_methods_domain_slice.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_expr_call_methods_domain_slice.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_expr_assignment_member_projection.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_expr_assignment_member_projection.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_stmt_let_resources.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_stmt_with.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_stmt_block.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_runtime_internal.h" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_runtime_row.c" "llvm_slot_runtime_expected_call_shape"
require_term "src/codegen/llvm_runtime_row.c" "mir_machine_layer_fact_matches_runtime_operation"
require_term "src/codegen/transpiler_slot_builtin_emit.c" "transpiler_slot_runtime_row_for_source_operation("
require_term "src/codegen/transpiler_slot_builtin_emit.c" "row->call_shape"
require_term "src/codegen/transpiler_slot_builtin_emit.c" "C source slot builtin %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_slot_runtime_row.c" "mir_abi_resource_runtime_row_by_kind("
require_term "src/codegen/transpiler_slot_runtime_row.c" "row->call_shape"
require_term "src/codegen/transpiler_slot_runtime_row.c" "transpiler_slot_runtime_expected_call_shape"
require_term "src/codegen/transpiler_slot_runtime_row.c" '"PinRead"'
require_term "src/codegen/transpiler_slot_runtime_row.c" '"UnpinCleanup"'
require_term "src/codegen/transpiler_slot_runtime_row.c" "C slot operation %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_slot_runtime_row.c" "transpiler_emit_nominal_container_runtime_rows"
require_term "src/codegen/transpiler_slot_runtime_row.c" "PGY_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "PGY_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "PGY_SECURE_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "PGY_BOX_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_generic_class_specialization_emit.c" "PGY_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_generic_class_specialization_emit.c" "PGY_SECURE_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_generic_class_specialization_emit.c" "PGY_BOX_DEFINE(%s, %s)"
require_term "src/codegen/transpiler_expr_call_member_emit.c" "transpiler_slot_runtime_fn("
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_let_slot_emit.c" "transpiler_slot_runtime_row_for_operation("
require_term "src/codegen/transpiler_let_slot_emit.c" "row->call_shape"
require_term "src/codegen/transpiler_let_slot_emit.c" "C let-slot %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_func_class_flow_emit.c" "transpiler_slot_runtime_fn("
reject_term "src/codegen/transpiler_func_class_flow_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_mir_destructure_emit.c" "transpiler_slot_runtime_fn("
reject_term "src/codegen/transpiler_mir_destructure_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_class_decl_emit.c" "transpiler_slot_runtime_fn("
reject_term "src/codegen/transpiler_class_decl_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_expr_stdlib_builtin.c" "transpiler_slot_runtime_fn("
reject_term "src/codegen/transpiler_expr_stdlib_builtin.c" "mir_abi_resource_runtime_fn_by_kind("
reject_term "src/codegen/llvm_expr_identifier_slot_helpers.c" 'is_secure ? "pgy_secure_read_%s" : "pgy_read_%s"'
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" "llvm_domain_slot_format_runtime_name"
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" '"pgy_secure_write", inner'
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" '"pgy_write", inner'
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" '"pgy_secure_read", inner'
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" '"pgy_read", inner'
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" '"pgy_secure_release", inner'
reject_term "src/codegen/llvm_expr_call_methods_domain_slice.c" '"pgy_release", inner'
reject_term "src/codegen/llvm_expr_assignment_member_projection.c" 'is_secure ? "pgy_secure_write_%s" : "pgy_write_%s"'
reject_term "src/codegen/llvm_stmt_let_names.c" 'is_secure ? "pgy_secure_write_%s" : "pgy_write_%s"'
reject_term "src/codegen/llvm_stmt_with.c" 'is_secure ? "pgy_secure_release_%s" : "pgy_release_%s"'
reject_term "src/codegen/llvm_stmt.c" "llvm_stmt_format_runtime_name"
reject_term "src/codegen/llvm_stmt.c" 'is_secure ? "pgy_secure_release_" : "pgy_release_"'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" 'is_secure ? "pgy_secure_write" : "pgy_write"'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" 'is_secure ? "pgy_secure_read" : "pgy_read"'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" 'is_secure ? "pgy_secure_release" : "pgy_release"'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" '"pgy_device_write", inner'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" '"pgy_device_read", inner'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" '"pgy_release_device", inner'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" '"pgy_submit_device_read", inner'
reject_term "src/codegen/llvm_expr_slot_device_calls.c" "llvm_slot_format_runtime_name"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_write_%s(%s, %s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_secure_write_%s(%s, %s, &%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_read_%s(%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_secure_read_%s(%s, &%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_release_%s(%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_secure_release_%s(%s, &%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_device_write_%s(&%s, %s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_device_read_%s(&%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_release_device_%s(&%s)"
reject_term "src/codegen/transpiler_slot_builtin_emit.c" "pgy_submit_device_read_%s(&%s)"
reject_term "src/codegen/transpiler_expr_assignment_emit.c" "pgy_write_%s(%s, %s)"
reject_term "src/codegen/transpiler_expr_assignment_emit.c" "pgy_secure_write_%s(%s, %s, &%s)"
reject_term "src/codegen/transpiler_expr_dispatch_emit.c" "pgy_read_%s(%s)"
reject_term "src/codegen/transpiler_expr_dispatch_emit.c" "pgy_secure_read_%s(%s, &%s)"
reject_term "src/codegen/transpiler_expr_dispatch_emit.c" "pgy_read_%s(&%s)"
reject_term "src/codegen/transpiler_expr_dispatch_emit.c" "pgy_secure_read_%s(&%s, &%s)"
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "pgy_write_%s(%s, %s)"
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "pgy_secure_write_%s(%s, %s, &%s)"
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "pgy_read_%s(%s)"
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "pgy_secure_read_%s(%s, &%s)"
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "pgy_release_%s(%s)"
reject_term "src/codegen/transpiler_expr_call_member_emit.c" "pgy_secure_release_%s(%s, &%s)"
reject_term "src/codegen/transpiler_let_slot_emit.c" "pgy_claim_%s()"
reject_term "src/codegen/transpiler_let_slot_emit.c" "pgy_claim_secure_%s(&%s_token)"
reject_term "src/codegen/transpiler_let_slot_emit.c" "pgy_claim_device_%s()"
reject_term "src/codegen/transpiler_let_slot_emit.c" "pgy_write_%s(&%s, %s)"
reject_term "src/codegen/transpiler_let_slot_emit.c" "pgy_secure_write_%s(&%s, %s, &%s_token)"
reject_term "src/codegen/transpiler_func_class_flow_emit.c" "pgy_claim_%s()"
reject_term "src/codegen/transpiler_func_class_flow_emit.c" "pgy_claim_secure_%s(&%s_token)"
reject_term "src/codegen/transpiler_func_class_flow_emit.c" "pgy_release_%s(&%s);"
reject_term "src/codegen/transpiler_func_class_flow_emit.c" "pgy_secure_release_%s(&%s, &%s_token);"
reject_term "src/codegen/transpiler_mir_destructure_emit.c" "pgy_claim_%s()"
reject_term "src/codegen/transpiler_mir_destructure_emit.c" "pgy_claim_secure_%s(&%s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "pgy_claim_%s()"
reject_term "src/codegen/transpiler_class_decl_emit.c" "pgy_claim_secure_%s(&self.%s)"
reject_term "src/codegen/transpiler_expr_stdlib_builtin.c" "PgySlot_%s _c = pgy_claim_%s()"
reject_term "src/codegen/transpiler_expr_stdlib_builtin.c" "pgy_write_%s(&_c, pgy_read_%s(&%s))"
require_term "src/test_abi_spec.c" "runtime size matches checked ABI"
reject_term "src/test_abi_spec.c" "PGY_RUNTIME_SLOT_MODE_CHECKED"
reject_term "src/test_abi_spec.c" "raw slot mode"
require_term "docs/semantics/13_slot_abi_single_owner.md" "Slot layout is no longer selected by build mode."
require_term "docs/semantics/13_slot_abi_single_owner.md" 'It must not be implemented as a macro that remaps `PgySlot_*`.'
require_term "src/runtime/pgy_abi_spec.h" "Rust-style niche encoding"
require_term "src/runtime/pgy_runtime_result_option_inline.h" "PGY_OPTION_DEFINE(Float, float)"
require_term "src/runtime/pgy_runtime_result_option_inline.h" "PGY_OPTION_DEFINE(Double, double)"
require_term "src/runtime/pgy_runtime_result_option_inline.h" "pgy_runtime_option_bool_inline.h"
require_term "src/runtime/pgy_runtime_option_bool_inline.h" "typedef struct"
require_term "src/runtime/pgy_runtime_option_bool_inline.h" "PgyOption_Bool"
require_term "src/runtime/pgy_runtime_result_option_inline.h" "#define Some_Float"
require_term "src/runtime/pgy_runtime_result_option_inline.h" "#define Some_Double"
require_term "src/runtime/pgy_abi_spec.h" "MIR_ABI_REPR_EXPLICIT_TAG"
require_term "src/runtime/pgy_abi_spec_asserts.h" "option_long_value_at_8"
require_term "src/runtime/pgy_abi_spec_asserts.h" "option_string_size_two_words"
require_term "src/compiler/mir_abi.h" "MIR_ABI_REPR_EXPLICIT_TAG"
require_term "src/compiler/mir_abi.h" "MIR_ABI_REPR_NICHE_RESERVED"
require_term "src/compiler/mir_abi.h" "niche_none_pattern"
require_term "src/compiler/mir_abi_layout.c" "ABI_TAGGED_TYPE(\"Option<Int>\""
require_term "src/compiler/mir_abi_layout.c" "ABI_TAGGED_TYPE(\"Option<Float>\""
require_term "src/compiler/mir_abi_layout.c" "ABI_TAGGED_TYPE(\"Option<Double>\""
require_term "src/compiler/mir_abi_layout.c" "PgyAbiOptionSome"
require_term "src/compiler/mir_abi_layout.c" "PgyAbiOptionNone"
require_term "src/codegen/transpiler_specialization_registry.c" "pgy_codegen_match_variant_c_option_tag(PGY_MATCH_VARIANT_SOME)"
require_term "src/codegen/transpiler_specialization_registry.c" "pgy_codegen_match_variant_c_option_tag(PGY_MATCH_VARIANT_NONE_CTOR)"
require_term "src/codegen/transpiler_specialization_registry.c" "pgy_codegen_match_variant_c_result_tag(PGY_MATCH_VARIANT_OK)"
require_term "src/codegen/transpiler_specialization_registry.c" "pgy_codegen_match_variant_c_result_tag(PGY_MATCH_VARIANT_ERR)"
require_term "src/codegen/transpiler_specialization_registry.c" "pgy_codegen_match_variant_c_payload_field(PGY_MATCH_VARIANT_OK)"
reject_term "src/codegen/transpiler_specialization_registry.c" "PgyOptionSome"
reject_term "src/codegen/transpiler_specialization_registry.c" "PgyOptionNone"
reject_term "src/codegen/transpiler_specialization_registry.c" "PgyResultOk"
reject_term "src/codegen/transpiler_specialization_registry.c" "PgyResultErr"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR ABI table records explicit Option tag representation"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR ABI resource runtime row table exposes native resource rows"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR ABI native resource rows match self-host runtime-call artifact"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_resource_runtime_row_count() == 150"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_resource_runtime_row_domain(0)"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_resource_runtime_row_materialization(0)"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_resource_runtime_row_call_shape(0)"
require_term "src/compiler/mir_abi.h" "runtime_call_abi_id"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_id"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_matches_owner"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_pin_owner_for_mir"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_pin_row_for_mir"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_is_constructed_nominal"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_instruction_for_abi"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_row_for_mir_abi"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_layout_id"
require_term "src/compiler/mir_abi_layout.c" "Stable identity for a complete static MIR ABI layout row"
require_term "src/compiler/mir_abi_layout.c" "UINT32_C(0x20000000)"
require_term "src/compiler/mir_abi_resource_runtime.c" "UINT32_C(0x40000000)"
require_term "src/compiler/mir_abi_resource_runtime_constructed.c" "constructed_resource_runtime_spelling"
require_term "src/compiler/mir_abi_resource_runtime_constructed.c" "mir_abi_resource_runtime_row_by_kind("
require_term "src/compiler/mir_types.h" "abi_layout_id"
require_term "src/compiler/mir_types.h" "resource_runtime_aux_fact_count"
require_term "src/compiler/mir_types.h" "resource_runtime_aux_facts"
require_term "src/compiler/mir_fact_surface_validate.c" "runtime_call_abi_id == 0"
require_term "src/compiler/mir_fact_surface_validate.c" "auxiliary runtime-call ABI row count exceeds capacity"
require_term "src/compiler/mir_json_dump_runtime_abi.c" "runtime_call_abi_aux"
require_term "src/compiler/mir_fact_surface_validate.c" "mir_abi_resource_runtime_row_matches_owner(row)"
require_term "src/compiler/mir_fact_surface_validate.c" "ABI layout fact has missing or mismatched stable identity"
require_term "src/compiler/mir_abi_resource_runtime_mir.c" "resource_runtime_fact_present"
require_term "src/compiler/mir_abi_resource_runtime_mir.c" "mir_abi_resource_runtime_instruction_for_source"
require_term "src/compiler/mir_abi_resource_runtime_mir.c" "mir_abi_resource_runtime_row_for_mir_abi"
require_term "src/codegen/llvm_runtime_row.c" "llvm_slot_runtime_operation_is_synthetic_pin"
require_term "src/codegen/llvm_runtime_row.c" "mir_abi_resource_runtime_row_matches_owner(row)"
require_term "src/codegen/transpiler_slot_runtime_row.c" "mir_abi_resource_runtime_row_matches_owner(row)"
require_term "src/codegen/llvm_mir_block_emit.c" "llvm_mir_def_is_resource_view_alias"
require_term "src/codegen/transpiler_slot_runtime_row.c" "C MIR source operation has no active instruction-owned runtime-call ABI row"
require_term "src/codegen/transpiler_slot_runtime_row.c" "MIR-only C path missing active routine for runtime-call ABI row"
require_term "src/codegen/transpiler_slot_runtime_row.c" "mir_abi_resource_runtime_row_is_constructed_nominal"
require_term "src/codegen/llvm_runtime_row.c" "LLVM MIR source operation is missing its lowered runtime-call ABI row"
require_term "src/codegen/llvm_mir_source_def_copy.c" "ctx->current_mir_routine != NULL"
require_term "src/codegen/llvm_mir_source_def_copy.c" "llvm_mir_source_local_type_fact"
require_term "src/codegen/llvm_mir_source_def_copy.c" "type_ann = !mir_active"
require_term "src/codegen/llvm_mir_source_def_copy.c" "!mir_active && type_ann != NULL"
require_term "src/codegen/llvm_mir_source_def_copy.c" "mir_active && inst->abi_type_name != NULL"
require_term "src/compiler/rir.h" "source_statement_syntax_id"
require_term "src/compiler/mir_types.h" "source_statement_stable_id"
require_term "src/compiler/mir_stmt_source_inventory.c" "mir_set_inst_source_statement_fact"
require_term "src/compiler/mir_stmt_population.c" "mir_set_inst_source_statement_fact(&def_inst, stmt"
require_term "src/compiler/mir_non_cfg_stmt_population.c" "mir_set_inst_source_statement_fact(inst, stmt, i)"
require_term "src/tests/mir/test_mir_runtime_call_abi.cases.h" "MIR ABI runtime-call identities are nonzero and unique"
require_term "src/tests/mir/test_mir_runtime_call_abi.cases.h" "MIR keeps multiple resource runtime rows distinct within one statement"
require_term "src/tests/mir/test_mir_runtime_call_abi.cases.h" "MIR slot-sugar DEF keeps Claim row and owns concrete Write row"
reject_term "src/compiler/mir_source_provenance.c" "ast_call_argument(consumer_expr"
reject_term "src/compiler/mir_source_provenance.c" "ast_call_callee(resource->ast"
reject_term "src/compiler/mir_source_provenance.c" "ast_node_stable_id(consumer->expr0)"
reject_term "src/compiler/mir_source_provenance.c" "resource->source_line == consumer->source_line"
require_term "src/test_mir.c" "runtime_call_abi_expected_native_rows_match"
require_term "src/test_mir.c" "runtime_call_abi_rows.txt"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR_ABI_REPR_EXPLICIT_TAG"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_lookup(\"Option<Float>\")"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_lookup(\"Option<Double>\")"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "niche_none_pattern == NULL"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR ABI layout rows carry stable content identity"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR validator rejects missing ABI layout identity"
require_term "src/compiler/mir_json_dump.c" "mir_json_emit_instruction_abi_layout"
require_term "src/compiler/mir_json_dump.c" "abi_layout_required"
require_term "src/compiler/mir_json_dump.c" "abi_layout"
require_term "src/self_hosted/mir_lower/abi_layout_fact_owner.pgy" "MirCapturedAbiLayoutFactReady"
require_term "src/self_hosted/mir_lower/abi_layout_fact_owner.pgy" "MirAbiLayoutRowCaptureWithin"
reject_term "src/self_hosted/mir_lower/abi_layout_fact_owner.pgy" "MirInstructionAbiLayoutFactReady"
require_term "src/self_hosted/mir_lower/abi_layout_fact_owner.pgy" "MirAbiLayoutIdFromRow"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "SelfMirJsonStaticAbiLayout"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "DeviceSlot<Int>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Slot<String>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "SecureSlot<String>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "DeviceSlot<String>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Option<String>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Result<String>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Array<Int>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Array<String>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Array<Long>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Array<Float>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Array<Double>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "Array<Bool>"
require_term "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" "CompilerAbiLayoutTargetPolicyReady"
require_term "src/self_hosted/mir/json_projection_owner.pgy" "abi_layout_required"
require_term "src/compiler/verified_projection_plan.h" "uint32_t              source_stable_id;"
reject_term "src/compiler/verified_projection_plan.h" "const struct ASTNode *site"
require_term "src/compiler/verified_projection_plan.c" "spawn boundary is missing stable source identity"
reject_term "src/compiler/verified_projection_plan.c" "rows[j].site"
reject_term "src/compiler/verified_projection_plan.c" ".site = boundary->ast"
require_term "src/codegen/transpiler_spawn_channel_emit.c" "ast_node_stable_id(node)"
require_term "src/codegen/llvm_expr_spawn_call_helpers.c" "ast_node_stable_id(node)"
require_term "src/compiler/verified_region_plan.h" "PgyRegionAllocationSiteId"
require_term "src/compiler/verified_region_plan.h" "allocation_site_id"
reject_term "src/compiler/verified_region_plan.h" "const struct ASTNode *site"
require_term "tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh" "missing-abi-layout-fact"
require_term "tests/self_hosted/parity/driver_rung2_mir_abi_layout_negative_owner.sh" "wrong-abi-layout-identity"
require_term "src/codegen/transpiler_specialization_registry.c" "strcmp(inner_type, \"Float\") == 0"
require_term "src/codegen/transpiler_specialization_registry.c" "strcmp(inner_type, \"Double\") == 0"
require_term "src/codegen/transpiler_specialization_registry.c" "PGY_LIST_DEFINE(%s, %s)"
require_term "src/codegen/transpiler_specialization_registry.c" "PGY_QUEUE_DEFINE(%s, %s)"
require_term "src/codegen/transpiler_specialization_registry.c" "PGY_HASHMAP_DEFINE(%s, %s)"
require_term "src/codegen/transpiler_specialization_registry.c" "PGY_SET_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_expr_stdlib_collection_support.c" "PGY_LIST_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_expr_stdlib_collection_support.c" "PGY_QUEUE_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_expr_stdlib_collection_support.c" "PGY_HASHMAP_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_expr_stdlib_collection_support.c" "PGY_SET_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_expr_stdlib_collection_support.c" "PGY_SET_VALUES_DEFINE(%s, %s, %s)"
require_term "src/runtime/pgy_abi_spec.h" "pgy_abi_pinned_slot_view_int"
require_term "src/runtime/pgy_abi_spec.h" "pgy_abi_pinned_secure_slot_view_int"
require_term "src/test_abi_spec.c" "sizeof(PgyPinnedSlotView_Int) == sizeof(pgy_abi_pinned_slot_view_int)"
require_term "src/test_abi_spec.c" "offsetof(PgyPinnedSecureSlotView_Int, token) == offsetof(pgy_abi_pinned_secure_slot_view_int, token)"
require_term "src/test_abi_spec.c" "Option<Long>: explicit tag layout is 16 bytes"
require_term "src/test_abi_spec.c" "Option<String>: explicit tag layout is two words"
require_term "src/test_abi_spec.c" "Array<Int>: runtime allocator offset matches"
require_term "src/test_abi_spec.c" "Array<String>: runtime size matches ABI spec"
require_term "src/test_abi_spec.c" "Slice<String>: runtime size matches ABI spec"
require_term "src/test_abi_spec.c" "Option<Float>: runtime size matches ABI spec"
require_term "src/test_abi_spec.c" "Option<Double>: runtime size matches ABI spec"

require_term "src/runtime/pgy_abi_spec.h" "PGY_ABI_ALLOC_SCRATCH"
require_term "src/runtime/pgy_abi_spec.h" "PGY_ABI_ALLOC_RESULT"
require_term "src/runtime/pgy_abi_spec.h" "PGY_ABI_ALLOC_PERSISTENT"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "PGY_ALLOC_SCRATCH"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "PGY_ALLOC_RESULT"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "PGY_ALLOC_PERSISTENT"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "pgy_allocator_scratch(void)"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "pgy_allocator_result(void)"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "pgy_allocator_persistent(void)"
require_term "src/runtime/pgy_runtime_allocator_inline.h" "pgy_allocator_destroy(PgyAllocator *alloc)"
require_term "src/runtime/pgy_runtime_lib_allocator_exports.h" "pgy_allocator_scratch_init"
require_term "src/runtime/pgy_runtime_lib_allocator_exports.h" "pgy_allocator_result_init"
require_term "src/runtime/pgy_runtime_lib_allocator_exports.h" "pgy_allocator_persistent_init"
require_term "src/runtime/pgy_runtime_lib_allocator_exports.h" "pgy_allocator_destroy_export"
require_term "src/codegen/transpiler_allocator_builtin_emit.c" "pgy_allocator_scratch()"
require_term "src/codegen/transpiler_allocator_builtin_emit.c" "pgy_allocator_result()"
require_term "src/codegen/transpiler_allocator_builtin_emit.c" "pgy_allocator_persistent()"
require_term "src/codegen/transpiler_allocator_builtin_emit.c" "AllocatorDestroy requires a named Allocator local"
require_term "src/codegen/transpiler_allocator_builtin_emit.c" "pgy_allocator_destroy(&%s)"
require_term "src/codegen/llvm_expr_allocator_calls.c" "pgy_allocator_scratch_init"
require_term "src/codegen/llvm_expr_allocator_calls.c" "pgy_allocator_result_init"
require_term "src/codegen/llvm_expr_allocator_calls.c" "pgy_allocator_persistent_init"
require_term "src/codegen/llvm_expr_allocator_calls.c" "AllocatorDestroy"
require_term "src/codegen/llvm_expr_allocator_calls.c" "pgy_allocator_destroy_export"
require_term "src/codegen/llvm_runtime.c" "pgy_allocator_scratch_init"
require_term "src/codegen/llvm_runtime.c" "pgy_allocator_result_init"
require_term "src/codegen/llvm_runtime.c" "pgy_allocator_persistent_init"
require_term "src/codegen/llvm_runtime.c" "pgy_allocator_destroy_export"

require_term "src/runtime/slot_manager.h" "PgyPinnedView"
require_term "src/runtime/slot_manager.h" "User-facing language syntax must keep PgyPinnedView scope-bound."
require_term "src/runtime/slot_manager_pin.c" "entry->pinThreadAffinity = tid"
require_term "src/runtime/slot_manager_pin.c" "entry->pinGeneration = handle->generation"
require_term "src/runtime/slot_manager_pin.c" "entry->pinThreadAffinity != tid"
require_term "src/runtime/slot_manager_pin.c" "entry->pinGeneration != view->generation"
require_term "src/runtime/slot_manager.c" "return (uintptr_t)pthread_self();"
require_term "src/runtime/slot_manager_internal.h" "uintptr_t current_thread_id(void);"
require_term "src/runtime/slot_manager.h" "uintptr_t pinThreadAffinity;"
if grep -Fq "0xffffffffu" "$ROOT_DIR/src/runtime/slot_manager.c"; then
    fail "slot pin thread affinity must not truncate pthread_self to 32 bits"
fi
require_term "src/runtime/slot_manager_pin.c" "slot_token_valid_for_entry_locked(manager, handle, token, entry)"
require_term "src/runtime/slot_manager.c" "if (entry->pinCount > 0)"
require_term "src/runtime/slot_manager.c" "return SLOT_ERROR_PINNED"
require_term "src/runtime/slot_manager.c" "slot_reset_entry_locked(entry)"
require_term "src/runtime/slot_manager_core_ops.c" "entry->pinMode == (uint32_t)PGY_SLOT_PIN_WRITE"
require_term "src/tests/security/test_security_slot_pin_lease.cases.h" "Write-pinned plain slot rejects concurrent read"

require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "PgyPinnedSlotView_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_pin_read_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_pin_write_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_pin_read_init_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_pin_write_init_##SuffixName"
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_unpin_cleanup_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "PgyPinnedSecureSlotView_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "pgy_secure_pin_read_init_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "pgy_secure_pin_write_init_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "pgy_secure_unpin_cleanup_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_unpin_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_pin_read_init_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_pin_write_init_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_unpin_##Suffix"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_pin_read_init_##Suffix"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_pin_write_init_##Suffix"

require_term "src/codegen/transpiler_mir_pin_emit.c" "transpiler_slot_runtime_row_for_operation("
require_term "src/codegen/transpiler_mir_pin_emit.c" "row->call_shape"
require_term "src/codegen/transpiler_mir_pin_emit.c" "transpiler_slot_runtime_expected_call_shape"
require_term "src/codegen/transpiler_mir_pin_emit.c" '"PinReadInit"'
require_term "src/codegen/transpiler_mir_pin_emit.c" '"PinWriteInit"'
reject_term "src/codegen/transpiler_mir_pin_emit.c" 'pin_op = block->pin_view_is_write ? "PinWrite" : "PinRead"'
require_term "src/codegen/transpiler_mir_pin_emit.c" '"Unpin"'
reject_term "src/codegen/transpiler_mir_pin_emit.c" "transpiler_mir_pin_expected_call_shape"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_pin_%s_%s"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_secure_pin_%s_%s"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_unpin_%s(&%s);"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_secure_unpin_%s(&%s);"
require_term "src/compiler/mir_types.h" "resource_owner_slot_anchor"
require_term "src/compiler/mir_types.h" "resource_owner_requires_metadata"
require_term "src/compiler/mir_lower_population.c" "MIRResourceBorrowLoweringFact"
require_term "src/compiler/mir_lower_population.c" "mir_resource_record_borrow_fact"
require_term "src/compiler/mir_lower_population.c" "inst.resource_owner_requires_metadata = resource_owner_slot_anchor != NULL"
require_term "src/compiler/mir_resource_runtime_population.c" "mir_materialize_resource_runtime_row"
require_term "src/compiler/mir_resource_runtime_population.c" "out->runtime_call_abi_id = mir_abi_resource_runtime_row_id(out)"
require_term "src/compiler/mir_resource_runtime_population.c" "mir_link_resource_runtime_facts"
require_term "src/compiler/mir_resource_runtime_population.h" "bool mir_materialize_resource_runtime_fact("
require_term "src/compiler/mir_lower_population.c" "mir_materialize_resource_runtime_fact(routine, &inst)"
reject_term "src/compiler/mir_lower_population.c" "mir_resource_runtime_operation_has_row"
reject_term "src/compiler/mir_lower_population.c" "out->runtime_call_abi_id = mir_abi_resource_runtime_row_id(out)"
reject_term "src/compiler/mir_lower_population.h" "mir_materialize_resource_runtime_row"
require_term "src/compiler/mir_fact_surface_validate.c" "inst->resource_owner_requires_metadata"
require_term "src/compiler/mir_fact_surface_validate_resource.c" "return inst->arg1 != NULL && strcmp(inst->arg1, view_name) == 0"
require_term "src/compiler/mir_fact_surface_validate.c" "view-backed resource op is missing owner slot ABI metadata"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR validator rejects view-backed resource owner metadata drift"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "inst->resource_owner_slot_anchor"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "inst->resource_owner_requires_metadata"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "expected_owner_slot"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "TypedVarEntry *view_entry = mir_active"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "strcmp(view_source_slot, expected_owner_slot) != 0"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "&& !mir_active"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "source_layout == NULL && !mir_active"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "const MIRTypeLayout *source_layout = inst->type_layout"
reject_term "src/codegen/transpiler_mir_resource_hook_emit.c" "transpiler_mir_find_prior_borrow_source_for_view"
reject_term "src/codegen/transpiler_mir_resource_hook_emit.c" "transpiler_mir_find_prior_resource_layout_for_slot"
reject_term "src/codegen/transpiler_mir_resource_hook_emit.c" "transpiler_mir_layout_from_type_annotation"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "MIR view-backed resource op '%s' is missing owner slot ABI metadata"
require_term "src/codegen/llvm_internal_api.h" "bool          llvm_mir_emit_borrow_view_alias"
require_term "src/codegen/llvm_mir_block_emit.c" "if (!llvm_mir_emit_borrow_view_alias(inst, ctx))"
require_term "src/codegen/llvm_mir_resource_view.c" "inst->resource_owner_slot_anchor"
require_term "src/codegen/llvm_mir_resource_view.c" "inst->resource_owner_requires_metadata"
require_term "src/codegen/llvm_mir_resource_view.c" "LLVM MIR borrow view alias '%s' is missing owner slot ABI metadata"
require_term "src/codegen/transpiler_block_emit.c" "__attribute__((cleanup(%s)))"
require_term "src/codegen/transpiler_block_emit.c" "transpiler_slot_runtime_row_for_operation("
require_term "src/codegen/transpiler_block_emit.c" "row->call_shape"
require_term "src/codegen/transpiler_block_emit.c" "transpiler_slot_runtime_expected_call_shape"
require_term "src/codegen/transpiler_block_emit.c" '"PinRead"'
require_term "src/codegen/transpiler_block_emit.c" '"PinWrite"'
require_term "src/codegen/transpiler_block_emit.c" '"UnpinCleanup"'
require_term "src/codegen/transpiler_block_emit.c" '"Release"'
require_term "src/codegen/transpiler_block_emit.c" "C source slot auto-release requires MIR ABI runtime function row"
reject_term "src/codegen/transpiler_block_emit.c" "mir_abi_resource_runtime_fn_by_kind("
reject_term "src/codegen/transpiler_block_emit.c" "transpiler_block_pin_expected_call_shape"
reject_term "src/codegen/transpiler_block_emit.c" "pgy_pin_%s_%s"
reject_term "src/codegen/transpiler_block_emit.c" "pgy_secure_pin_%s_%s"
reject_term "src/codegen/transpiler_block_emit.c" "cleanup(pgy_unpin_cleanup_%s)"
reject_term "src/codegen/transpiler_block_emit.c" "cleanup(pgy_secure_unpin_cleanup_%s)"
reject_term "src/codegen/transpiler_block_emit.c" "pgy_release_%s(&%s);"
reject_term "src/codegen/transpiler_block_emit.c" "pgy_secure_release_%s(&%s, &%s_token);"
require_term "src/codegen/llvm_runtime.c" '"PinReadInit"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinReadInit"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinWriteInit"'
require_term "src/codegen/llvm_runtime.c" '"Unpin"'
require_term "src/codegen/llvm_mir_pin_region.c" "llvm_mir_emit_plain_pin_inline_enter"
require_term "src/codegen/llvm_mir_pin_region.c" "LLVMBuildStructGEP2(ctx->builder, slot_ty, slot_ptr_arg, 1"
reject_term "src/codegen/llvm_mir_pin_region.c" "pgy_pin_%s_init_%s"
reject_term "src/codegen/llvm_mir_pin_region.c" "pgy_secure_pin_%s_init_%s"
require_term "src/codegen/llvm_mir_pin_region.c" "llvm_slot_runtime_row_for_operation("
require_term "src/codegen/llvm_mir_pin_region.c" "row->call_shape"
require_term "src/codegen/llvm_mir_pin_region.c" '"PinReadInit"'
require_term "src/codegen/llvm_mir_pin_region.c" '"PinWriteInit"'
require_term "src/codegen/llvm_mir_pin_region.c" "llvm_mir_emit_plain_pin_inline_exit"
require_term "src/codegen/llvm_mir_pin_region.c" '"Unpin"'
reject_term "src/codegen/llvm_mir_pin_region.c" "mir_abi_resource_runtime_fn_by_kind("
reject_term "src/codegen/llvm_mir_pin_region.c" "pgy_secure_unpin_%s"
reject_term "src/codegen/llvm_mir_pin_region.c" "llvm_mir_unpin_name"

require_term "src/compiler/mir_cleanup_fact_names.h" "pin-unpin-cleanup-edge"
require_term "src/compiler/mir_cleanup_fact_names.h" "MIR_CLEANUP_FACT_PIN_UNPIN_EDGE"
require_term "src/compiler/mir_cfg_contract_validate_cleanup.c" "pin-region block[%zu] missing pin-unpin cleanup fact"
require_term "tests/cfg_body_dataflow_smoke.sh" "ReleaseAfterUnpin(slot, all_cfg_exits)"
require_term "tests/cfg_body_dataflow_smoke.sh" "WriteView requires exclusive slot view access"
require_term "tests/compare_backends.sh" "tests/cases/backend_compare/pin_read_view_block"
require_term "tests/compare_backends.sh" "tests/cases/backend_compare/pin_break_cleanup_block"
require_term "tests/compare_backends.sh" "tests/cases/backend_compare/pin_secure_param_read_view_block"

require_term "docs/74_slot_pinning_caching.md" "Pin/Lease is a typed lexical lease"
require_term "docs/74_slot_pinning_caching.md" "Evidence View Cache Policy"
require_term "docs/74_slot_pinning_caching.md" "The cache must be fail-closed."
require_term "docs/74_slot_pinning_caching.md" "Generated code may cache a typed view only when MIR pin-region facts"
require_term "docs/100_beta_readiness_checklist.md" "Slot/Pin/Zone-bound handle/runtime-none/raw escape"
require_term "docs/107_beta_stable_subset.md" "Non-pin handle expiration is not claimed as a single-mechanism proof"
require_term "docs/107_beta_stable_subset.md" "Rust-style niche optimization and user-directed explicit layout are not"
require_term "docs/107_beta_stable_subset.md" 'Option<T>` uses the explicit tagged ABI'
require_term "docs/118_slot_model_rigor_audit.md" "Zone-Bound Handle typing"
require_term "docs/125_source_of_truth_spine.md" 'ABI layout facts live in `src/runtime/pgy_abi_spec.h`'
require_term "docs/125_source_of_truth_spine.md" 'Option<T>` is currently an explicit tagged'
require_term "docs/125_source_of_truth_spine.md" "semantic/DAG proof types"
require_term "docs/125_source_of_truth_spine.md" "C and LLVM backends must not"
require_term "docs/125_source_of_truth_spine.md" 'future `unsafe(ffi, layout)`'
require_term "docs/136_abi_niche_and_explicit_layout.md" 'Option<T>` stays explicitly tagged'
require_term "docs/136_abi_niche_and_explicit_layout.md" "Frozen Beta Layout Facts"
require_term "docs/136_abi_niche_and_explicit_layout.md" '`Option<String>` | explicit tag | 16'
require_term "docs/136_abi_niche_and_explicit_layout.md" "Beta-closure decision: do not add niche optimization before the proof surface"
require_term "docs/136_abi_niche_and_explicit_layout.md" "Value-invariant proof types are prerequisite"
require_term "docs/136_abi_niche_and_explicit_layout.md" "MIR ABI fact must be the only backend input"
require_term "docs/136_abi_niche_and_explicit_layout.md" "The promotion ladder is deliberately ordered"
require_term "docs/136_abi_niche_and_explicit_layout.md" "Semantic/DAG proves the value invariant"
require_term "docs/136_abi_niche_and_explicit_layout.md" "MIR_ABI_REPR_EXPLICIT_TAG"
require_term "docs/136_abi_niche_and_explicit_layout.md" "MIR_ABI_REPR_NICHE_RESERVED"
require_term "docs/136_abi_niche_and_explicit_layout.md" "Current Golden Gates"
require_term "docs/136_abi_niche_and_explicit_layout.md" '`let mut` and `inout` do not weaken this rule'
require_term "docs/136_abi_niche_and_explicit_layout.md" '`let mut` is local-storage mutability'
require_term "docs/136_abi_niche_and_explicit_layout.md" 'reject `let mut` / `inout` access to partial-width packed'
require_term "docs/136_abi_niche_and_explicit_layout.md" "reject address-like treatment of bit slices"
require_term "docs/136_abi_niche_and_explicit_layout.md" 'diagnostic'
require_term "docs/136_abi_niche_and_explicit_layout.md" 'missing `LayoutFact` owner'
require_term "docs/136_abi_niche_and_explicit_layout.md" "bit width, read"
require_term "docs/136_abi_niche_and_explicit_layout.md" "mask, write mask, and shift"
require_term "docs/136_abi_niche_and_explicit_layout.md" "backends must consume the same fact row"
require_term "docs/136_abi_niche_and_explicit_layout.md" 'extern "C" ABI'
require_term "docs/136_abi_niche_and_explicit_layout.md" "unsafe(ffi, layout)"
require_term "docs/136_abi_niche_and_explicit_layout.md" "boundary-scoped, never the default aggregate model"
require_term "docs/136_abi_niche_and_explicit_layout.md" "Pergyra must not hide a wire-order convention"
require_term "docs/136_abi_niche_and_explicit_layout.md" 'bits(value, order = ...)'
require_term "docs/136_abi_niche_and_explicit_layout.md" 'reinterpret(value, layout = ..., endian = ..., abi = ...)'
require_term "docs/136_abi_niche_and_explicit_layout.md" "No safe surface may default to hidden little-endian"
require_term "docs/semantics/04_ownership_abi.md" "Safe bit conversion is not memory reinterpretation"
require_term "docs/semantics/04_ownership_abi.md" "hidden default bit order"
require_term "docs/145_bit_layout_boundary_matrix.md" "Pergyra must not copy a hidden \"logical bits\" default"
require_term "docs/145_bit_layout_boundary_matrix.md" "bits(value, order = LSB-first | MSB-first | named-order)"
require_term "docs/145_bit_layout_boundary_matrix.md" "reinterpret(value, layout = ..., endian = ..., abi = ..., world = ...)"
require_term "docs/145_bit_layout_boundary_matrix.md" "Layer-Width Contract"
require_term "docs/145_bit_layout_boundary_matrix.md" "Slot is a resource boundary, not a bitstring"
require_term "docs/145_bit_layout_boundary_matrix.md" "Language Comparison And Pergyra Gaps"
require_term "docs/145_bit_layout_boundary_matrix.md" "Zig"
require_term "docs/145_bit_layout_boundary_matrix.md" "Rust"
require_term "docs/145_bit_layout_boundary_matrix.md" "C#"
require_term "docs/145_bit_layout_boundary_matrix.md" "WebAssembly"

echo "[abi-ownership-shape] Slot/Pin ABI shape, cleanup, and docs contract are gated"
