#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mapfile -d '' inc_files < <(
    cd "$ROOT_DIR"
    find src/runtime src/codegen src/compiler -name '*.inc' -type f -print0
)

if ((${#inc_files[@]} > 0)); then
    echo "runtime/codegen/compiler production .inc files are not allowed:" >&2
    printf '  %s\n' "${inc_files[@]}" >&2
    exit 1
fi

large_impl_headers="$(
    cd "$ROOT_DIR"
    find src/runtime src/codegen src/compiler -name '*.h' -type f \
        ! -path 'src/tests/*' \
        -exec wc -l {} + \
        | awk '$2 != "total" && $1 > 600 { print }'
)"
if [ -n "$large_impl_headers" ]; then
    echo "production implementation-style headers exceeded the 600 LOC signal:" >&2
    printf '%s\n' "$large_impl_headers" >&2
    exit 1
fi

for legacy_header in \
    rir_builder.h \
    rir_flow.h \
    rir_names.h \
    rir_public_surface.h \
    rir_validation.h
do
    if [ -e "$ROOT_DIR/src/compiler/$legacy_header" ]; then
        echo "RIR implementation-style header reappeared: src/compiler/$legacy_header" >&2
        exit 1
    fi
    if grep -RIn "$legacy_header" "$ROOT_DIR/src" "$ROOT_DIR/Makefile" >/dev/null 2>&1; then
        echo "RIR implementation-style header include/reference remains: $legacy_header" >&2
        grep -RIn "$legacy_header" "$ROOT_DIR/src" "$ROOT_DIR/Makefile" >&2 || true
        exit 1
    fi
done

task_channel_owner="$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
task_runtime_owner="$ROOT_DIR/src/codegen/llvm_expr_task_calls.c"

if grep -RIn "fallback_ty = ctx->type_i32" \
    "$task_channel_owner" >/dev/null 2>&1; then
    echo "LLVM task/channel receive reintroduced anonymous Option<Int> fallback" >&2
    grep -RIn "fallback_ty = ctx->type_i32" \
        "$task_channel_owner" >&2 || true
    exit 1
fi

if grep -RIn "return LLVMConstInt(ctx->type_i[13]2\\?, 0" \
    "$task_channel_owner" >/dev/null 2>&1; then
    echo "LLVM task/channel builtin reintroduced silent zero/false fallback" >&2
    grep -RIn "return LLVMConstInt(ctx->type_i[13]2\\?, 0" \
        "$task_channel_owner" >&2 || true
    exit 1
fi

for required_term in \
    "llvm_required_channel_function" \
    "llvm_required_channel_var"
do
    if ! grep -q "$required_term" \
        "$task_channel_owner"; then
        echo "LLVM task/channel builtin lost explicit failure helper: $required_term" >&2
        exit 1
    fi
done

if ! grep -q "llvm_required_task_function" "$task_runtime_owner"; then
    echo "LLVM task runtime builtin lost explicit failure helper: llvm_required_task_function" >&2
    exit 1
fi

echo "[backend-inc-size] runtime/codegen/compiler production .inc files = 0; production implementation headers <= 600 LOC; legacy RIR implementation headers = 0; LLVM task/channel fallback = 0"
