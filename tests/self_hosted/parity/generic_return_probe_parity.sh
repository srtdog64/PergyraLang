#!/usr/bin/env bash
# Focused C/LLVM proof for exact and composite generic return substitution.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 ||
    ! command -v tr >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:generic-return"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/generic_return}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/main.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/expected.txt"
CALLABLE_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/callable_resolution_expected.txt"
MISMATCH_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/mismatch_expected.txt"
NESTED_MISMATCH_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/nested_mismatch_expected.txt"
EXPLICIT_MISMATCH_EXPECTED="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/explicit_mismatch_expected.txt"
EXPLICIT_OK="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/explicit_ok.pgy"
EXPLICIT_MISMATCH="$ROOT_DIR/src/self_hosted/tools/generic_return_probe/explicit_mismatch.pgy"
mkdir -p "$BUILD_DIR"

for input in "$SOURCE" "$EXPECTED" "$CALLABLE_EXPECTED" "$MISMATCH_EXPECTED" \
    "$NESTED_MISMATCH_EXPECTED" \
    "$EXPLICIT_MISMATCH_EXPECTED" "$EXPLICIT_OK" "$EXPLICIT_MISMATCH"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    # This executable is parity-harness infrastructure. The public direct-MIR
    # LLVM route is a bounded replacement classifier and is covered by its own
    # replacement gates; it must not become the compiler for this source-level
    # C/LLVM semantic oracle pair.
    local native_subject="--native-pipeline"
    rm -f "$bin" "$BUILD_DIR/probe_${backend}.o"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend="$backend" \
        $native_subject \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[$LABEL] backend=$backend compile failed" >&2
        cat "$log" >&2
        exit 1
    fi
}

run_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local out="$BUILD_DIR/probe_${backend}.out"
    local mismatch="$BUILD_DIR/probe_${backend}.mismatch.out"
    local nested_mismatch="$BUILD_DIR/probe_${backend}.nested_mismatch.out"
    local explicit_mismatch="$BUILD_DIR/probe_${backend}.explicit_mismatch.out"
    local callable="$BUILD_DIR/probe_${backend}.callable.out"
    (cd "$ROOT_DIR" && "$bin") >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
    (cd "$ROOT_DIR" && "$bin" --callable-resolution) >"$callable"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$CALLABLE_EXPECTED" "$callable" \
        "run_output"
    local rc=0
    (cd "$ROOT_DIR" && "$bin" --target-mismatch) >"$mismatch" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend mismatch exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$MISMATCH_EXPECTED" "$mismatch" \
        "run_output"
    rc=0
    (cd "$ROOT_DIR" && "$bin" --nested-mismatch) >"$nested_mismatch" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend nested mismatch exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$NESTED_MISMATCH_EXPECTED" \
        "$nested_mismatch" "run_output"
    rc=0
    (cd "$ROOT_DIR" && "$bin" --explicit-mismatch) >"$explicit_mismatch" || rc=$?
    [[ "$rc" -eq 1 ]] || {
        echo "[$LABEL] backend=$backend explicit mismatch exit=$rc, expected=1" >&2
        exit 1
    }
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPLICIT_MISMATCH_EXPECTED" \
        "$explicit_mismatch" "run_output"
}

BACKENDS="${PGY_SELFHOST_GENERIC_RETURN_BACKENDS:-c llvm}"
ran=0
for backend in $BACKENDS; do
    if compile_probe "$backend"; then
        run_probe "$backend"
        ran=$((ran + 1))
    else
        echo "[$LABEL] backend=$backend skipped (LLVM unavailable)"
    fi
done
[[ "$ran" -gt 0 ]] || { echo "[$LABEL] no backend ran" >&2; exit 1; }
echo "[$LABEL] inferred and explicit generic parameter/return parity ok"
