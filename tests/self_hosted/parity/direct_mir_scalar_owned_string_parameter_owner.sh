#!/usr/bin/env bash
# Exact String owner-handle transfer reaches its C/LLVM terminal consumer.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-owned-string-parameter"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_owned_string_parameter"; WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_owned_string_parameter.pgy"; MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"; POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_string_parameter_policy_owner.pgy"
CALLABLE_POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"; ROLE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
LITERAL_OPERAND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_operand_admission_owner.pgy"; KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_builtin_signature_owner.pgy"; READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_expression_readiness_owner.pgy"
C_COLLECTION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_expression_owner.pgy"
C_CALL_ARGUMENT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_readonly_ref_owner.pgy"
LLVM_COLLECTION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_collection_expression_owner.pgy"
C_EXPRESSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy"
LLVM_EXPRESSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"
C_STORAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_storage_materialization_owner.pgy"
MEMBER_ARGUMENT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_string_member_argument_owner.pgy"
CALL_RESULT_ARGUMENT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_owned_string_call_result_argument_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'func DirectMirScalarProgramOwnedStringParameterReady(' "$POLICY" ||
    fail "owned String parameter policy is missing"
for term in 'CompilerAbiLayoutStringTypeName()' '== "owner-handle"' \
    '== "none"' '== "direct"' '!signature.parameters.abi_required[ordinal]'; do
    grep -Fq "$term" "$POLICY" || fail "owned String policy omits: $term"
done
grep -Fq 'import "direct_mir_scalar_program_owned_string_parameter_policy_owner.pgy";' \
    "$CALLABLE_POLICY" &&
    grep -Fq 'DirectMirScalarProgramOwnedStringParameterReady(signature, ordinal)' \
        "$CALLABLE_POLICY" ||
    fail "callable parameter admission bypasses the owned String policy"
grep -Fq 'DirectMirScalarProgramOwnedStringParameterReady(' "$ROLE" ||
    fail "parameter-role plan bypasses the owned String policy"
grep -Fq 'DirectMirScalarProgramOwnedStringParameterReady(signature, ordinal)' \
    "$LITERAL_OPERAND" ||
    fail "Array<String> literal bypasses the owned String parameter policy"
grep -Fq 'func DirectMirScalarProgramExprArrayStringDropOwned() -> Int { return 93; }' \
    "$KIND" || fail "owned String drop expression identity is missing"
grep -Fq 'name == "ArrayDropOwnedStrings"' "$SIGNATURE" ||
    fail "owned String drop bypasses the semantic builtin signature"
for term in 'let left_kind: Int = facts.node_kinds[left];' \
    'left_kind == DirectMirScalarProgramExprLocal()' \
    'left_kind == DirectMirScalarProgramExprParameter()'; do
    grep -Fq "$term" "$READINESS" ||
        fail "owned String drop omits an addressable receiver shape: $term"
done
for owner in "$C_COLLECTION" "$LLVM_COLLECTION"; do
    grep -Fq 'CollectionRuntimeCOwnedStringDropFn()' "$owner" ||
        fail "target drop projection bypasses the canonical runtime symbol"
done
for term in 'routines.parameter_carriages[parameter_row] == "owner-handle"' \
    'DirectMirScalarProgramOwnedDirectParameterTypeReady('; do
    grep -Fq "$term" "$C_CALL_ARGUMENT" ||
        fail "C call arguments bypass the admitted owner-handle policy: $term"
done
for owner in "$C_EXPRESSION" "$LLVM_EXPRESSION"; do
    grep -Fq 'DirectMirScalarProgramArrayStringLiteralOwnsElements(' "$owner" ||
        fail "target literal projection re-infers element ownership"
done
grep -Fq 'DirectMirScalarProgramCStringArrayOwnedSingleFn()' \
    "$C_STORAGE" || fail "C omits heap-backed owned literal storage"
for term in 'DirectMirScalarProgramExprLogicalRecordMember()' \
    'facts.node_kinds[base] != DirectMirScalarProgramExprLocal()' \
    'DirectMirScalarProgramLogicalRecordFieldType('; do
    grep -Fq "$term" "$MEMBER_ARGUMENT" ||
        fail "owned String member policy omits: $term"
done
for term in 'target_carriage != "owner-handle"' \
    'facts.node_kinds[argument] != DirectMirScalarProgramExprDirectCall()' \
    'routines.return_types[callable] == CompilerAbiLayoutStringTypeName()'; do
    grep -Fq "$term" "$CALL_RESULT_ARGUMENT" ||
        fail "owned String call-result policy omits: $term"
done
mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"ReleaseOwnedString"' "$MIR" ||
    fail "producer omitted the owner sink"
grep -Fq '"type":"String","carriage":"owner-handle"' "$MIR" ||
    fail "producer omitted the String owner-handle receipt"
grep -Fq 'ReleaseOwnedString(fact.value)' "$MIR" ||
    fail "producer omitted the owned local-record String member"
grep -Fq 'ReleaseOwnedString(BuildOwnedString())' "$MIR" ||
    fail "producer omitted the owned String direct-call result"
grep -Fq '"text":"ArrayDropOwnedStrings(owned_values)"' "$MIR" ||
    fail "producer omitted the terminal owned storage consumer"
printf 'released\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'static void pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0\)' "$artifact" ||
            fail "C omitted the by-value String owner signature"
        grep -Fq 'pgy_as_owned_single(pgy_param_0)' "$artifact" ||
            fail "C kept the owned literal on borrowed stack storage"
        grep -Fq 'pgy_as_drop_owned(&pgy_local_' "$artifact" ||
            fail "C omitted the terminal owned storage consumer"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin"); "${command[@]}" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal void @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\)' "$artifact" ||
            fail "LLVM omitted the by-value String owner signature"
        grep -Fq 'call void @pgy_as_push(ptr %pgy.expr.' "$artifact" ||
            fail "LLVM kept the owned literal on borrowed stack storage"
        grep -Fq 'call void @pgy_as_drop_owned' "$artifact" ||
            fail "LLVM omitted the terminal owned storage consumer"
        "$CLANG" -x ir "$artifact" -o "$bin" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done
for mutation in owned-string-parameter-carriage owned-string-parameter-type \
    owned-string-parameter-pass owned-string-drop-call-target \
    owned-string-call-result-return-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done
echo "[$LABEL] String owner-handle C/LLVM lifecycle and negatives: PASS"
