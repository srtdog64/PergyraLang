#!/usr/bin/env bash
# The Pergyra expression-graph owner rejects an invalid nested collection call
# before MIR publication while a valid nested scalar call remains executable.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/nested_collection_call_arity_semantic_admission"
WORK_DIR="$ROOT_DIR/$WORK_REL"
INVALID_REL="tests/self_hosted/parity/fixture/nested_setsize_arity_invalid.pgy"
EXPECTED="$ROOT_DIR/tests/self_hosted/parity/fixture/nested_setsize_arity_expected.diag"
VALID_REL="tests/self_hosted/parity/fixture/nested_setsize_arity_valid.pgy"
VERDICT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_concrete_scalar_verdict_owner.pgy"
PROTOCOL_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_collection_call_protocol_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"

fail() {
    echo "[self-host-nested-collection-call-arity] $*" >&2
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
expected_text="$(tr -d '\r' < "$EXPECTED")"
actual_text="$(tr -d '\r' < "$WORK_DIR/direct.text.out")"
[[ "$actual_text" == "$expected_text" ]] ||
    fail "direct text mode lost exact nested arity facts"
[[ "$json_rc" -ne 0 && ! -s "$WORK_DIR/direct.json.err" ]] ||
    fail "direct JSON mode changed its private channels"
grep -Fxq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/direct.json.out" ||
    fail "direct JSON mode lost its wire marker"
tail -n +2 "$WORK_DIR/direct.json.out" >"$WORK_DIR/expected.json"
for fact in \
    '"severity":"error"' \
    '"stage":"semantic"' \
    '"layer":"type"' \
    '"code":"PGY_SEM_BUILTIN_ARGS_INVALID"' \
    '"cause_ir":"semantic:builtin:signature_mismatch"' \
    '"fix_source":"match-builtin-signature"'; do
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
        fail "$label published output for invalid nested arity"
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
        '"code":"PGY_SEM_BUILTIN_ARGS_INVALID"' \
        '"cause_ir":"semantic:builtin:signature_mismatch"' \
        '"fix_source":"match-builtin-signature"'; do
        require_text "$WORK_DIR/native-$mode.err" "$fact"
    done
done

(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-diagnostic-verified \
    "$VALID_REL") >"$WORK_DIR/valid-direct.out" 2>"$WORK_DIR/valid-direct.err" ||
    fail "valid nested SetSize left the Pergyra MIR subset"
[[ -s "$WORK_DIR/valid-direct.out" && ! -s "$WORK_DIR/valid-direct.err" ]] ||
    fail "valid direct control changed channels"
(cd "$ROOT_DIR" && "$PGY" --mir "$VALID_REL" --error-format=json) \
    >"$WORK_DIR/valid-public-mir.out" 2>"$WORK_DIR/valid-public-mir.err" ||
    fail "valid public MIR control failed"
[[ -s "$WORK_DIR/valid-public-mir.out" && ! -s "$WORK_DIR/valid-public-mir.err" ]] ||
    fail "valid public MIR control changed channels"
valid_bin="$WORK_DIR/valid-c"
[[ "$PGY" == *.exe ]] && valid_bin="$valid_bin.exe"
(cd "$ROOT_DIR" && "$PGY" --backend=c "$VALID_REL" -o \
    "$(pgy_path_for_compiler "$PGY" "$valid_bin")") \
    >"$WORK_DIR/valid-c.out" 2>"$WORK_DIR/valid-c.err" ||
    fail "valid public C control did not compile"
"$valid_bin" || fail "valid public C control did not run"

require_text "$PROTOCOL_OWNER" 'if name == "SetSize" {'
require_text "$PROTOCOL_OWNER" 'true, "Set", name, 1, 0, -1, -1, "Int"'
require_text "$VERDICT_OWNER" \
    'if expected_count != ArrayLength(call.argument_nodes) {'
! grep -Fq 'SetSize' "$VERDICT_OWNER" ||
    fail "concrete scalar traversal gained a function-name table"
! grep -Eq 'PGY_SEM_BUILTIN_ARGS_INVALID|semantic:builtin:signature_mismatch|match-builtin-signature' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic nested-call authority"

echo "[self-host-nested-collection-call-arity] graph-owned nested SetSize arity rejects before MIR, exact public MIR/C receipt, native identity, and valid-call control: PASS"
