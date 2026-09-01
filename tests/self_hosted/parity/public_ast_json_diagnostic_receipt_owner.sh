#!/usr/bin/env bash
# JSON-selected public AST keeps AST success bytes while relaying only an
# admitted parser-owned diagnostic receipt on failure.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/public_ast_json_diagnostic_receipt"
WORK_DIR="$ROOT_DIR/$WORK_REL"
VALID_REL="examples/hello.pgy"
INVALID_REL="tests/cases/callable_contract_vocabulary/duplicate_cap/main.pgy"
MALFORMED_FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/malformed_public_diagnostic_self_host_driver.c"
SILENT_FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/silent_self_host_driver.c"

fail() {
    echo "[self-host-public-ast-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
command -v "$CC" >/dev/null || fail "missing C compiler"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

(cd "$ROOT_DIR" && "$SELF_DRIVER" --ast "$VALID_REL") \
    >"$WORK_DIR/direct-text.ast" 2>"$WORK_DIR/direct-text.err" ||
    fail "direct text AST request failed"
(cd "$ROOT_DIR" && "$SELF_DRIVER" \
    --ast-json-diagnostic-verified "$VALID_REL") \
    >"$WORK_DIR/direct-json.ast" 2>"$WORK_DIR/direct-json.err" ||
    fail "direct JSON-selected AST request failed"
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$VALID_REL" --ast --error-format=text) \
    >"$WORK_DIR/public-text.ast" 2>"$WORK_DIR/public-text.err" ||
    fail "public text AST request failed"
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$VALID_REL" --ast --error-format=json) \
    >"$WORK_DIR/public-json.ast" 2>"$WORK_DIR/public-json.err" ||
    fail "public JSON-selected AST request failed"
(cd "$ROOT_DIR" && "$PGY" "$VALID_REL" --native-pipeline --ast) \
    >"$WORK_DIR/native.ast" 2>"$WORK_DIR/native.err" ||
    fail "native AST oracle failed"
for artifact in direct-json.ast public-text.ast public-json.ast native.ast; do
    cmp -s "$WORK_DIR/direct-text.ast" "$WORK_DIR/$artifact" ||
        fail "$artifact differs from the installed text AST"
done
for diagnostic in direct-text.err direct-json.err public-text.err \
    public-json.err native.err; do
    [[ ! -s "$WORK_DIR/$diagnostic" ]] ||
        fail "$diagnostic changed a valid AST diagnostic channel"
done

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --ast "$INVALID_REL") \
    >"$WORK_DIR/direct-invalid-text.out" \
    2>"$WORK_DIR/direct-invalid-text.err"
direct_text_rc=$?
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$INVALID_REL" --ast --error-format=text) \
    >"$WORK_DIR/public-invalid-text.out" \
    2>"$WORK_DIR/public-invalid-text.err"
public_text_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" \
    --ast-json-diagnostic-verified "$INVALID_REL") \
    >"$WORK_DIR/direct-invalid-json.out" \
    2>"$WORK_DIR/direct-invalid-json.err"
direct_json_rc=$?
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$INVALID_REL" --ast --error-format=json) \
    >"$WORK_DIR/public-invalid-json.out" \
    2>"$WORK_DIR/public-invalid-json.err"
public_json_rc=$?
set -e
[[ "$direct_text_rc" -ne 0 && "$public_text_rc" -ne 0 && \
   -s "$WORK_DIR/direct-invalid-text.out" && \
   ! -s "$WORK_DIR/direct-invalid-text.err" && \
   ! -s "$WORK_DIR/public-invalid-text.err" ]] ||
    fail "text AST rejection changed its existing channels"
cmp -s "$WORK_DIR/direct-invalid-text.out" \
    "$WORK_DIR/public-invalid-text.out" ||
    fail "public text AST rejection differs from the installed parser"
[[ "$direct_json_rc" -ne 0 && ! -s "$WORK_DIR/direct-invalid-json.err" ]] ||
    fail "direct JSON AST request changed its private wire channel"
head -n 1 "$WORK_DIR/direct-invalid-json.out" | \
    grep -Fxq 'pgy.selfhost.public-diagnostic.v1' ||
    fail "direct AST diagnostic wire marker is missing"
tail -n +2 "$WORK_DIR/direct-invalid-json.out" >"$WORK_DIR/expected.json"
[[ "$public_json_rc" -ne 0 && ! -s "$WORK_DIR/public-invalid-json.out" ]] ||
    fail "public JSON AST rejection did not fail on stderr only"
cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/public-invalid-json.err" ||
    fail "public AST did not relay the exact parser-owned JSON receipt"
! grep -Fq 'pgy.selfhost.public-diagnostic.v1' \
    "$WORK_DIR/public-invalid-json.err" || fail "public AST leaked the private marker"
! grep -Fq '[pipeline timing]' "$WORK_DIR/public-invalid-json.err" ||
    fail "public AST retried the native pipeline"
for fact in '"stage":"parse"' '"layer":"syntax"' \
    '"code":"PGY_PARSE_SYNTAX"' '"cause_ir":"parse:unexpected_token"' \
    '"fix_source":"check-syntax"' 'callable_contract_duplicate_name' \
    'axis: capability' 'name: io_read'; do
    require_text "$WORK_DIR/expected.json" "$fact"
done

set +e
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-self-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$VALID_REL" --ast --error-format=json) \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
set -e
[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing child published JSON-selected AST stdout"
require_text "$WORK_DIR/missing.err" 'self-host driver is unavailable'
! grep -Fq '[pipeline timing]' "$WORK_DIR/missing.err" ||
    fail "missing AST child retried native parsing"

silent_driver="$WORK_DIR/silent-self-driver"
malformed_driver="$WORK_DIR/malformed-self-driver"
if [[ "$PGY" == *.exe ]]; then
    silent_driver="$silent_driver.exe"
    malformed_driver="$malformed_driver.exe"
fi
"$CC" -std=c11 -Wall -Wextra -Werror "$SILENT_FIXTURE" -o "$silent_driver"
"$CC" -std=c11 -Wall -Wextra -Werror "$MALFORMED_FIXTURE" -o "$malformed_driver"
set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$silent_driver" \
    "$PGY" "$VALID_REL" --ast --error-format=json) \
    >"$WORK_DIR/silent.out" 2>"$WORK_DIR/silent.err"
silent_rc=$?
set -e
[[ "$silent_rc" -ne 0 && ! -s "$WORK_DIR/silent.out" ]] ||
    fail "silent AST child was accepted"
require_text "$WORK_DIR/silent.err" 'success without an AST diagnostic payload'
for mode in malformed missing crosswired; do
    set +e
    (cd "$ROOT_DIR" && PGY_PUBLIC_DIAG_FIXTURE_MODE="$mode" \
        PGY_SELF_DRIVER_BIN="$malformed_driver" \
        PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$VALID_REL" --ast --error-format=json) \
        >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    mode_rc=$?
    set -e
    [[ "$mode_rc" -ne 0 && ! -s "$WORK_DIR/$mode.out" ]] ||
        fail "$mode AST receipt was accepted or leaked stdout"
    ! grep -Eq 'pgy\.selfhost\.|\[\{' "$WORK_DIR/$mode.err" ||
        fail "$mode AST receipt leaked a partial child payload"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$mode.err" ||
        fail "$mode AST receipt retried native parsing"
done
require_text "$WORK_DIR/malformed.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/crosswired.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/missing.err" 'failed (exit 1) emitting AST diagnostic'

for mode in --capability-manifest --dir --mir-json; do
    set +e
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
        "$PGY" "$VALID_REL" "$mode" --error-format=json) \
        >"$WORK_DIR/excluded.out" 2>"$WORK_DIR/excluded.err"
    excluded_rc=$?
    set -e
    [[ "$excluded_rc" -ne 0 && ! -s "$WORK_DIR/excluded.out" ]] ||
        fail "$mode JSON was admitted by the AST-only rung"
    require_text "$WORK_DIR/excluded.err" 'outside the installed self-host driver contract'
done
set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" --machine-manifest-json --error-format=json) \
    >"$WORK_DIR/excluded.out" 2>"$WORK_DIR/excluded.err"
machine_rc=$?
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$VALID_REL" --ast --error-format=json --verbose) \
    >"$WORK_DIR/verbose.out" 2>"$WORK_DIR/verbose.err"
verbose_rc=$?
set -e
[[ "$machine_rc" -ne 0 && ! -s "$WORK_DIR/excluded.out" && \
   "$verbose_rc" -ne 0 && ! -s "$WORK_DIR/verbose.out" ]] ||
    fail "AST-only selection relaxed machine-manifest or verbose ownership"
require_text "$WORK_DIR/excluded.err" 'outside the installed self-host driver contract'
require_text "$WORK_DIR/verbose.err" 'outside the installed self-host driver contract'

SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
DRIVER_OWNER="$ROOT_DIR/src/compiler/self_host_driver.c"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_stdout_process_owner.c"
MIR_OWNER="$ROOT_DIR/src/compiler/self_host_mir_diagnostic_stdout_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
require_text "$SELECTION_OWNER" 'flags->dump_ast && flags->diag_format == DIAG_FORMAT_JSON'
require_text "$SELECTION_OWNER" '"--ast-json-diagnostic-verified"'
require_text "$DRIVER_OWNER" 'DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_AST'
require_text "$REQUEST_OWNER" 'DriverCliSourceAstStdout(String, Bool)'
require_text "$REQUEST_OWNER" 'args[0] == "--ast-json-diagnostic-verified"'
require_text "$READ_OWNER" 'CompileSourceToAstArtifactForPublicDiagnosticRequest('
require_text "$PROCESS_OWNER" 'pgy_exec_argv_capture_stdout('
require_text "$PROCESS_OWNER" 'driver_self_host_public_diagnostic_wire_relay('
require_text "$MIR_OWNER" 'DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_MIR'
! grep -Fq 'pgy_exec_argv_capture_stdout(' "$MIR_OWNER" ||
    fail "MIR caller restored a second diagnostic stdout process owner"
! grep -Eq 'PGY_PARSE_SYNTAX|parse:unexpected_token|check-syntax|callable_contract_' \
    "$SELECTION_OWNER" "$DRIVER_OWNER" "$PROCESS_OWNER" "$MIR_OWNER" ||
    fail "C AST transport regained parser diagnostic meaning"
! grep -Eq 'driver_run_pipeline|compiler_emit|system\(' \
    "$DRIVER_OWNER" "$PROCESS_OWNER" "$MIR_OWNER" ||
    fail "AST diagnostic transport regained native/string-shell fallback"

echo "[self-host-public-ast-json-diagnostic] AST bytes, exact parser receipt, and single opaque process owner: PASS"
