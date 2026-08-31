#!/usr/bin/env bash
# JSON-selected source artifacts execute one installed Pergyra producer. C
# owns only bounded process capture and opaque receipt relay.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_artifact_json_diagnostic_receipt"
WORK_DIR="$ROOT_DIR/$WORK_REL"
INVALID_REL="$WORK_REL/undefined_function.pgy"
VALID_REL="examples/hello.pgy"
FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/malformed_public_diagnostic_self_host_driver.c"

fail() {
    echo "[self-host-public-artifact-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] && pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" ]] || fail "public launcher is missing: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "installed self-host driver is missing: $SELF_DRIVER"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
cat >"$ROOT_DIR/$INVALID_REL" <<'PGY_SOURCE'
func Main() -> Void {
    MissingSourceMirSurface();
}
PGY_SOURCE

for backend in c llvm; do
    valid_artifact="$WORK_REL/valid-$backend.bin"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
        "$PGY" --error-format=json --backend="$backend" \
        "$VALID_REL" -o "$valid_artifact") \
        >"$WORK_DIR/valid-$backend.out" \
        2>"$WORK_DIR/valid-$backend.err" ||
        fail "valid JSON-selected $backend artifact failed"
    [[ -s "$ROOT_DIR/$valid_artifact" ]] ||
        fail "valid JSON-selected $backend artifact is missing"
    ! grep -Eq 'pgy\.selfhost\.public-diagnostic|"severity":"error"' \
        "$WORK_DIR/valid-$backend.err" ||
        fail "valid JSON-selected $backend artifact emitted a public diagnostic"

    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" --error-format=json --backend="$backend" \
        "$INVALID_REL" -o "$WORK_REL/invalid-$backend.bin") \
        >"$WORK_DIR/invalid-$backend.out" \
        2>"$WORK_DIR/invalid-$backend.err"
    invalid_rc=$?
    set -e
    [[ "$invalid_rc" -ne 0 && ! -s "$WORK_DIR/invalid-$backend.out" ]] ||
        fail "invalid $backend request did not fail on stderr only"
    [[ ! -e "$WORK_DIR/invalid-$backend.bin" ]] ||
        fail "invalid $backend request published an artifact"
    for fact in \
        '"severity":"error"' \
        '"stage":"semantic"' \
        '"layer":"type"' \
        '"code":"PGY_SEM_UNDEFINED_SYMBOL"' \
        '"cause_ir":"semantic:symbol:undefined"' \
        '"fix_source":"import-or-declare-symbol"'; do
        require_text "$WORK_DIR/invalid-$backend.err" "$fact"
    done
    ! grep -Fq 'pgy.selfhost.public-diagnostic.v1' \
        "$WORK_DIR/invalid-$backend.err" || fail "$backend leaked wire marker"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/invalid-$backend.err" ||
        fail "$backend retried the native pipeline"
done

malformed_driver="$WORK_DIR/malformed-self-driver"
[[ "$PGY" == *.exe ]] && malformed_driver="$malformed_driver.exe"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror "$FIXTURE" -o "$malformed_driver"
for mode in malformed missing crosswired; do
    set +e
    (cd "$ROOT_DIR" && PGY_PUBLIC_DIAG_FIXTURE_MODE="$mode" \
        PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$malformed_driver" \
        "$PGY" --error-format=json --backend=c "$VALID_REL" \
        -o "$WORK_REL/$mode.bin") \
        >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    mode_rc=$?
    set -e
    [[ "$mode_rc" -ne 0 && ! -s "$WORK_DIR/$mode.out" ]] ||
        fail "$mode receipt was accepted or leaked to stdout"
    [[ ! -e "$WORK_DIR/$mode.bin" ]] || fail "$mode published an artifact"
    ! grep -Eq 'pgy\.selfhost\.|\[\{' "$WORK_DIR/$mode.err" ||
        fail "$mode leaked a partial child payload"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$mode.err" ||
        fail "$mode retried the native pipeline"
done
require_text "$WORK_DIR/malformed.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/crosswired.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/missing.err" 'without a JSON diagnostic receipt'

PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
C_REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_request_owner.pgy"
MIR_REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy"
require_text "$PROCESS_OWNER" 'driver_self_host_public_diagnostic_wire_relay('
require_text "$PROCESS_OWNER" 'pgy_exec_argv_capture_stdout('
require_text "$REQUEST_OWNER" '--emit-c-artifact-json-diagnostic-verified'
require_text "$REQUEST_OWNER" '--emit-source-llvm-ir-json-diagnostic-verified'
require_text "$C_REQUEST_OWNER" 'DriverSourceCRequestEmitsJsonDiagnostic('
require_text "$MIR_REQUEST_OWNER" 'SourceMirJsonDiagnostic'
! grep -Eq 'driver_diag_code_from_message|driver_route_stage|PGY_SEM_UNDEFINED_SYMBOL|semantic:symbol:undefined|import-or-declare-symbol' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport regained semantic diagnostic authority"
! grep -Eq 'driver_run_pipeline|system\(' "$PROCESS_OWNER" ||
    fail "artifact transport regained native or string-shell fallback"

echo "[self-host-public-artifact-json-diagnostic] C/LLVM success, exact Pergyra failure identity, and fail-closed wire: PASS"
