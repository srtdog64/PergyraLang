#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the slot contract changed.
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

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
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

BACKENDS="${PGY_SLOT_CONTRACT_BACKENDS:-c llvm}"
CASE_ROOT="$ROOT_DIR/tests/cases/slot_contract"
WORK_BASE="$ROOT_DIR/.tmp/slot-contract-smoke"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

POSITIVE_CASES=(
    plain_read_write_release
    pin_read_write_cleanup
    secure_token_and_view
    device_slot_async_read
)

REJECT_CASES=(
    released_slot_read
    released_slot_write
    double_release_slot
    secure_wrong_token_write
    secure_released_read
    device_released_read
    device_double_release
)

normalize_runtime_stdout() {
    tr -d '\r' | awk '
        /^[0-9]+ error\(s\), [0-9]+ warning\(s\)$/ { seen_summary = 1; next }
        /^pgy: compiled/ { next }
        seen_summary { print }
    '
}

normalize_first_error() {
    tr -d '\r' | awk '/^\[ERROR\] / { print; found = 1; exit } END { if (!found) exit 1 }'
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
    if command -v diff >/dev/null 2>&1; then
        diff -u "$left" "$right" || true
        return 0
    fi
    echo "--- expected ---"
    cat "$left"
    echo "--- actual ---"
    cat "$right"
}

run_positive_case() {
    local backend="$1"
    local name="$2"
    local entry="$CASE_ROOT/positive/$name/main.pgy"
    local expected="$CASE_ROOT/positive/$name/expected_stdout.txt"
    local out_bin="$WORK_DIR/${name}-${backend}"
    local raw_output
    local actual_file="$WORK_DIR/${name}-${backend}.stdout"
    local expected_file="$WORK_DIR/${name}-${backend}.expected"
    local entry_arg
    local out_arg
    local rc

    if [[ ! -f "$entry" || ! -f "$expected" ]]; then
        echo "[slot-contract] missing positive fixture '$name'" >&2
        exit 1
    fi

    entry_arg="$(pgy_path_for_compiler "$PGY" "$entry")"
    out_arg="$(pgy_path_for_compiler "$PGY" "$out_bin")"

    set +e
    raw_output="$("$PGY" "$entry_arg" --run --backend="$backend" -o "$out_arg" 2>&1)"
    rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "[slot-contract] $name backend=$backend failed (exit=$rc)" >&2
        echo "$raw_output" >&2
        exit "$rc"
    fi
    if ! grep -Fq "0 error(s), 0 warning(s)" <<<"$raw_output"; then
        echo "[slot-contract] $name backend=$backend missing clean compile summary" >&2
        echo "$raw_output" >&2
        exit 1
    fi

    printf '%s' "$raw_output" | normalize_runtime_stdout > "$actual_file"
    tr -d '\r' < "$expected" > "$expected_file"
    if ! files_equal "$expected_file" "$actual_file"; then
        echo "[slot-contract] $name backend=$backend stdout mismatch" >&2
        show_diff "$expected_file" "$actual_file" >&2
        exit 1
    fi
    echo "[slot-contract] positive $name backend=$backend ok"
}

run_reject_case() {
    local backend="$1"
    local name="$2"
    local entry="$CASE_ROOT/reject/$name/main.pgy"
    local expected="$CASE_ROOT/reject/$name/expected_diagnostic.txt"
    local out_bin="$WORK_DIR/${name}-${backend}"
    local raw_output
    local actual_file="$WORK_DIR/${name}-${backend}.diagnostic"
    local expected_file="$WORK_DIR/${name}-${backend}.expected"
    local entry_arg
    local out_arg
    local rc

    if [[ ! -f "$entry" || ! -f "$expected" ]]; then
        echo "[slot-contract] missing reject fixture '$name'" >&2
        exit 1
    fi

    entry_arg="$(pgy_path_for_compiler "$PGY" "$entry")"
    out_arg="$(pgy_path_for_compiler "$PGY" "$out_bin")"

    set +e
    raw_output="$("$PGY" "$entry_arg" --backend="$backend" -o "$out_arg" 2>&1)"
    rc=$?
    set -e
    if [[ "$rc" -eq 0 ]]; then
        echo "[slot-contract] $name backend=$backend unexpectedly compiled" >&2
        echo "$raw_output" >&2
        exit 1
    fi

    if ! printf '%s' "$raw_output" | normalize_first_error > "$actual_file"; then
        echo "[slot-contract] $name backend=$backend did not emit a semantic error" >&2
        echo "$raw_output" >&2
        exit 1
    fi
    cat "$expected" | normalize_first_error > "$expected_file"
    if ! files_equal "$expected_file" "$actual_file"; then
        echo "[slot-contract] $name backend=$backend diagnostic mismatch" >&2
        show_diff "$expected_file" "$actual_file" >&2
        echo "--- raw output ---" >&2
        echo "$raw_output" >&2
        exit 1
    fi
    echo "[slot-contract] reject $name backend=$backend ok"
}

for backend in $BACKENDS; do
    case "$backend" in
        c|llvm) ;;
        *)
            echo "[slot-contract] unknown backend '$backend'" >&2
            exit 1
            ;;
    esac
    for name in "${POSITIVE_CASES[@]}"; do
        run_positive_case "$backend" "$name"
    done
    for name in "${REJECT_CASES[@]}"; do
        run_reject_case "$backend" "$name"
    done
done

echo "[slot-contract] Slot/SecureSlot/DeviceSlot behavior goldens ok ($BACKENDS)"
