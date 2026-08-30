#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OWNER="$ROOT_DIR/src/codegen/llvm_api.c"

fail() {
    echo "[llvm-large-aggregate-return-stack] $*" >&2
    exit 1
}

require_term() {
    local term="$1"
    grep -Fq "$term" "$OWNER" || fail "owner missing term: $term"
}

reject_term() {
    local term="$1"
    if grep -Fq "$term" "$OWNER"; then
        fail "owner contains forbidden term: $term"
    fi
}

require_term "#define PGY_LLVM_LARGE_AGGREGATE_RETURN_BYTES 2048u"
require_term "llvm_function_returns_large_aggregate"
require_term "LLVMGetModuleDataLayout(ctx->module)"
require_term "LLVMABISizeOfType(layout, return_type)"
grep -A2 -F "llvm_function_returns_large_aggregate(ctx, fn)" "$OWNER" | \
    grep -Fq "llvm_add_fn_attr(ctx, fn, noinline_kind);" || \
    fail "large aggregate return must retain its noinline value boundary"

reject_term "LLVMPassBuilderOptionsSetInlinerThreshold"
reject_term "SelfMirLowerBlockFromArtifact"
reject_term "dish_result_collect"
reject_term "class_method_result_loop"

echo "[llvm-large-aggregate-return-stack] target-layout stack policy ok"
