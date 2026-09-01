#!/usr/bin/env bash
# JSON-selected public LLVM IR uses the existing typed Pergyra request. C owns
# only option admission, opaque receipt relay, and stdout/file publication.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/public_llvm_ir_json_diagnostic_receipt"
WORK_DIR="$ROOT_DIR/$WORK_REL"
VALID_REL="examples/hello.pgy"
INVALID_REL="tests/cases/callable_contract_vocabulary/duplicate_cap/main.pgy"
FIXTURE="$ROOT_DIR/tests/self_hosted/parity/fixture/malformed_public_diagnostic_self_host_driver.c"

fail() {
    echo "[self-host-public-llvm-ir-json-diagnostic] $*" >&2
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

run_valid_stdout() {
    local label="$1" format="$2" profile="$3"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
        "$PGY" "$VALID_REL" --emit-llvm \
        --error-format="$format" --opt="$profile") \
        >"$WORK_DIR/$label.ll" 2>"$WORK_DIR/$label.err" ||
        fail "$label stdout request failed"
    [[ -s "$WORK_DIR/$label.ll" && ! -s "$WORK_DIR/$label.err" ]] ||
        fail "$label stdout changed artifact or diagnostic channels"
}

run_valid_file() {
    local label="$1" format="$2" profile="$3"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
        "$PGY" "$VALID_REL" --emit-llvm \
        --error-format="$format" --opt="$profile" \
        -o "$WORK_REL/$label.ll") \
        >"$WORK_DIR/$label.out" 2>"$WORK_DIR/$label.err" ||
        fail "$label file request failed"
    [[ -s "$WORK_DIR/$label.ll" && ! -s "$WORK_DIR/$label.err" ]] ||
        fail "$label file changed artifact or diagnostic channels"
}

run_valid_stdout text-release-stdout text release
run_valid_stdout json-release-stdout json release
run_valid_stdout json-dev-stdout json dev
run_valid_file text-release-file text release
run_valid_file json-release-file json release
run_valid_file json-dev-file json dev
for artifact in \
    json-release-stdout.ll json-dev-stdout.ll text-release-file.ll \
    json-release-file.ll json-dev-file.ll; do
    cmp -s "$WORK_DIR/text-release-stdout.ll" "$WORK_DIR/$artifact" ||
        fail "$artifact differs from the owned text release LLVM IR"
done

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" \
    --emit-source-llvm-ir-json-diagnostic-verified "$INVALID_REL" \
    -o "$WORK_REL/direct-invalid.ll") \
    >"$WORK_DIR/direct-invalid.out" 2>"$WORK_DIR/direct-invalid.err"
direct_rc=$?
set -e
[[ "$direct_rc" -ne 0 && ! -s "$WORK_DIR/direct-invalid.err" && \
   ! -e "$WORK_DIR/direct-invalid.ll" ]] ||
    fail "direct JSON diagnostic request changed channels or published LLVM IR"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
    "$WORK_DIR/direct-invalid.out" || fail "direct wire marker is missing"
tail -n +2 "$WORK_DIR/direct-invalid.out" >"$WORK_DIR/expected.json"

run_invalid_public() {
    local label="$1"
    shift
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$INVALID_REL" --emit-llvm --error-format=json "$@") \
        >"$WORK_DIR/$label.out" 2>"$WORK_DIR/$label.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$label.out" ]] ||
        fail "$label did not fail on stderr only"
    cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/$label.err" ||
        fail "$label did not relay the exact Pergyra-owned JSON receipt"
    ! grep -Fq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/$label.err" ||
        fail "$label leaked the private wire marker"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$label.err" ||
        fail "$label retried the native pipeline"
}

run_invalid_public invalid-stdout
run_invalid_public invalid-file -o "$WORK_REL/invalid-file.ll"
[[ ! -e "$WORK_DIR/invalid-file.ll" ]] ||
    fail "invalid JSON file request published LLVM IR"
for fact in \
    '"stage":"parse"' \
    '"layer":"syntax"' \
    '"code":"PGY_PARSE_SYNTAX"' \
    '"cause_ir":"parse:unexpected_token"' \
    '"fix_source":"check-syntax"' \
    'callable_contract_duplicate_name' \
    'axis: capability' \
    'name: io_read'; do
    require_text "$WORK_DIR/expected.json" "$fact"
done

printf 'stale\n' >"$WORK_DIR/missing-file.ll"
set +e
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-self-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$VALID_REL" --emit-llvm --error-format=json \
    -o "$WORK_REL/missing-file.ll") \
    >"$WORK_DIR/missing-file.out" 2>"$WORK_DIR/missing-file.err"
missing_file_rc=$?
(cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
    PGY_SELF_DRIVER_BIN="$WORK_DIR/missing-self-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 \
    "$PGY" "$VALID_REL" --emit-llvm --error-format=json) \
    >"$WORK_DIR/missing-stdout.out" 2>"$WORK_DIR/missing-stdout.err"
missing_stdout_rc=$?
set -e
[[ "$missing_file_rc" -ne 0 && ! -e "$WORK_DIR/missing-file.ll" ]] ||
    fail "missing sibling retained a stale JSON-selected LLVM file"
[[ "$missing_stdout_rc" -ne 0 && ! -s "$WORK_DIR/missing-stdout.out" ]] ||
    fail "missing sibling published JSON-selected LLVM stdout"
require_text "$WORK_DIR/missing-file.err" 'self-host driver is unavailable'
require_text "$WORK_DIR/missing-stdout.err" 'self-host driver is unavailable'
! grep -Fq '[pipeline timing]' "$WORK_DIR/missing-file.err" \
    "$WORK_DIR/missing-stdout.err" || fail "missing sibling retried native LLVM"

malformed_driver="$WORK_DIR/malformed-self-driver"
[[ "$PGY" == *.exe ]] && malformed_driver="$malformed_driver.exe"
"$CC" -std=c11 -Wall -Wextra -Werror "$FIXTURE" -o "$malformed_driver"
for mode in malformed missing crosswired; do
    set +e
    (cd "$ROOT_DIR" && PGY_PUBLIC_DIAG_FIXTURE_MODE="$mode" \
        PGY_DEBUG_PIPELINE_TIMING=1 PGY_SELF_DRIVER_BIN="$malformed_driver" \
        "$PGY" "$VALID_REL" --emit-llvm --error-format=json \
        -o "$WORK_REL/$mode.ll") \
        >"$WORK_DIR/$mode.out" 2>"$WORK_DIR/$mode.err"
    mode_rc=$?
    set -e
    [[ "$mode_rc" -ne 0 && ! -s "$WORK_DIR/$mode.out" && \
       ! -e "$WORK_DIR/$mode.ll" ]] ||
        fail "$mode receipt was accepted or published LLVM IR"
    ! grep -Eq 'pgy\.selfhost\.|\[\{' "$WORK_DIR/$mode.err" ||
        fail "$mode leaked a partial child payload"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$mode.err" ||
        fail "$mode retried the native pipeline"
done
require_text "$WORK_DIR/malformed.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/crosswired.err" 'self-host JSON diagnostic receipt is malformed'
require_text "$WORK_DIR/missing.err" 'without a JSON diagnostic receipt'

SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_llvm_selection_owner.c"
STDOUT_OWNER="$ROOT_DIR/src/compiler/self_host_llvm_ir_stdout_owner.c"
FILE_OWNER="$ROOT_DIR/src/compiler/self_host_llvm_ir_artifact_owner.c"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"
selector_body="$(sed -n '/driver_self_host_llvm_ir_request_supported(/,/^}/p' \
    "$SELECTION_OWNER")"
grep -Fq 'DIAG_FORMAT_JSON' "$SELECTION_OWNER" ||
    fail "LLVM IR selector lost JSON diagnostic admission"
grep -Fq 'emit_json_diagnostic' "$STDOUT_OWNER" ||
    fail "stdout owner lost explicit JSON carriage"
grep -Fq 'emit_json_diagnostic' "$FILE_OWNER" ||
    fail "file owner lost explicit JSON carriage"
! grep -Fq 'workspace.secondary_path, false, false' "$STDOUT_OWNER" \
    "$FILE_OWNER" || fail "LLVM IR wrapper restored hard-coded text carriage"
grep -Fq -- '--emit-source-llvm-ir-json-diagnostic-verified' "$REQUEST_OWNER" ||
    fail "typed Pergyra LLVM JSON request is missing"
! grep -Eq 'driver_diag_code_from_message|driver_route_stage|PGY_PARSE_SYNTAX|parse:unexpected_token|check-syntax' \
    "$SELECTION_OWNER" "$STDOUT_OWNER" "$FILE_OWNER" "$PROCESS_OWNER" ||
    fail "C LLVM transport regained diagnostic meaning"
! grep -Eq 'driver_run_pipeline|compiler_emit_llvm_ir|system\(' \
    "$STDOUT_OWNER" "$FILE_OWNER" "$PROCESS_OWNER" ||
    fail "C LLVM transport regained native or string-shell fallback"
grep -Fq 'flags->runtime_mode == RUNTIME_DEFAULT' <<<"$selector_body" ||
    fail "LLVM IR selector relaxed runtime ownership"

echo "[self-host-public-llvm-ir-json-diagnostic] stdout/file bytes, exact Pergyra receipt, and opaque C relay: PASS"
