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
src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy|80
src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy|190
src/self_hosted/mir/expression_identity_json_projection_owner.pgy|70
src/self_hosted/mir_lower/expression_graph_persisted_read_owner.pgy|450
src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy|340
src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy|225
src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy|125
EOF

FACT="src/self_hosted/semantic/ast_expression_identity_fact_owner.pgy"
RESOLVE="src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy"
WRITE="src/self_hosted/mir/expression_identity_json_projection_owner.pgy"
READ="src/self_hosted/mir_lower/expression_graph_persisted_read_owner.pgy"
BIND_CONSUME="src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy"
CALL_CONSUME="src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy"

for field in call_target_syntax_ids binding_kinds binding_ordinals; do
    require_text "$FACT" "$field"
done
require_text "$RESOLVE" 'SemanticExpressionDirectTargetSyntaxId('
require_text "$RESOLVE" 'SemanticExpressionFormalParameterOrdinal('
require_text "$WRITE" '"call_target_syntax_id"'
require_text "$WRITE" '"binding_kind"'
require_text "$WRITE" '"binding_ordinal"'
require_text "$READ" 'member_count == 6'
require_text "$READ" 'member_count == 9'
require_text "$CALL_CONSUME" 'call_target_syntax_ids[chain.call_node]'
require_text "$BIND_CONSUME" 'SemanticExpressionBindingFormalParameter()'
reject_text "$BIND_CONSUME" 'sequence.arena.node_texts[node] == parameter_name'
reject_text "$CALL_CONSUME" 'sequence.arena.node_texts[node] == parameter_name'

for owner in "$FACT" "$RESOLVE" "$WRITE" "$READ" "$BIND_CONSUME" "$CALL_CONSUME"; do
    reject_text "$owner" 'string_equality.pgy'
done

echo "[$LABEL] ok: persisted call and formal-binding identities are owner-carried"
