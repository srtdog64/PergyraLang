#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
CASE_DIR="$ROOT_DIR/tests/cases/stage4_determinism/collection_iteration"

if [[ ! -x "$PGY" ]]; then
    echo "[stage4-determinism] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_require_runnable_binary_here "stage4-determinism" "$PGY"

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_stage4_determinism.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

normalize_output() {
    tr -d '\r' | awk '
        /^[0-9]+ error\(s\), [0-9]+ warning\(s\)$/ { seen_summary = 1; next }
        /^pgy: compiled/ { next }
        seen_summary && length($0) > 0 { print }
    '
}

run_backend() {
    local backend="$1"
    local entry="$CASE_DIR/main.pgy"
    local expected="$CASE_DIR/expected_stdout.txt"
    local out_bin="$WORK_DIR/collection_iteration_${backend}"
    local entry_arg
    local out_bin_arg
    local raw_output
    local actual_file="$WORK_DIR/${backend}.actual"

    entry_arg="$(pgy_path_for_compiler "$PGY" "$entry")"
    out_bin_arg="$(pgy_path_for_compiler "$PGY" "$out_bin")"

    set +e
    raw_output="$("$PGY" "$entry_arg" --run --backend="$backend" -o "$out_bin_arg" 2>&1)"
    local rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "[stage4-determinism] backend=$backend compiler/run failed (exit=$rc)" >&2
        echo "--- output ---" >&2
        echo "$raw_output" >&2
        echo "--------------" >&2
        exit "$rc"
    fi

    printf '%s\n' "$raw_output" | normalize_output > "$actual_file"
    if ! diff -u "$expected" "$actual_file" >/dev/null; then
        echo "[stage4-determinism] backend=$backend collection iteration drift" >&2
        diff -u "$expected" "$actual_file" >&2 || true
        exit 1
    fi

    echo "[stage4-determinism] collection iteration backend=$backend ok"
}

BACKENDS="${PGY_STAGE4_DETERMINISM_BACKENDS:-c llvm}"

for backend in $BACKENDS; do
    run_backend "$backend"
done
