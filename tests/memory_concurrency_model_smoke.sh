#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_BIN_WAS_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
    PGY_BIN_WAS_EXPLICIT=1
else
    PGY="$ROOT_DIR/bin/pgy"
fi
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
    if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[memory-concurrency] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
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
