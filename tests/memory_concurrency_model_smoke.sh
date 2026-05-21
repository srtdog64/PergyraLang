#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

MODEL_DOC="$ROOT_DIR/docs/113_memory_concurrency_model.md"
if [[ ! -f "$MODEL_DOC" ]]; then
    echo "[memory-concurrency] missing model doc: $MODEL_DOC" >&2
    exit 1
fi

for required in \
    "Memory And Concurrency Model Beta Contract" \
    "beta-freeze-source-of-truth" \
    "core execution primitive" \
    "spawn Worker(args...)" \
    "joins before control continues" \
    "Shared " \
    "task-boundary conflicts are rejected" \
    "Undefined-Behavior Hygiene Contract" \
    "Non-atomic shared counters are forbidden across worker threads" \
    "insert/rehash invalidates concurrent readers" \
    "published as an immutable snapshot" \
    "Static local buffers/state are not thread-safe by default" \
    "no non-atomic" \
    "Non-blocking/timeout receive is copy-only for beta" \
    "ChannelClose(Channel<T>)" \
    "Cancel(Future<T>)" \
    "Capture-bearing detached async block stability" \
    "Full weak-memory ordering vocabulary" \
    "make memory-concurrency-model-test-smoke"; do
    if ! grep -Fq "$required" "$MODEL_DOC"; then
        echo "[memory-concurrency] model doc missing: $required" >&2
        exit 1
    fi
done

if ! grep -Fq "static _Thread_local unsigned pgy_zone_stale_warn_count" \
    "$ROOT_DIR/src/runtime/pgy_runtime_zone_result_option_inline.h"; then
    echo "[memory-concurrency] stale zone warning counter must be thread-local" >&2
    exit 1
fi

if [[ ! -x "$PGY" ]]; then
    echo "[memory-concurrency] SKIP executable probe; source model is gated"
    exit 0
fi

bash "$ROOT_DIR/tests/async_model_positioning_smoke.sh"
bash "$ROOT_DIR/tests/parallel_core_contract_smoke.sh"

BACKENDS="${PGY_MEMORY_CONCURRENCY_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    PGY_BIN="$PGY" bash "$ROOT_DIR/tests/compare_backends.sh" \
        tests/cases/backend_compare/parallel_channel_sum \
        tests/cases/backend_compare/parallel_channel_dual \
        tests/cases/backend_compare/triple_paradigm
else
    echo "[memory-concurrency] skipping backend compare for backends=$BACKENDS"
fi

echo "[memory-concurrency] beta model ok"
