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
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_program_routine_index_owner}"
mkdir -p "$BUILD_DIR"

DECLARATION_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_declaration_index_owner.pgy"
ROUTINE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_routine_index_owner.pgy"
BLOCK_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_routine_block_fact_owner.pgy"
ROUTINE_FACT_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/routine_fact_index_owner.pgy"
MACHINE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/machine_layer_fact_owner.pgy"

fail() {
    echo "[self-host-parity:mir-program-routine-index] $*" >&2
    exit 1
}

lines_at_most() {
    local file="$1"
    local limit="$2"
    source_size_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$file" | tr -d ' ')" || fail "source-size measurement failed"
    [[ "$source_size_lines" -le "$limit" ]] ||
        fail "${file#"$ROOT_DIR/"} exceeds $limit lines"
}

lines_at_most "$DECLARATION_OWNER" 120
lines_at_most "$ROUTINE_OWNER" 530
lines_at_most "$BLOCK_OWNER" 180
lines_at_most "$ROUTINE_FACT_OWNER" 600
lines_at_most "$MACHINE_OWNER" 440

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

C_OUT="$(run_backend c)"
[[ "$C_OUT" == "mir-program-routine-index-owner-ok" ]] || {
    fail "C mismatch: $C_OUT"
}
LLVM_OUT="$(run_backend llvm)"
[[ "$LLVM_OUT" == "$C_OUT" ]] || {
    fail "LLVM mismatch: $LLVM_OUT"
}

echo "[self-host-parity:mir-program-routine-index] C/LLVM structure parity ok"
