#!/usr/bin/env bash
# Function-scoped ownership-mode ratchet for the reached Direct MIR owners.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LABEL="self-host-parity:direct-mir-mutation-ownership-signatures"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_function_text() {
    local file="$1" function_start="$2" contract="$3"
    awk -v start="$function_start" -v want="$contract" '
        index($0, start) { inside = 1 }
        inside && index($0, want) { found = 1 }
        inside && /^}/ { exit }
        END { exit (inside && found) ? 0 : 1 }
    ' "$ROOT_DIR/$file" || fail "$file $function_start lacks: $contract"
}
while IFS='|' read -r file contract; do
    grep -Fq -- "$contract" "$ROOT_DIR/$file" ||
        fail "$file lacks ownership contract: $contract"
done <<'EOF'
src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy|own plan: DirectMirArrayArgumentPlan
src/self_hosted/compiler/direct_mir_array_int_plan_owner.pgy|own plan: DirectMirArrayIntPlan
src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy|own plan: DirectMirArrayReturnPlan
src/self_hosted/compiler/direct_mir_collection_program_plan_owner.pgy|own plan: DirectMirCollectionProgramPlan
src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_abi_mutation_owner.pgy|own empty: DirectMirScalarCfgProgramExtensionFact
src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_mutation_owner.pgy|own fact: DirectMirScalarCfgRoutinePartitionFact
src/self_hosted/compiler/direct_mir_struct_argument_plan_owner.pgy|own plan: DirectMirStructArgumentPlan
src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_binding_owner.pgy|inout source: DirectMirScalarCfgCollectionPlan
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy|own routine_partition: DirectMirScalarCfgRoutinePartitionFact
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy|own locals: DirectMirScalarCfgLocalRefPlan
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy|own types: DirectMirScalarCfgValueTypePlan
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_verification_owner.pgy|own plan: DirectMirScalarCfgGraphPlan
src/self_hosted/compiler/direct_mir_scalar_cfg_collection_expression_owner.pgy|ref value_names: Array<String>
src/self_hosted/compiler/direct_mir_scalar_cfg_operation_plan_owner.pgy|ref value_local_rows: Array<Int>
src/self_hosted/compiler/direct_mir_scalar_cfg_program_value_storage_owner.pgy|own storage: DirectMirScalarCfgProgramValueStorage
src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_binding_owner.pgy|own source: DirectMirScalarCfgStringArrayPlan
src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_binding_owner.pgy|inout source: DirectMirScalarCfgStringArrayPlan
EOF
while IFS='|' read -r file function_start contract; do
    require_function_text "$file" "$function_start" "$contract"
done <<'EOF'
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchPatternCount(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchPatternAt(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchVariant(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchBindingCount(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchBindingAt(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchBindingTypeCount(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchBindingTypeAt(|ref instruction: JsonObjectFactTable
src/self_hosted/mir_lower/match_json_fact_owner.pgy|func MirMatchInstructionCaptureFromTable(|ref instruction: JsonObjectFactTable
src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy|func DirectMirEnumValueMatchPlanFactFromOwners(|own route: DirectMirEnumValueMatchRouteFact
src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy|func DirectMirEnumValueMatchPlanFactFromOwners(|own identity_match: DirectMirIdentityMatchCfgCertificateFact
src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy|func DirectMirCfgPlanFromAdmitted(|own enum_route: DirectMirEnumValueMatchRouteFact
src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy|func CompileAdmittedDirectMirCfgPayloadForTarget(|own enum_route: DirectMirEnumValueMatchRouteFact
src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy|func DirectMirAggregateValueFlowFactSeal(|own representation: DirectMirInferredGenericMemberRepresentationFact
src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy|func DirectMirAggregateValueFlowFactFromCapturedArrayAbi(|own representation: DirectMirInferredGenericMemberRepresentationFact
src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy|func DirectMirAggregateValueFlowFactFromTypedArrayAbsence(|own representation: DirectMirInferredGenericMemberRepresentationFact
src/self_hosted/compiler/direct_mir_aggregate_value_flow_target_projection_owner.pgy|func DirectMirAggregateValueFlowTargetProjectionFromFacts(|ref flow: DirectMirAggregateValueFlowFact
EOF
target_owner="$ROOT_DIR/src/self_hosted/compiler/direct_mir_aggregate_value_flow_target_projection_owner.pgy"
! grep -Fq 'let flow: DirectMirAggregateValueFlowFact' "$target_owner" ||
    fail "target projection must store the digest, not the whole flow fact"
echo "[$LABEL] function-scoped ownership signatures ok"
