#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGION_OWNER="$ROOT_DIR/src/codegen/llvm_mir_region_scope.c"
FUNCTION_OWNER="$ROOT_DIR/src/codegen/llvm_mir_emit.c"
CONTEXT_OWNER="$ROOT_DIR/src/codegen/llvm_internal.h"

fail() {
    echo "[llvm-mir-region-scope] $*" >&2
    exit 1
}

function_body() {
    local name="$1"
    local path="$2"
    awk -v name="$name" '
        index($0, name "(") { active = 1 }
        active { print }
        active && /^}/ { exit }
    ' "$path"
}

require_text() {
    local text="$1"
    local term="$2"
    local message="$3"
    grep -Fq "$term" <<<"$text" || fail "$message"
}

require_ordered_terms() {
    local text="$1"
    shift
    local last=0
    local term
    for term in "$@"; do
        local line
        line="$(grep -nF -m1 "$term" <<<"$text" | cut -d: -f1 || true)"
        [[ -n "$line" ]] || fail "missing ordered term: $term"
        (( line > last )) || fail "out-of-order term: $term"
        last="$line"
    done
}

begin_body="$(function_body llvm_mir_region_scope_begin "$REGION_OWNER")"
destroy_body="$(function_body llvm_mir_region_scope_destroy "$REGION_OWNER")"
end_body="$(function_body llvm_mir_region_scope_end "$REGION_OWNER")"

require_ordered_terms "$begin_body" \
    "if (ctx == NULL)" \
    "ctx->region_alloca = NULL;" \
    "ctx->region_owner_function = NULL;" \
    "ctx->region_scope_id = 0;" \
    "ctx->region_scope_active = false;" \
    "if (routine == NULL || ctx->region_plan == NULL" \
    "LLVMGetEntryBasicBlock(ctx->current_function) != entry_block" \
    "ctx->region_alloca = LLVMBuildAlloca" \
    "ctx->region_owner_function = ctx->current_function;" \
    "ctx->region_scope_active = true;"
require_text "$begin_body" \
    "LLVMGetBasicBlockParent(entry_block) != ctx->current_function" \
    "begin allocation no longer proves the current function owner"
require_text "$begin_body" \
    "LLVM MIR region scope begin requires the owning function entry block" \
    "begin allocation dominance failure is not fail-closed"

require_text "$destroy_body" "if (!ctx->region_scope_active)" \
    "begin-less destroy no longer distinguishes an inactive scope"
require_text "$destroy_body" "ctx->region_owner_function != NULL" \
    "inactive destroy no longer rejects ambient caller ownership"
require_text "$destroy_body" \
    "LLVM MIR inactive region scope retained ambient function state" \
    "ambient caller region leak has no owned diagnostic"
require_text "$destroy_body" \
    "ctx->region_alloca == NULL || ctx->region_owner_function == NULL" \
    "active destroy no longer requires a begin allocation owner"
require_text "$destroy_body" "LLVMGetInstructionParent(ctx->region_alloca)" \
    "destroy no longer resolves the allocation block"
require_text "$destroy_body" \
    "ctx->current_function != ctx->region_owner_function" \
    "destroy no longer rejects a cross-function region owner"
require_text "$destroy_body" "LLVMGetEntryBasicBlock(ctx->region_owner_function)" \
    "destroy no longer proves entry-block dominance"
require_text "$destroy_body" \
    "LLVM MIR region destroy escaped its owning function entry block" \
    "cross-function destroy has no owned diagnostic"
if grep -Fq "ctx == NULL || !ctx->region_scope_active" <<<"$destroy_body"; then
    fail "begin-less destroy silently accepts stale ambient state"
fi

require_text "$end_body" "ctx->region_owner_function = NULL;" \
    "scope end does not clear the function owner"
require_text "$(<"$CONTEXT_OWNER")" "LLVMValueRef    region_owner_function;" \
    "LLVM context does not carry the region function owner"
require_text "$(<"$FUNCTION_OWNER")" \
    "saved_region_owner_function = ctx->region_owner_function;" \
    "nested emission does not snapshot the caller region owner"
require_text "$(<"$FUNCTION_OWNER")" \
    "ctx->region_owner_function = saved_region_owner_function;" \
    "nested emission does not restore the caller region owner"

echo "[llvm-mir-region-scope] function-local allocation/destroy ownership: PASS"
