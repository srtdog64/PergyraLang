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
} > "$llvm_branch_fallback"
fail_if_nonempty "LLVM backend reintroduced synthetic branch condition fallback" "$llvm_branch_fallback"

echo "[backend-fail-closed] C/LLVM fail-open fallback guards ok"
