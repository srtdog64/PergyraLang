#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:mir-program-error-storage"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || {
    echo "[$LABEL] missing compiler binary: $PGY" >&2
    exit 1
}

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/mir_program_error_storage_owner.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_program_error_storage}"
mkdir -p "$BUILD_DIR"

SOURCE_ARG="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
assert_llvm_leg "$LABEL" "$SOURCE_ARG" "$BUILD_DIR"
grep -Fxq 'mir-program-error-storage=fresh' "$BUILD_DIR/main_c_leg.out" || {
    echo "[$LABEL] fresh diagnostic-only receipt is missing" >&2
    exit 1
}

echo "[$LABEL] diagnostic-only failure storage C/LLVM parity: PASS"
