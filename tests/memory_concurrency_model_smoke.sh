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

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[memory-concurrency] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
    echo "[memory-concurrency] SKIP executable probe"
    exit 0
fi

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
