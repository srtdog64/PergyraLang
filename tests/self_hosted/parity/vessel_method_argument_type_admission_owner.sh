#!/usr/bin/env bash
# Resolved vessel method arguments are checked from the graph-owned target and
# signature before MIR/artifact publication, independently of return shape.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/vessel_method_argument_type_admission"
WORK_DIR="$ROOT_DIR/$WORK_REL"
INVALID_REL="tests/self_hosted/parity/fixture/vessel_method_argument_type_bad.pgy"
VALID_REL="tests/self_hosted/parity/fixture/vessel_method_argument_type_valid.pgy"
GENERAL_REL="src/self_hosted/semantic/fixture/bad_user_arg.pgy"
GRAPH_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy"
VERDICT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
RECEIPT_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"

fail() {
    echo "[self-host-vessel-method-argument-type] $*" >&2
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
    "$INVALID_REL") >"$WORK_DIR/direct.text.out" 2>"$WORK_DIR/direct.text.err"
text_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$INVALID_REL") >"$WORK_DIR/direct.json.out" 2>"$WORK_DIR/direct.json.err"
json_rc=$?
set -e
[[ "$text_rc" -ne 0 && ! -s "$WORK_DIR/direct.text.err" ]] ||
    fail "direct text mode did not reject on stdout only"
for fact in \
    'Code: member_call_arg_type_mismatch' \
    '- expected: V' \
    '- actual: Int'; do
    require_text "$WORK_DIR/direct.text.out" "$fact"
done
[[ "$json_rc" -ne 0 && ! -s "$WORK_DIR/direct.json.err" ]] ||
    fail "direct JSON mode changed its private channels"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/direct.json.out" ||
    fail "direct JSON mode lost its wire marker"
tail -n +2 "$WORK_DIR/direct.json.out" >"$WORK_DIR/expected.json"
for fact in \
    '"severity":"error"' \
    '"stage":"semantic"' \
    '"layer":"type"' \
    '"code":"PGY_SEM_TYPE_MISMATCH"' \
    '"cause_ir":"semantic:call:arg_type_mismatch"' \
    '"fix_source":"align-arg-type"'; do
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
        fail "$label published output for an invalid method call"
    cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/$label.err" ||
        fail "$label did not relay the exact Pergyra-owned receipt"
    [[ -z "$artifact" || ! -e "$ROOT_DIR/$artifact" ]] ||
        fail "$label published an invalid artifact"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$label.err" ||
        fail "$label retried the native pipeline"
}

run_public_failure mir "" --mir --error-format=json "$INVALID_REL"
run_public_failure c "$WORK_REL/invalid-c.bin" --backend=c \
    --error-format=json "$INVALID_REL" -o "$WORK_REL/invalid-c.bin"
run_public_failure llvm "$WORK_REL/invalid-llvm.bin" --backend=llvm \
    --error-format=json "$INVALID_REL" -o "$WORK_REL/invalid-llvm.bin"

for mode in mir c; do
    native_args=("$INVALID_REL" --native-pipeline --error-format=json)
    if [[ "$mode" == "mir" ]]; then
        native_args+=(--mir)
    else
        native_args+=(--emit-c -o "$WORK_REL/native-invalid.c")
    fi
    set +e
    (cd "$ROOT_DIR" && "$PGY" "${native_args[@]}") \
        >"$WORK_DIR/native-$mode.out" 2>"$WORK_DIR/native-$mode.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/native-$mode.out" ]] ||
        fail "native $mode did not reject the same source"
    for fact in \
        '"code":"PGY_SEM_TYPE_MISMATCH"' \
        '"cause_ir":"semantic:call:arg_type_mismatch"' \
        '"fix_source":"align-arg-type"'; do
        require_text "$WORK_DIR/native-$mode.err" "$fact"
    done
done

set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$GENERAL_REL") >"$WORK_DIR/general.json.out" 2>"$WORK_DIR/general.json.err"
general_rc=$?
set -e
[[ "$general_rc" -ne 0 && ! -s "$WORK_DIR/general.json.err" ]] ||
    fail "general call mismatch changed its private channels"
require_text "$WORK_DIR/general.json.out" '"cause_ir":"semantic:assignability_check"'
require_text "$WORK_DIR/general.json.out" '"fix_source":"annotate-or-convert"'
! grep -Fq '"cause_ir":"semantic:call:arg_type_mismatch"' \
    "$WORK_DIR/general.json.out" || fail "member identity widened to general calls"

(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$VALID_REL") >"$WORK_DIR/valid-direct.out" 2>"$WORK_DIR/valid-direct.err" ||
    fail "valid direct method call left the Pergyra MIR subset"
[[ -s "$WORK_DIR/valid-direct.out" && ! -s "$WORK_DIR/valid-direct.err" ]] ||
    fail "valid direct control changed channels"
(cd "$ROOT_DIR" && "$PGY" --mir "$VALID_REL" --error-format=json) \
    >"$WORK_DIR/valid-public-mir.out" 2>"$WORK_DIR/valid-public-mir.err" ||
    fail "valid public MIR control failed"
(cd "$ROOT_DIR" && "$PGY" "$VALID_REL" --native-pipeline --mir \
    --error-format=json) >"$WORK_DIR/valid-native-mir.out" \
    2>"$WORK_DIR/valid-native-mir.err" || fail "valid native MIR control failed"
valid_bin="$WORK_DIR/valid-c"
[[ "$PGY" == *.exe ]] && valid_bin="$valid_bin.exe"
(cd "$ROOT_DIR" && "$PGY" --backend=c "$VALID_REL" -o \
    "$(pgy_path_for_compiler "$PGY" "$valid_bin")") \
    >"$WORK_DIR/valid-c.out" 2>"$WORK_DIR/valid-c.err" ||
    fail "valid public C control did not compile"
"$valid_bin" || fail "valid public C control did not run"

require_text "$GRAPH_OWNER" 'facts.target.kind != SemanticCallTargetMember()'
require_text "$GRAPH_OWNER" '"member_call_arg_type_mismatch"'
require_text "$VERDICT_OWNER" 'SemanticExpressionGraphResolvedMemberCallArgumentsOwned('
require_text "$RECEIPT_OWNER" 'if code == "member_call_arg_type_mismatch" {'
! grep -Fq 'vessel V' "$GRAPH_OWNER" "$VERDICT_OWNER" ||
    fail "semantic owner gained fixture-specific vessel syntax"
! grep -Fq 'member_call_arg_type_mismatch' "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic member-call authority"

echo "[self-host-vessel-method-argument-type] graph-owned vessel method argument rejection, exact MIR/C/LLVM receipt, native identity, general-call distinction, and valid control: PASS"
