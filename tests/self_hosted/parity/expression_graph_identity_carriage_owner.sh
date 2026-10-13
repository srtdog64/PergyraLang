#!/usr/bin/env bash
# Semantic binding/call IDs must survive the persisted MIR graph boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SOURCE_SIZE_COUNTER="$ROOT_DIR/scripts/source_size_count.py"
LABEL="self-host-expression-graph-identity-carriage"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

while IFS='|' read -r owner cap; do
    lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$ROOT_DIR/$owner")" || fail "source-size measurement failed"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <<'EOF'
src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy|125
src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy|480
src/self_hosted/mir/expression_identity_json_projection_owner.pgy|115
src/self_hosted/mir_lower/expression_graph_persisted_read_owner.pgy|450
src/self_hosted/mir_lower/expression_graph_persisted_node_read_owner.pgy|300
src/self_hosted/mir_lower/expression_graph_persisted_node_identity_owner.pgy|50
src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy|340
EOF

# Scalar limits are owned by the same table as the complete component gate.
# Missing, duplicate or malformed rows fail; there is no local numeric fallback.
require_scalar_identity_owner_caps() {
    local owner cap lines
    for owner in \
        src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy \
        src/self_hosted/compiler/direct_mir_scalar_program_leaf_identity_fact_owner.pgy \
        src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy \
        src/self_hosted/compiler/direct_mir_scalar_program_call_with_arguments_admission_owner.pgy; do
        cap="$(awk -F'|' -v owner="$owner" '
            $1 == owner {
                matches++
                value = $2
                sub(/\r$/, "", value)
                if (NF != 2 || value !~ /^(0|[1-9][0-9]*)$/) invalid = 1
            }
            END { if (matches != 1 || invalid) exit 1; print value }
        ' "$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv")" ||
            fail "missing or invalid shared owner cap: $owner"
        [[ -f "$ROOT_DIR/$owner" ]] || fail "missing shared-cap source: $owner"
        lines="$(python3 "$SOURCE_SIZE_COUNTER" "$ROOT_DIR/$owner")" ||
            fail "unreadable shared-cap source: $owner"
        [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
    done
}
require_scalar_identity_owner_caps

echo "[$LABEL] ok: persisted call and formal-binding identities are owner-carried"
