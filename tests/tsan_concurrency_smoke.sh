#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_BIN="${PGY_TSAN_CONCURRENCY_BIN:-}"
TIMEOUT_SECONDS="${PGY_TSAN_TIMEOUT_SECONDS:-120}"
LOG_DIR="$ROOT_DIR/.tmp/tsan-concurrency"
STDOUT_LOG="$LOG_DIR/stdout.log"
STDERR_LOG="$LOG_DIR/stderr.log"

if [[ -z "$TEST_BIN" || ! -x "$TEST_BIN" ]]; then
    echo "[tsan-concurrency] no instrumented concurrency test at '$TEST_BIN'" >&2
    exit 1
fi

mkdir -p "$LOG_DIR"
# shellcheck source=tests/portable_process_helpers.sh
source "$ROOT_DIR/tests/portable_process_helpers.sh"
export TSAN_OPTIONS="${TSAN_OPTIONS:-halt_on_error=1:history_size=7}"

set +e
pgy_run_with_timeout "$TIMEOUT_SECONDS" "$STDOUT_LOG" "$STDERR_LOG" \
    "$TEST_BIN"
rc=$?
set -e

if [[ "$rc" -eq 124 ]]; then
    echo "[tsan-concurrency] runtime battery hung for more than ${TIMEOUT_SECONDS}s" >&2
    exit 1
fi
if grep -Eq 'WARNING: ThreadSanitizer|SUMMARY: ThreadSanitizer' \
    "$STDOUT_LOG" "$STDERR_LOG"; then
    echo "[tsan-concurrency] ThreadSanitizer reported a runtime race" >&2
    grep -Eh 'WARNING: ThreadSanitizer|SUMMARY: ThreadSanitizer' \
        "$STDOUT_LOG" "$STDERR_LOG" | head -3 >&2
    exit 1
fi
if [[ "$rc" -ne 0 ]]; then
    echo "[tsan-concurrency] instrumented runtime battery exited $rc" >&2
    tail -n 20 "$STDERR_LOG" >&2
    tail -n 20 "$STDOUT_LOG" >&2
    exit 1
fi
if ! grep -Fq 'All concurrency tests passed.' "$STDOUT_LOG"; then
    echo "[tsan-concurrency] runtime battery did not emit its success witness" >&2
    cat "$STDOUT_LOG" >&2
    exit 1
fi

echo "[tsan-concurrency] pool, channel, cancellation, and zone battery clean"
