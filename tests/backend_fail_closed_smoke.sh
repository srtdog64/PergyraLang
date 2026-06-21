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
grep -Fq "C backend collection specialization name is too long" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c"
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
grep -Fq "MIR resource op '%s' is missing runtime ABI layout metadata" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
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
grep -Fq "MIR-only LLVM path missing function body return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "MIR-only LLVM path missing function body parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "MIR-only LLVM path missing function parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
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
grep -Fq "MIR-only LLVM path missing role forward declaration name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
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
grep -Fq "MIR-only LLVM path missing match subject routine" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing match subject signature metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing match subject return type-name metadata" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "MIR-only LLVM path missing method type inference return type-name metadata" \
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
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "MIR-only C path missing included role method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "MIR-only C path missing included role method return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "MIR-only C path missing included role method parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "MIR-only C path missing role operator method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role operator return type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role operator parameter type-name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
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
grep -Fq "host_method_meta == NULL && !transpiler_active_has_mir(ctx)" \
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
grep -Fq "llvm_register_callable_param_if_needed" \
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

echo "[backend-fail-closed] C/LLVM fail-open fallback guards ok"
