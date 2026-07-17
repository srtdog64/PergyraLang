#!/usr/bin/env bash
# Focused C/LLVM parity for semantic-owned assignment projections.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:assignment-projection"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[$LABEL] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/assignment_projection"
PATHS_FILE="$BUILD_DIR/codegen_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "$LABEL" "$BUILD_DIR" "codegen-parity-paths" "$PATHS_FILE"

paths=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && paths+=("$line")
done <"$PATHS_FILE"
if [[ "${#paths[@]}" -ne 15 ]]; then
    echo "[$LABEL] expected 15 codegen TestHarness paths, got ${#paths[@]}" >&2
    exit 1
fi

PROBE_SOURCE="$ROOT_DIR/${paths[11]}"
EXPECTED="$ROOT_DIR/${paths[12]}"
ASSIGN_EMITTER="$ROOT_DIR/src/self_hosted/codegen/emission/assign_emit_owner.pgy"
for input in "$PROBE_SOURCE" "$EXPECTED"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

# Registry ratchets:
# - source_assignment_type_rescan: expected type must not be recovered with ExprKind.
# - backend_assignment_type_guess: indexed target type must not come from env lookup.
# - missing_expected_type_success: missing facts must fail in both generated probes.
if grep -Fq 'ExprKind(expr, env)' "$ASSIGN_EMITTER"; then
    echo "[$LABEL] assignment expected type was re-derived from source text" >&2
    exit 1
fi
if grep -Fq 'LookupKindType(env, arr_name' "$ASSIGN_EMITTER"; then
    echo "[$LABEL] indexed assignment target type was guessed by codegen" >&2
    exit 1
fi

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$PROBE_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] \
            && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[$LABEL] backend=$backend compile failed" >&2
        cat "$log" >&2
        exit 1
    fi
}

run_positive() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local raw="$BUILD_DIR/probe_${backend}.raw"
    local out="$BUILD_DIR/probe_${backend}.out"
    local err="$BUILD_DIR/probe_${backend}.err"
    if ! (cd "$ROOT_DIR" && "$bin" >"$raw" 2>"$err"); then
        echo "[$LABEL] backend=$backend positive probe failed" >&2
        cat "$raw" "$err" >&2
        exit 1
    fi
    pgy_selfhost_normalize_text_artifact <"$raw" >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
}

run_missing_type_negative() {
    local backend="$1"
    local mode="$2"
    local diagnostic="$3"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local out="$BUILD_DIR/probe_${backend}.${mode}.out"
    local err="$BUILD_DIR/probe_${backend}.${mode}.err"
    local rc
    set +e
    (cd "$ROOT_DIR" && "$bin" "--$mode" >"$out" 2>"$err")
    rc=$?
    set -e
    if [[ "$rc" -eq 0 ]]; then
        echo "[$LABEL] backend=$backend $mode did not fail closed" >&2
        exit 1
    fi
    if ! grep -Fq "$diagnostic" "$out" "$err"; then
        echo "[$LABEL] backend=$backend $mode diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    fi
}

run_backend() {
    local backend="$1"
    compile_probe "$backend"
    run_positive "$backend"
    run_missing_type_negative "$backend" "missing-expected-type" \
        "assignment expected type is missing"
    run_missing_type_negative "$backend" "missing-target-type" \
        "indexed assignment target type is missing"
}

run_backend c

BACKENDS="${PGY_SELFHOST_ASSIGNMENT_PROJECTION_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    set +e
    compile_probe llvm
    llvm_rc=$?
    set -e
    if [[ "$llvm_rc" -eq 0 ]]; then
        run_positive llvm
        run_missing_type_negative llvm "missing-expected-type" \
            "assignment expected type is missing"
        run_missing_type_negative llvm "missing-target-type" \
            "indexed assignment target type is missing"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.out" "$BUILD_DIR/probe_llvm.out"
    elif [[ "$llvm_rc" -eq 2 ]]; then
        echo "[$LABEL] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        exit "$llvm_rc"
    fi
fi

echo "[$LABEL] semantic assignment projection parity ok"
