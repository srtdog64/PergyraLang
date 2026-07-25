#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[self-host-parity:json-bounded-string] missing compiler binary: $PGY" >&2
    exit 1
fi

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/json_bounded_string_owner.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/json_bounded_string_owner}"
mkdir -p "$BUILD_DIR"

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/json_bounded_string_${backend}.exe"
    local compile_log="$BUILD_DIR/json_bounded_string_${backend}.compile.log"
    local run_err="$BUILD_DIR/json_bounded_string_${backend}.err"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1); then
        cat "$compile_log" >&2
        return 1
    fi
    pgy_require_runnable_binary_here \
        "self-host-parity:json-bounded-string:$backend" "$bin" || return 1
    (cd "$ROOT_DIR" && "$bin" 2>"$run_err") | tr -d '\r'
}

C_OUT="$(run_backend c)"
[[ "$C_OUT" == "json-bounded-string-owner-ok" ]] || {
    echo "[self-host-parity:json-bounded-string] C verdict mismatch: $C_OUT" >&2
    exit 1
}

LLVM_OUT="$(run_backend llvm)"
[[ "$LLVM_OUT" == "$C_OUT" ]] || {
    echo "[self-host-parity:json-bounded-string] LLVM verdict mismatch: $LLVM_OUT" >&2
    exit 1
}

echo "[self-host-parity:json-bounded-string] C/LLVM exact-bound decode parity ok"
