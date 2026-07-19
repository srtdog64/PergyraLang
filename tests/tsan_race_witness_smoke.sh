#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-cc}"
SOURCE="$ROOT_DIR/tests/cases/memory_adversarial/c_witness/pthread_data_race.c"
WORK_BASE="$ROOT_DIR/.tmp/tsan-race-witness"
mkdir -p "$ROOT_DIR/.tmp"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

BIN="$WORK_DIR/pthread-data-race"
LOG="$WORK_DIR/pthread-data-race.log"

"$CC_BIN" -std=c11 -O1 -g -fno-omit-frame-pointer -fsanitize=thread \
    "$SOURCE" -pthread -o "$BIN"

set +e
TSAN_OPTIONS=halt_on_error=1 "$BIN" >"$LOG" 2>&1
rc=$?
set -e

if [[ "$rc" -eq 0 ]]; then
    echo "[tsan-race-witness] intentional pthread data race escaped detection" >&2
    cat "$LOG" >&2
    exit 1
fi
if grep -Fq 'FATAL: ThreadSanitizer: unexpected memory mapping' "$LOG"; then
    echo "[tsan-race-witness] TSan runtime could not reserve its shadow mapping" >&2
    echo "  On WSL2, retry the whole gate with: setarch \"\$(uname -m)\" -R make test-tsan" >&2
    cat "$LOG" >&2
    exit 1
fi
if ! grep -Eq 'WARNING: ThreadSanitizer: data race|SUMMARY: ThreadSanitizer: data race' "$LOG"; then
    echo "[tsan-race-witness] failing witness did not produce a TSan race report" >&2
    cat "$LOG" >&2
    exit 1
fi

echo "[tsan-race-witness] intentional pthread data race detected; sanitizer oracle is live"
