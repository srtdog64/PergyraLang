#!/usr/bin/env bash
# Inferred Option source-local admission after complete SSA type consensus.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-inferred-option-local"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_inferred_option_local"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_inferred_option_local.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_inferred_option_local_mutations.py"
TYPE_FAMILY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_type_family_owner.pgy"
TYPE_PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_value_type_owner.pgy"
INVENTORY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_inventory_owner.pgy"
LOCAL_REF="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy"
GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
ROUTINE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy"
PHI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy"
OP_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_op_code_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for file in "$MUTATIONS" "$TYPE_FAMILY" "$TYPE_PLAN" "$INVENTORY" "$LOCAL_REF" "$GRAPH" "$ROUTINE" "$PHI_OWNER" "$OP_OWNER"; do
    [[ -f "$file" ]] || fail "missing owner: ${file#"$ROOT_DIR/"}"
done
phi_body="$(sed -n '/func DirectMirScalarCfgPhiValueTypeReady(/,/^}/p' "$PHI_OWNER")"
grep -Fq 'CompilerAbiLayoutOptionIntTypeName()' <<<"$phi_body" ||
    fail "common PhiValue type owner omits Option<Int>"
grep -Fq 'CompilerAbiLayoutOptionStringTypeName()' <<<"$phi_body" ||
    fail "common PhiValue type owner omits Option<String>"
grep -Fq 'func DirectMirScalarCfgOpPhiValue() -> Int { return 29; }' "$OP_OWNER" ||
    fail "common PhiValue operation identity drifted"
! grep -Rq 'DirectMirScalarCfgOpPhiOptionInt' "$ROOT_DIR/src/self_hosted/compiler" ||
    fail "Option<Int>-only phi operation was introduced"
! grep -Rq 'DirectMirScalarCfgOpPhiOptionString' "$ROOT_DIR/src/self_hosted/compiler" ||
    fail "Option<String>-only phi operation was introduced"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'func DirectMirScalarCfgSourceLocalTypeMatchesResolved(' "$TYPE_FAMILY" ||
    fail "type-family owner omitted inferred source-local matching"
grep -Fq 'source_type == "Option<Unknown>"' "$TYPE_FAMILY" ||
    fail "type-family owner omitted the exact inferred Option source type"
grep -Fq 'DirectMirScalarCfgSourceLocalTypeMatchesResolved(' "$INVENTORY" ||
    fail "source-local inventory does not consume resolved type evidence"
grep -Fq 'local_names: Array<String>, local_types: Array<String>' "$INVENTORY" ||
    fail "source-local inventory omitted the aligned local type table"
! grep -Fq 'DirectMirScalarCfgLocalInventoryReady(' "$LOCAL_REF" ||
    fail "LocalRef normalization still admits inventory before type consensus"
for owner in "$GRAPH" "$ROUTINE"; do
    type_line="$(grep -n 'DirectMirScalarCfgValueTypePlanFromOwners(' "$owner" | head -n1 | cut -d: -f1)"
    inventory_line="$(grep -n 'DirectMirScalarCfgLocalInventoryReady(' "$owner" | head -n1 | cut -d: -f1)"
    [[ -n "$type_line" && -n "$inventory_line" && "$type_line" -lt "$inventory_line" ]] ||
        fail "inventory admission does not follow complete type planning"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"selected","type":"Option<Unknown>"' "$MIR" ||
    fail "producer omitted the inferred Option source-local row"
grep -Fq '"result":"selected.1"' "$MIR" ||
    fail "producer omitted the initial selected definition"
grep -Fq '"abi_type_name":"Option<Int>"' "$MIR" ||
    fail "producer omitted concrete Option<Int> SSA evidence"
[[ "$(grep -o '"abi_type_name":"Option<Int>"' "$MIR" | wc -l)" -ge 2 ]] ||
    fail "producer omitted multiple concrete Option<Int> definitions"
grep -Fq '"kind":"phi","name":"selected"' "$MIR" ||
    fail "producer omitted the selected Option<Int> phi"
grep -Fq '"kind":"phi","name":"chosen"' "$MIR" ||
    fail "producer omitted the chosen Option<String> phi"

printf '29\n11\nright\nleft\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$binary")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        "$CLANG" -x ir "$artifact" -o "$binary" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$binary" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in source-type-mismatch foreign-local-identity mixed-concrete-types \
    wrong-phi-incoming-type duplicate-phi-incoming missing-phi-incoming \
    string-mixed-concrete-types; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$MUTATIONS" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] inferred Option local C/LLVM parity + identity/type negatives: PASS"
