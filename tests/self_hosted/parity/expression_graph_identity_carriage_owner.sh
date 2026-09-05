#!/usr/bin/env bash
# Semantic binding/call IDs must survive the persisted MIR graph boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LABEL="self-host-expression-graph-identity-carriage"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$ROOT_DIR/$1" || fail "missing $1: $2"; }
reject_text() { ! grep -Fq -- "$2" "$ROOT_DIR/$1" || fail "forbidden $1: $2"; }

while IFS='|' read -r owner cap; do
    lines="$(wc -l <"$ROOT_DIR/$owner")"
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
        lines="$(awk 'END { print NR }' "$ROOT_DIR/$owner")" ||
            fail "unreadable shared-cap source: $owner"
        [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
    done
}
require_scalar_identity_owner_caps

FACT="src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy"
GRAPH_FACT="src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy"
RESOLVE="src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy"
WRITE="src/self_hosted/mir/expression_identity_json_projection_owner.pgy"
READ="src/self_hosted/mir_lower/expression_graph_persisted_read_owner.pgy"
# The member-count admission moved with the line-cap owner split (73b3e661).
NODE_READ="src/self_hosted/mir_lower/expression_graph_persisted_node_read_owner.pgy"
NODE_ID="src/self_hosted/mir_lower/expression_graph_persisted_node_identity_owner.pgy"
BIND_CONSUME="src/self_hosted/compiler/direct_mir_scalar_program_leaf_identity_fact_owner.pgy"
CALL_CONSUME="src/self_hosted/compiler/direct_mir_scalar_program_call_with_arguments_admission_owner.pgy"

for field in call_target_syntax_ids runtime_call_abi_ids binding_syntax_ids binding_kinds binding_ordinals; do
    require_text "$FACT" "$field"
done
require_text "$RESOLVE" 'SemanticExpressionDirectTargetSyntaxId('
require_text "$RESOLVE" 'SemanticCallTargetNamespace()'
require_text "$RESOLVE" 'SemanticCallTargetMember()'
require_text "$GRAPH_FACT" 'call_target_kind != SemanticCallTargetNamespace()'
require_text "$GRAPH_FACT" 'call_target_kind != SemanticCallTargetMember())'
require_text "$RESOLVE" 'SemanticExpressionFormalParameterOrdinal('
require_text "$WRITE" '"call_target_syntax_id"'
require_text "$WRITE" '"runtime_call_abi_id"'
require_text "$WRITE" '"binding_syntax_id"'
require_text "$WRITE" '"binding_kind"'
require_text "$WRITE" '"binding_ordinal"'
require_text "$NODE_READ" 'member_count == 6'
require_text "$NODE_READ" 'member_count == 9'
require_text "$NODE_READ" 'member_count == 7'
require_text "$NODE_READ" 'member_count == 11'
require_text "$NODE_READ" '"binding_syntax_id"'
require_text "$NODE_ID" 'target_kind != SemanticCallTargetNamespace()'
require_text "$NODE_ID" 'target_kind != SemanticCallTargetMember())'
require_text "$CALL_CONSUME" 'call_target_syntax_ids[chain.call_node]'
require_text "$BIND_CONSUME" 'SemanticExpressionBindingFormalParameter()'
reject_text "$BIND_CONSUME" 'sequence.arena.node_texts[node] == parameter_name'
reject_text "$CALL_CONSUME" 'sequence.arena.node_texts[node] == parameter_name'

for owner in "$FACT" "$RESOLVE" "$WRITE" "$READ" "$NODE_READ" "$NODE_ID" "$BIND_CONSUME" "$CALL_CONSUME"; do
    reject_text "$owner" 'string_equality.pgy'
done

echo "[$LABEL] ok: persisted call and formal-binding identities are owner-carried"
