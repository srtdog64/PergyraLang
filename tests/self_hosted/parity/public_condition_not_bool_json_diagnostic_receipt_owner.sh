#!/usr/bin/env bash
# Pergyra-owned if/while condition verdicts publish one exact JSON identity
# through public MIR, C, and LLVM requests.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/public_condition_not_bool_json_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"
PROBE_REL="tests/self_hosted/parity/fixture/public_mir_json_diagnostic_receipt_probe.pgy"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/diagnostic_contract_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"
CONDITION_FIXTURES="bad_condition_not_bool bad_while_condition"
RETURN_REL="src/self_hosted/semantic/fixture/bad_return_type.pgy"
LOGICAL_REL="src/self_hosted/semantic/fixture/bad_logical_right.pgy"
UNARY_REL="src/self_hosted/semantic/fixture/bad_not_operand.pgy"

fail() {
    echo "[self-host-public-condition-not-bool-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

for base in $CONDITION_FIXTURES; do
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
        fail "$base changed its text diagnostic or condition facts"
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
        '"cause_ir":"semantic:condition:non_bool"' \
        '"fix_source":"convert-condition-to-bool"'; do
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

run_public_failure mir \
    "$WORK_DIR/bad_condition_not_bool.expected.json" "" \
    --mir --error-format=json \
    src/self_hosted/semantic/fixture/bad_condition_not_bool.pgy
run_public_failure c \
    "$WORK_DIR/bad_while_condition.expected.json" \
    "$WORK_REL/invalid-c.bin" \
    --error-format=json --backend=c \
    src/self_hosted/semantic/fixture/bad_while_condition.pgy \
    -o "$WORK_REL/invalid-c.bin"
run_public_failure llvm \
    "$WORK_DIR/bad_condition_not_bool.expected.json" \
    "$WORK_REL/invalid-llvm.bin" \
    --error-format=json --backend=llvm \
    src/self_hosted/semantic/fixture/bad_condition_not_bool.pgy \
    -o "$WORK_REL/invalid-llvm.bin"

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$RETURN_REL") >"$WORK_DIR/return.out" 2>"$WORK_DIR/return.err"
return_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$LOGICAL_REL") >"$WORK_DIR/logical.out" 2>"$WORK_DIR/logical.err"
logical_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$UNARY_REL") >"$WORK_DIR/unary.out" 2>"$WORK_DIR/unary.err"
unary_rc=$?
set -e
[[ "$return_rc" -ne 0 && ! -s "$WORK_DIR/return.err" ]] ||
    fail "same-public-code return verdict changed its private channels"
require_text "$WORK_DIR/return.out" '"cause_ir":"semantic:assignability_check"'
! grep -Fq 'semantic:condition:non_bool' "$WORK_DIR/return.out" ||
    fail "return verdict gained condition identity"
[[ "$logical_rc" -ne 0 && ! -s "$WORK_DIR/logical.err" ]] ||
    fail "logical verdict changed its private channels"
require_text "$WORK_DIR/logical.out" '"cause_ir":"semantic:binop:operand_types"'
! grep -Fq 'semantic:condition:non_bool' "$WORK_DIR/logical.out" ||
    fail "logical verdict gained condition identity"
[[ "$unary_rc" -ne 0 && ! -s "$WORK_DIR/unary.err" ]] ||
    fail "unary exclusion changed its private channels"
require_text "$WORK_DIR/unary.out" \
    '"cause_ir":"semantic:unary_operator:operand"'
! grep -Fq 'semantic:condition:non_bool' "$WORK_DIR/unary.out" ||
    fail "unary verdict gained the condition identity"

probe_bin="$WORK_DIR/message-independence-probe"
[[ "$PGY" == *.exe ]] && probe_bin="$probe_bin.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --backend=c -o \
    "$(pgy_path_for_compiler "$PGY" "$probe_bin")") \
    >"$WORK_DIR/probe.compile.out" 2>"$WORK_DIR/probe.compile.err" ||
    fail "message-independence probe did not compile"
"$probe_bin" >"$WORK_DIR/probe.out" 2>"$WORK_DIR/probe.err" ||
    fail "message-independence probe failed"
grep -Fxq 'message-independent' "$WORK_DIR/probe.out" ||
    fail "message wording changed condition diagnostic identity"

require_text "$DIAGNOSTIC_OWNER" 'if code == "condition_not_bool" {'
require_text "$DIAGNOSTIC_OWNER" '"semantic:condition:non_bool"'
require_text "$DIAGNOSTIC_OWNER" '"convert-condition-to-bool"'
require_text "$CONTRACT_OWNER" \
    'condition_first.message != condition_second.message'
! grep -Eq 'condition_not_bool|semantic:condition:non_bool|convert-condition-to-bool' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic condition authority"

echo "[self-host-public-condition-not-bool-json-diagnostic] exact condition identity, if/while contexts, MIR/C/LLVM relay, and return/logical/unary distinction: PASS"
