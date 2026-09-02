#!/usr/bin/env bash
# One existing mutation policy owns builtin and direct-index default-parameter
# rejection before its exact receipt reaches installed MIR, C, and LLVM paths.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/public_value_param_collection_mutation_json_diagnostic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"
PROBE_REL="tests/self_hosted/parity/fixture/public_mir_json_diagnostic_receipt_probe.pgy"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/public_diagnostic_receipt_owner.pgy"
CONTRACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/diagnostic_contract_owner.pgy"
ASSIGNMENT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_assignment_type_fact_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
WIRE_OWNER="$ROOT_DIR/src/compiler/self_host_public_diagnostic_wire_owner.c"
MUTATION_FIXTURES="bad_value_param_arraypush bad_value_param_array_index_assign"
EXCLUDED_FIXTURES="bad_import_enum_variant bad_issome_none_call"

fail() {
    echo "[self-host-public-value-param-collection-mutation-json-diagnostic] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] ||
    fail "public launcher or current self-host driver is missing"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

for base in $MUTATION_FIXTURES; do
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
        fail "$base changed its text code or policy facts"
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
        '"code":"PGY_SEM_BUILTIN_ARGS_INVALID"' \
        '"cause_ir":"semantic:builtin:signature_mismatch"' \
        '"fix_source":"match-builtin-signature"'; do
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
    "$WORK_DIR/bad_value_param_array_index_assign.expected.json" "" \
    --mir --error-format=json \
    src/self_hosted/semantic/fixture/bad_value_param_array_index_assign.pgy
run_public_failure c "$WORK_DIR/bad_value_param_arraypush.expected.json" \
    "$WORK_REL/invalid-c.bin" --error-format=json --backend=c \
    src/self_hosted/semantic/fixture/bad_value_param_arraypush.pgy \
    -o "$WORK_REL/invalid-c.bin"
run_public_failure llvm \
    "$WORK_DIR/bad_value_param_array_index_assign.expected.json" \
    "$WORK_REL/invalid-llvm.bin" --error-format=json --backend=llvm \
    src/self_hosted/semantic/fixture/bad_value_param_array_index_assign.pgy \
    -o "$WORK_REL/invalid-llvm.bin"

for base in $EXCLUDED_FIXTURES; do
    rel="src/self_hosted/semantic/fixture/$base.pgy"
    set +e
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
        "$rel") >"$WORK_DIR/$base.excluded.out" \
        2>"$WORK_DIR/$base.excluded.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$base.excluded.err" ]] ||
        fail "$base exclusion changed its private channels"
    ! grep -q '[^[:space:]]' "$WORK_DIR/$base.excluded.out" ||
        fail "$base gained value-parameter admission"
done

probe_bin="$WORK_DIR/message-independence-probe"
[[ "$PGY" == *.exe ]] && probe_bin="$probe_bin.exe"
(cd "$ROOT_DIR" && "$PGY" "$PROBE_REL" --backend=c -o \
    "$(pgy_path_for_compiler "$PGY" "$probe_bin")") \
    >"$WORK_DIR/probe.compile.out" 2>"$WORK_DIR/probe.compile.err" ||
    fail "message-independence probe did not compile"
"$probe_bin" >"$WORK_DIR/probe.out" 2>"$WORK_DIR/probe.err" ||
    fail "message-independence probe failed"
grep -Fxq 'message-independent' "$WORK_DIR/probe.out" ||
    fail "message wording changed mutation diagnostic identity"

require_text "$ASSIGNMENT_OWNER" 'SemanticCollectionMutationError('
require_text "$ASSIGNMENT_OWNER" '"ArraySet", target_root'
require_text "$DIAGNOSTIC_OWNER" \
    'if code == "value_param_collection_mutation" {'
require_text "$DIAGNOSTIC_OWNER" '"semantic:builtin:signature_mismatch"'
require_text "$DIAGNOSTIC_OWNER" '"match-builtin-signature"'
require_text "$CONTRACT_OWNER" \
    'value_param_first.message != value_param_second.message'
require_text "$CONTRACT_OWNER" \
    'unknown, "unregistered_public_diagnostic_code"'
! grep -Eq 'PGY_SEM_BUILTIN_ARGS_INVALID|semantic:builtin:signature_mismatch|match-builtin-signature' \
    "$PROCESS_OWNER" "$WIRE_OWNER" ||
    fail "C transport gained semantic mutation authority"

echo "[self-host-public-value-param-collection-mutation-json-diagnostic] one mutation policy rejects ArrayPush/ArraySet, relays exact MIR/C/LLVM identity, and excludes symbol/missing-oracle codes: PASS"
