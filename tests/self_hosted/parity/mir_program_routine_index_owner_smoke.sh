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
    echo "[self-host-parity:mir-program-routine-index] missing compiler: $PGY" >&2
    exit 1
fi

SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/mir_program_routine_index_owner.pgy"
NEGATIVE_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/mir_routine_block_local_admission_negative.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_program_routine_index_owner}"
mkdir -p "$BUILD_DIR"

run_backend() {
    local backend="$1"
    local bin="$BUILD_DIR/mir_program_routine_index_${backend}.exe"
    local compile_log="$BUILD_DIR/mir_program_routine_index_${backend}.compile.log"
    local run_err="$BUILD_DIR/mir_program_routine_index_${backend}.err"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1); then
        cat "$compile_log" >&2
        return 1
    fi
    pgy_require_runnable_binary_here \
        "self-host-parity:mir-program-routine-index:$backend" "$bin" || return 1
    (cd "$ROOT_DIR" && "$bin" 2>"$run_err") | tr -d '\r'
}

run_cross_block_negative() {
    local backend="$1"
    local bin="$BUILD_DIR/mir_routine_block_local_admission_${backend}.exe"
    local compile_log="$BUILD_DIR/mir_routine_block_local_admission_${backend}.compile.log"
    local run_out="$BUILD_DIR/mir_routine_block_local_admission_${backend}.out"
    local run_err="$BUILD_DIR/mir_routine_block_local_admission_${backend}.err"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$NEGATIVE_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$compile_log" 2>&1); then
        cat "$compile_log" >&2
        return 1
    fi
    if (cd "$ROOT_DIR" && "$bin" >"$run_out" 2>"$run_err"); then
        echo "[self-host-parity:mir-program-routine-index] $backend cross-block local start was accepted" >&2
        return 1
    fi
    grep -Fq "invalid block instruction slice" "$run_out" "$run_err" || {
        echo "[self-host-parity:mir-program-routine-index] $backend cross-block diagnostic drifted" >&2
        cat "$run_out" "$run_err" >&2
        return 1
    }
}

C_OUT="$(run_backend c)"
[[ "$C_OUT" == "mir-program-routine-index-owner-ok" ]] || {
    echo "[self-host-parity:mir-program-routine-index] C mismatch: $C_OUT" >&2
    exit 1
}
LLVM_OUT="$(run_backend llvm)"
[[ "$LLVM_OUT" == "$C_OUT" ]] || {
    echo "[self-host-parity:mir-program-routine-index] LLVM mismatch: $LLVM_OUT" >&2
    exit 1
}
run_cross_block_negative c
run_cross_block_negative llvm

echo "[self-host-parity:mir-program-routine-index] C/LLVM structure parity ok"
