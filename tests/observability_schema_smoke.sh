#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the observability schema drifted.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

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
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
CASE_ROOT="$ROOT_DIR/tests/cases/abi_pipeline"

require_text() {
    local path="$1"
    local text="$2"
    if ! grep -Fq "$text" "$path"; then
        echo "[observability-schema] missing '$text' in ${path#$ROOT_DIR/}" >&2
        exit 1
    fi
}

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_BIN_WAS_EXPLICIT" -eq 1 ]]; then
        echo "[observability-schema] missing explicit compiler binary: $PGY" >&2
        exit 1
    fi
    echo "[observability-schema] SKIP executable probe; compiler binary unavailable"
    exit 0
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_observability_schema.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

AIR_JSON_OUT="$WORK_DIR/air_observability.json"
"$PGY" --native-pipeline --air-json "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy")" --backend=c > "$AIR_JSON_OUT"
for required in \
    '"schema":"pgy.air.graph.v1"' \
    '"observability"' \
    '"abi_schema":"pgy.intent.observability.v1"' \
    '"trace_schema":"pgy.intent.trace.v1"' \
    '"surfaces":["last","history","active","recent"]' \
    '"event_kinds"' \
    '"intent.enter"' \
    '"transfer"' \
    '"history_fields"' \
    '"from_zone"' \
    '"to_slot"'; do
    require_text "$AIR_JSON_OUT" "$required"
done

normalize_output() {
    tr -d '\r' | awk '
        /^[0-9]+ error\(s\), [0-9]+ warning\(s\)$/ { seen_summary = 1; next }
        /^\[WARNING\] / { next }
        !seen_summary { pre[++pre_count] = $0; next }
        /^--- output ---$/ { saw_output_marker = 1; in_output = 1; next }
        /^--- end ---$/ { in_output = 0; next }
        in_output && (seen || length($0) > 0) { print; seen = 1 }
        saw_output_marker { next }
        /^pgy: compiled/ { next }
        /^In file included from / { in_diag = 1; next }
        /^[^[:space:]].*:[0-9]+:[0-9]+: (warning|note|error):/ {
            in_diag = 1
            next
        }
        in_diag && /^[[:space:]]*[0-9]+[[:space:]]*\|/ { next }
        in_diag && /^[[:space:]]*\|/ { next }
        in_diag && /^[0-9]+ warnings? generated\./ { in_diag = 0; next }
        in_diag && /^$/ { in_diag = 0; next }
        !in_diag && (seen || length($0) > 0) { print; seen = 1 }
        END {
            if (!seen_summary) {
                for (i = 1; i <= pre_count; i++) {
                    if (seen || length(pre[i]) > 0) {
                        print pre[i];
                        seen = 1;
                    }
                }
            }
        }
    '
}

files_equal() {
    local left="$1"
    local right="$2"

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import pathlib
import sys

left = pathlib.Path(sys.argv[1]).read_bytes()
right = pathlib.Path(sys.argv[2]).read_bytes()
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi

    cmp -s "$left" "$right"
}

show_diff() {
    local left="$1"
    local right="$2"

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import difflib
import pathlib
import sys

left = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines(True)
right = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace").splitlines(True)
sys.stdout.writelines(difflib.unified_diff(left, right, fromfile=sys.argv[1], tofile=sys.argv[2]))
PY
        return 0
    fi

    diff -u "$left" "$right" || true
}

run_case() {
    local backend="$1"
    local name="$2"
    local entry="$CASE_ROOT/$name/main.pgy"
    local expected_stdout="$CASE_ROOT/$name/expected_stdout.txt"
    local out_bin="$WORK_DIR/${name}_${backend}"
    local raw_output
    local actual_file
    local expected_file

    if [[ ! -f "$entry" ]]; then
        echo "[observability-schema] missing case: $entry" >&2
        exit 1
    fi
    if [[ ! -f "$expected_stdout" ]]; then
        echo "[observability-schema] missing expected stdout: $expected_stdout" >&2
        exit 1
    fi

    raw_output="$("$PGY" "$(pgy_path_for_compiler "$PGY" "$entry")" --run --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" 2>&1)"
    if ! grep -Fq "0 error(s), 0 warning(s)" <<<"$raw_output"; then
        echo "[observability-schema] $name backend=$backend did not compile cleanly" >&2
        echo "$raw_output" >&2
        exit 1
    fi

    actual_file="$(mktemp "$WORK_DIR/${name}_${backend}_actual.XXXXXX")"
    expected_file="$(mktemp "$WORK_DIR/${name}_${backend}_expected.XXXXXX")"
    printf '%s' "$raw_output" | normalize_output > "$actual_file"
    cat "$expected_stdout" | normalize_output > "$expected_file"

    if ! files_equal "$expected_file" "$actual_file"; then
        echo "[observability-schema] $name backend=$backend stdout mismatch" >&2
        show_diff "$expected_file" "$actual_file" >&2
        exit 1
    fi

    echo "[observability-schema] $name backend=$backend ok"
}

CASES=(
    intent_trace_abi
    intent_recent_abi
    intent_active_abi
    intent_failure_abi
    authority_failure_abi
)

BACKENDS="${PGY_OBSERVABILITY_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    for name in "${CASES[@]}"; do
        run_case "$backend" "$name"
    done
done

echo "[observability-schema] beta schema ok"
