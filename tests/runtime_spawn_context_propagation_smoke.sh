#!/usr/bin/env bash
#
# runtime_spawn_context_propagation_smoke.sh — executable proof that every
# execution lane carries the spawning runtime authority into the task and
# restores the surrounding TLS context afterward.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
CC_CMD=($CC)
OUT_DIR="$ROOT_DIR/build"

fail() { echo "[runtime-spawn-context-propagation] FAIL: $*" >&2; exit 1; }

mkdir -p "$OUT_DIR" || fail "could not create build output dir"

PREFIX="$OUT_DIR/runtime_spawn_context_$$"
INLINE_BIN="$PREFIX.inline.exe"
CEXT_PROBE_OBJ="$PREFIX.cext-probe.o"
CEXT_OBJ="$PREFIX.cext.o"
CEXT_BIN="$PREFIX.cext.exe"
LLVM_PROBE_OBJ="$PREFIX.llvm-probe.o"
LLVM_OBJ="$PREFIX.llvm.o"
LLVM_BIN="$PREFIX.llvm.exe"
COMPILE_OUT="$PREFIX.compile.out"
COMPILE_ERR="$PREFIX.compile.err"
DENY_OUT="$PREFIX.deny.out"
DENY_ERR="$PREFIX.deny.err"

cleanup() {
    rm -f "$INLINE_BIN" "$CEXT_PROBE_OBJ" "$CEXT_OBJ" "$CEXT_BIN" \
        "$LLVM_PROBE_OBJ" "$LLVM_OBJ" "$LLVM_BIN" "$COMPILE_OUT" \
        "$COMPILE_ERR" "$DENY_OUT" "$DENY_ERR"
}
trap cleanup EXIT

COMMON_FLAGS=(
    -Wall -Wextra -Werror -Wno-unused-function -std=c11 -O1
    -fwrapv -fno-strict-aliasing -pthread
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime"
)

compile_or_fail() {
    local label="$1"
    shift
    if ! "${CC_CMD[@]}" "$@" >"$COMPILE_OUT" 2>"$COMPILE_ERR"; then
        cat "$COMPILE_OUT" >&2 || true
        cat "$COMPILE_ERR" >&2 || true
        fail "$label compile failed"
    fi
}

expect_child_denial() {
    local label="$1"
    local binary="$2"
    local status

    set +e
    "$binary" deny-child-io-write >"$DENY_OUT" 2>"$DENY_ERR"
    status=$?
    set -e
    if [[ "$status" -eq 0 ]]; then
        cat "$DENY_OUT" >&2 || true
        cat "$DENY_ERR" >&2 || true
        fail "$label worker widened the parent's capability grant"
    fi
    grep -Fq "class=capability-denied" "$DENY_ERR" \
        || fail "$label child failure was not capability-denied"
    grep -Fq "op=context-child-io-write" "$DENY_ERR" \
        || fail "$label child denial lost the operation identity"
}

compile_or_fail "inline runtime probe" \
    "${COMMON_FLAGS[@]}" \
    "$ROOT_DIR/tests/runtime_context_smoke.c" \
    -lpthread -lm -o "$INLINE_BIN"
"$INLINE_BIN" || fail "inline runtime context proof failed"
expect_child_denial "inline runtime" "$INLINE_BIN"

compile_or_fail "C extern runtime probe object" \
    "${COMMON_FLAGS[@]}" \
    -DPGY_RUNTIME_DECLS_ONLY \
    -DPGY_CONTEXT_CEXT_RUNTIME \
    -DPGY_CONTEXT_EXPECT_MOVABLE \
    -c "$ROOT_DIR/tests/runtime_context_smoke.c" \
    -o "$CEXT_PROBE_OBJ"

compile_or_fail "C extern runtime object" \
    "${COMMON_FLAGS[@]}" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_cext_lib.c" \
    -o "$CEXT_OBJ"
compile_or_fail "C extern runtime probe" \
    "${COMMON_FLAGS[@]}" \
    "$CEXT_PROBE_OBJ" "$CEXT_OBJ" \
    -lpthread -lm -o "$CEXT_BIN"
"$CEXT_BIN" || fail "C extern runtime context proof failed"
expect_child_denial "C extern runtime" "$CEXT_BIN"

compile_or_fail "LLVM runtime probe object" \
    "${COMMON_FLAGS[@]}" \
    -DPGY_RUNTIME_DECLS_ONLY \
    -DPGY_CONTEXT_LLVM_RUNTIME \
    -DPGY_CONTEXT_EXPECT_MOVABLE \
    -c "$ROOT_DIR/tests/runtime_context_smoke.c" \
    -o "$LLVM_PROBE_OBJ"
compile_or_fail "LLVM runtime object" \
    "${COMMON_FLAGS[@]}" \
    -DPGY_LLVM_ENABLED \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
    -o "$LLVM_OBJ"
compile_or_fail "LLVM runtime probe" \
    "${COMMON_FLAGS[@]}" \
    "$LLVM_PROBE_OBJ" "$LLVM_OBJ" \
    -lpthread -lm -o "$LLVM_BIN"
"$LLVM_BIN" || fail "LLVM runtime context proof failed"
expect_child_denial "LLVM runtime" "$LLVM_BIN"

grep -Fq "PgyBudgetState *budget_owner" \
    "$ROOT_DIR/src/runtime/pgy_runtime_context.h" \
    || fail "runtime context lost its shared quantitative budget owner"
grep -Fq "pgy_runtime_context_capture_task" \
    "$ROOT_DIR/src/runtime/pgy_runtime_context.h" \
    || fail "runtime context task snapshot owner is missing"
grep -Fq "pthread_once(&g_pgy_runtime_context_default_once" \
    "$ROOT_DIR/src/runtime/pgy_runtime_context.h" \
    || fail "worker access can race default runtime context initialization"

for carrier in \
    "$ROOT_DIR/src/runtime/pgy_parallel.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_spawn.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_blocking.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_coroutine.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_mn_exports.h"; do
    grep -Fq "pgy_runtime_context_capture_task" "$carrier" \
        || fail "task carrier does not capture runtime authority: $carrier"
done

grep -Fq "pgy_runtime_context_bind(&task->runtime_context)" \
    "$ROOT_DIR/src/runtime/pgy_parallel.h" \
    || fail "thread/M:N execution boundary does not bind captured authority"
grep -Fq "pgy_runtime_context_bind(previous_context)" \
    "$ROOT_DIR/src/runtime/pgy_parallel.h" \
    || fail "thread/M:N execution boundary does not restore surrounding authority"
grep -Fq "scheduler_runtime_context" \
    "$ROOT_DIR/src/runtime/pgy_parallel_coroutine.h" \
    || fail "coroutine scheduler context carrier is missing"
grep -Fq "pgy_runtime_context_bind(&current->runtime_context)" \
    "$ROOT_DIR/src/runtime/pgy_parallel_coroutine.h" \
    || fail "coroutine yield does not restore task authority"
grep -Fq "pgy_runtime_context_bind(&current->runtime_context)" \
    "$ROOT_DIR/src/runtime/pgy_parallel_task_ops.h" \
    || fail "coroutine await does not restore task authority"

for task_path in \
    "$ROOT_DIR/src/runtime/pgy_parallel_spawn.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_blocking.h" \
    "$ROOT_DIR/src/runtime/pgy_parallel_coroutine.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_mn_exports.h"; do
    if grep -Fq "pgy_runtime_context_init(" "$task_path"; then
        fail "task path reopened environment grants or an independent budget: $task_path"
    fi
done

echo "[runtime-spawn-context-propagation] inline/C-extern/LLVM-runtime lanes share captured authority and restore TLS: PASS"
