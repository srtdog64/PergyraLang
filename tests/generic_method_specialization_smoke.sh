#!/usr/bin/env bash
# MIR owns generic member specialization identity consumed by both backends.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi

TRANSPILE_TEST="${PGY_TRANSPILE_TEST_BIN:-$ROOT_DIR/bin/test_transpile}"
if [[ "$TRANSPILE_TEST" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${TRANSPILE_TEST}.exe"; then
    TRANSPILE_TEST="${TRANSPILE_TEST}.exe"
fi
if [[ ! -x "$TRANSPILE_TEST" ]]; then
    echo "[generic-method-specialization] missing transpile test binary: $TRANSPILE_TEST" >&2
    exit 1
fi
if [[ ! -x "$PGY" ]]; then
    echo "[generic-method-specialization] missing compiler binary: $PGY" >&2
    exit 1
fi

BUILD_DIR="${PGY_GENERIC_METHOD_BUILD_DIR:-$ROOT_DIR/.tmp/generic_method_specialization}"
FIXTURE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/generic_member_inferred_flow.pgy"
MIR_JSON="$BUILD_DIR/native.mir.json"
mkdir -p "$BUILD_DIR"

(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")") \
    | tr -d '\r' >"$MIR_JSON"
grep -Fq '"generic_method_specializations":[{' "$MIR_JSON" || {
    echo "[generic-method-specialization] MIR specialization row is missing" >&2
    exit 1
}
grep -Fq '"owner":"Box","method":"Echo","symbol":"Box_Echo_Int"' \
    "$MIR_JSON" || {
    echo "[generic-method-specialization] MIR specialization identity drifted" >&2
    exit 1
}
grep -Fq '"generic_params":["T"],"actual_types":["Int"]' \
    "$MIR_JSON" || {
    echo "[generic-method-specialization] MIR generic binding drifted" >&2
    exit 1
}

compile_and_run() {
    local backend="$1"
    local program="$BUILD_DIR/program_${backend}"
    local log="$BUILD_DIR/${backend}.compile.log"
    local output="$BUILD_DIR/${backend}.run"

    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$program")" \
        >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            echo "[generic-method-specialization] llvm leg skipped (LLVM unavailable)"
            return 2
        fi
        cat "$log" >&2
        return 1
    fi
    "$program" | tr -d '\r' >"$output"
    if [[ "$(<"$output")" != "41" ]]; then
        echo "[generic-method-specialization] $backend runtime output drifted" >&2
        cat "$output" >&2
        return 1
    fi
}

compile_and_run c
set +e
compile_and_run llvm
llvm_rc=$?
set -e
if [[ "$llvm_rc" -ne 0 && "$llvm_rc" -ne 2 ]]; then
    exit "$llvm_rc"
fi

for required in \
    "src/compiler/mir_generic_method_specialization.c:mir_generic_method_specializations_capture(" \
    "src/codegen/transpiler_expr_call_member_emit.c:mir_generic_method_specialization_for_call(" \
    "src/codegen/llvm_member_call_emit.c:mir_generic_method_specialization_for_call("; do
    path="${required%%:*}"
    term="${required#*:}"
    grep -Fq "$term" "$ROOT_DIR/$path" || {
        echo "[generic-method-specialization] missing owner/consumer term: $required" >&2
        exit 1
    }
done

PGY_TEST_TMPDIR="$BUILD_DIR/test_tmp" "$TRANSPILE_TEST" \
    generic-method-specialization

echo "[generic-method-specialization] MIR owner, C/LLVM consumers, runtime parity, and C missing-row mutation gate ok"
