#!/usr/bin/env bash
# JSON-selected public token dumping preserves token bytes while relaying only
# an admitted lexer-owned diagnostic receipt on failure.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/public_tokens_json_diagnostic_receipt"
WORK_DIR="$ROOT_DIR/$WORK_REL"
VALID_REL="examples/hello.pgy"
INVALID_REL="$WORK_REL/invalid_character.pgy"
MALFORMED_FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/malformed_public_diagnostic_self_host_driver.c"
SILENT_FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/silent_self_host_driver.c"

fail() {
    echo "[self-host-public-tokens-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

normalize() {
    pgy_selfhost_normalize_text_artifact <"$1" >"$2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
command -v "$CC" >/dev/null || fail "missing C compiler"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
cat >"$ROOT_DIR/$INVALID_REL" <<'PGY_SOURCE'
func Main() -> Void {
    ~
}
PGY_SOURCE

(cd "$ROOT_DIR" && "$SELF_DRIVER" --tokens "$VALID_REL") \
    >"$WORK_DIR/direct-text.tokens" 2>"$WORK_DIR/direct-text.err" ||
    fail "direct text token request failed"
(cd "$ROOT_DIR" && "$SELF_DRIVER" \
    --tokens-json-diagnostic-verified "$VALID_REL") \
    >"$WORK_DIR/direct-json.tokens" 2>"$WORK_DIR/direct-json.err" ||
    fail "direct JSON-selected token request failed"
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$VALID_REL" --tokens --error-format=text) \
    >"$WORK_DIR/public-text.tokens" 2>"$WORK_DIR/public-text.err" ||
    fail "public text token request failed"
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$VALID_REL" --tokens --error-format=json) \
    >"$WORK_DIR/public-json.tokens" 2>"$WORK_DIR/public-json.err" ||
    fail "public JSON-selected token request failed"
(cd "$ROOT_DIR" && "$PGY" "$VALID_REL" --native-pipeline --tokens) \
    >"$WORK_DIR/native.tokens" 2>"$WORK_DIR/native.err" ||
    fail "native token oracle failed"
for artifact in direct-json.tokens public-text.tokens public-json.tokens; do
    cmp -s "$WORK_DIR/direct-text.tokens" "$WORK_DIR/$artifact" ||
        fail "$artifact differs from the installed text token stream"
done
normalize "$WORK_DIR/direct-text.tokens" "$WORK_DIR/direct.norm"
normalize "$WORK_DIR/native.tokens" "$WORK_DIR/native.norm"
cmp -s "$WORK_DIR/direct.norm" "$WORK_DIR/native.norm" ||
    fail "installed token stream differs from the native oracle"
for diagnostic in direct-text.err direct-json.err public-text.err \
    public-json.err native.err; do
    [[ ! -s "$WORK_DIR/$diagnostic" ]] ||
        fail "$diagnostic changed a valid token diagnostic channel"
done

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --tokens "$INVALID_REL") \
    >"$WORK_DIR/direct-invalid-text.out" 2>"$WORK_DIR/direct-invalid-text.err"
direct_text_rc=$?
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$INVALID_REL" --tokens --error-format=text) \
    >"$WORK_DIR/public-invalid-text.out" 2>"$WORK_DIR/public-invalid-text.err"
public_text_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" \
    --tokens-json-diagnostic-verified "$INVALID_REL") \
    >"$WORK_DIR/direct-invalid-json.out" 2>"$WORK_DIR/direct-invalid-json.err"
direct_json_rc=$?
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$INVALID_REL" --tokens --error-format=json) \
    >"$WORK_DIR/public-invalid-json.out" 2>"$WORK_DIR/public-invalid-json.err"
public_json_rc=$?
set -e
[[ "$direct_text_rc" -ne 0 && "$public_text_rc" -ne 0 && \
   -s "$WORK_DIR/direct-invalid-text.out" && \
   ! -s "$WORK_DIR/direct-invalid-text.err" && \
   ! -s "$WORK_DIR/public-invalid-text.err" ]] ||
    fail "text token rejection changed its existing channels"
cmp -s "$WORK_DIR/direct-invalid-text.out" \
    "$WORK_DIR/public-invalid-text.out" ||
    fail "public text token rejection differs from the installed lexer"
require_text "$WORK_DIR/direct-invalid-text.out" \
    'LEXER ERROR: invalid source character at line 2'
[[ "$direct_json_rc" -ne 0 && ! -s "$WORK_DIR/direct-invalid-json.err" ]] ||
    fail "direct JSON token request changed its private wire channel"
head -n 1 "$WORK_DIR/direct-invalid-json.out" | \
    grep -Fxq 'pgy.selfhost.public-diagnostic.v1' ||
    fail "direct token diagnostic wire marker is missing"
tail -n +2 "$WORK_DIR/direct-invalid-json.out" >"$WORK_DIR/expected.json"
[[ "$public_json_rc" -ne 0 && ! -s "$WORK_DIR/public-invalid-json.out" ]] ||
    fail "public JSON token rejection did not fail on stderr only"
cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/public-invalid-json.err" ||
    fail "public tokens did not relay the exact lexer-owned JSON receipt"
! grep -Fq 'pgy.selfhost.public-diagnostic.v1' \
    "$WORK_DIR/public-invalid-json.err" || fail "public tokens leaked the private marker"
! grep -Fq '[pipeline timing]' "$WORK_DIR/public-invalid-json.err" ||
    fail "public tokens retried the native pipeline"
for fact in '"stage":"lex"' '"layer":"syntax"' \
    '"code":"PGY_LEX_INVALID_TOKEN"' '"cause_ir":"lex:invalid_token"' \
    '"fix_source":"remove-or-escape-character"' \
    'LEXER ERROR: invalid source character at line 2'; do
    require_text "$WORK_DIR/expected.json" "$fact"
done

set +e
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-self-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$VALID_REL" --tokens --error-format=json) \
    >"$WORK_DIR/missing-child.out" 2>"$WORK_DIR/missing-child.err"
missing_child_rc=$?
set -e
[[ "$missing_child_rc" -ne 0 && ! -s "$WORK_DIR/missing-child.out" ]] ||
    fail "missing child published JSON-selected token stdout"
require_text "$WORK_DIR/missing-child.err" 'self-host driver is unavailable'
! grep -Fq '[pipeline timing]' "$WORK_DIR/missing-child.err" ||
    fail "missing token child retried native lexing"

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
    "$PGY" "$VALID_REL" --tokens --error-format=json) \
    >"$WORK_DIR/silent.out" 2>"$WORK_DIR/silent.err"
silent_rc=$?
set -e
[[ "$silent_rc" -ne 0 && ! -s "$WORK_DIR/silent.out" ]] ||
    fail "silent token child was accepted"
require_text "$WORK_DIR/silent.err" 'success without a token diagnostic payload'
for mode in malformed missing crosswired; do
    set +e
    (cd "$ROOT_DIR" && PGY_PUBLIC_DIAG_FIXTURE_MODE="$mode" \
        PGY_SELF_DRIVER_BIN="$malformed_driver" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$VALID_REL" --tokens --error-format=json) \
        >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    mode_rc=$?
    set -e
    [[ "$mode_rc" -ne 0 && ! -s "$WORK_DIR/$mode.out" ]] ||
        fail "$mode token receipt was accepted or leaked stdout"
    ! grep -Eq 'pgy\.selfhost\.|\[\{' "$WORK_DIR/$mode.err" ||
        fail "$mode token receipt leaked a partial child payload"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$mode.err" ||
        fail "$mode token receipt retried native lexing"
done
require_text "$WORK_DIR/malformed.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/crosswired.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/missing.err" 'failed (exit 1) emitting token diagnostic'

for mode in --capability-manifest --dir --mir-json; do
    set +e
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
        "$PGY" "$VALID_REL" "$mode" --error-format=json) \
        >"$WORK_DIR/excluded.out" 2>"$WORK_DIR/excluded.err"
    excluded_rc=$?
    set -e
    [[ "$excluded_rc" -ne 0 && ! -s "$WORK_DIR/excluded.out" ]] ||
        fail "$mode JSON was admitted by the tokens-only rung"
    require_text "$WORK_DIR/excluded.err" 'outside the installed self-host driver contract'
done
set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" --machine-manifest-json --error-format=json) \
    >"$WORK_DIR/excluded.out" 2>"$WORK_DIR/excluded.err"
machine_rc=$?
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" "$VALID_REL" --tokens --error-format=json --verbose) \
    >"$WORK_DIR/verbose.out" 2>"$WORK_DIR/verbose.err"
verbose_rc=$?
set -e
[[ "$machine_rc" -ne 0 && ! -s "$WORK_DIR/excluded.out" && \
   "$verbose_rc" -ne 0 && ! -s "$WORK_DIR/verbose.out" ]] ||
    fail "tokens-only selection relaxed machine-manifest or verbose ownership"
require_text "$WORK_DIR/excluded.err" 'outside the installed self-host driver contract'
require_text "$WORK_DIR/verbose.err" 'outside the installed self-host driver contract'

SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
DRIVER_OWNER="$ROOT_DIR/src/compiler/self_host_driver.c"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_stdout_process_owner.c"
MIR_OWNER="$ROOT_DIR/src/compiler/self_host_mir_diagnostic_stdout_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
SCAN_OWNER="$ROOT_DIR/src/self_hosted/lexer/scan_owner.pgy"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/lexer/public_diagnostic_receipt_owner.pgy"
require_text "$SELECTION_OWNER" 'flags->dump_tokens && flags->diag_format == DIAG_FORMAT_JSON'
require_text "$SELECTION_OWNER" '"--tokens-json-diagnostic-verified"'
require_text "$DRIVER_OWNER" 'DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_TOKENS'
require_text "$REQUEST_OWNER" 'DriverCliSourceTokensStdout(String, Bool)'
require_text "$REQUEST_OWNER" 'args[0] == "--tokens-json-diagnostic-verified"'
require_text "$READ_OWNER" 'LexContentForPublicDiagnosticRequest('
require_text "$SCAN_OWNER" 'func LexContentFacts(content: String)'
require_text "$SCAN_OWNER" 'LexerInvalidTokenDiagnosticPayloadForPublicBoundary('
for fact in 'PGY_LEX_INVALID_TOKEN' 'lex:invalid_token' \
    'remove-or-escape-character'; do
    require_text "$DIAGNOSTIC_OWNER" "$fact"
done
require_text "$PROCESS_OWNER" 'pgy_exec_argv_capture_stdout('
require_text "$PROCESS_OWNER" 'driver_self_host_public_diagnostic_wire_relay('
require_text "$MIR_OWNER" 'DRIVER_SELF_HOST_PUBLIC_DIAGNOSTIC_STDOUT_MIR'
! grep -Fq 'pgy_exec_argv_capture_stdout(' "$MIR_OWNER" ||
    fail "MIR caller restored a second diagnostic stdout process owner"
! grep -Eq 'PGY_LEX_INVALID_TOKEN|lex:invalid_token|remove-or-escape-character|LEXER ERROR' \
    "$SELECTION_OWNER" "$DRIVER_OWNER" "$PROCESS_OWNER" "$MIR_OWNER" ||
    fail "C token transport regained lexer diagnostic meaning"
! grep -Eq 'driver_run_pipeline|compiler_emit|system\(' \
    "$DRIVER_OWNER" "$PROCESS_OWNER" "$MIR_OWNER" ||
    fail "token diagnostic transport regained native/string-shell fallback"

echo "[self-host-public-tokens-json-diagnostic] token bytes, exact lexer receipt, formatter stability, and shared opaque process owner: PASS"
