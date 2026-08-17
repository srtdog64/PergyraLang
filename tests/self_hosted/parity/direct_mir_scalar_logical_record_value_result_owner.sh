#!/usr/bin/env bash
# Logical-record copy lifecycle and exact member rebinds reach C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-value-result"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_value_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_value_result_policy_owner.pgy"
ROLE_PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_role_plan_owner.pgy"
TARGET="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_value_result_target_owner.pgy"
MEMBER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_member_rebind_owner.pgy"
MEMBER_PATH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_member_path_owner.pgy"
INDEXED_ASSIGNMENT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_indexed_assignment_fact_owner.pgy"
READINESS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_assignment_readiness_owner.pgy"
IDENTITY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy"
PARAMETERS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_routine_parameter_set_fact_owner.pgy"
LOCAL_REFS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy"
LOCAL_INVENTORY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_inventory_owner.pgy"
VALUE_TYPES="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_value_type_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
C_LOCALS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_emission_owner.pgy"
LLVM_LOCALS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_local_emission_owner.pgy"
C_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"; LLVM_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy"
C_COPYOUT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_value_result_owner.pgy"
LLVM_COPYOUT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_value_result_owner.pgy"
C_MEMBER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_member_rebind_owner.pgy"
LLVM_MEMBER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_member_rebind_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'return copyout_count >= 1;' "$POLICY" ||
    fail "signature owner does not pin positive logical-record copyouts"
grep -Fq 'signature.parameters.pass_shapes[ordinal] == "direct"' "$POLICY" ||
    fail "parameter identity omits direct copyout carriage"
grep -Fq 'DirectMirScalarProgramLogicalRecordValueResultParameterReady(' "$ROLE_PLAN" ||
    fail "common parameter-role plan omits logical-record copyouts"
grep -Fq 'logical_record_value_result_count' "$ROLE_PLAN" ||
    fail "common parameter-role plan omits the copyout role census"
grep -Fq 'DirectMirScalarProgramLogicalRecordValueResultAt(' "$TARGET" ||
    fail "target-neutral copyout identity is missing"
grep -Fq 'DirectMirScalarProgramLogicalRecordValueResultLocalRow(' "$TARGET" ||
    fail "target-neutral copyout local identity is missing"
grep -Fq 'DirectMirScalarProgramLogicalRecordMemberRebindTargetFromGraph(' "$MEMBER" ||
    fail "member rebind omits exact target identity"
grep -Fq 'DirectMirScalarProgramLogicalRecordMemberPathFromGraphRoot(' "$MEMBER" ||
    fail "member rebind reconstructs the admitted member path"
grep -Fq 'let expected_receiver: Int = base;' "$MEMBER_PATH" ||
    fail "member path omits exact receiver-chain identity"
grep -Fq 'target_rows' "$MEMBER" ||
    fail "member rebind operation omits its persisted target expression"
grep -Fq 'target_root + 1 == target_graph.ready_node_count' "$MEMBER" ||
    fail "member rebind does not admit the exact terminal member root"
! grep -Fq 'target_graph.ready_node_count == 3' "$MEMBER" ||
    fail "member rebind restored the retired one-member target gate"
grep -Fq 'capture.abi_type_name == CompilerAbiLayoutArrayIntTypeName()' "$INDEXED_ASSIGNMENT" &&
    grep -Fq 'capture.abi_type_name == CompilerAbiLayoutArrayStringTypeName()' "$INDEXED_ASSIGNMENT" ||
    fail "indexed-assignment owner can still claim logical-record member rebinds"
grep -Fq 'capture.arg1 == "default_param"' "$MEMBER" ||
    fail "member rebind omits by-value parameter copy admission"
grep -Fq 'capture.arg1 == "local"' "$MEMBER" ||
    fail "member rebind omits exact local target admission"
grep -Fq 'MirRoutineLatestDominatingLocalValueRow(' "$MEMBER" ||
    fail "member rebind omits latest local predecessor identity"
grep -Fq 'predecessor >= 0 && predecessor == latest' "$MEMBER" ||
    fail "member rebind omits target LocalRef join"
grep -Fq 'plan.local_ref_kinds[local_row] ==' "$READINESS" ||
    fail "member rebind readiness omits admitted local identity"
for owner in "$READINESS" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_assignment_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_assignment_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_array_indexed_assignment_owner.pgy"; do
    ! grep -Eq '(^|[[:space:]])let local[[:space:]]*:' "$owner" ||
        fail "logical-record owner reintroduced reserved local as a binding: $owner"
done
grep -Fq 'let parameter: Int = SelfMirLocalRefParameterOrdinalForOwner(' "$READINESS" &&
    grep -Fq 'ordinal != parameter' "$READINESS" ||
    fail "member rebind target expression can consume a foreign predecessor"
grep -Fq 'member_rebind.use_prefix_count' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_definition_route_owner.pgy" ||
    fail "member rebind omits predecessor-use ordering"
grep -Fq 'DirectMirScalarCfgOpLogicalRecordMemberRebind() -> Int { return 40; }' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_op_code_owner.pgy" ||
    fail "member rebind operation identity drifted"
grep -Fq '.field_' "$C_MEMBER" || fail "C member store is missing"
grep -Fq ' = insertvalue ' "$LLVM_MEMBER" || fail "LLVM member insertvalue is missing"
grep -Fq 'logical_record_copyout' "$IDENTITY" ||
    fail "direct-call identity does not consume logical-record copyout facts"
grep -Fq 'DirectMirRoutineLocalParameterOrdinalAtName(' "$PARAMETERS" ||
    fail "parameter owner omits direct value-result local identity"
grep -Fq 'fact.carriages[row] != "value-result"' "$PARAMETERS" ||
    fail "local parameter identity omits value-result carriage"
grep -Fq 'fact.carriages[ordinal] != "value"' "$PARAMETERS" ||
    fail "value-only parameter lookup was widened"
grep -Fq 'DirectMirRoutineLocalParameterOrdinalAtName(' "$LOCAL_REFS" ||
    fail "LocalRef owner does not consume direct value-result identity"
grep -Fq 'DirectMirRoutineLocalParameterTypeAtName(' "$LOCAL_INVENTORY" ||
    fail "local inventory omits direct value-result type identity"
[[ "$(grep -Fc 'DirectMirRoutineLocalParameterTypeAtName(' "$VALUE_TYPES")" -eq 2 ]] ||
    fail "value storage does not consume one direct local parameter type owner"
! grep -Fq 'DirectMirRoutineValueParameterTypeAtName(' "$VALUE_TYPES" ||
    fail "value storage retained the value-only parameter lookup"
grep -Fq 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78' "$PLAN" ||
    fail "GraphPlan schema did not advance with value-result local identity"
for owner in "$C_LOCALS" "$LLVM_LOCALS"; do
    grep -Fq 'carriage != "value-result"' "$owner" ||
        fail "local emission omits value-result copy-in identity"
done
for owner in "$C_EMISSION" "$LLVM_EMISSION"; do
    copyin_last="$(grep -nF 'ValueResultCopyIn(' "$owner" | tail -n 1 | cut -d: -f1)"; local_init="$(grep -nF 'LocalDeclarationsInRange(' "$owner" | head -n 1 | cut -d: -f1)"
    [[ -n "$copyin_last" && -n "$local_init" && "$copyin_last" -lt "$local_init" ]] ||
        fail "value-result copy-in does not precede routine-local initialization: $owner"
done
grep -Fq 'pgy_local_' "$C_COPYOUT" ||
    fail "C copyout does not consume the latest value-result local"
grep -Fq 'source_ptr = Concat("%pgy.local."' "$LLVM_COPYOUT" ||
    fail "LLVM copyout does not consume the latest value-result local"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"ValidationSession"' "$MIR" ||
    fail "producer omitted the record declaration"
grep -Fq '"type":"ValidationSession","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the record copyout identity"
grep -Fq '"type":"IntentCompensationProbe","carriage":"value-result"' "$MIR" ||
    fail "producer omitted the eight-field record copyout identity"
grep -Fq '"name":"IntentStepLines"' "$MIR" ||
    fail "producer omitted the production-shaped composable copyout"
[[ "$(grep -Fo '"type":"IntentCompensationProbe","carriage":"value-result"' "$MIR" | wc -l)" -eq 2 ]] ||
    fail "producer omitted the production-shaped record copyout identity"
grep -Fq '"expr0":"ValidationSession(keys, ids, layouts)"' "$MIR" ||
    fail "producer omitted the whole-record value-result rebind"
for field in keys ids layouts; do
    grep -Fq "\"expr1\":\"session.$field\"" "$MIR" ||
        fail "producer omitted session.$field member rebind"
done
grep -Fq '"arg0":"local_session","arg1":"local"' "$MIR" ||
    fail "producer omitted the local record member rebind"
grep -Fq '"arg0":"scalar","arg1":"local"' "$MIR" ||
    fail "producer omitted the preceding scalar local rebind"
grep -Fq '"expr1":"local_session.keys"' "$MIR" ||
    fail "producer omitted the local member target"
grep -Fq '"expr1":"analysis.expression_surfaces.expression_graph"' "$MIR" ||
    fail "producer omitted the nested value-result member target"
[[ "$(grep -Fo '"arg0":"ArrayPush"' "$MIR" | wc -l)" -ge 3 ]] ||
    fail "producer omitted ordered collection mutations"
printf 'record-copyout-ready\nintent-step-ready\n1\n7\n1\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_0_mutref' "$artifact" ||
            fail "C signature omitted the record mutref"
        grep -Eq 'static int32_t pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0, int32_t pgy_param_1, int32_t pgy_param_2, int32_t pgy_param_3, pgy_scalar_logical_record_value_[0-9]+ \*pgy_param_4_mutref\)' "$artifact" ||
            fail "C omitted the eight-field record copyout signature"
        grep -Eq 'pgy_scalar_logical_record_value_[0-9]+ pgy_param_0 = \*pgy_param_0_mutref;' "$artifact" ||
            fail "C omitted the record copy-in"
        [[ "$(grep -Ec '\*pgy_param_0_mutref = pgy_local_[0-9]+;' "$artifact")" -ge 2 ]] ||
            fail "C omitted an early/final latest-local record copy-out"
        grep -Fq '*pgy_param_10_mutref = pgy_local_' "$artifact" ||
            fail "C omitted production-shaped record copy-out"
        [[ "$(grep -Ec 'pgy_local_[0-9]+\.field_[0-9]+ = pgy_local_[0-9]+;' "$artifact")" -ge 5 ]] ||
            fail "C omitted ordered logical-record member stores"
        grep -Eq 'pgy_local_[0-9]+\.field_[0-9]+\.field_[0-9]+ = pgy_local_[0-9]+;' "$artifact" ||
            fail "C omitted the nested logical-record member store"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        grep -Eq '%pgy\.scalar\.logical\.record\.value\.[0-9]+ = type \{ %pgy\.array\.string, %pgy\.array\.int, %pgy\.array\.string \}' "$artifact" ||
            fail "LLVM record layout drifted"
        grep -Fq 'ptr %pgy.param.0.mutref' "$artifact" ||
            fail "LLVM signature omitted the record mutref"
        grep -Eq 'define internal i64 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, i64 %pgy\.param\.1, i64 %pgy\.param\.2, i64 %pgy\.param\.3, ptr %pgy\.param\.4\.mutref\)' "$artifact" ||
            fail "LLVM omitted the eight-field record copyout signature"
        grep -Eq '%pgy\.param\.0\.local = alloca %pgy\.scalar\.logical\.record\.value\.[0-9]+' "$artifact" ||
            fail "LLVM omitted record copy-in storage"
        [[ "$(grep -Fc '%pgy.param.0.record.copyout.' "$artifact")" -ge 4 ]] ||
            fail "LLVM omitted an early/final record copy-out"
        grep -Fq '%pgy.param.10.record.copyout.' "$artifact" ||
            fail "LLVM omitted production-shaped record copy-out"
        [[ "$(grep -Fc '%pgy.record.rebind.value.' "$artifact")" -ge 5 ]] ||
            fail "LLVM omitted ordered logical-record member insertvalues"
        grep -Eq ' = insertvalue .*, [0-9]+, [0-9]+' "$artifact" ||
            fail "LLVM omitted the nested logical-record member path"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in record-copyout-carriage record-copyout-pass \
    member-field member-rhs-type member-binding member-source-kind \
    member-local-field member-local-ref-missing member-local-ref-foreign \
    member-local-use-missing member-local-use-reordered \
    member-local-result-stale member-local-binding \
    production-copyout-pass production-copyout-abi \
    production-collection-carriage production-readonly-pass \
    production-scalar-carriage nested-member nested-binding nested-rhs-type \
    nested-missing-use; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] logical-record copy-in/out C/LLVM parity + negatives: PASS"
