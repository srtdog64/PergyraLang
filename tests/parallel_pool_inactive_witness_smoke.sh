#!/usr/bin/env bash
#
# parallel_pool_inactive_witness_smoke.sh -- the runtime's two availability
# fallbacks must stay observable (docs/177 F1, docs/188 R5).
#
# `pgy_spawn` on a pool that was never initialised runs the task inline and
# warns; `pgy_default_worker_count` with a malformed PGY_WORKERS falls back to
# the hardware count and warns. Both are warn-and-continue paths, which
# CLAUDE.md section 1.1 tolerates only while the branch is observable. Until
# this gate existed nothing in tests/ read either warning, so deleting the
# fprintf or rewording it turned nothing red: the observability contract was
# itself unobserved.
#
# The witness compiles an emitted-shaped extern harness against the real
# cext runtime object (the production link shape), drives both fallbacks with
# stderr captured, and then proves the warning is conditional by spawning
# again on a live pool and requiring silence.
set -euo pipefail

LABEL="pool-inactive-witness"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CC="${CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[$LABEL] SKIP: no C compiler ($CC)"
    exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

CFLAGS=(-std=c11 -O1 -fwrapv -fno-strict-aliasing -pthread
        -I"$ROOT_DIR/src/runtime" -I"$ROOT_DIR/src" -DPGY_LLVM_ENABLED)

# The exact text is the contract. A reworded warning must come here first.
INACTIVE_TEXT="worker pool inactive; task runs inline (serial)."
WORKERS_TEXT="PGY_WORKERS is not a valid worker count (want 1..4096)"
grep -Fq "$INACTIVE_TEXT" "$ROOT_DIR/src/runtime/pgy_parallel_spawn.h" || {
    echo "[$LABEL] spawn owner no longer carries the pool-inactive warning text" >&2
    exit 1
}
grep -Fq "$WORKERS_TEXT" "$ROOT_DIR/src/runtime/pgy_parallel_pool_lifecycle.h" || {
    echo "[$LABEL] pool lifecycle owner no longer carries the PGY_WORKERS warning text" >&2
    exit 1
}

if ! "$CC" "${CFLAGS[@]}" -c "$ROOT_DIR/src/runtime/pgy_runtime_cext_lib.c" \
        -o "$WORK/rt.o" 2>"$WORK/rt.err"; then
    echo "[$LABEL] cext runtime object build failed" >&2
    tail -20 "$WORK/rt.err" >&2
    exit 1
fi

cat >"$WORK/harness.c" <<'EOF'
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

static void *task_triple(void *arg)
{
    intptr_t v = (intptr_t)arg;
    return (void *)(v * 3);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: harness <inactive|workers|live>\n");
        return 2;
    }
    if (argv[1][0] == 'i') {
        /* No pgy_pool_init: the spawn must still yield the task's result. */
        PgyTaskHandle h = pgy_spawn(task_triple, (void *)(intptr_t)5);
        intptr_t r = (intptr_t)pgy_await(h);
        printf("inactive result=%ld\n", (long)r);
        return r == 15 ? 0 : 1;
    }
    if (argv[1][0] == 'w') {
        size_t n = pgy_default_worker_count();
        printf("workers=%zu\n", n);
        return n >= 1 ? 0 : 1;
    }
    /* live: the same spawn on an initialised pool must not warn. */
    pgy_pool_init(2);
    PgyTaskHandle h = pgy_spawn(task_triple, (void *)(intptr_t)7);
    intptr_t r = (intptr_t)pgy_await(h);
    pgy_pool_shutdown();
    printf("live result=%ld\n", (long)r);
    return r == 21 ? 0 : 1;
}
EOF

if ! "$CC" "${CFLAGS[@]}" -DPGY_RUNTIME_DECLS_ONLY \
        "$WORK/harness.c" "$WORK/rt.o" -o "$WORK/harness.exe" \
        -lm 2>"$WORK/harness.err"; then
    echo "[$LABEL] harness build failed" >&2
    tail -20 "$WORK/harness.err" >&2
    exit 1
fi

# 1. inactive pool: correct result AND the warning, once.
if ! "$WORK/harness.exe" inactive >"$WORK/inactive.out" 2>"$WORK/inactive.err"; then
    echo "[$LABEL] inline fallback did not produce the task result" >&2
    cat "$WORK/inactive.out" "$WORK/inactive.err" >&2
    exit 1
fi
count="$(grep -Fc "$INACTIVE_TEXT" "$WORK/inactive.err" || true)"
if [[ "$count" != "1" ]]; then
    echo "[$LABEL] expected exactly one pool-inactive warning on stderr, saw $count" >&2
    cat "$WORK/inactive.err" >&2
    exit 1
fi
grep -Fq "[pgy][parallel] spawn failed:" "$WORK/inactive.err" || {
    echo "[$LABEL] pool-inactive warning lost its [pgy][parallel] spawn prefix" >&2
    cat "$WORK/inactive.err" >&2
    exit 1
}

# 2. malformed PGY_WORKERS: hardware fallback AND the warning.
if ! PGY_WORKERS=abc "$WORK/harness.exe" workers >"$WORK/workers.out" 2>"$WORK/workers.err"; then
    echo "[$LABEL] PGY_WORKERS fallback did not yield a worker count" >&2
    cat "$WORK/workers.out" "$WORK/workers.err" >&2
    exit 1
fi
grep -Fq "$WORKERS_TEXT" "$WORK/workers.err" || {
    echo "[$LABEL] malformed PGY_WORKERS produced no warning" >&2
    cat "$WORK/workers.err" >&2
    exit 1
}
# A valid override must be silent.
if ! PGY_WORKERS=3 "$WORK/harness.exe" workers >"$WORK/workers_ok.out" 2>"$WORK/workers_ok.err"; then
    echo "[$LABEL] valid PGY_WORKERS override failed" >&2
    exit 1
fi
grep -Fq "workers=3" "$WORK/workers_ok.out" || {
    echo "[$LABEL] PGY_WORKERS=3 was not honoured" >&2
    cat "$WORK/workers_ok.out" >&2
    exit 1
}
if [[ -s "$WORK/workers_ok.err" ]]; then
    echo "[$LABEL] valid PGY_WORKERS override warned" >&2
    cat "$WORK/workers_ok.err" >&2
    exit 1
fi

# 3. live pool: same spawn, no fallback warning.
if ! "$WORK/harness.exe" live >"$WORK/live.out" 2>"$WORK/live.err"; then
    echo "[$LABEL] live-pool spawn failed" >&2
    cat "$WORK/live.out" "$WORK/live.err" >&2
    exit 1
fi
if grep -Fq "$INACTIVE_TEXT" "$WORK/live.err"; then
    echo "[$LABEL] pool-inactive warning fired on a live pool" >&2
    cat "$WORK/live.err" >&2
    exit 1
fi

echo "[$LABEL] ok (inline fallback warns once and returns the result; malformed PGY_WORKERS warns; live pool is silent)"
