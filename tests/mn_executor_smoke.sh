#!/usr/bin/env bash
# M:N movable-lane executor witness (WO-MN-1 R1/R2, docs/194).
#
# Proves, with two emitted-shaped C harnesses against the REAL runtime
# sources, that the MovableScheduler lane is now backed by the M:N fiber
# scheduler in the extern materialization and stays fail-closed in the
# inline one:
#
#   extern leg (the production shape: PGY_RUNTIME_DECLS_ONLY harness linked
#   against the runtime object built from pgy_runtime_lib.c):
#     1. the object materializes the M:N core (nm finds the scheduler);
#     2. a movable-lane fan-out computes the same sum as the worker-pool
#        lane on the identical task set (executor-invariance witness);
#     3. cancellation is cooperative on the movable lane: a task that WAITS
#        for its cancel flag observes it and returns the flagged value --
#        never a dropped/null result (the docs/190-era pool bug class must
#        not reappear on the new executor).
#
#   inline leg (legacy PGY_RUNTIME_INLINE shape: full header inlining, no
#   PGY_RUNTIME_LIB_INTERNAL):
#     4. a movable dispatch is refused with a null handle (the declared
#        defence-in-depth backstop behind the driver's compile-time refusal).
#
# Usage: bash tests/mn_executor_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABEL="mn-executor"

CC="${CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[$LABEL] SKIP: no C compiler ($CC)"
    exit 0
fi
NM="${NM:-nm}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

CFLAGS=(-std=c11 -O1 -fwrapv -fno-strict-aliasing -pthread
        -I"$ROOT_DIR/src/runtime" -I"$ROOT_DIR/src" -DPGY_LLVM_ENABLED)

# Linux-only acceleration must not be inferred from "not Windows". The M:N
# worker core is portable pthread code; epoll and MAP_STACK are optional
# machine capabilities. This ratchet keeps macOS on the core path without
# pretending that the Linux I/O integration exists there.
grep -Fq '#ifdef MAP_STACK' "$ROOT_DIR/src/runtime/async/fiber.c" || {
    echo "[$LABEL] MAP_STACK is not capability-gated" >&2
    exit 1
}
for source in \
    "$ROOT_DIR/src/runtime/async/scheduler.c" \
    "$ROOT_DIR/src/runtime/async/scheduler.h"; do
    if grep -Eq '^#ifndef[[:space:]]+_WIN32' "$source"; then
        echo "[$LABEL] $source treats not-Windows as epoll capability" >&2
        exit 1
    fi
    grep -Fq '#if defined(__linux__)' "$source" || {
        echo "[$LABEL] $source does not declare the Linux epoll boundary" >&2
        exit 1
    }
done

# ---- runtime objects (both linked-runtime materializations) ---------------
# The C-leg cext object is what emitted extern programs actually link; the
# LLVM-leg lib object is the bitcode twin's source. Both must carry the core.
if ! "$CC" "${CFLAGS[@]}" -c "$ROOT_DIR/src/runtime/pgy_runtime_cext_lib.c" \
        -o "$WORK/rt.o" 2>"$WORK/rt.err"; then
    echo "[$LABEL] cext runtime object build failed" >&2
    tail -20 "$WORK/rt.err" >&2
    exit 1
fi
if ! "$CC" "${CFLAGS[@]}" -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
        -o "$WORK/rt_llvm.o" 2>"$WORK/rt_llvm.err"; then
    echo "[$LABEL] llvm-leg runtime object build failed" >&2
    tail -20 "$WORK/rt_llvm.err" >&2
    exit 1
fi

if command -v "$NM" >/dev/null 2>&1; then
    for obj in "$WORK/rt.o" "$WORK/rt_llvm.o"; do
        if ! "$NM" "$obj" | grep -qw "pgy_mn_scheduler_create"; then
            echo "[$LABEL] $obj does not materialize the M:N core" >&2
            exit 1
        fi
    done
else
    echo "[$LABEL] note: nm unavailable, skipping symbol-level check"
fi

# ---- extern harness -------------------------------------------------------
cat > "$WORK/extern_harness.c" <<'EOF'
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 0
#endif
#include "pgy_runtime.h"
#include "pgy_parallel.h"
#include "pgy_lane_scheduler.h"

#define TASKS 32

static void *task_triple(void *arg)
{
    intptr_t v = (intptr_t)arg;
    return (void *)(v * 3);
}

/* Cooperative cancellation: WAIT for the flag, then report having seen it.
 * A dropped task would surface as a null await result; a non-cooperative
 * executor would never let the task observe the flag. */
static void *task_wait_for_cancel(void *arg)
{
    (void)arg;
    for (int spin = 0; spin < 200000000; spin++) {
        if (pgy_task_is_cancelled())
            return (void *)9;
    }
    return (void *)7;
}

static long fan_out_sum(PgyExecutionLane lane)
{
    PgyTaskHandle handles[TASKS];
    long sum = 0;

    for (intptr_t i = 0; i < TASKS; i++) {
        handles[i] = pgy_lane_spawn_dispatch(lane, task_triple, (void *)i);
        if (handles[i].task == NULL) {
            fprintf(stderr, "dispatch returned null handle (lane %d)\n",
                    (int)lane);
            exit(1);
        }
    }
    for (int i = 0; i < TASKS; i++)
        sum += (long)(intptr_t)pgy_lane_await(handles[i]);
    return sum;
}

int main(void)
{
    pgy_pool_init(0);

    long movable = fan_out_sum(PGY_LANE_MOVABLE_SCHEDULER);
    long pooled = fan_out_sum(PGY_LANE_WORKER_POOL);
    if (movable != pooled) {
        fprintf(stderr, "executor variance: movable=%ld pooled=%ld\n",
                movable, pooled);
        return 1;
    }
    long expected = 3L * (TASKS * (TASKS - 1) / 2);
    if (movable != expected) {
        fprintf(stderr, "wrong sum: %ld != %ld\n", movable, expected);
        return 1;
    }

    PgyTaskHandle waiting = pgy_lane_spawn_dispatch(
        PGY_LANE_MOVABLE_SCHEDULER, task_wait_for_cancel, NULL);
    if (waiting.task == NULL) {
        fprintf(stderr, "cancel witness dispatch failed\n");
        return 1;
    }
    if (!pgy_lane_cancel(waiting)) {
        fprintf(stderr, "cancel request failed\n");
        return 1;
    }
    long seen = (long)(intptr_t)pgy_lane_await(waiting);
    if (seen != 9) {
        fprintf(stderr, "cooperative cancel broken on movable lane: %ld\n",
                seen);
        return 1;
    }

    pgy_pool_shutdown();
    printf("mn-extern ok movable=%ld cancel_seen=%ld\n", movable, seen);
    return 0;
}
EOF

if ! "$CC" "${CFLAGS[@]}" -DPGY_RUNTIME_DECLS_ONLY \
        "$WORK/extern_harness.c" "$WORK/rt.o" -o "$WORK/extern_harness.exe" \
        -lm 2>"$WORK/extern.err"; then
    echo "[$LABEL] extern harness build failed" >&2
    tail -20 "$WORK/extern.err" >&2
    exit 1
fi
if ! "$WORK/extern_harness.exe" >"$WORK/extern.out" 2>&1; then
    echo "[$LABEL] extern harness FAILED" >&2
    cat "$WORK/extern.out" >&2
    exit 1
fi
grep -q "mn-extern ok" "$WORK/extern.out" || {
    echo "[$LABEL] extern harness produced no verdict" >&2
    cat "$WORK/extern.out" >&2
    exit 1
}

# ---- inline harness (fail-closed backstop) --------------------------------
cat > "$WORK/inline_harness.c" <<'EOF'
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 0
#endif
#include "pgy_runtime.h"
#include "pgy_parallel.h"
#include "pgy_lane_scheduler.h"

static void *task_noop(void *arg) { return arg; }

int main(void)
{
    pgy_pool_init(0);
    PgyTaskHandle h = pgy_lane_spawn_dispatch(
        PGY_LANE_MOVABLE_SCHEDULER, task_noop, NULL);
    pgy_pool_shutdown();
    if (h.task != NULL) {
        fprintf(stderr, "inline runtime ran a movable task; the backstop is gone\n");
        return 1;
    }
    printf("mn-inline fail-closed ok\n");
    return 0;
}
EOF

if ! "$CC" "${CFLAGS[@]}" \
        "$WORK/inline_harness.c" -o "$WORK/inline_harness.exe" \
        -lm 2>"$WORK/inline.err"; then
    echo "[$LABEL] inline harness build failed" >&2
    tail -20 "$WORK/inline.err" >&2
    exit 1
fi
if ! "$WORK/inline_harness.exe" >"$WORK/inline.out" 2>&1; then
    echo "[$LABEL] inline harness FAILED" >&2
    cat "$WORK/inline.out" >&2
    exit 1
fi
grep -q "mn-inline fail-closed ok" "$WORK/inline.out" || {
    echo "[$LABEL] inline harness produced no verdict" >&2
    cat "$WORK/inline.out" >&2
    exit 1
}

echo "[$LABEL] ok (M:N materialized + movable==pool invariance + cooperative cancel + inline backstop)"
