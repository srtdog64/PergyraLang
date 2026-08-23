#!/usr/bin/env bash
# One flat GraphPlan must carry explicit canonical routine ownership ranges.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LABEL="self-host-scalar-cfg-routine-partition"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$ROOT_DIR/$1" || fail "missing $1: $2"; }
reject_text() { ! grep -Fq -- "$2" "$ROOT_DIR/$1" || fail "forbidden $1: $2"; }

while IFS='|' read -r owner cap; do
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <<'EOF'
src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_fact_owner.pgy|180
src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_mutation_owner.pgy|70
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy|125
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_identity_owner.pgy|100
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy|85
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_verification_owner.pgy|25
src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy|250
EOF

FACT="src/self_hosted/compiler/direct_mir_scalar_cfg_routine_partition_fact_owner.pgy"
GRAPH="src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
SEAL="src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_seal_owner.pgy"
VERIFY="src/self_hosted/compiler/direct_mir_scalar_cfg_graph_plan_verification_owner.pgy"
ADMISSION="src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
READY="src/self_hosted/compiler/direct_mir_scalar_cfg_graph_readiness_owner.pgy"
MUTATION="src/self_hosted/compiler/direct_mir_scalar_cfg_graph_mutation_owner.pgy"

require_text "$GRAPH" 'let routines: DirectMirScalarCfgRoutinePartitionFact;'
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v80'
reject_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v16'
require_text "$ADMISSION" 'DirectMirScalarCfgSingleRoutinePartitionFromOwners('
require_text "$SEAL" 'own routine_partition: DirectMirScalarCfgRoutinePartitionFact'
reject_text "$SEAL" 'ref routine_partition: DirectMirScalarCfgRoutinePartitionFact'
require_text "$SEAL" 'DirectMirScalarCfgGraphPlanVerified(plan)'
require_text "$VERIFY" 'DirectMirScalarCfgGraphPlanMutationRejected(verified)'
require_text "$READY" 'DirectMirScalarCfgRoutinePartitionReady('
require_text "$MUTATION" \
    'let routine_partition: DirectMirScalarCfgRoutinePartitionFact ='
require_text "$MUTATION" \
    'DirectMirScalarCfgRoutinePartitionMutationRejected(routine_partition)'
reject_text "$MUTATION" \
    'DirectMirScalarCfgRoutinePartitionMutationRejected(plan.routines)'
for column in source_syntax_ids signature_digests cfg_digests \
        local_starts value_starts block_starts operation_starts phi_starts; do
    require_text "$FACT" "let $column:"
done
for forbidden in 'routine/block-count classification' 'string_equality.pgy'; do
    reject_text "$FACT" "$forbidden"
done
reject_text "$GRAPH" 'graph-plan.v15'

echo "[$LABEL] ok: GraphPlan routine ranges are sealed, contiguous, and mutation-tested"
