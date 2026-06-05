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
grep -Fq "C backend: slot builtin expression formatting failed" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "C backend: slot builtin expression allocation failed" \
    "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
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
grep -Fq "MIR-only LLVM path missing role vtable ability name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "MIR-only LLVM path missing method forward name metadata for role" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role forward declaration name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "MIR-only LLVM path missing role operator forward name metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
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
grep -Fq "MIR-only C path missing role vtable ability name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "MIR-only C path missing role operator method name metadata" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "C backend role operator method name metadata is missing" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "LLVM destructuring let binding name metadata is missing" \
    "$ROOT_DIR/src/codegen/llvm_stmt_destructure.c"
grep -Fq 'run_case "lambda_expr"' "$ROOT_DIR/tests/llvm_smoke.sh"
grep -Fq "llvm_register_callable_param_if_needed" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "llvm_stmt_callable_entry_return_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_lookup_callable_entry(ctx, callee)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
if grep -Fq "llvm_stmt_find_with_slot_inner_in_body" \
        "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"; then
    echo "[backend-fail-closed] LLVM call type inference reintroduced with-slot AST body rescan" >&2
    exit 1
fi
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
if grep -Fq "ast_func_param_count(current_decl)" \
        "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"; then
    echo "[backend-fail-closed] LLVM callable variable call reintroduced AST parameter rescan" >&2
    exit 1
fi
grep -Fq "async block cannot capture Slot<T> local" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "async block cannot capture non-Channel local" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "LLVM async block cannot capture Slot<T> local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM async block cannot capture non-Channel local" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"

echo "[backend-fail-closed] C/LLVM fail-open fallback guards ok"
