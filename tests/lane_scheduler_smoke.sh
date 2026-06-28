#!/usr/bin/env bash
#
# lane_scheduler_smoke.sh — build + run the SEA runtime facade contract proof.
# Compiles the facade + the execution-lane policy + the test (pthread), and
# checks executor-invariance and fail-closed Reject.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
OUT="$(mktemp -d)/lane_scheduler_test"

fail() { echo "[lane-scheduler] FAIL: $*" >&2; exit 1; }

"$CC" -Wall -Wextra -Werror -std=c11 \
    -I"$ROOT_DIR/src" \
    -I"$ROOT_DIR/src/runtime" \
    "$ROOT_DIR/src/tests/lane_scheduler_test.c" \
    "$ROOT_DIR/src/runtime/pgy_lane_scheduler.c" \
    "$ROOT_DIR/src/compiler/execution_lane.c" \
    -lpthread \
    -o "$OUT" || fail "compile failed"

"$OUT" || fail "facade contract violated"

grep -Fq "pgy_lane_spawn_dispatch(" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c" \
    || fail "C spawn lowering does not consume the lane spawn facade"
grep -Fq "pgy_lane_spawn_dispatch_export" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c" \
    || fail "LLVM spawn lowering does not consume the lane spawn facade export"
grep -Fq "pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c" \
    || fail "C parallel lowering does not consume the WorkerPool lane facade"
grep -Fq "pgy_lane_spawn_dispatch(PGY_LANE_LOCAL_ASYNC" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c" \
    || fail "C async block lowering does not consume the LocalAsync lane facade"
grep -Fq "pgy_lane_spawn_dispatch_export" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM async/parallel lowering does not consume the lane spawn facade export"
grep -Fq "PGY_LANE_WORKER_POOL" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM parallel lowering does not emit the WorkerPool lane fact"
grep -Fq "PGY_LANE_LOCAL_ASYNC" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM async block lowering does not emit the LocalAsync lane fact"
if grep -Fq '"pgy_spawn_blocking" : "pgy_async_spawn"' \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"; then
    fail "C spawn lowering reintroduced direct executor selection"
fi
if grep -Fq "pgy_spawn(" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
    fail "C parallel lowering reintroduced direct pgy_spawn"
fi
if grep -Fq "pgy_async_spawn(" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
    fail "C async block lowering reintroduced direct pgy_async_spawn"
fi
if grep -Fq "pgy_spawn_blocking_export" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"; then
    fail "LLVM spawn lowering reintroduced direct executor selection"
fi
if grep -Fq "pgy_async_spawn_export" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"; then
    fail "LLVM spawn lowering reintroduced direct executor selection"
fi
if grep -Fq "pgy_spawn_export" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
    fail "LLVM parallel lowering reintroduced direct pgy_spawn_export"
fi
if grep -Fq "pgy_async_spawn_export" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
    fail "LLVM async block lowering reintroduced direct pgy_async_spawn_export"
fi

echo "[lane-scheduler] PASS"
