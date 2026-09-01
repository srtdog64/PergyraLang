#!/usr/bin/env bash
# Parser-owned callable-contract rejection reaches every installed public JSON
# path as one typed receipt. C owns only opaque process transport.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_parser_callable_contract_json_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURE_REL="tests/cases/callable_contract_vocabulary/duplicate_cap/main.pgy"
VALID_REL="examples/hello.pgy"
PARSER_OWNER="$ROOT_DIR/src/self_hosted/parser/diagnostic_owner.pgy"
WIRE_OWNER="$ROOT_DIR/src/self_hosted/lib/public_diagnostic_receipt_owner.pgy"
SEMANTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
PIPELINE_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_pipeline_owner.pgy"
SOURCE_MIR_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"
C_MIR_OWNER="$ROOT_DIR/src/compiler/self_host_mir_diagnostic_stdout_owner.c"
C_ARTIFACT_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
C_WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"

fail() {
    echo "[self-host-public-parser-callable-contract-json-diagnostic] $*" >&2
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

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$FIXTURE_REL") >"$WORK_DIR/direct-json.out" \
    2>"$WORK_DIR/direct-json.err"
direct_json_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$FIXTURE_REL") >"$WORK_DIR/direct-text.out" \
    2>"$WORK_DIR/direct-text.err"
direct_text_rc=$?
set -e

[[ "$direct_json_rc" -ne 0 && ! -s "$WORK_DIR/direct-json.err" ]] ||
    fail "direct JSON parser rejection did not fail on stdout only"
[[ "$direct_text_rc" -ne 0 && ! -s "$WORK_DIR/direct-text.err" ]] ||
    fail "direct text parser rejection did not retain stdout-only transport"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/direct-json.out" ||
    fail "direct JSON parser rejection lost its public wire schema"
tail -n +2 "$WORK_DIR/direct-json.out" >"$WORK_DIR/expected-public.json"
for fact in \
    '"severity":"error"' \
    '"stage":"parse"' \
    '"layer":"syntax"' \
    '"code":"PGY_PARSE_SYNTAX"' \
    '"cause_ir":"parse:unexpected_token"' \
    '"fix_source":"check-syntax"' \
    'callable_contract_duplicate_name' \
    'axis: capability' \
    'name: io_read'; do
    require_text "$WORK_DIR/expected-public.json" "$fact"
done
for fact in \
    'Diagnostic: pgy.selfhost.parse.v1' \
    'Code: callable_contract_duplicate_name' \
    '- axis: capability' \
    '- name: io_read'; do
    require_text "$WORK_DIR/direct-text.out" "$fact"
done
! grep -Eq 'pgy\.selfhost\.public-diagnostic|"severity":"error"' \
    "$WORK_DIR/direct-text.out" ||
    fail "text mode changed to the public JSON envelope"

for mode in mir c llvm; do
    artifact="$WORK_REL/invalid-$mode.bin"
    args=(--error-format=json --backend="$mode" "$FIXTURE_REL" -o "$artifact")
    [[ "$mode" == mir ]] &&
        args=(--mir --error-format=json "$FIXTURE_REL")
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
        "$PGY" "${args[@]}") >"$WORK_DIR/public-$mode.out" \
        2>"$WORK_DIR/public-$mode.err"
    mode_rc=$?
    set -e
    [[ "$mode_rc" -ne 0 && ! -s "$WORK_DIR/public-$mode.out" ]] ||
        fail "public $mode parser rejection did not fail on stderr only"
    cmp -s "$WORK_DIR/expected-public.json" "$WORK_DIR/public-$mode.err" ||
        fail "public $mode did not relay the exact parser-owned JSON receipt"
    [[ "$mode" == mir || ! -e "$ROOT_DIR/$artifact" ]] ||
        fail "public $mode parser rejection published a partial artifact"
    ! grep -Eq 'pgy\.selfhost\.public-diagnostic|malformed receipt|\[pipeline timing\]' \
        "$WORK_DIR/public-$mode.err" ||
        fail "public $mode leaked its wire marker, fallback, or native timing"
done

(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" "$PGY" --mir --error-format=json \
    "$VALID_REL") >"$WORK_DIR/valid.out" 2>"$WORK_DIR/valid.err" ||
    fail "valid JSON-selected public MIR request failed"
[[ -s "$WORK_DIR/valid.out" && ! -s "$WORK_DIR/valid.err" ]] ||
    fail "valid JSON-selected public MIR output changed channels"
! grep -Eq 'pgy\.selfhost\.public-diagnostic|"severity":"error"' \
    "$WORK_DIR/valid.out" || fail "valid public MIR emitted a diagnostic"

for term in \
    'enum ParseDiagnosticProjection' \
    'ParsePublicDiagnosticIdentityForOwnedCode(' \
    'SelfHostPublicDiagnosticReceiptWireFromOwnedFacts('; do
    require_text "$PARSER_OWNER" "$term"
done
require_text "$PIPELINE_OWNER" \
    'CompileSourceToAstArtifactForPublicDiagnosticRequest('
require_text "$SOURCE_MIR_OWNER" \
    'DriverSourceMirRequestEmitsJsonDiagnostic(request)'
require_text "$SOURCE_MIR_OWNER" \
    'CompileSourceToAstArtifactForPublicDiagnosticRequest('
! grep -Fq 'CompileSourceToAstArtifact(source_path)' "$SOURCE_MIR_OWNER" ||
    fail "source-MIR regained the diagnostic-unaware parser path"
require_text "$SEMANTIC_OWNER" \
    'SelfHostPublicDiagnosticReceiptWireFromOwnedFacts('
require_text "$WIRE_OWNER" 'SelfHostPublicDiagnosticReceiptSchema()'

mapfile -t pgy_wire_literals < <(
    grep -RIl --include='*.pgy' 'pgy.selfhost.public-diagnostic.v1' \
        "$ROOT_DIR/src/self_hosted"
)
[[ "${#pgy_wire_literals[@]}" -eq 1 &&
   "${pgy_wire_literals[0]}" == "$WIRE_OWNER" ]] ||
    fail "Pergyra public diagnostic wire schema regained multiple owners"
! grep -Eq 'Args\(|GetEnv|Environment|driver_diag_code_from_message' \
    "$PARSER_OWNER" "$PIPELINE_OWNER" "$SOURCE_MIR_OWNER" ||
    fail "parser diagnostic projection regained hidden CLI or environment authority"
! grep -Eq 'driver_diag_code_from_message|driver_route_stage|PGY_PARSE_SYNTAX|parse:unexpected_token|check-syntax' \
    "$C_MIR_OWNER" "$C_ARTIFACT_OWNER" "$C_WIRE_OWNER" ||
    fail "C transport regained parser diagnostic identity"
! grep -Eq 'driver_run_pipeline|mir_dump|system\(' \
    "$C_MIR_OWNER" "$C_ARTIFACT_OWNER" ||
    fail "C transport regained native retry or string-shell fallback"

echo "[self-host-public-parser-callable-contract-json-diagnostic] parser-owned MIR/C/LLVM receipt, text preservation, and opaque C relay: PASS"
