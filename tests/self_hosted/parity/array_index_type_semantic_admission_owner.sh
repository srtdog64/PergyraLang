#!/usr/bin/env bash
# Every reached array index consumes its graph-owned scalar type before MIR or
# artifact publication, including an index nested under a call argument.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/array_index_type_semantic_admission"
WORK_DIR="$ROOT_DIR/$WORK_REL"
INVALID_REL="tests/self_hosted/parity/fixture/array_index_type_bad.pgy"
MIN_INVALID_REL="tests/self_hosted/parity/fixture/array_index_type_min_bad.pgy"
VALID_REL="tests/self_hosted/parity/fixture/array_index_type_valid.pgy"
GRAPH_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_scalar_verdict_owner.pgy"
VERDICT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
RECEIPT_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"

fail() {
    echo "[self-host-array-index-type] $*" >&2
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
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    "$MIN_INVALID_REL") >"$WORK_DIR/min.json.out" 2>"$WORK_DIR/min.json.err"
min_rc=$?
set -e

[[ "$text_rc" -ne 0 && ! -s "$WORK_DIR/direct.text.err" ]] ||
    fail "direct text mode did not reject on stdout only"
for fact in \
    'Code: array_index_type_mismatch' \
    '- expected: Int' \
    '- actual: String'; do
    require_text "$WORK_DIR/direct.text.out" "$fact"
done
[[ "$json_rc" -ne 0 && ! -s "$WORK_DIR/direct.json.err" ]] ||
    fail "direct JSON mode changed its private channels"
[[ "$min_rc" -ne 0 && ! -s "$WORK_DIR/min.json.err" ]] ||
    fail "deletion-minimum JSON mode did not reject on stdout only"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
    "$WORK_DIR/direct.json.out" || fail "direct JSON lost its wire marker"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
    "$WORK_DIR/min.json.out" || fail "deletion minimum lost its wire marker"
tail -n +2 "$WORK_DIR/direct.json.out" >"$WORK_DIR/expected.json"
tail -n +2 "$WORK_DIR/min.json.out" >"$WORK_DIR/min.expected.json"
cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/min.expected.json" ||
    fail "deletion minimum changed the owned public identity"
for fact in \
    '"severity":"error"' \
    '"stage":"semantic"' \
    '"layer":"type"' \
    '"code":"PGY_SEM_TYPE_MISMATCH"' \
    '"cause_ir":"semantic:array_access:index_non_int"' \
    '"fix_source":"use-int-index"'; do
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
        fail "$label published output for a non-Int index"
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
run_public_failure min-mir "" --mir --error-format=json "$MIN_INVALID_REL"

for mode in mir c llvm; do
    native_args=("$INVALID_REL" --native-pipeline --error-format=json)
    artifact=""
    if [[ "$mode" == "mir" ]]; then
        native_args+=(--mir)
    elif [[ "$mode" == "c" ]]; then
        artifact="$WORK_REL/native-invalid.c"
        native_args+=(--emit-c -o "$artifact")
    else
        artifact="$WORK_REL/native-invalid-llvm.bin"
        native_args+=(--backend=llvm -o "$artifact")
    fi
    [[ -z "$artifact" ]] || rm -f "$ROOT_DIR/$artifact"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "${native_args[@]}") \
        >"$WORK_DIR/native-$mode.out" 2>"$WORK_DIR/native-$mode.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/native-$mode.out" ]] ||
        fail "native $mode did not reject the same index"
    [[ -z "$artifact" || ! -e "$ROOT_DIR/$artifact" ]] ||
        fail "native $mode published an invalid artifact"
    for fact in \
        '"code":"PGY_SEM_TYPE_MISMATCH"' \
        '"cause_ir":"semantic:array_access:index_non_int"' \
        '"fix_source":"use-int-index"'; do
        require_text "$WORK_DIR/native-$mode.err" "$fact"
    done
done

(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$VALID_REL") >"$WORK_DIR/valid-direct.out" 2>"$WORK_DIR/valid-direct.err" ||
    fail "valid direct index left the Pergyra MIR subset"
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
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$SELF_DRIVER" \
    "$PGY" --backend=c "$VALID_REL" -o \
    "$(pgy_path_for_compiler "$PGY" "$valid_bin")") \
    >"$WORK_DIR/valid-c.out" 2>"$WORK_DIR/valid-c.err" ||
    fail "valid public C control did not compile"
valid_output="$("$valid_bin" | tr -d '\r')" ||
    fail "valid public C control did not run"
[[ "$valid_output" == $'2\n5\n11' ]] ||
    fail "valid public C control changed runtime output"

require_text "$GRAPH_OWNER" 'func SemanticExpressionGraphIndexAccessErrorFromTree('
require_text "$GRAPH_OWNER" 'kind == AstExpressionNodeIndex()'
require_text "$GRAPH_OWNER" '"array_index_type_mismatch"'
require_text "$VERDICT_OWNER" 'SemanticExpressionGraphIndexAccessErrorFromTree('
require_text "$RECEIPT_OWNER" 'if code == "array_index_type_mismatch" {'
! grep -Fq 'primes[' "$GRAPH_OWNER" "$VERDICT_OWNER" ||
    fail "semantic owner gained fixture-specific source spelling"
! grep -Fq 'array_index_type_mismatch' "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained array-index semantic authority"

echo "[self-host-array-index-type] graph-owned nested index rejection, exact MIR/C/LLVM receipt, native identity, deletion minimum, and valid execution: PASS"
