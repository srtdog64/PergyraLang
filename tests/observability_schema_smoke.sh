#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
CASE_ROOT="$ROOT_DIR/tests/cases/abi_pipeline"

if [[ ! -x "$PGY" ]]; then
    echo "[observability-schema] missing compiler binary: $PGY" >&2
    exit 1
fi

SCHEMA_DOC="$ROOT_DIR/docs/112_observability_trace_schema.md"
if [[ ! -f "$SCHEMA_DOC" ]]; then
    echo "[observability-schema] missing schema doc: $SCHEMA_DOC" >&2
    exit 1
fi

for required in \
    "Observability And Trace Schema Beta Contract" \
    "beta-freeze-source-of-truth" \
    "IntentLast*" \
    "IntentHistory*" \
    "IntentActive*" \
    "IntentRecent*" \
    "runtime-borrowed strings" \
    "authority-token-mismatch" \
    "newest-first" \
    "General event streaming schema" \
    "Structured JSON trace export" \
    "make observability-schema-test-smoke"; do
    if ! grep -Fq "$required" "$SCHEMA_DOC"; then
        echo "[observability-schema] schema doc missing: $required" >&2
        exit 1
    fi
done

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

normalize_output() {
    tr -d '\r' | sed -E \
        -e '/^[0-9]+ error\(s\), [0-9]+ warning\(s\)$/d' \
        -e '/^\[WARNING\] /d' \
        -e '/^pgy: compiled/d' \
        -e '/^pgy: compiled \(LLVM\)/d' \
        -e '/^--- output ---$/d' \
        -e '/^--- end ---$/d' | awk 'seen || length($0) > 0 { print; seen = 1 }'
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

    raw_output="$("$PGY" "$entry" --run --backend="$backend" -o "$out_bin" 2>&1)"
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
