#!/usr/bin/env bash
# RIR/AIR/HIR do not yet have complete installed Pergyra producers. Keep their
# native implementations reachable only through the explicit bootstrap/oracle
# opt-out instead of silently selecting the C pipeline.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/public-native-ir-explicit-opt-in"
SOURCE="examples/minimal.pgy"
LAUNCHER="$ROOT_DIR/src/pgy_driver.c"
USAGE_OWNER="$ROOT_DIR/src/compiler/driver_usage.c"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
REJECTION_OWNER="$ROOT_DIR/src/compiler/driver_diag.c"

fail() {
    echo "[self-host-public-native-ir-opt-in] $*" >&2
    exit 1
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || fail "public launcher is missing: $PGY"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

modes=(--rir --rir-json --air --air-json --hir --hir-cfg --hir-dom --hir-ssa)
for mode in "${modes[@]}"; do
    label="${mode#--}"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-self-driver" \
        PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$mode" "$SOURCE" \
        >"$WORK_DIR/$label.public.out" \
        2>"$WORK_DIR/$label.public.err")
    public_rc=$?
    set -e
    [[ "$public_rc" -ne 0 ]] ||
        fail "bare $mode silently entered the native pipeline"
    [[ ! -s "$WORK_DIR/$label.public.out" ]] ||
        fail "bare $mode emitted a partial payload"
    grep -Fq -- "$mode has no installed Pergyra fact owner" \
        "$WORK_DIR/$label.public.err" ||
        fail "bare $mode lost its owned missing-fact diagnostic"
    if grep -Fq '[pipeline timing]' "$WORK_DIR/$label.public.err"; then
        fail "bare $mode reached native pipeline timing"
    fi

    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-self-driver" \
        "$PGY" --native-pipeline "$mode" "$SOURCE" \
        >"$WORK_DIR/$label.native.out" \
        2>"$WORK_DIR/$label.native.err") ||
        fail "explicit native $mode stopped working"
    [[ -s "$WORK_DIR/$label.native.out" ]] ||
        fail "explicit native $mode emitted no diagnostic payload"
done

default_native_calls="$(grep -Fc 'return driver_run_pipeline(&flags);' "$LAUNCHER")"
[[ "$default_native_calls" == 2 ]] ||
    fail "launcher regained a third/default driver_run_pipeline call"
grep -Fq 'driver_emit_uninstalled_self_host_request_fail(&flags)' "$LAUNCHER" ||
    fail "launcher bypassed the uninstalled-request rejection owner"
grep -Fq 'has no installed Pergyra fact owner; use explicit --native-pipeline' \
    "$REJECTION_OWNER" || fail "rejection owner lost the native-only diagnostic"
for mode in --rir --rir-json --air --air-json --hir --hir-cfg --hir-dom --hir-ssa; do
    grep -Fq -- "\"$mode\"" "$SELECTION_OWNER" ||
        fail "selection owner lost the $mode request identity"
    grep -Fq -- "--native-pipeline $mode" "$USAGE_OWNER" ||
        fail "usage does not declare explicit native ownership for $mode"
done

echo "[self-host-public-native-ir-opt-in] bare RIR/AIR/HIR fail closed; explicit native diagnostics remain reachable"
