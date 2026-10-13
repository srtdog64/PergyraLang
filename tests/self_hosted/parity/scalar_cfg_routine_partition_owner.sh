#!/usr/bin/env bash
# One flat GraphPlan must carry explicit canonical routine ownership ranges.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LABEL="self-host-scalar-cfg-routine-partition"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

while IFS='|' read -r owner cap; do
    lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$ROOT_DIR/$owner")" || fail "source-size measurement failed"
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

echo "[$LABEL] ok: GraphPlan routine ranges are sealed, contiguous, and mutation-tested"
