#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-backend-fail-closed.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

fail_if_nonempty() {
    local label="$1"
    local path="$2"

    if [[ -s "$path" ]]; then
        echo "[backend-fail-closed] $label" >&2
        cat "$path" >&2
        exit 1
    fi
}

c_zero="$WORK_DIR/c-zero.txt"
grep -R -n --include='*.c' --include='*.h' \
    'return pergyra_strdup("0")' "$ROOT_DIR/src/codegen" \
    | grep -Fv 'src/codegen/transpiler_let_box_emit.c:' \
    > "$c_zero" || true
fail_if_nonempty "C backend reintroduced silent numeric fallback" "$c_zero"

c_false="$WORK_DIR/c-false.txt"
grep -R -n --include='*.c' --include='*.h' \
    'return pergyra_strdup("false")' "$ROOT_DIR/src/codegen" \
    > "$c_false" || true
fail_if_nonempty "C backend reintroduced silent boolean fallback" "$c_false"

c_comment="$WORK_DIR/c-comment.txt"
grep -R -n --include='*.c' --include='*.h' \
    -F 'return pergyra_strdup("/*' "$ROOT_DIR/src/codegen" \
    > "$c_comment" || true
fail_if_nonempty "C backend reintroduced comment-only fallback" "$c_comment"

c_empty_expr="$WORK_DIR/c-empty-expression.txt"
grep -n -F 'return pergyra_strdup("")' \
    "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c" \
    "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c" \
    "$ROOT_DIR/src/codegen/transpiler_symbols.c" \
    "$ROOT_DIR/src/codegen/transpiler_type_render.c" \
    > "$c_empty_expr" || true
fail_if_nonempty "C backend reintroduced empty generated-expression fallback" "$c_empty_expr"

c_format_fallback="$WORK_DIR/c-format-fallback.txt"
awk '/strdup_fmt\(const char \*fmt, \.\.\.\)/,/^}/ { print }' \
    "$ROOT_DIR/src/codegen/transpiler_format.c" \
    | grep -n -F 'pergyra_strdup("")' \
    > "$c_format_fallback" || true
fail_if_nonempty "C format owner reintroduced empty formatting fallback" \
    "$c_format_fallback"

c_missing_type="$WORK_DIR/c-missing-type-fallback.txt"
{
    grep -n -F 'copy_capped_string(out, out_size, "int32_t")' \
        "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" || true
    grep -n -F 'return "int32_t";' \
        "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" || true
} > "$c_missing_type"
fail_if_nonempty "C type mapping reintroduced missing-type int32 fallback" \
    "$c_missing_type"
grep -Fq "missing primitive type name fails closed" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "pergyra_type_to_c_copy fails closed on missing type name" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "missing AST type name render fails closed" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "malformed AST type name render fails closed" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "slot_ref_expr fails closed on missing slot expression" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "C backend slot reference requires a lowered slot expression" \
    "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "C backend slot registry exceeded MAX_SLOT_VARS" \
    "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "C backend typed registry exceeded MAX_SLOT_VARS" \
    "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "mir_source_local_expr_type_name(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "llvm_mir_local_expected_type_name(" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
if grep -F "Option<Int>" \
    "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c" >/dev/null; then
    echo "[backend-fail-closed] assignment expected type must not default to Option<Int>" >&2
    exit 1
fi
grep -Fq "C backend alias registry exceeded MAX_ALIAS_VARS" \
    "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "C backend collection specialization registry exceeded MAX_COLLECTION_SPECIALIZATIONS" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "C backend generic function specialization registry exceeded MAX_GENERIC_SPECIALIZATIONS" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c"
grep -Fq "C backend generic class specialization registry exceeded MAX_GENERIC_CLASS_SPECIALIZATIONS" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "C backend generic binding registry exceeded MAX_GENERIC_BINDINGS" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c"
grep -Fq "C backend generic binding registry exceeded MAX_GENERIC_BINDINGS" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "C backend generic ability binding registry exceeded MAX_GENERIC_BINDINGS" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c"
grep -Fq "C backend ability vtable specialization registry exceeded MAX_ABILITY_VTABLE_SPECIALIZATIONS" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c"
grep -Fq "generated specialization name is too long while lowering %s" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "PGY_LIST_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "PGY_QUEUE_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "PGY_HASHMAP_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "PGY_SET_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
if grep -F "PGY_LIST_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c" \
        >/dev/null; then
    echo "[backend-fail-closed] List runtime rows must be emitted by transpiler_specialization_registry" >&2
    exit 1
fi
if grep -F "PGY_QUEUE_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c" \
        >/dev/null; then
    echo "[backend-fail-closed] Queue runtime rows must be emitted by transpiler_specialization_registry" >&2
    exit 1
fi
if grep -F "PGY_HASHMAP_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c" \
        >/dev/null; then
    echo "[backend-fail-closed] HashMap runtime rows must be emitted by transpiler_specialization_registry" >&2
    exit 1
fi
if grep -F "PGY_SET_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c" \
        >/dev/null; then
    echo "[backend-fail-closed] Set runtime rows must be emitted by transpiler_specialization_registry" >&2
    exit 1
fi
if grep -F "PGY_SET_VALUES_DEFINE(%s, %s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c" \
        >/dev/null; then
    echo "[backend-fail-closed] Set values runtime rows must be emitted by transpiler_specialization_registry" >&2
    exit 1
fi
grep -Fq "C backend loop registry exceeded TRANSPILE_MAX_LOOP_DEPTH" \
    "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq "C backend defer registry exceeded TRANSPILE_MAX_DEFER_PER_SCOPE" \
    "$ROOT_DIR/src/codegen/transpiler_defer_emit.c"
grep -Fq "LLVM loop registry exceeded MAX_SCOPE_DEPTH" \
    "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM defer registry exceeded MAX_DEFER_PER_SCOPE" \
    "$ROOT_DIR/src/codegen/llvm_stmt_defer_scope.c"
grep -Fq "event handler capacity exceeded" \
    "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "event handler capacity exceeded" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "event.subscribe.overflow" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "C backend: slot builtin expression formatting failed" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "C backend: slot builtin expression allocation failed" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "transpiler_slot_runtime_row_for_source_operation(" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "C source slot builtin %s requires MIR ABI runtime function row" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "MIR resource op '%s' is missing runtime ABI layout metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "C MIR resource op '%s' is missing its lowered ABI layout fact" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "C MIR resource op '%s' carries a missing or mismatched ABI layout identity" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "C MIR source operation has no active instruction-owned runtime-call ABI row" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "C MIR source operation has a missing or mismatched ABI layout identity" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "LLVM MIR source operation has no active instruction-owned runtime-call ABI row" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "LLVM MIR source operation has a missing or mismatched ABI layout identity" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "mir_abi_resource_runtime_row_by_type_name(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "mir_abi_resource_runtime_instruction_for_abi(" \
    "$ROOT_DIR/src/compiler/mir_abi_resource_runtime_mir.c"
mir_mir_row_helper="$(awk '
    /^mir_abi_resource_runtime_row_for_mir_abi\(/ { inside = 1 }
    inside { print }
    inside && /^}/ { exit }
' "$ROOT_DIR/src/compiler/mir_abi_resource_runtime_mir.c")"
if grep -F 'mir_abi_resource_runtime_row_by_type_name(' <<<"$mir_mir_row_helper" >/dev/null \
    || grep -F 'mir_abi_resource_runtime_row_for_type_name(' <<<"$mir_mir_row_helper" >/dev/null; then
    echo "[backend-fail-closed] active MIR runtime-row helper must not rebuild a row from the global ABI table" >&2
    exit 1
fi
if grep -R -n --include='*.c' --include='*.h' \
    'mir_abi_resource_runtime_pin_row_for_mir(' \
    "$ROOT_DIR/src/codegen" >/dev/null; then
    echo "[backend-fail-closed] backend pin consumers must use instruction-owned MIR rows" >&2
    exit 1
fi
grep -Fq "runtime_row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "inst->resource_runtime_fact_present" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "C MIR resource op '%s' is missing its lowered runtime-call ABI row" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
if grep -E 'transpiler_mir_find_prior_(borrow_source_for_view|resource_layout_for_slot)|transpiler_mir_layout_from_type_annotation' \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c" >/dev/null; then
    echo "[backend-fail-closed] active C MIR view hooks must consume carried owner facts, not inventory or AST recovery" >&2
    exit 1
fi
grep -Fq "inst->resource_owner_slot_anchor" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "if (!mir_active && fn == NULL" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "resource_runtime_fact_present" \
    "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "resource op is missing lowered runtime-call ABI row fact" \
    "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "inst->resource_runtime_fact_present" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "LLVM MIR resource operation is missing its lowered runtime-call ABI row" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "mir_abi_resource_runtime_row_matches_owner(row)" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "mir_abi_resource_runtime_row_matches_owner(row)" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq '"returns_container"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"container_ptr_to_value"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"container_ptr_value_to_void"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"container_ptr_to_void"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"PinRead"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"PinWrite"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"PinReadInit"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"PinWriteInit"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"Unpin"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
if grep -F 'llvm_runtime_slot_name' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot runtime declarations must not synthesize runtime function names locally" >&2
    exit 1
fi
if grep -F 'mir_abi_resource_runtime_fn_by_type_name(' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot runtime declarations must consume runtime ABI row records, not symbol-only accessors" >&2
    exit 1
fi
if grep -F 'mir_abi_resource_runtime_fn_by_type_name(' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM runtime-row selection must consume row records, not symbol-only accessors" >&2
    exit 1
fi
llvm_direct_row_consumers="$({
    grep -RIlE --include='llvm_*.c' \
        'mir_abi_resource_runtime_row_by_(kind|type_name)\(' \
        "$ROOT_DIR/src/codegen" || true
} | grep -v '/llvm_runtime_row.c$' || true)"
if [[ -n "$llvm_direct_row_consumers" ]]; then
    echo "[backend-fail-closed] LLVM runtime-row consumers must enter through llvm_slot_runtime_row_for_operation:" >&2
    printf '%s\n' "$llvm_direct_row_consumers" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot claim declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "read", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot read declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "write", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot write declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot release declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
grep -Fq "device_abi_type_name" \
    "$ROOT_DIR/src/codegen/llvm_runtime.c"
grep -Fq '"SubmitRead"' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c"
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim_device", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM device slot claim declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_read", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM device slot read declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_write", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM device slot write declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release_device", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM device slot release declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_slot_name(fn_name, sizeof(fn_name), "submit_device_read", suffix)' \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM device slot submit-read declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
grep -Fq '"token_ptr_to_container"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"container_ptr_token_ptr_to_value"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"container_ptr_value_token_ptr_to_void"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"container_ptr_token_ptr_to_void"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq '"PinRead"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
grep -Fq '"PinWrite"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
grep -Fq '"PinReadInit"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
grep -Fq '"PinWriteInit"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
grep -Fq '"Unpin"' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c"
if grep -F 'llvm_runtime_secure_slot_name' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot runtime declarations must not synthesize runtime function names locally" >&2
    exit 1
fi
if grep -F 'mir_abi_resource_runtime_fn_by_type_name(' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot runtime declarations must consume runtime ABI row records, not symbol-only accessors" >&2
    exit 1
fi
if grep -F 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "claim_secure", suf)' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot claim declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_read", suf)' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot read declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_write", suf)' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot write declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
if grep -F 'llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_release", suf)' \
    "$ROOT_DIR/src/codegen/llvm_runtime_secure_slot_decl.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot release declaration must consume MIR ABI runtime function rows" >&2
    exit 1
fi
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "MIR_RESOURCE_ABI_SECURE_SLOT" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "MIR_RESOURCE_ABI_DEVICE_SLOT" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "MIR_RESOURCE_ABI_SECURE_SLOT" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "MIR_RESOURCE_ABI_SECURE_SLOT" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "MIR_RESOURCE_ABI_SECURE_SLOT" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_stmt_block.c"
if grep -F 'is_secure ? "pgy_secure_read_%s" : "pgy_read_%s"' \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot identifier auto-read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'llvm_domain_slot_format_runtime_name' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot method calls must not synthesize runtime function names locally" >&2
    exit 1
fi
if grep -F 'llvm_stmt_format_runtime_name' \
    "$ROOT_DIR/src/codegen/llvm_stmt.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM statement auto-release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_release_" : "pgy_release_"' \
    "$ROOT_DIR/src/codegen/llvm_stmt.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM statement auto-release must not synthesize release function names locally" >&2
    exit 1
fi
if grep -F '"pgy_secure_write", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot method Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_write", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot method Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_secure_read", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot method Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_read", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot method Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_secure_release", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure slot method Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_release", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot method Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_write_%s" : "pgy_write_%s"' \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot assignment must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_write_%s" : "pgy_write_%s"' \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_names.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot initializer must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_release_%s" : "pgy_release_%s"' \
    "$ROOT_DIR/src/codegen/llvm_stmt_with.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM with-slot cleanup must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_write" : "pgy_write"' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot Write call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_read" : "pgy_read"' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot Read call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'is_secure ? "pgy_secure_release" : "pgy_release"' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot Release call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_device_write", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM DeviceWrite call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_device_read", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM DeviceRead call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_release_device", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM ReleaseDeviceSlot call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F '"pgy_submit_device_read", inner' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM SubmitDeviceRead call emission must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'llvm_slot_format_runtime_name' \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM slot/device call emission must not synthesize runtime function names locally" >&2
    exit 1
fi
if grep -F 'transpiler_format_slot_runtime_fn(' \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR resource op must consume MIR ABI runtime function rows" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq "transpiler_slot_runtime_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq '"PinRead"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq '"PinWrite"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq '"Unpin"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR pin enter/cleanup must consume MIR ABI runtime row records" >&2
    exit 1
fi
if grep -F "transpiler_mir_pin_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR pin call shape must be owned by slot runtime row owner" >&2
    exit 1
fi
if grep -F 'pgy_pin_%s_%s' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR pin enter must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_pin_%s_%s' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR secure pin enter must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_unpin_%s(&%s);' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR pin cleanup must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_unpin_%s(&%s);' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR secure pin cleanup must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "transpiler_slot_runtime_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"PinRead"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"PinWrite"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"UnpinCleanup"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"Release"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "C source slot auto-release requires MIR ABI runtime function row" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source pin/auto-release must consume MIR ABI runtime row records" >&2
    exit 1
fi
if grep -F 'pgy_pin_%s_%s' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source pin enter must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_pin_%s_%s' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure pin enter must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'cleanup(pgy_unpin_cleanup_%s)' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source pin cleanup attribute must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'cleanup(pgy_secure_unpin_cleanup_%s)' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure pin cleanup attribute must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F "transpiler_block_pin_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source pin call shape must be owned by slot runtime row owner" >&2
    exit 1
fi
if grep -F 'pgy_release_%s(&%s);' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source auto-release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_release_%s(&%s, &%s_token);' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure auto-release must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "llvm_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq '"PinReadInit"' \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq '"PinWriteInit"' \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq '"Unpin"' \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM MIR pin-region emission must consume MIR ABI runtime row records" >&2
    exit 1
fi
if grep -F 'pgy_secure_pin_%s_init_%s' \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure pin enter must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_unpin_%s' \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM secure pin cleanup must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'llvm_mir_unpin_name' \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM pin cleanup must not synthesize runtime function names locally" >&2
    exit 1
fi
grep -Fq "if (fn == NULL)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "if (mir_active)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "slot_anchor != NULL && !mir_active" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
if grep -F 'slot_builtin_strdup_fmt(const char *fmt' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] slot builtin formatter lost backend diagnostics" >&2
    exit 1
fi
grep -Fq "mir_abi_resource_runtime_row_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "C slot operation %s requires MIR ABI runtime function row" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "transpiler_emit_nominal_container_runtime_rows" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "PGY_SLOT_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
if grep -F "PGY_SLOT_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c" \
        >/dev/null; then
    echo "[backend-fail-closed] nominal container runtime rows must be emitted by transpiler_slot_runtime_row" >&2
    exit 1
fi
if grep -F "PGY_SECURE_SLOT_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c" \
        >/dev/null; then
    echo "[backend-fail-closed] nominal secure-slot runtime rows must be emitted by transpiler_slot_runtime_row" >&2
    exit 1
fi
if grep -F "PGY_BOX_DEFINE(%s, %s)" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c" \
        >/dev/null; then
    echo "[backend-fail-closed] nominal box runtime rows must be emitted by transpiler_slot_runtime_row" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_fn(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C slot methods must consume transpiler_slot_runtime_row" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_row_for_operation(" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "C let-slot %s requires MIR ABI runtime function row" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
if grep -F 'pgy_write_%s(%s, %s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source slot Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_write_%s(%s, %s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure slot Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_read_%s(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source slot Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_read_%s(%s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure slot Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_release_%s(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source slot Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_release_%s(%s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure slot Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_device_write_%s(&%s, %s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source DeviceWrite must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_device_read_%s(&%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source DeviceRead must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_release_device_%s(&%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source ReleaseDeviceSlot must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_submit_device_read_%s(&%s)' \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source SubmitDeviceRead must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_write_%s(%s, %s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C expression slot Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_write_%s(%s, %s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C expression secure slot Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_read_%s(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C expression slot Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_read_%s(%s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C expression secure slot Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_read_%s(&%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C expression SSA slot Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_read_%s(&%s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C expression secure SSA slot Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_write_%s(%s, %s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C slot method Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_write_%s(%s, %s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C secure slot method Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_read_%s(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C slot method Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_read_%s(%s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C secure slot method Read must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_release_%s(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C slot method Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_release_%s(%s, &%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C secure slot method Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_claim_%s()' \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C let-slot Claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_claim_secure_%s(&%s_token)' \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C let secure-slot Claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_fn(" \
    "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source with-slot must consume transpiler_slot_runtime_row" >&2
    exit 1
fi
if grep -F 'pgy_claim_%s()' \
    "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source with-slot Claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_claim_secure_%s(&%s_token)' \
    "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure with-slot Claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_fn(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR destructuring must consume transpiler_slot_runtime_row" >&2
    exit 1
fi
if grep -F 'pgy_claim_%s()' \
    "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR ClaimSlot destructuring must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_claim_secure_%s(&%s)' \
    "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR ClaimSecureSlot destructuring must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_fn(" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C class field slot claims must consume transpiler_slot_runtime_row" >&2
    exit 1
fi
if grep -F 'pgy_claim_%s()' \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C class field Slot claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_claim_secure_%s(&self.%s)' \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C class field SecureSlot claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "transpiler_slot_runtime_fn(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
if grep -F "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c" >/dev/null; then
    echo "[backend-fail-closed] C stdlib Slot<T> Clone must consume transpiler_slot_runtime_row" >&2
    exit 1
fi
if grep -F 'PgySlot_%s _c = pgy_claim_%s()' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c" >/dev/null; then
    echo "[backend-fail-closed] C stdlib Slot<T> Clone claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_write_%s(&_c, pgy_read_%s(&%s))' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c" >/dev/null; then
    echo "[backend-fail-closed] C stdlib Slot<T> Clone read/write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_release_%s(&%s);' \
    "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source with-slot Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_release_%s(&%s, &%s_token);' \
    "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C source secure with-slot Release must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_claim_device_%s()' \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C let device-slot Claim must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_write_%s(&%s, %s)' \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C let-slot initializer Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
if grep -F 'pgy_secure_write_%s(&%s, %s, &%s_token)' \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C let secure-slot initializer Write must consume MIR ABI runtime rows" >&2
    exit 1
fi
grep -Fq "typed declarator fails closed on malformed AST type" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "function signature declarator fails closed on malformed return type" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "event handler declarator fails closed on malformed parameter type" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "event handler type-to-C copy requires declarator owner" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "type fact policy rejects Unknown sentinel without rejecting names" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "let type registry skips inferred Unknown facts" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "let type registry skips generic Unknown sentinel wrappers" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_0.cases.h"
grep -Fq "transpiler_type_name_is_concrete_fact" \
    "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_contains_unknown_sentinel" \
    "$ROOT_DIR/src/codegen/llvm_type.c"
grep -Fq "C backend declarator cannot render C type" \
    "$ROOT_DIR/src/codegen/transpiler_type_declarator.c"
grep -Fq "C backend function type requires declarator owner" \
    "$ROOT_DIR/src/codegen/transpiler_type_render.c"
if grep -F 'register_typed_var(ctx, name, infer_expression_type_name(ctx, init));' \
    "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c" >/dev/null; then
    echo "[backend-fail-closed] let type registry reintroduced inferred Unknown fact registration" >&2
    exit 1
fi
if grep -F 'pgy_result_type_ident_char' \
    "$ROOT_DIR/src/codegen/llvm_type.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM Result layout reintroduced local Unknown token policy" >&2
    exit 1
fi
if grep -n -F 'ast_func_return_type(ctx->current_func_decl)' \
    "$ROOT_DIR/src/codegen/llvm_stmt.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c" \
    "$ROOT_DIR/src/codegen/llvm_type.c" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM return lowering reintroduced AST return-type fallback" >&2
    exit 1
fi
if grep -n -F 'ast_func_return_type((ASTNode *)ctx->current_func_decl)' \
    "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c" >/dev/null; then
    echo "[backend-fail-closed] C return callable context reintroduced AST current-func fallback" >&2
    exit 1
fi
grep -Fq "current_function_ret_type" "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c"
grep -Fq "current_return_type_name" "$ROOT_DIR/src/codegen/llvm_type.c"
grep -Fq "current_return_callable_type" "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "current_return_callable_type" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "current_return_callable_type" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "MIR-only C path missing function signature return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_signature.c"
grep -Fq "MIR-only C path missing function signature parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_signature.c"
grep -Fq "MIR-only C path missing function forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"
grep -Fq "MIR-only C path missing function forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"
grep -Fq "MIR-only C path missing function forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
grep -Fq "MIR-only C path missing function forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
grep -Fq "MIR-only C path missing hosted method forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_metadata.c"
grep -Fq "MIR-only C path missing hosted method forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_metadata.c"
for rel in \
    src/codegen/transpiler_class_decl_emit.c \
    src/codegen/transpiler_enum_decl_emit.c \
    src/codegen/transpiler_generic_class_specialization_emit.c \
    src/codegen/transpiler_domain_nominal_emit.c \
    src/codegen/transpiler_roster_decl_emit.c \
    src/codegen/transpiler_relation_effect_emit.c \
    src/codegen/transpiler_world_select_event_emit.c \
    src/codegen/transpiler_zone_methods_emit.c; do
    grep -Fq "MIR-only C path missing hosted method forward metadata row" \
        "$ROOT_DIR/$rel"
done
for rel in \
    src/codegen/transpiler_class_decl_emit.c \
    src/codegen/transpiler_enum_decl_emit.c \
    src/codegen/transpiler_generic_class_specialization_emit.c; do
    grep -Fq "MIR-only C path missing method body metadata row" \
        "$ROOT_DIR/$rel"
done
grep -Fq "MIR-only C path missing method body metadata row for" \
    "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c"
grep -Fq "MIR-only C path missing method name metadata for" \
    "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c"
grep -Fq "MIR-only C path missing role operator method metadata for role" \
    "$ROOT_DIR/src/codegen/transpiler_operator.c"
grep -Fq "MIR-only LLVM path missing role operator method metadata for role" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only C path missing function body return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "MIR-only C path missing function body parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "MIR-only LLVM path missing function forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_backend_forward_declare.c"
grep -Fq "MIR-only LLVM path missing function forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_backend_forward_declare.c"
grep -Fq "MIR-only LLVM path missing function declaration return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
grep -Fq "MIR-only LLVM path missing function declaration parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
if grep -Fq "llvm_decl_required_param_type_name_first" \
        "$ROOT_DIR/src/codegen/llvm_decl.c"; then
    echo "[backend-fail-closed] active MIR LLVM declaration reintroduced AST parameter type recovery" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM path missing function body return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "MIR-only LLVM path missing function body parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "MIR-only LLVM path missing function parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
for llvm_mir_param_owner in \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"; do
    if grep -E 'llvm_mir_required_type_from_ast|llvm_mir_param_uses_pointer_self' \
            "$llvm_mir_param_owner" >/dev/null; then
        echo "[backend-fail-closed] active MIR parameter ABI reintroduced AST type recovery: $llvm_mir_param_owner" >&2
        exit 1
    fi
done
grep -Fq "MIR-only LLVM path missing array return inference routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq "MIR-only LLVM path missing array return inference signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq "MIR-only LLVM path missing array return inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
if grep -n -F 'ast_func_within_zone(ctx->current_func_decl)' \
    "$ROOT_DIR/src/codegen/llvm_decl_authority.c" \
    "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM zone boundary lookup reintroduced AST current-func fallback" >&2
    exit 1
fi
if grep -n -F 'ast_func_within_zone(func_decl)' \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM MIR emission reintroduced source-AST within-zone fallback" >&2
    exit 1
fi
grep -Fq "current_within_zone_name" "$ROOT_DIR/src/codegen/llvm_decl_authority.c"
grep -Fq "current_within_zone_name" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "llvm_mir_routine_within_zone(routine)" "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
for llvm_generated_owner in \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c" \
    "$ROOT_DIR/src/codegen/llvm_intent.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_helpers.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"; do
    grep -Fq "ctx->current_return_type_name = NULL;" \
        "$llvm_generated_owner" || {
        echo "[backend-fail-closed] LLVM generated function owner does not clear source return metadata: $llvm_generated_owner" >&2
        exit 1
    }
done
if grep -E 'codebuf_write\([^,]+, "void \*"\)|: "int32_t"|declarator_strdup_fmt' \
    "$ROOT_DIR/src/codegen/transpiler_type_declarator.c" >/dev/null; then
    echo "[backend-fail-closed] C declarator owner reintroduced type fallback" >&2
    exit 1
fi
if grep -E 'pergyra_str_copy\(out, out_size, "void \*"\)|const char \*pt = "void\*"' \
    "$ROOT_DIR/src/codegen/transpiler_type_render.c" \
    "$ROOT_DIR/src/codegen/transpiler_event_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C backend reintroduced raw function-type fallback" >&2
    exit 1
fi
grep -Fq "MIR type rendering fails closed for missing or unsupported types" \
    "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_e.cases.h"
if grep -E 'pergyra_strdup\("Int"\)|pergyra_strdup\("Future<Int>"\)|pergyra_strdup\("Channel<Int>"\)|inner != NULL \? inner : "Int"' \
    "$ROOT_DIR/src/compiler/mir_type_helpers.c" >/dev/null; then
    echo "[backend-fail-closed] MIR type rendering reintroduced async/container Int fallback" >&2
    exit 1
fi
if grep -E 'rendered == NULL.*AST_TYPE|return ast_type_name\(value_type\)|type_name = ast_type_name\(value_type\)' \
    "$ROOT_DIR/src/compiler/mir_intent.c" >/dev/null; then
    echo "[backend-fail-closed] MIR intent metadata reintroduced outer-name type fallback" >&2
    exit 1
fi
if grep -E 'pergyra_strdup\("Int"\)|inner != NULL \? inner : "Int"|arg_text != NULL \? arg_text : "Int"|: "Int"' \
    "$ROOT_DIR/src/compiler/dir.c" >/dev/null; then
    echo "[backend-fail-closed] DIR type rendering reintroduced Int fallback" >&2
    exit 1
fi

grep -Fq "return node->data.async_func_decl.param_count" \
    "$ROOT_DIR/src/parser/ast_func_accessors.c"
grep -Fq "return node->data.async_func_decl.params" \
    "$ROOT_DIR/src/parser/ast_func_accessors.c"
grep -Fq "return node->data.async_func_decl.return_type" \
    "$ROOT_DIR/src/parser/ast_func_accessors.c"
if grep -F 'spawn_callable_param_at' \
    "$ROOT_DIR/src/semantic/type_checker_async_channel.c" >/dev/null; then
    echo "[backend-fail-closed] async spawn boundary reintroduced local callable parameter dispatch" >&2
    exit 1
fi
if grep -F 'ast_async_func_params' \
        "$ROOT_DIR/src/semantic/slot_analyzer_access.c" \
        "$ROOT_DIR/src/semantic/slot_analyzer_escape.c" >/dev/null; then
    echo "[backend-fail-closed] slot analyzer reintroduced async-specific parameter dispatch" >&2
    exit 1
fi

c_missing_ast_type="$WORK_DIR/c-missing-ast-type-render.txt"
{
    grep -n -F 'pergyra_strdup("Int")' \
        "$ROOT_DIR/src/codegen/transpiler_type_render.c" || true
    grep -n -F 'codebuf_write(buf, "Int")' \
        "$ROOT_DIR/src/codegen/transpiler_type_render.c" || true
} > "$c_missing_ast_type"
fail_if_nonempty "C type render reintroduced missing-AST Int fallback" \
    "$c_missing_ast_type"

llvm_i32_zero="$WORK_DIR/llvm-i32-zero.txt"
grep -R -n --include='llvm*.c' \
    -E 'return LLVMConstInt\(ctx->type_i32, 0, 0\)|result = LLVMConstInt\(ctx->type_i32, 0, 0\)|\*out = LLVMConstInt\(ctx->type_i32, 0, 0\)|\*out_result = LLVMConstInt\(ctx->type_i32, 0, 0\)' \
    "$ROOT_DIR/src/codegen" \
    | grep -Fv 'src/codegen/llvm_expr_emit_support.c:' \
    > "$llvm_i32_zero" || true
fail_if_nonempty "LLVM backend bypassed void placeholder owner" "$llvm_i32_zero"

llvm_branch_fallback="$WORK_DIR/llvm-branch-fallback.txt"
{
    grep -R -n --include='llvm*.c' \
        -F 'cond = LLVMConstInt(ctx->type_i1, 0, 0)' \
        "$ROOT_DIR/src/codegen" || true
    grep -n -F 'LLVMConstInt(LLVMInt1TypeInContext(' \
        "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" || true
} > "$llvm_branch_fallback"
fail_if_nonempty "LLVM backend reintroduced synthetic branch condition fallback" "$llvm_branch_fallback"

grep -Fq "llvm_stmt_require_non_void_value" \
    "$ROOT_DIR/src/codegen/llvm_stmt_emit_support.c"
grep -Fq "LLVM return statement cannot consume a Void expression value" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM MIR return cannot consume a Void expression value" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "has two successors without a branch condition terminator" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "transpiler_log_string_error" \
    "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
grep -Fq "LLVM host field access requires a self receiver" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "inner = llvm_lookup_slot_inner(ctx, source_name)" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
if grep -Fq "llvm_derive_slot_inner_from_current_decl" \
        "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c" \
        || grep -Fq "ast_func_param_count(current_decl)" \
        "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"; then
    echo "[backend-fail-closed] LLVM slot identifier resolution reintroduced AST parameter rescan" >&2
    exit 1
fi

host_lookup="$WORK_DIR/llvm-host-lookup.txt"
awk '/llvm_find_host_decl_in_active_inventory/,/^}/ { print }' \
    "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c" > "$host_lookup"
grep -Fq "pgy_host_decl_compat_types(&host_type_count)" "$host_lookup"
if grep -E 'AST_(CLASS|ENUM|PARTY|ROLE|ROSTER|RELATION|EFFECT|ZONE|WORLD)_DECL' \
    "$host_lookup" >/dev/null; then
    echo "[backend-fail-closed] LLVM host lookup reintroduced a partial host-kind chain" >&2
    cat "$host_lookup" >&2
    exit 1
fi
grep -Fq "pgy_host_decl_compat_is_type" \
    "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "AST_PARTY_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "AST_ROLE_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "AST_ROSTER_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "MIR-only LLVM path missing role declaration name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing method name metadata for role" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing role operator method name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing registered role operator method function" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing role vtable method name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing role vtable method source metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing role vtable method metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing role vtable ability-ref metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing method forward name metadata for role" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role method forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role method forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "llvm_mir_decl_method_routine" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "llvm_mir_decl_method_routine" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
if grep -Fq "llvm_domain_forward_required_param_type" \
        "$ROOT_DIR/src/codegen/llvm_domain_forward.c" \
        "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"; then
    echo "[backend-fail-closed] active MIR hosted forward declarations reintroduced AST parameter type recovery" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM path missing role forward declaration name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator receiver type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role subject type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c"
grep -Fq "MIR-only LLVM path missing constructor field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"
grep -Fq "MIR-only C path missing constructor field type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"
grep -Fq "transpiler_emit_ctor_arg_with_expected_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_class_constructor_emit.c"
grep -Fq "MIR-only C path missing class constructor field type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_class_constructor_emit.c"
grep -Fq "transpiler_emit_ctor_arg_from_field_abi" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
grep -Fq "MIR-only C path missing constructor field type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
if grep -Fq "transpiler_emit_ctor_arg_with_expected_type(ctx" \
        "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"; then
    echo "[backend-fail-closed] active MIR domain constructors reintroduced AST expected-type lowering" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM path missing class constructor field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "llvm_hosted_shared_field_view_type_name(view, i)" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "MIR-only LLVM path missing class constructor shared-field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "llvm_emit_constructor_field_arg(node, ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
if grep -Fq "llvm_emit_expression(initializer, ctx)" \
        "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    echo "[backend-fail-closed] LLVM constructor shared defaults reintroduced untyped initializer lowering" >&2
    exit 1
fi
if grep -Fq "field_type = field_meta != NULL" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c" \
    && ! grep -Fq "if (llvm_active_has_mir(ctx))" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"; then
    echo "[backend-fail-closed] LLVM class constructor type recovery lost its active-MIR guard" >&2
    exit 1
fi
grep -Fq "mir_decl_header_role_subject_type_name" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c"
grep -Fq "llvm_hosted_shared_field_view_type_name" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"
grep -Fq "llvm_domain_required_type_name" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"
grep -Fq "MIR-only LLVM path missing %s type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c"
grep -Fq "MIR-only LLVM path missing current-field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_lookup.c"
grep -Fq "MIR-only LLVM path missing current-field metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_lookup.c"
grep -Fq "MIR-only LLVM path missing active routine for runtime-call ABI row" \
    "$ROOT_DIR/src/codegen/llvm_runtime_row.c"
grep -Fq "MIR-only C path missing active routine for runtime-call ABI row" \
    "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "MIR-only LLVM path missing domain projection field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
grep -Fq "MIR-only LLVM path missing projection field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "MIR-only LLVM path missing projection field metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "MIR-only C path missing projection class-field type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
grep -Fq "MIR-only C path missing projection class-field metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
grep -Fq "MIR-only C path missing projection class-field type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_field_path.c"
grep -Fq "MIR-only C path missing nominal field type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"
if grep -Fq "llvm_role_for_type_name(ASTNode *role)" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_lookup.c"; then
    echo "legacy context-free role subject lookup remains" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM path missing domain method forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "MIR-only LLVM path missing domain method forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "MIR-only LLVM path missing method forward metadata row for domain" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "MIR-only LLVM path missing method body metadata row for domain" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"
grep -Fq "MIR-only LLVM path missing method body metadata row for role" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing member-call parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq "llvm_mir_routine_param_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_member_call_specialize.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_member_call_specialize.c"
grep -Fq "MIR-only LLVM path missing generic method parameter ABI fact" \
    "$ROOT_DIR/src/codegen/llvm_member_call_specialize.c"
if grep -E 'ast_type_to_llvm\(ctx, p->type\)|ast_func_param\(method_decl' \
        "$ROOT_DIR/src/codegen/llvm_member_call_specialize.c" >/dev/null; then
    echo "[backend-fail-closed] active MIR generic method specialization reintroduced AST ABI recovery" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM generic class specialization missing class header metadata" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "MIR-only LLVM generic class specialization missing field type metadata" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "llvm_mir_routine_param_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "MIR-only LLVM path missing generic function parameter ABI fact" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
if grep -Fq "llvm_spawn_required_param_type(ctx, generic_ast, p" \
        "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"; then
    echo "[backend-fail-closed] active MIR generic spawn reintroduced AST parameter ABI recovery" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM path missing member-call receiver type metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "closed instead of clearing the source-of-truth diagnostic" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "MIR-only LLVM path missing hosted self-call parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
grep -Fq "MIR-only LLVM path missing hosted self-call method metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
grep -Fq "MIR-only LLVM path missing boundary call routine" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "MIR-only LLVM path missing boundary call signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "MIR-only LLVM path missing boundary call parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_mir_routine_param_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "MIR-only LLVM path missing boundary call parameter ABI fact" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "MIR-only LLVM path missing match subject routine" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing match subject signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing match subject return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing match subject return ABI fact" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing method type inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_mir_decl_method_routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "MIR-only LLVM path missing method type inference return ABI fact" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "MIR-only LLVM path missing let method return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing declared return inference routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing declared return inference signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing declared return inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing spawn future inference routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing spawn future inference signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing spawn future inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing spawn future inference parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "MIR-only LLVM path missing callable let routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "MIR-only LLVM path missing callable let signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "MIR-only LLVM path missing callable let parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "MIR-only LLVM path missing callable let return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "MIR-only LLVM path missing callable call-return routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "MIR-only LLVM path missing callable call-return signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "MIR-only LLVM path missing enum method registry return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "MIR-only LLVM path missing enum method registry parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "MIR-only LLVM path missing class method registry return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "MIR-only LLVM path missing class method registry parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "llvm_hosted_field_view_type_name(&field_view, j)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "pergyra_type_to_llvm(ctx, field_type_name)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "MIR-only LLVM path missing class field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "llvm_find_decl_header_in_context_of_type(" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "AST_CLASS_DECL, cls_name" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "MIR-only LLVM path missing class declaration header" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "mir_decl_header_nominal_kind_or(class_header" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "mir_decl_header_uses_pointer_self(class_header)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "MIR-only LLVM path missing class field type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
if grep -Fq "ast_type_to_llvm(ctx, return_type)" \
        "$ROOT_DIR/src/codegen/llvm_register.c"; then
    echo "[backend-fail-closed] nominal method registration reintroduced AST return-type recovery" >&2
    exit 1
fi
if grep -Fq "llvm_register_required_ast_type(ctx, stmt" \
        "$ROOT_DIR/src/codegen/llvm_register.c"; then
    echo "[backend-fail-closed] nominal method registration reintroduced AST parameter-type recovery" >&2
    exit 1
fi
grep -Fq "MIR-only C path missing role method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role declaration name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role subject type metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role vtable method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role vtable method source metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role vtable method metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role vtable ability-ref metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing included role method metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_include_emit.c"
grep -Fq "MIR-only C path missing included role method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_include_emit.c"
grep -Fq "MIR-only C path missing included role method return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_include_emit.c"
grep -Fq "MIR-only C path missing included role method parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_include_emit.c"
grep -Fq "MIR-only C path missing role operator method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role operator return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role operator parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "role_override_method_count" \
    "$ROOT_DIR/src/compiler/mir_decl.h"
grep -Fq "ast_override_func_decl(impl)" \
    "$ROOT_DIR/src/compiler/mir_decl_headers.c"
grep -Fq "hir_append_hidden_method_routine" \
    "$ROOT_DIR/src/compiler/hir_routines.c"
if grep -Fq "MIR-only C path missing role override method metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"; then
    echo "retired role override AST-only fallback diagnostic remains" >&2
    exit 1
fi
grep -Fq "MIR-only C path missing member-call parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "MIR-only C path missing member-call return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "MIR-only C path missing user-call parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "MIR-only C path missing member-call inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "MIR-only C path missing function inference routine" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "MIR-only C path missing function inference signature metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "MIR-only C path missing function inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "MIR-only C path missing hosted self-call inference return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "MIR-only C path missing hosted self-call inference method metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "MIR-only C path missing callable let return routine" \
    "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "MIR-only C path missing callable let return signature metadata" \
    "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_find_host_method_metadata_in_context" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "host_method_meta == NULL" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "decl == NULL" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "MIR-only C path missing hosted self-call method metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "MIR-only C path missing MIR local member-call return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_find_mir_function(ctx, callee_decl)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
if grep -Fq "transpiler_find_active_function_routine_for_call" \
        "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"; then
    echo "[backend-fail-closed] C MIR local type lookup reintroduced local routine scan" >&2
    exit 1
fi
grep -Fq "MIR-only C path missing nominal member-call return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"
grep -Fq "MIR-only C path missing nominal function-call routine metadata" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"
grep -Fq "MIR-only C path missing nominal function-call signature metadata" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"
grep -Fq "MIR-only C path missing nominal function-call return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"
grep -Fq "MIR-only C path missing projection invalidation method metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_method_invalidation.c"
grep -Fq "MIR-only C path missing zone action within-zone metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"
grep -Fq "MIR-only C path missing world effect sync within-zone metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.c"
grep -Fq "MIR-only LLVM path missing zone action within-zone metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"
grep -Fq "MIR-only LLVM path missing world effect sync within-zone metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"
grep -Fq "MIR-only LLVM path missing method forward name metadata for domain" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "MIR-only LLVM path missing method forward metadata row for role" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "append_overlay_method_projection_invalidations_from_metadata" \
    "$ROOT_DIR/src/codegen/transpiler_projection_method_invalidation.c"
grep -Fq "transpiler_find_host_method_metadata_in_context" \
    "$ROOT_DIR/src/codegen/transpiler_projection_method_invalidation.c"
if grep -Fq "transpiler_mir_decl_method_body_decl(ctx, method_meta)" \
        "$ROOT_DIR/src/codegen/transpiler_projection_method_invalidation.c"; then
    echo "[backend-fail-closed] C projection invalidation reintroduced method source recovery" >&2
    exit 1
fi
grep -Fq "C backend role operator method name metadata is missing" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "LLVM destructuring let binding name metadata is missing" \
    "$ROOT_DIR/src/codegen/llvm_stmt_destructure.c"
grep -Fq 'run_case "lambda_expr"' "$ROOT_DIR/tests/llvm_smoke.sh"
if grep -Fq "llvm_register_callable_param_if_needed" \
        "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"; then
    echo "[backend-fail-closed] active MIR parameter binding reintroduced AST callable registration" >&2
    exit 1
fi
grep -Fq "llvm_register_callable_mir_signature" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "llvm_stmt_callable_entry_return_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_lookup_callable_entry(ctx, callee)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "MIR-only LLVM path missing declared call return routine" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "MIR-only LLVM path missing declared call return signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "MIR-only LLVM path missing declared call return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
if grep -Fq "llvm_stmt_find_with_slot_inner_in_body" \
        "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"; then
    echo "[backend-fail-closed] LLVM call type inference reintroduced with-slot AST body rescan" >&2
    exit 1
fi
grep -Fq "llvm_stmt_array_elem_type_from_collection_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq "llvm_stmt_array_elem_type_from_current_field" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq "llvm_class_field_type_at_index(host_cls, field_idx)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
if grep -Fq "llvm_find_local_let_type_in_block" \
        "$ROOT_DIR/src/codegen/llvm_expr_common.c" \
        || grep -Fq "llvm_infer_local_let_type_in_block" \
        "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_nominal.c"; then
    echo "[backend-fail-closed] LLVM nominal type inference reintroduced AST body rescan" >&2
    exit 1
fi
grep -Fq "llvm_expr_unwrap_option_payload_class_name" \
    "$ROOT_DIR/src/codegen/llvm_expr_common.c"
grep -Fq "mir_routine_source_local_type_name(routine," \
    "$ROOT_DIR/src/codegen/llvm_expr_common.c"
grep -Fq "if (routine != NULL)" \
    "$ROOT_DIR/src/codegen/llvm_expr_common.c"
grep -Fq "llvm_expr_custom_type_name(init, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_nominal.c"
grep -Fq "llvm_lookup_callable_entry(ctx, callee_name)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, callee_name, &callee_var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
if grep -Fq "llvm_scope_lookup(ctx, callee_name)" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"; then
    echo "[backend-fail-closed] LLVM callable variable call reintroduced borrowed scope lookup" >&2
    exit 1
fi
if grep -Fq "LLVMVarEntry *llvm_scope_lookup" \
        "$ROOT_DIR/src/codegen/llvm_internal_api.h"; then
    echo "[backend-fail-closed] LLVM internal API re-exposed borrowed scope lookup" >&2
    exit 1
fi
grep -Fq "struct LLVMGenCtx *owner_ctx" \
    "$ROOT_DIR/src/codegen/llvm_internal.h"
grep -Fq "LLVM class field registry exceeded MAX_CLASS_FIELDS" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "entry->owner_ctx   = ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
if grep -Fq "ast_func_param_count(current_decl)" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"; then
    echo "[backend-fail-closed] LLVM callable variable call reintroduced AST parameter rescan" >&2
    exit 1
fi
grep -Fq "async block cannot capture Slot<T> local" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "async block cannot capture Channel<T> local" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "async block cannot capture non-Channel local" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "TranspilerParallelCallableCapture capture_typed_callables" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "pergyra_func_pointer_declarator_from_type_names_in_ctx" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_current_local_callable_capture" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
grep -Fq "MIR-only C path missing parallel capture callable source-local fact" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
if grep -R -n -F "transpiler_find_local_type_ast" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' >/dev/null; then
    echo "[backend-fail-closed] C backend reintroduced generic local type-AST lookup; use type-name facts or EventHandler-specific owner" >&2
    exit 1
fi
if grep -Fq "transpiler_mir_local_type_ast_lookup.h" \
        "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"; then
    echo "[backend-fail-closed] C parallel capture reintroduced EventHandler AST lookup include" >&2
    exit 1
fi
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(type_name, false)" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "LLVM async block cannot capture Slot<T> local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM async block cannot capture Channel<T> local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM async block cannot capture non-Channel local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_constructor_name" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "\"Array/Slice\", false, true" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "transpiler_spawn_reject_worker_storage" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, callee_decl)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_decl_is_extern_function(ctx, fn_decl)" \
    "$ROOT_DIR/src/codegen/transpiler_nominal.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_decl_is_extern_function(ctx, decl)" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "llvm_decl_is_extern_function(ctx, callee_decl)" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "MIR-only C path missing user-call routine" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "MIR-only C path missing user-call signature metadata" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "MIR-only C path missing spawn routine" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "MIR-only C path missing spawn signature metadata" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "MIR-only C path missing spawn return routine" \
    "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "MIR-only C path missing spawn return signature metadata" \
    "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "MIR-only C path missing spawn parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "MIR-only C path missing spawn return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "C backend: spawn argument %llu" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
if grep -R -n -E 'return "(Array/Slice|Array|Slice|List|Queue|Set|HashMap|Channel)"' \
        "$ROOT_DIR/src/codegen" --include='*.c' --include='*.h'; then
    echo "[backend-fail-closed] worker-boundary storage display strings must stay in common policy owner" >&2
    exit 1
fi
grep -Fq "pgy_worker_boundary_storage_kind_name" \
    "$ROOT_DIR/src/common/worker_boundary_storage_policy.c"
grep -Fq "pgy_worker_boundary_storage_kind_from_type_name" \
    "$ROOT_DIR/src/common/worker_boundary_storage_policy.c"
grep -Fq "llvm_spawn_reject_worker_storage_arg" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "llvm_spawn_reject_worker_storage_param" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "llvm_active_function_routine_by_name" \
    "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
if grep -R -Fq "llvm_active_function_routine_for_source_ast" \
        "$ROOT_DIR/src/codegen"; then
    echo "[backend-fail-closed] LLVM function routine lookup must not depend on source AST identity" >&2
    exit 1
fi
grep -Fq "MIR-only LLVM path missing user-call routine" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "MIR-only LLVM path missing user-call signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_forward_declare_func_from_mir(callee_routine, decl, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "MIR-only LLVM path missing spawn routine" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "MIR-only LLVM path missing spawn parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_constructor_name" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "\"Array/Slice\", true, true" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "LLVM spawn argument %zu" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "parallel capture registry exceeded MAX_SLOT_VARS while capturing Slot<T> local" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
grep -Fq "parallel capture registry exceeded MAX_SLOT_VARS while capturing local" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
grep -Fq "parallel capture registry exceeded MAX_SLOT_VARS while capturing inferred local" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
if grep -Fq "_pgy_async_ctx_" \
        "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
    echo "[backend-fail-closed] C detached async reintroduced capture context emission" >&2
    exit 1
fi
if grep -Fq "LLVM async capture field allocation" \
        "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
    echo "[backend-fail-closed] LLVM detached async reintroduced capture context emission" >&2
    exit 1
fi
grep -Fq "if (name == NULL || name[0] == '\\0')" \
    "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c"
awk '/pgy_hashmap_key_c_infix/,/^}/ { print }' \
    "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c" \
    | grep -Fq "return NULL;"
for fn in \
    "pgy_hashmap_key_raw_export_name" \
    "pgy_hashmap_key_raw_string_value_export_name"; do
    awk "/${fn}/,/^}/ { print }" \
        "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c" \
        | grep -Fq "if (spec == NULL)"
done
grep -Fq "transpiler_map_require_supported_key" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "pgy_hashmap_key_policy_type_text" \
    "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c"
grep -Fq "pgy_hashmap_key_policy_type_text()" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq 'Do not let detached `async { ... }` capture local storage by pointer' \
    "$ROOT_DIR/AGENTS.md"
grep -Fq '`Channel<T>` is a mutex/condvar-backed value today' \
    "$ROOT_DIR/AGENTS.md"

grep -Fq "pgy_channel_runtime_name" \
    "$ROOT_DIR/src/codegen/codegen_channel_runtime_abi.c"
grep -Fq "pgy_lane_channel_runtime_name" \
    "$ROOT_DIR/src/codegen/codegen_channel_runtime_abi.c"
if grep -F 'llvm_runtime_channel_name' \
    "$ROOT_DIR/src/codegen/llvm_runtime_channels.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM channel runtime declarations must not synthesize runtime function names locally" >&2
    exit 1
fi
if grep -F 'llvm_runtime_lane_channel_name' \
    "$ROOT_DIR/src/codegen/llvm_runtime_channels.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM lane channel runtime declarations must not synthesize runtime function names locally" >&2
    exit 1
fi
if grep -F 'llvm_channel_format_runtime_name' \
    "$ROOT_DIR/src/codegen/llvm_expr_channel.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM channel expressions must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'llvm_task_channel_format_runtime_name' \
    "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM task/channel calls must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'llvm_task_channel_format_op_runtime_name' \
    "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM task/channel query calls must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'snprintf(fn_name, sizeof(fn_name), "pgy_lane_channel_' \
    "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM MIR CFG channel control must consume channel runtime ABI names" >&2
    exit 1
fi
grep -Fq "pgy_lane_channel_runtime_name(fn_name, sizeof(fn_name)," \
    "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
if grep -F 'strdup_fmt("pgy_channel_%s_%s(&%s)"' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c" >/dev/null; then
    echo "[backend-fail-closed] C channel query builtins must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_%s_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c" >/dev/null; then
    echo "[backend-fail-closed] C channel timeout builtins must consume channel runtime ABI names" >&2
    exit 1
fi
grep -Fq "transpiler_channel_runtime_symbol" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"
if grep -F 'pgy_channel_init_%s(&%s, %s)' \
    "$ROOT_DIR/src/codegen/transpiler_let_channel_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C channel let init must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_channel_init_%s", inner' \
    "$ROOT_DIR/src/codegen/llvm_mir_source_resource_defs.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM MIR source channel init must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F '"pgy_channel_init_", channel_inner' \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM Channel let init must consume channel runtime ABI names" >&2
    exit 1
fi
grep -Fq "pgy_channel_runtime_name(init_fn, sizeof(init_fn), \"init\", inner)" \
    "$ROOT_DIR/src/codegen/transpiler_let_channel_emit.c"
grep -Fq "pgy_channel_runtime_name(init_fn_name, sizeof(init_fn_name)," \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "pgy_channel_runtime_name(init_fn_name, sizeof(init_fn_name)," \
    "$ROOT_DIR/src/codegen/llvm_mir_source_resource_defs.c"
if grep -F 'pgy_lane_channel_send_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C channel send lowering must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_recv_val_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C channel recv lowering must consume channel runtime ABI names" >&2
    exit 1
fi
grep -Fq "transpiler_spawn_channel_runtime_symbol" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
if grep -F 'pgy_lane_channel_try_recv_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_select.c" >/dev/null; then
    echo "[backend-fail-closed] C select bound receive must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_ready_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_select.c" >/dev/null; then
    echo "[backend-fail-closed] C select readiness must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_recv_val_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_select.c" >/dev/null; then
    echo "[backend-fail-closed] C select unbound consume must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_ready_%s(PGY_LANE_PINNED_ZONE' \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c" >/dev/null; then
    echo "[backend-fail-closed] C MIR select readiness must consume channel runtime ABI names" >&2
    exit 1
fi
grep -Fq "select_channel_runtime_symbol" \
    "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "pgy_lane_channel_runtime_name(runtime_fn, sizeof(runtime_fn)," \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
if grep -F 'pgy_lane_channel_try_recv_' \
    "$ROOT_DIR/src/codegen/llvm_stmt_select.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM select bound receive must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_ready_' \
    "$ROOT_DIR/src/codegen/llvm_stmt_select.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM select readiness must consume channel runtime ABI names" >&2
    exit 1
fi
if grep -F 'pgy_lane_channel_recv_val_' \
    "$ROOT_DIR/src/codegen/llvm_stmt_select.c" >/dev/null; then
    echo "[backend-fail-closed] LLVM select unbound consume must consume channel runtime ABI names" >&2
    exit 1
fi
grep -Fq "pgy_lane_channel_runtime_name(out, out_size, op, inner)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_names.c"
grep -Fq "mir_decl_field_claim_abi_capture(" \
    "$ROOT_DIR/src/compiler/mir_decl_header_fields.c"
grep -Fq "mir_decl_header_field_claim_abi_validate(" \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c"
grep -Fq "transpiler_slot_runtime_fn_for_decl_claim(" \
    "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
grep -Fq "const char *abi_type_name;" \
    "$ROOT_DIR/src/codegen/llvm_mir_vars.h"
grep -Fq "vars[count].abi_type_name = mir_routine_param_type_name(routine, i);" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "entry->abi_type_name != NULL ? entry->abi_type_name : type_name" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_scope.c"
grep -Fq "base_entry->abi_type_name != NULL" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_scope.c"
grep -Fq "llvm_register_typed_var_abi_binding(ctx, owned_base, alloca," \
    "$ROOT_DIR/src/codegen/llvm_mir_scope_bind.c"
grep -Fq "if (mir_active && inst->abi_type_name != NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "llvm_mir_callable_sig_to_llvm" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "llvm_register_callable_mir_signature" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "llvm_register_callable_mir_value" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "llvm_stmt_register_mir_callable_local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "llvm_mir_routine_param_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "value_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "return_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "MIR-only LLVM path missing callable source-local fact" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_callable.c"
grep -Fq "const MIRCallableSig *param_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "const MIRCallableSig *return_callable_sig" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "llvm_register_callable_mir_signature(ctx, p->name" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "llvm_mir_async_fact_future_inner_from_source_local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c"
grep -Fq "bool mir_is_remote = false" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c"
grep -Fq "missing callable return signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_signature.c"
grep -Fq "missing callable parameter signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_signature.c"
if grep -F "llvm_mir_type_from_ast(ctx, return_type)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c" >/dev/null; then
    echo "[backend-fail-closed] active MIR LLVM return ABI must not recover callable/type shape from AST" >&2
    exit 1
fi
grep -Fq "} else if (!mir_active && !event_handler_param && p->type != NULL)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "if (!mir_active && ctx != NULL && ctx->generic_binding_count > 0" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "if (!mir_active && type_name == NULL && p->type != NULL)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
if grep -F "if (type_name == NULL && p->type != NULL)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c" >/dev/null; then
    echo "[backend-fail-closed] active MIR local registration must not recover type names from AST" >&2
    exit 1
fi
if grep -F "} else if (!event_handler_param && p->type != NULL)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c" >/dev/null; then
    echo "[backend-fail-closed] active MIR function parameters must not recover C types from AST" >&2
    exit 1
fi
if grep -F "if (ctx != NULL && ctx->generic_binding_count > 0" \
    "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c" >/dev/null; then
    echo "[backend-fail-closed] active MIR generic parameter emission must not reopen AST substitution" >&2
    exit 1
fi
grep -Fq "mir_source_local_type_append_callable(program, routine," \
    "$ROOT_DIR/src/compiler/mir_source_local_types.c"
grep -Fq "!source_local_fact->is_callable" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"

# Active MIR event emission must consume the declaration-header ABI rows and
# fail closed when a row is absent; the AST event-parameter branch is legacy
# compatibility only.
grep -Fq "emit_event_decl_from_mir_header" \
    "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "MIR-only C path missing event parameter ABI metadata" \
    "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "is missing event parameter metadata" \
    "$ROOT_DIR/src/compiler/mir_decl_header_validate.c"
grep -Fq "transpiler_require_type_name_c_type_copy" \
    "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "MIR-only LLVM path missing event parameter ABI metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "mir_decl_header_event_param_type_name" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "pergyra_type_to_llvm(ctx, type_name)" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c"

echo "[backend-fail-closed] C/LLVM fail-open fallback guards ok"
