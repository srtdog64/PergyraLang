#!/usr/bin/env bash
# Native enum parsing must consume grammar or return a parse error. A timeout
# is a distinct failure, never an acceptable malformed-source receipt.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || {
    echo "[native-enum-progress] missing compiler binary: $PGY" >&2
    exit 1
}

TIMEOUT_SECONDS="${PGY_ENUM_PROGRESS_TIMEOUT_SECONDS:-1}"
CONTROL_TIMEOUT_SECONDS="${PGY_ENUM_CONTROL_TIMEOUT_SECONDS:-5}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-native-enum-progress.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

fail() {
    echo "[native-enum-progress] $*" >&2
    exit 1
}

write_case() {
    printf '%s' "$2" >"$WORK_DIR/$1.pgy"
}

run_mode() {
    local label="$1"
    local expectation="$2"
    local budget_seconds="$3"
    shift 3
    local rc

    set +e
    pgy_run_with_timeout "$budget_seconds" \
        "$WORK_DIR/$label.out" "$WORK_DIR/$label.err" "$@"
    rc=$?
    set -e

    [[ "$rc" -ne 124 ]] || fail "$label exceeded ${budget_seconds}s"
    [[ "$rc" -ne 127 ]] || fail "$label lost the portable timeout runner"
    [[ "$rc" -lt 128 ]] || fail "$label crashed with exit $rc"
    if [[ "$expectation" == "accept" && "$rc" -ne 0 ]]; then
        tail -10 "$WORK_DIR/$label.err" >&2
        fail "$label rejected a valid enum (exit $rc)"
    fi
    if [[ "$expectation" == "reject" && "$rc" -eq 0 ]]; then
        fail "$label accepted malformed enum source"
    fi
}

write_case enum_lparen_lbrace 'enum({'
write_case enum_rparen 'enum)'
write_case enum_lbracket 'enum['
write_case enum_rbracket 'enum]'
write_case enum_double_lbrace 'enum{{'
write_case enum_bad_variant 'enum Body { ]'
write_case enum_valid 'enum Ready { A, B }'

valid_source="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/enum_valid.pgy")"
run_mode valid_native_ast accept "$CONTROL_TIMEOUT_SECONDS" \
    "$PGY" "$valid_source" --native-pipeline --ast

for name in enum_lparen_lbrace enum_rparen enum_lbracket enum_rbracket \
    enum_double_lbrace enum_bad_variant; do
    source_path="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/$name.pgy")"
    run_mode "${name}_native_ast" reject "$TIMEOUT_SECONDS" \
        "$PGY" "$source_path" --native-pipeline --ast
    grep -Fq 'PGY_PARSE_SYNTAX' \
        "$WORK_DIR/${name}_native_ast.out" \
        "$WORK_DIR/${name}_native_ast.err" ||
        fail "${name}_native_ast lost the structured parse diagnostic"
    run_mode "${name}_tokens" survive "$CONTROL_TIMEOUT_SECONDS" \
        "$PGY" "$source_path" --tokens
    run_mode "${name}_public_ast" reject "$CONTROL_TIMEOUT_SECONDS" \
        "$PGY" "$source_path" --ast
    run_mode "${name}_public_mir" reject "$CONTROL_TIMEOUT_SECONDS" \
        "$PGY" "$source_path" --mir
done

echo "[native-enum-progress] five deletion minima and one body-loop control reject without hang; valid enum AST remains green"
