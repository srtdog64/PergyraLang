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
    "src/compiler/mir.h" \
    "src/compiler/mir_lower_population.c" \
    "src/codegen/llvm_runtime.c" \
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
reject_term "src/compiler/mir_abi_layout.c" "Slot<Int>_rel"
reject_term "src/compiler/mir_abi_layout.c" 'mir_abi_format_owned("%s_rel"'
reject_term "src/compiler/mir_abi_layout.c" "mir_abi_lookup_runtime_fmt"
reject_term "src/compiler/mir_abi_layout.c" "mir_extract_inner_type_suffix_owned"
reject_term "src/compiler/mir_abi_layout.c" "runtime function name pattern"
require_term "src/compiler/mir_abi_layout.c" "Runtime function spelling is payload carried"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_fn"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_fn_by_type_name"
require_term "src/compiler/mir_abi_layout.c" "MIRResourceRuntimeFnRow"
require_term "src/runtime/pgy_abi_spec.h" "allocator provenance as a fourth field"
require_term "src/compiler/mir_abi_layout.c" 'ABI_TYPE("Array<Long>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("allocator", pgy_abi_array_int, allocator)'
reject_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("len", pgy_abi_array_int, len)'
reject_term "src/compiler/mir_abi_layout.c" 'ABI_FIELD_STRUCT("cap", pgy_abi_array_int, cap)'
require_term "src/compiler/mir_abi_layout.c" 'ABI_RESOURCE_OPS("Slot<Int>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_RESOURCE_OPS("SecureSlot<Int>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_RESOURCE_OPS("SecureSlot<Long>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_PIN_OPS("Slot<Int>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_PIN_OPS("SecureSlot<Int>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_RESOURCE_OPS("DeviceSlot<Int>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_RESOURCE_OPS("DeviceSlot<Long>"'
require_term "src/compiler/mir_abi_layout.c" 'ABI_RESOURCE_OP("DeviceSlot<Int>", "SubmitRead"'
require_term "src/codegen/transpiler_mir_resource_op_core.c" "mir_abi_resource_runtime_fn(effective_layout, op_name)"
reject_term "src/codegen/transpiler_mir_resource_op_core.c" "transpiler_format_slot_runtime_fn"
require_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Claim\")"
require_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Read\")"
require_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Write\")"
require_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Release\")"
require_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name,"
require_term "src/codegen/llvm_runtime.c" '"PinRead"'
require_term "src/codegen/llvm_runtime.c" '"PinWrite"'
require_term "src/codegen/llvm_runtime.c" '"PinReadInit"'
require_term "src/codegen/llvm_runtime.c" '"PinWriteInit"'
require_term "src/codegen/llvm_runtime.c" '"Unpin"'
require_term "src/compiler/mir_abi_layout.c" '"UnpinCleanup"'
reject_term "src/codegen/llvm_runtime.c" "llvm_runtime_slot_name"
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "read", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "write", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release", suffix)'
require_term "src/codegen/llvm_runtime.c" "mir_abi_resource_runtime_fn_by_type_name(device_abi_type_name,"
require_term "src/codegen/llvm_runtime.c" '"SubmitRead"'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim_device", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_read", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_write", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release_device", suffix)'
reject_term "src/codegen/llvm_runtime.c" 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "submit_device_read", suffix)'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Claim\")"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Read\")"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Write\")"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name, \"Release\")"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" "mir_abi_resource_runtime_fn_by_type_name(abi_type_name,"
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinRead"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinWrite"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinReadInit"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"PinWriteInit"'
require_term "src/codegen/llvm_runtime_secure_slot_decl.c" '"Unpin"'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" "llvm_runtime_secure_slot_name"
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "claim_secure", suf)'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_read", suf)'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_write", suf)'
reject_term "src/codegen/llvm_runtime_secure_slot_decl.c" 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_release", suf)'
require_term "src/compiler/mir_abi_layout.h" "MIRResourceAbiKind"
require_term "src/compiler/mir_abi_layout.h" "mir_abi_resource_runtime_fn_by_kind"
require_term "src/codegen/llvm_expr_slot_device_calls.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/llvm_expr_slot_device_calls.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_expr_slot_device_calls.c" "MIR_RESOURCE_ABI_DEVICE_SLOT"
require_term "src/codegen/llvm_expr_identifier_slot_helpers.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/llvm_expr_identifier_slot_helpers.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_expr_call_methods_domain_slice.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/llvm_expr_call_methods_domain_slice.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_expr_assignment_member_projection.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/llvm_expr_assignment_member_projection.c" "MIR_RESOURCE_ABI_SECURE_SLOT"
require_term "src/codegen/llvm_stmt_let_resources.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/llvm_stmt_with.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/llvm_stmt.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_slot_builtin_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_slot_builtin_emit.c" "C source slot builtin %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_slot_runtime_row.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_slot_runtime_row.c" "C expression slot %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_slot_runtime_row.c" "transpiler_emit_nominal_container_runtime_rows"
require_term "src/codegen/transpiler_slot_runtime_row.c" "PGY_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "PGY_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "PGY_SECURE_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_class_decl_emit.c" "PGY_BOX_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_generic_class_specialization_emit.c" "PGY_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_generic_class_specialization_emit.c" "PGY_SECURE_SLOT_DEFINE(%s, %s)"
reject_term "src/codegen/transpiler_generic_class_specialization_emit.c" "PGY_BOX_DEFINE(%s, %s)"
require_term "src/codegen/transpiler_expr_call_member_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_expr_call_member_emit.c" "C slot method %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_let_slot_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_let_slot_emit.c" "C let-slot %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_func_class_flow_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_func_class_flow_emit.c" "C source with-slot %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_mir_destructure_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_mir_destructure_emit.c" "C MIR destructuring %s requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_class_decl_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_class_decl_emit.c" "C class field slot Claim requires MIR ABI runtime function row"
require_term "src/codegen/transpiler_expr_stdlib_builtin.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_expr_stdlib_builtin.c" "C stdlib Slot<T> Clone %s requires MIR ABI runtime function row"
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
require_term "src/compiler/mir.h" "MIR_ABI_REPR_EXPLICIT_TAG"
require_term "src/compiler/mir.h" "MIR_ABI_REPR_NICHE_RESERVED"
require_term "src/compiler/mir.h" "niche_none_pattern"
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
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR_ABI_REPR_EXPLICIT_TAG"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_lookup(\"Option<Float>\")"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "mir_abi_lookup(\"Option<Double>\")"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "niche_none_pattern == NULL"
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
require_term "src/runtime/pgy_runtime_plain_slot_inline.h" "pgy_unpin_cleanup_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "PgyPinnedSecureSlotView_##SuffixName"
require_term "src/runtime/pgy_runtime_slot_macros.h" "pgy_secure_unpin_cleanup_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_unpin_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_pin_read_init_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_intent_slot_core_exports.h" "pgy_pin_write_init_##SuffixName"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_unpin_##Suffix"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_pin_read_init_##Suffix"
require_term "src/runtime/pgy_runtime_lib_secure_slot_exports.h" "pgy_secure_pin_write_init_##Suffix"

require_term "src/codegen/transpiler_mir_pin_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_mir_pin_emit.c" '"PinRead"'
require_term "src/codegen/transpiler_mir_pin_emit.c" '"PinWrite"'
require_term "src/codegen/transpiler_mir_pin_emit.c" '"Unpin"'
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_pin_%s_%s"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_secure_pin_%s_%s"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_unpin_%s(&%s);"
reject_term "src/codegen/transpiler_mir_pin_emit.c" "pgy_secure_unpin_%s(&%s);"
require_term "src/compiler/mir.h" "resource_owner_slot_anchor"
require_term "src/compiler/mir.h" "resource_owner_requires_metadata"
require_term "src/compiler/mir_lower_population.c" "MIRResourceBorrowLoweringFact"
require_term "src/compiler/mir_lower_population.c" "mir_resource_record_borrow_fact"
require_term "src/compiler/mir_lower_population.c" "inst.resource_owner_requires_metadata = resource_owner_slot_anchor != NULL"
require_term "src/compiler/mir_fact_surface_validate.c" "inst->resource_owner_requires_metadata"
require_term "src/compiler/mir_fact_surface_validate.c" "return inst->arg1 != NULL && strcmp(inst->arg1, view_name) == 0"
require_term "src/compiler/mir_fact_surface_validate.c" "view-backed resource op is missing owner slot ABI metadata"
require_term "src/tests/mir/test_mir_lowering_part_a_1.cases.h" "MIR validator rejects view-backed resource owner metadata drift"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "inst->resource_owner_slot_anchor"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "inst->resource_owner_requires_metadata"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "expected_owner_slot"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "TypedVarEntry *view_entry = mir_active"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "strcmp(view_source_slot, expected_owner_slot) != 0"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "&& !mir_active"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "source_layout == NULL && !mir_active"
require_term "src/codegen/transpiler_mir_resource_hook_emit.c" "MIR view-backed resource op '%s' is missing owner slot ABI metadata"
require_term "src/codegen/llvm_internal_api.h" "bool          llvm_mir_emit_borrow_view_alias"
require_term "src/codegen/llvm_mir_block_emit.c" "if (!llvm_mir_emit_borrow_view_alias(inst, ctx))"
require_term "src/codegen/llvm_mir_resource_view.c" "inst->resource_owner_slot_anchor"
require_term "src/codegen/llvm_mir_resource_view.c" "inst->resource_owner_requires_metadata"
require_term "src/codegen/llvm_mir_resource_view.c" "LLVM MIR borrow view alias '%s' is missing owner slot ABI metadata"
require_term "src/codegen/transpiler_block_emit.c" "__attribute__((cleanup(%s)))"
require_term "src/codegen/transpiler_block_emit.c" "mir_abi_resource_runtime_fn_by_kind("
require_term "src/codegen/transpiler_block_emit.c" '"PinRead"'
require_term "src/codegen/transpiler_block_emit.c" '"PinWrite"'
require_term "src/codegen/transpiler_block_emit.c" '"UnpinCleanup"'
require_term "src/codegen/transpiler_block_emit.c" '"Release"'
require_term "src/codegen/transpiler_block_emit.c" "C source slot auto-release requires MIR ABI runtime function row"
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
require_term "src/codegen/llvm_mir_pin_region.c" '"PinReadInit"'
require_term "src/codegen/llvm_mir_pin_region.c" '"PinWriteInit"'
require_term "src/codegen/llvm_mir_pin_region.c" "llvm_mir_emit_plain_pin_inline_exit"
require_term "src/codegen/llvm_mir_pin_region.c" '"Unpin"'
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
