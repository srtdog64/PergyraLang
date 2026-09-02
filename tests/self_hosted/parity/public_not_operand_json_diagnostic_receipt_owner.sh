#!/usr/bin/env bash
# One Pergyra-owned logical-not verdict publishes its exact JSON identity
# through public MIR, C, and LLVM requests.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/public_not_operand_json_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
EXPECTED="$ROOT_DIR/src/self_hosted/semantic/expected/bad_not_operand.diag"
NOT_REL="src/self_hosted/semantic/fixture/bad_not_operand.pgy"
CONDITION_REL="src/self_hosted/semantic/fixture/bad_condition_not_bool.pgy"
LOGICAL_REL="src/self_hosted/semantic/fixture/bad_logical_right.pgy"
RETURN_REL="src/self_hosted/semantic/fixture/bad_return_type.pgy"
UNADMITTED_REL="src/self_hosted/semantic/fixture/bad_import_enum_variant.pgy"
PROBE_REL="tests/self_hosted/parity/fixture/public_mir_json_diagnostic_receipt_probe.pgy"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/diagnostic_contract_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"

fail() {
    echo "[self-host-public-not-operand-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$NOT_REL") >"$WORK_DIR/direct.text.out" 2>"$WORK_DIR/direct.text.err"
text_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$NOT_REL") >"$WORK_DIR/direct.json.out" 2>"$WORK_DIR/direct.json.err"
json_rc=$?
set -e
[[ "$text_rc" -ne 0 && ! -s "$WORK_DIR/direct.text.err" ]] ||
    fail "logical-not verdict changed its direct text channels"
expected_text="$(tr -d '\r' < "$EXPECTED")"
actual_text="$(tr -d '\r' < "$WORK_DIR/direct.text.out")"
[[ "$actual_text" == "$expected_text" ]] ||
    fail "logical-not verdict changed its text diagnostic or actual fact"
[[ "$json_rc" -ne 0 && ! -s "$WORK_DIR/direct.json.err" ]] ||
    fail "logical-not verdict changed its private JSON channels"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
    "$WORK_DIR/direct.json.out" || fail "logical-not receipt lost its marker"
tail -n +2 "$WORK_DIR/direct.json.out" >"$WORK_DIR/expected.json"
[[ -s "$WORK_DIR/expected.json" ]] || fail "logical-not JSON receipt is empty"
for fact in \
    '"severity":"error"' \
    '"stage":"semantic"' \
    '"layer":"type"' \
    '"code":"PGY_SEM_UNOP_TYPE_MISMATCH"' \
    '"cause_ir":"semantic:unary_operator:operand"' \
    '"fix_source":"align-operand-type"'; do
    require_text "$WORK_DIR/expected.json" "$fact"
done

run_public_failure() {
    local label="$1" artifact="$2"
    shift 2
    [[ -z "$artifact" ]] || rm -f "$ROOT_DIR/$artifact"
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$@") >"$WORK_DIR/$label.out" 2>"$WORK_DIR/$label.err"
    local rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$label.out" ]] ||
        fail "$label did not fail on stderr only"
    cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/$label.err" ||
        fail "$label did not relay the exact Pergyra-owned receipt"
    [[ -z "$artifact" || ! -e "$ROOT_DIR/$artifact" ]] ||
        fail "$label published an artifact"
    ! grep -Fq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/$label.err" ||
        fail "$label leaked the private wire marker"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$label.err" ||
        fail "$label retried the native pipeline"
}

run_public_failure mir "" --mir --error-format=json "$NOT_REL"
run_public_failure c "$WORK_REL/invalid-c.bin" \
    --error-format=json --backend=c "$NOT_REL" -o "$WORK_REL/invalid-c.bin"
run_public_failure llvm "$WORK_REL/invalid-llvm.bin" \
    --error-format=json --backend=llvm "$NOT_REL" \
    -o "$WORK_REL/invalid-llvm.bin"

for pair in \
    "condition|$CONDITION_REL|semantic:condition:non_bool" \
    "logical|$LOGICAL_REL|semantic:binop:operand_types" \
    "return|$RETURN_REL|semantic:assignability_check"; do
    label="${pair%%|*}"
    rest="${pair#*|}"
    rel="${rest%%|*}"
    cause="${rest#*|}"
    set +e
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
        "$rel") >"$WORK_DIR/$label.distinct.out" \
        2>"$WORK_DIR/$label.distinct.err"
    distinct_rc=$?
    set -e
    [[ "$distinct_rc" -ne 0 && ! -s "$WORK_DIR/$label.distinct.err" ]] ||
        fail "$label distinction changed its private channels"
    require_text "$WORK_DIR/$label.distinct.out" "\"cause_ir\":\"$cause\""
    ! grep -Fq 'semantic:unary_operator:operand' \
        "$WORK_DIR/$label.distinct.out" || fail "$label gained unary identity"
done

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$UNADMITTED_REL") >"$WORK_DIR/unadmitted.out" \
    2>"$WORK_DIR/unadmitted.err"
unadmitted_rc=$?
set -e
[[ "$unadmitted_rc" -ne 0 && ! -s "$WORK_DIR/unadmitted.err" ]] ||
    fail "unadmitted symbol verdict changed its private channels"
! grep -q '[^[:space:]]' "$WORK_DIR/unadmitted.out" ||
    fail "unadmitted symbol verdict gained unary identity"

probe_bin="$WORK_DIR/message-independence-probe"
[[ "$PGY" == *.exe ]] && probe_bin="$probe_bin.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --backend=c -o \
    "$(pgy_path_for_compiler "$PGY" "$probe_bin")") \
    >"$WORK_DIR/probe.compile.out" 2>"$WORK_DIR/probe.compile.err" ||
    fail "message-independence probe did not compile"
"$probe_bin" >"$WORK_DIR/probe.out" 2>"$WORK_DIR/probe.err" ||
    fail "message-independence probe failed"
grep -Fxq 'message-independent' "$WORK_DIR/probe.out" ||
    fail "message wording changed unary diagnostic identity"

require_text "$DIAGNOSTIC_OWNER" 'if code == "not_operand_not_bool" {'
require_text "$DIAGNOSTIC_OWNER" '"semantic:unary_operator:operand"'
require_text "$DIAGNOSTIC_OWNER" '"align-operand-type"'
require_text "$CONTRACT_OWNER" 'unary_first.message != unary_second.message'
! grep -Eq 'not_operand_not_bool|semantic:unary_operator:operand|align-operand-type' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic unary authority"

echo "[self-host-public-not-operand-json-diagnostic] exact unary identity, MIR/C/LLVM relay, admitted-family distinction, and undefined-symbol exclusion: PASS"
