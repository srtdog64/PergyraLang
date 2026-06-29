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

grep -Fq "PgyExecutionLane lane;" \
    "$ROOT_DIR/src/runtime/pgy_parallel.h" \
    || fail "runtime task header does not preserve the ExecutionLane fact"
grep -Fq "pgy_task_handle_lane" \
    "$ROOT_DIR/src/runtime/pgy_parallel.h" \
    || fail "task handle lane accessor is missing"
grep -Fq "task_handle_fields[] = { ctx->type_i8ptr }" \
    "$ROOT_DIR/src/codegen/llvm_runtime.c" \
    || fail "LLVM PgyTaskHandle ABI should remain a stable task pointer"
grep -Fq "pgy_task_handle_set_lane(handle, lane)" \
    "$ROOT_DIR/src/runtime/pgy_lane_scheduler.h" \
    || fail "lane spawn facade does not store the consumed lane fact"
for lane_task_op in pgy_lane_await pgy_lane_detach pgy_lane_cancel; do
    grep -Fq "$lane_task_op" \
        "$ROOT_DIR/src/runtime/pgy_lane_scheduler.h" \
        || fail "lane scheduler is missing task operation facade: $lane_task_op"
done
grep -Fq "return pgy_lane_await(h)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_quantum_exports.h" \
    || fail "LLVM await export does not consume the lane task facade"
grep -Fq "pgy_lane_detach(h)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_quantum_exports.h" \
    || fail "LLVM detach export does not consume the lane task facade"
grep -Fq "return pgy_lane_cancel(h)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_quantum_exports.h" \
    || fail "LLVM cancel export does not consume the lane task facade"

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
grep -Fq "pgy_lane_await(_ph_" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c" \
    || fail "C parallel lowering does not consume the lane await facade"
grep -Fq "pgy_lane_detach(_ah_" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c" \
    || fail "C async block lowering does not consume the lane detach facade"
grep -Fq 'pgy_lane_cancel(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c" \
    || fail "C Cancel builtin does not consume the lane cancel facade"
grep -Fq "pgy_lane_channel_send_%s(PGY_LANE_PINNED_ZONE" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c" \
    || fail "C channel send lowering does not consume the pinned channel lane facade"
grep -Fq "pgy_lane_channel_recv_val_%s(PGY_LANE_PINNED_ZONE" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c" \
    || fail "C channel recv lowering does not consume the pinned channel lane facade"
grep -Fq "pgy_lane_channel_try_recv_result_%s(PGY_LANE_PINNED_ZONE" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c" \
    || fail "C TryRecv lowering does not consume the pinned channel lane facade"
grep -Fq "pgy_lane_channel_ready_%s(PGY_LANE_PINNED_ZONE" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c" \
    || fail "C MIR select readiness does not consume the pinned channel lane facade"
grep -Fq "pgy_lane_spawn_dispatch_export" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM async/parallel lowering does not consume the lane spawn facade export"
grep -Fq "PGY_LANE_WORKER_POOL" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM parallel lowering does not emit the WorkerPool lane fact"
grep -Fq "PGY_LANE_LOCAL_ASYNC" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM async block lowering does not emit the LocalAsync lane fact"
grep -Fq "pgy_lane_channel_send" \
    "$ROOT_DIR/src/codegen/llvm_expr_channel.c" \
    || fail "LLVM channel send lowering does not consume the channel lane facade"
grep -Fq "pgy_lane_channel_recv_val" \
    "$ROOT_DIR/src/codegen/llvm_expr_channel.c" \
    || fail "LLVM channel recv lowering does not consume the channel lane facade"
grep -Fq "pgy_lane_channel_try_recv_result" \
    "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c" \
    || fail "LLVM TryRecv lowering does not consume the channel lane facade"
grep -Fq "pgy_lane_channel_ready_" \
    "$ROOT_DIR/src/codegen/llvm_stmt_select.c" \
    || fail "LLVM select readiness does not consume the channel lane facade"
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
if grep -Fq "pgy_await(_ph_" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
    fail "C parallel lowering reintroduced direct pgy_await"
fi
if grep -Fq "pgy_async_detach(_ah_" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
    fail "C async block lowering reintroduced direct pgy_async_detach"
fi
if grep -Fq 'pgy_task_cancel(%s)' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"; then
    fail "C Cancel builtin reintroduced direct pgy_task_cancel"
fi
if grep -Fq "pgy_channel_send_%s(&%s" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"; then
    fail "C channel send reintroduced direct pgy_channel_send"
fi
if grep -Fq "pgy_channel_recv_val_%s(&%s" \
    "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"; then
    fail "C channel recv reintroduced direct pgy_channel_recv_val"
fi
if grep -Fq "pgy_channel_try_recv_result_%s(&%s" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"; then
    fail "C TryRecv reintroduced direct pgy_channel_try_recv_result"
fi
if grep -Fq "pgy_channel_ready_%s(&%s)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"; then
    fail "C MIR select readiness reintroduced direct pgy_channel_ready"
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
