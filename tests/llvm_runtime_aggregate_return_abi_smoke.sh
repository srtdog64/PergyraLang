#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
cd "$ROOT_DIR"

if [ ! -f "$PGY_BIN" ] && command -v cygpath >/dev/null 2>&1; then
    PGY_BIN="$(cygpath -u "$PGY_BIN")"
fi
if [ -d "/c/Program Files/LLVM/bin" ]; then
    PATH="/c/Program Files/LLVM/bin:$PATH"
fi

fail() {
    echo "[llvm-runtime-aggregate-return-abi] $*" >&2
    exit 1
}

require_term() {
    local file="$1"
    local term="$2"
    grep -Fq "$term" "$file" || fail "$file missing term: $term"
}

require_absent() {
    local file="$1"
    local term="$2"
    if grep -Fq "$term" "$file"; then
        fail "$file still contains forbidden term: $term"
    fi
}

owner="src/codegen/llvm_runtime_aggregate_return.c"
decl="src/codegen/llvm_runtime_core_builtin_decl.c"
emit="src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
generic="src/codegen/llvm_expr_call_args.c"

require_term "$owner" "llvm_runtime_aggregate_return_apply_decl_shape"
require_term "$owner" "llvm_emit_runtime_aggregate_return_call"
for runtime in StringSplit pgy_args pgy_dir_walk; do
    require_term "$owner" "\"$runtime\""
done

require_term "$decl" "llvm_runtime_aggregate_return_apply_decl_shape"
require_term "$emit" "llvm_runtime_aggregate_return_is_sret_function"
require_term "$emit" "llvm_emit_runtime_aggregate_return_call"
require_term "$generic" "llvm_runtime_aggregate_return_is_sret_function"
require_absent "$emit" "Args special case"
require_absent "$generic" "Args special case"

if [ ! -f "$PGY_BIN" ]; then
    echo "[llvm-runtime-aggregate-return-abi] static contract ok (compiler not built)"
    exit 0
fi

run_case() {
    local backend="$1"
    local file="$2"
    "$PGY_BIN" "--backend=$backend" --run "$file" 2>&1
}

expect_marker() {
    local backend="$1"
    local file="$2"
    local marker="$3"
    local out rc
    set +e
    out="$(run_case "$backend" "$file")"
    rc=$?
    set -e
    if [ "$rc" -eq 127 ]; then
        echo "[llvm-runtime-aggregate-return-abi] compiler/backend unavailable for $backend; static contract already checked"
        return 2
    fi
    if [ "$rc" -ne 0 ]; then
        fail "$backend $file failed (rc=$rc): $(printf '%s' "$out" | tail -20)"
    fi
    printf '%s' "$out" | grep -Fq "$marker" ||
        fail "$backend $file missing marker '$marker': $(printf '%s' "$out" | tail -20)"
    return 0
}

for backend in c llvm; do
    expect_marker "$backend" tests/capability/cap_env_demo.pgy "args ok" || break
    expect_marker "$backend" tests/capability/strsplit_probe.pgy "total-len:" || break
    expect_marker "$backend" tests/capability/dirwalk_probe.pgy "dirwalk ok" || break
done

echo "[llvm-runtime-aggregate-return-abi] aggregate-return ABI owner ok"
