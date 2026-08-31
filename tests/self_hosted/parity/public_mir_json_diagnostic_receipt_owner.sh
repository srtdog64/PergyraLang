#!/usr/bin/env bash
# Default public MIR JSON failures relay one typed Pergyra receipt. The C
# adapter owns only process transport and an opaque wire-envelope admission.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_mir_json_diagnostic_receipt"
WORK_DIR="$ROOT_DIR/$WORK_REL"
INVALID_REL="$WORK_REL/undefined_function.pgy"
PROBE_REL="tests/self_hosted/parity/fixture/public_mir_json_diagnostic_receipt_probe.pgy"
C_FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/malformed_public_diagnostic_self_host_driver.c"
C_OWNER="$ROOT_DIR/src/compiler/self_host_mir_diagnostic_stdout_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/diagnostic_contract_owner.pgy"
PROTOCOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"

fail() {
    echo "[self-host-public-mir-json-diagnostic-receipt] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
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

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$INVALID_REL") >"$WORK_DIR/direct.out" 2>"$WORK_DIR/direct.err"
direct_rc=$?
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" "$PGY" --mir --error-format=json \
    "$INVALID_REL") >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err"
public_rc=$?
set -e

[[ "$direct_rc" -ne 0 ]] || fail "direct typed diagnostic unexpectedly succeeded"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/direct.out" ||
    fail "direct typed diagnostic lost its wire schema"
[[ "$public_rc" -ne 0 && ! -s "$WORK_DIR/public.out" ]] ||
    fail "public invalid source did not fail on stderr only"
for fact in \
    '"severity":"error"' \
    '"stage":"semantic"' \
    '"layer":"type"' \
    '"code":"PGY_SEM_UNDEFINED_SYMBOL"' \
    '"cause_ir":"semantic:symbol:undefined"' \
    '"fix_source":"import-or-declare-symbol"'; do
    require_text "$WORK_DIR/direct.out" "$fact"
    require_text "$WORK_DIR/public.err" "$fact"
done
! grep -Fq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/public.err" ||
    fail "opaque wire marker leaked through the public boundary"
! grep -Fq '[pipeline timing]' "$WORK_DIR/public.err" ||
    fail "public diagnostic retried the native pipeline"

for mode in text json; do
    args=(--mir examples/hello.pgy)
    [[ "$mode" == json ]] && args=(--mir --error-format=json examples/hello.pgy)
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" "$PGY" "${args[@]}") \
        >"$WORK_DIR/valid.$mode.out" 2>"$WORK_DIR/valid.$mode.err" ||
        fail "valid public MIR $mode diagnostic failed"
done
cmp -s "$WORK_DIR/valid.text.out" "$WORK_DIR/valid.json.out" ||
    fail "JSON error selection changed the valid MIR diagnostic"
cmp -s "$WORK_DIR/valid.text.err" "$WORK_DIR/valid.json.err" ||
    fail "JSON error selection changed valid MIR stderr"

probe_bin="$WORK_DIR/message-independence-probe"
[[ "$PGY" == *.exe ]] && probe_bin="$probe_bin.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --backend=c -o \
    "$(pgy_path_for_compiler "$PGY" "$probe_bin")") \
    >"$WORK_DIR/probe.compile.out" 2>"$WORK_DIR/probe.compile.err" ||
    fail "message-independence probe did not compile"
"$probe_bin" >"$WORK_DIR/probe.out" 2>"$WORK_DIR/probe.err" ||
    fail "message-independence probe failed"
grep -Fxq 'message-independent' "$WORK_DIR/probe.out" ||
    fail "message wording changed typed diagnostic identity"

malformed_driver="$WORK_DIR/malformed-self-driver"
[[ "$PGY" == *.exe ]] && malformed_driver="$malformed_driver.exe"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror "$C_FIXTURE" \
    -o "$malformed_driver"
for mode in malformed missing crosswired; do
    set +e
    (cd "$ROOT_DIR" && PGY_PUBLIC_DIAG_FIXTURE_MODE="$mode" \
        PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$malformed_driver" \
        "$PGY" --mir --error-format=json examples/hello.pgy) \
        >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    mode_rc=$?
    set -e
    [[ "$mode_rc" -ne 0 && ! -s "$WORK_DIR/$mode.out" ]] ||
        fail "$mode receipt was accepted or leaked to stdout"
    ! grep -Eq 'pgy\.selfhost\.|\[\{' "$WORK_DIR/$mode.err" ||
        fail "$mode receipt leaked a partial child payload"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$mode.err" ||
        fail "$mode receipt retried the native pipeline"
done
require_text "$WORK_DIR/malformed.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/crosswired.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/missing.err" 'self-host driver failed'

for term in 'SemanticPublicDiagnosticReceipt' \
    'SemanticPublicDiagnosticReceiptFromOwnedFacts(' \
    'SemanticDiagnosticOracleCode(code)' \
    'SemanticDiagnosticPayloadForPublicBoundary('; do
    require_text "$DIAGNOSTIC_OWNER" "$term"
done
require_text "$CONTRACT_OWNER" 'SemanticPublicDiagnosticReceiptMessageIndependenceReady('
require_text "$PROTOCOL_OWNER" 'SourceMirJsonDiagnostic'
require_text "$PROTOCOL_OWNER" 'DriverSourceMirRequestEmitsJsonDiagnostic('
require_text "$REQUEST_OWNER" 'DriverCliSourceMirJsonDiagnosticStdout(String)'
require_text "$REQUEST_OWNER" 'args[0] == "--emit-mir-json-diagnostic-verified"'
require_text "$READ_OWNER" 'DriverSourceMirJsonDiagnosticPayloadOrDie('
require_text "$C_OWNER" 'driver_self_host_public_diagnostic_wire_relay('
require_text "$C_OWNER" '"--emit-mir-json-diagnostic-verified"'
require_text "$WIRE_OWNER" 'driver_self_host_public_diagnostic_wire_admit('
require_text "$WIRE_OWNER" 'pgy.selfhost.public-diagnostic.v1'
! grep -Eq 'driver_diag_code_from_message|driver_route_stage|PGY_SEM_UNDEFINED_SYMBOL|semantic:symbol:undefined|import-or-declare-symbol' \
    "$C_OWNER" "$WIRE_OWNER" || fail "C transport regained semantic diagnostic authority"
! grep -Eq 'driver_run_pipeline|mir_dump|system\(' "$C_OWNER" ||
    fail "C transport regained a native or string-shell fallback"

echo "[self-host-public-mir-json-diagnostic-receipt] typed Pergyra identity, opaque C relay, wording independence, and fail-closed wire: PASS"
