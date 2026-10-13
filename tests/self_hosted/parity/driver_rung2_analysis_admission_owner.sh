#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:driver-rung2-analysis-admission"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" \
    || fail "PGY_BIN is not runnable"

OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
PROBE_SRC="$ROOT_DIR/tests/self_hosted/fixtures/driver_rung2_mutable_analysis_reject.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/driver_rung2_analysis_admission"
PROBE_BIN="$BUILD_DIR/mutable-analysis-reject.exe"
PROBE_SRC_ARG="$(pgy_selfhost_path_relative_to_root "$PROBE_SRC")"
PROBE_BIN_ARG="$(pgy_selfhost_path_relative_to_root "$PROBE_BIN")"

source_size_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$OWNER")" || fail "source-size measurement failed"
[[ "$source_size_lines" -le 500 ]] \
    || fail "driver_rung2_owner.pgy exceeds its 500-line cap"

mkdir -p "$BUILD_DIR"
(cd "$ROOT_DIR" && "$PGY" \
    "$PROBE_SRC_ARG" --backend=c -o "$PROBE_BIN_ARG" \
    >"$BUILD_DIR/compile.log" 2>&1) \
    || { cat "$BUILD_DIR/compile.log" >&2; fail "mutable analysis probe build failed"; }
PROBE_BIN="$(pgy_select_optional_exe_binary "$PROBE_BIN")"
pgy_require_runnable_binary_here "$LABEL:probe" "$PROBE_BIN" \
    || fail "mutable analysis probe is not runnable"

set +e
(cd "$ROOT_DIR" && "$PROBE_BIN" \
    >"$BUILD_DIR/run.out" 2>"$BUILD_DIR/run.err")
status=$?
set -e
[[ "$status" -ne 0 ]] || fail "raw driver accepted same-shape mutable analysis"
grep -Fq -- 'driver rung-2 semantic analysis deep proof failed' \
    "$BUILD_DIR/run.out" "$BUILD_DIR/run.err" \
    || { cat "$BUILD_DIR/run.out" "$BUILD_DIR/run.err" >&2; fail "raw boundary diagnostic is missing"; }
if grep -Fq -- '[driver-pressure-stage] body-types:start' \
    "$BUILD_DIR/run.out" "$BUILD_DIR/run.err"; then
    fail "raw boundary reached body materialization before rejecting analysis"
fi

echo "[$LABEL] PASS"
