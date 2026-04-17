#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
elif [[ -x "$TMP_PGY" && ( ! -x "$DEFAULT_PGY" || "$TMP_PGY" -nt "$DEFAULT_PGY" ) ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

BACKENDS="${PGY_ABI_PIPELINE_BACKENDS:-c}"
CASE_ROOT="$ROOT_DIR/tests/cases/abi_pipeline"
WORK_BASE="$ROOT_DIR/.tmp/abi-pipeline-smoke"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
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
import pathlib, sys
left = pathlib.Path(sys.argv[1]).read_bytes()
right = pathlib.Path(sys.argv[2]).read_bytes()
raise SystemExit(0 if left == right else 1)
PY
        return $?
    fi

    if command -v git >/dev/null 2>&1; then
        git diff --no-index --quiet -- "$left" "$right"
        return $?
    fi

    if command -v cmp >/dev/null 2>&1; then
        cmp -s "$left" "$right"
        return $?
    fi

    [[ "$(cat "$left")" == "$(cat "$right")" ]]
}

show_diff() {
    local left="$1"
    local right="$2"

    if [[ -n "$PYTHON_BIN" ]]; then
        "$PYTHON_BIN" - "$left" "$right" <<'PY'
import difflib, pathlib, sys
left = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines(True)
right = pathlib.Path(sys.argv[2]).read_text(encoding="utf-8", errors="replace").splitlines(True)
sys.stdout.writelines(difflib.unified_diff(left, right, fromfile=sys.argv[1], tofile=sys.argv[2]))
PY
        return 0
    fi

    if command -v git >/dev/null 2>&1; then
        git --no-pager diff --no-index --no-prefix -- "$left" "$right" || true
        return 0
    fi

    if command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
        return 0
    fi

    echo "--- expected ---"
    cat "$left"
    echo "--- actual ---"
    cat "$right"
    return 0
}

compile_expect_for_case() {
    case "$1" in
        projection_abi) printf '0 error(s), 2 warning(s)' ;;
        *) printf '0 error(s), 0 warning(s)' ;;
    esac
}

run_case() {
    local backend="$1"
    local name="$2"
    local entry="$CASE_ROOT/$name/main.pgy"
    local expected_stdout="$CASE_ROOT/$name/expected_stdout.txt"
    local expected_compile
    local out_bin="$WORK_DIR/${name}_${backend}"
    local raw_output
    local actual_file
    local expected_file

    expected_compile="$(compile_expect_for_case "$name")"
    if [[ ! -f "$entry" ]]; then
        echo "[abi-pipeline] missing case: $entry" >&2
        exit 1
    fi
    if [[ ! -f "$expected_stdout" ]]; then
        echo "[abi-pipeline] missing expected stdout: $expected_stdout" >&2
        exit 1
    fi

    raw_output="$($PGY "$entry" --run --backend="$backend" -o "$out_bin" 2>&1)"

    if ! grep -Fq "$expected_compile" <<<"$raw_output"; then
        echo "[abi-pipeline] $name backend=$backend missing compile diagnostics '$expected_compile'" >&2
        echo "--- output ---" >&2
        echo "$raw_output" >&2
        echo "--------------" >&2
        exit 1
    fi

    actual_file="$(mktemp "$WORK_DIR/${name}_${backend}_actual.XXXXXX")"
    expected_file="$(mktemp "$WORK_DIR/${name}_${backend}_expected.XXXXXX")"
    printf '%s' "$raw_output" | normalize_output > "$actual_file"
    cat "$expected_stdout" | normalize_output > "$expected_file"

    if ! files_equal "$expected_file" "$actual_file"; then
        echo "[abi-pipeline] $name backend=$backend stdout mismatch" >&2
        show_diff "$expected_file" "$actual_file" >&2
        exit 1
    fi

    echo "[abi-pipeline] $name backend=$backend ok"
}

CASES=(
    projection_abi
    zone_projection_abi
    intent_trace_abi
    intent_value_params_abi
    intent_recent_abi
    intent_active_abi
    intent_failure_abi
    world_clone_ownership_abi
    world_handoff_mutation_abi
    world_zone_query_abi
    relation_effect_zone_abi
    relation_effect_propagation_abi
    runtime_floor
)

for backend in $BACKENDS; do
    for name in "${CASES[@]}"; do
        run_case "$backend" "$name"
    done
done
