#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-cc}"
SOURCE="$ROOT_DIR/tests/cases/memory_adversarial/c_witness/heap_use_after_free.c"
WORK_BASE="$ROOT_DIR/.tmp/asan-uaf-witness"
mkdir -p "$ROOT_DIR/.tmp"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

BIN="$WORK_DIR/heap-use-after-free"
LOG="$WORK_DIR/heap-use-after-free.log"

"$CC_BIN" -std=c11 -O1 -g -fno-omit-frame-pointer -fsanitize=address \
    "$SOURCE" -o "$BIN"

set +e
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$BIN" >"$LOG" 2>&1
rc=$?
set -e

if [[ "$rc" -eq 0 ]]; then
    echo "[asan-uaf-witness] intentional heap UAF escaped detection" >&2
    cat "$LOG" >&2
    exit 1
fi
if ! grep -Eq 'heap-use-after-free|ERROR: AddressSanitizer' "$LOG"; then
    echo "[asan-uaf-witness] failing witness did not produce an ASan UAF report" >&2
    cat "$LOG" >&2
    exit 1
fi

echo "[asan-uaf-witness] intentional C heap UAF detected; sanitizer oracle is live"
