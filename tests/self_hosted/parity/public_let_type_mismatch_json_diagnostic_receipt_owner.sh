#!/usr/bin/env bash
# Pergyra-owned let mismatch verdicts retain thirteen contexts while one exact
# assignability identity reaches installed MIR, C, and LLVM requests.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/public_let_type_mismatch_json_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"
PROBE_REL="tests/self_hosted/parity/fixture/public_mir_json_diagnostic_receipt_probe.pgy"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/diagnostic_contract_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"
LET_FIXTURES="bad_arith_assign bad_call_assign bad_clamp_assign bad_fileexists_assign bad_for_body bad_inner_let_type bad_let_type bad_option_payload_let bad_result_unwrap_assign bad_toint_assign bad_tolower_assign bad_tostring_assign bad_while_body"
RETURN_REL="src/self_hosted/semantic/fixture/bad_return_type.pgy"
UNADMITTED_REL="src/self_hosted/semantic/fixture/bad_binop_assign.pgy"

fail() {
    echo "[self-host-public-let-type-mismatch-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

for base in $LET_FIXTURES; do
    rel="src/self_hosted/semantic/fixture/$base.pgy"
    set +e
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
        "$rel") >"$WORK_DIR/$base.text.out" 2>"$WORK_DIR/$base.text.err"
    text_rc=$?
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
        "$rel") >"$WORK_DIR/$base.json.out" 2>"$WORK_DIR/$base.json.err"
    json_rc=$?
    set -e

    [[ "$text_rc" -ne 0 && ! -s "$WORK_DIR/$base.text.err" ]] ||
        fail "$base changed its direct text channels"
    expected_text="$(tr -d '\r' < "$EXPECTED_DIR/$base.diag")"
    actual_text="$(tr -d '\r' < "$WORK_DIR/$base.text.out")"
    [[ "$actual_text" == "$expected_text" ]] ||
        fail "$base changed its text code or expected/actual facts"
    [[ "$json_rc" -ne 0 && ! -s "$WORK_DIR/$base.json.err" ]] ||
        fail "$base changed its private JSON channels"
    grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
        "$WORK_DIR/$base.json.out" || fail "$base lost its wire marker"
    tail -n +2 "$WORK_DIR/$base.json.out" >"$WORK_DIR/$base.expected.json"
    [[ -s "$WORK_DIR/$base.expected.json" ]] ||
        fail "$base produced an empty JSON receipt"
    for fact in \
        '"severity":"error"' \
        '"stage":"semantic"' \
        '"layer":"type"' \
        '"code":"PGY_SEM_TYPE_MISMATCH"' \
        '"cause_ir":"semantic:assignability_check"' \
        '"fix_source":"annotate-or-convert"'; do
        require_text "$WORK_DIR/$base.expected.json" "$fact"
    done
done

run_public_failure() {
    local label="$1" expected="$2" artifact="$3"
    shift 3
    [[ -z "$artifact" ]] || rm -f "$ROOT_DIR/$artifact"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$@") >"$WORK_DIR/$label.out" 2>"$WORK_DIR/$label.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$label.out" ]] ||
        fail "$label did not fail on stderr only"
    cmp -s "$expected" "$WORK_DIR/$label.err" ||
        fail "$label did not relay the exact Pergyra-owned receipt"
    [[ -z "$artifact" || ! -e "$ROOT_DIR/$artifact" ]] ||
        fail "$label published an artifact"
    ! grep -Fq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/$label.err" ||
        fail "$label leaked the private wire marker"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$label.err" ||
        fail "$label retried the native pipeline"
}

run_public_failure mir "$WORK_DIR/bad_let_type.expected.json" "" \
    --mir --error-format=json \
    src/self_hosted/semantic/fixture/bad_let_type.pgy
run_public_failure c "$WORK_DIR/bad_call_assign.expected.json" \
    "$WORK_REL/invalid-c.bin" --error-format=json --backend=c \
    src/self_hosted/semantic/fixture/bad_call_assign.pgy \
    -o "$WORK_REL/invalid-c.bin"
run_public_failure llvm "$WORK_DIR/bad_while_body.expected.json" \
    "$WORK_REL/invalid-llvm.bin" --error-format=json --backend=llvm \
    src/self_hosted/semantic/fixture/bad_while_body.pgy \
    -o "$WORK_REL/invalid-llvm.bin"

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$RETURN_REL") >"$WORK_DIR/return.out" 2>"$WORK_DIR/return.err"
return_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$UNADMITTED_REL") >"$WORK_DIR/unadmitted.out" \
    2>"$WORK_DIR/unadmitted.err"
unadmitted_rc=$?
set -e
[[ "$return_rc" -ne 0 && ! -s "$WORK_DIR/return.err" ]] ||
    fail "shared-identity return verdict changed its private channels"
for fact in \
    '"code":"PGY_SEM_TYPE_MISMATCH"' \
    '"cause_ir":"semantic:assignability_check"' \
    '"fix_source":"annotate-or-convert"'; do
    require_text "$WORK_DIR/return.out" "$fact"
done
[[ "$unadmitted_rc" -ne 0 && ! -s "$WORK_DIR/unadmitted.err" ]] ||
    fail "unadmitted arithmetic verdict changed its private channels"
! grep -q '[^[:space:]]' "$WORK_DIR/unadmitted.out" ||
    fail "unadmitted arithmetic verdict gained let admission"

probe_bin="$WORK_DIR/message-independence-probe"
[[ "$PGY" == *.exe ]] && probe_bin="$probe_bin.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --backend=c -o \
    "$(pgy_path_for_compiler "$PGY" "$probe_bin")") \
    >"$WORK_DIR/probe.compile.out" 2>"$WORK_DIR/probe.compile.err" ||
    fail "message-independence probe did not compile"
"$probe_bin" >"$WORK_DIR/probe.out" 2>"$WORK_DIR/probe.err" ||
    fail "message-independence probe failed"
grep -Fxq 'message-independent' "$WORK_DIR/probe.out" ||
    fail "message wording changed let-type diagnostic identity"

require_text "$DIAGNOSTIC_OWNER" 'if code == "let_type_mismatch" {'
require_text "$DIAGNOSTIC_OWNER" '"semantic:assignability_check"'
require_text "$DIAGNOSTIC_OWNER" '"annotate-or-convert"'
require_text "$CONTRACT_OWNER" 'let_first.message != let_second.message'
! grep -Eq 'PGY_SEM_TYPE_MISMATCH|semantic:assignability_check|annotate-or-convert' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic let-type authority"

echo "[self-host-public-let-type-mismatch-json-diagnostic] exact assignability identity, thirteen semantic contexts, MIR/C/LLVM relay, and arithmetic exclusion: PASS"
