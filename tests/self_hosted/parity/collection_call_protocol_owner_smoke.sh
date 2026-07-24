#!/usr/bin/env bash
# Static owner gate for the shared List/Queue/Set call protocol.
# The protocol owner is the only source of collection-call operation shape;
# family consumers may validate their own ABI facts but may not redeclare
# collection names, arity, argument positions, or result shape.
# collection_call_name_redeclaration collection_call_arity_redeclaration
# collection_call_argument_position_redeclaration
# collection_call_return_shape_redeclaration missing_collection_call_protocol_success

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LABEL="self-host-parity:collection-call-protocol"
PROTOCOL="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_collection_call_protocol_owner.pgy"

owners=(
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy"
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_queue_call_owner.pgy"
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_set_call_owner.pgy"
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_verdict_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_composite_literal_emit_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/list_call_type_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/queue_call_type_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/set_call_type_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/list_call_emit_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/queue_call_emit_owner.pgy"
    "$ROOT_DIR/src/self_hosted/codegen/emission/set_call_emit_owner.pgy"
)

[[ -f "$PROTOCOL" ]] || {
    echo "[$LABEL] protocol owner is missing" >&2
    exit 1
}
grep -Fq 'struct SemanticExpressionGraphCollectionCallProtocolFact' "$PROTOCOL"
grep -Fq 'SemanticExpressionGraphCollectionCallProtocolFromName' "$PROTOCOL"

for operation in \
    ListNew ListPush ListGet ListSet ListRemove ListSize \
    QueueNew QueuePush QueuePop QueueSize QueueEmpty \
    SetNew SetAdd SetHas SetRemove SetSize; do
    grep -Fq "\"$operation\"" "$PROTOCOL" || {
        echo "[$LABEL] protocol operation is missing: $operation" >&2
        exit 1
    }
done

for owner in "${owners[@]}"; do
    [[ -f "$owner" ]] || {
        echo "[$LABEL] migrated consumer is missing: $owner" >&2
        exit 1
    }
    [[ "$owner" == "$PROTOCOL" ]] ||
        grep -Fq 'ast_expression_graph_collection_call_protocol_owner.pgy' "$owner" || {
            echo "[$LABEL] consumer does not import protocol owner: $owner" >&2
            exit 1
        }
done

for forbidden in \
    'func SemanticExpressionGraphListCallName' \
    'func SemanticExpressionGraphListCallExpectedArity' \
    'func SemanticExpressionGraphQueueCallName' \
    'func SemanticExpressionGraphQueueCallExpectedArity' \
    'func SemanticExpressionGraphSetCallName' \
    'func SemanticExpressionGraphSetCallExpectedArity' \
    'func CodegenListCallName' \
    'func CodegenQueueCallName' \
    'func CodegenSetCallName'; do
    if grep -Fq "$forbidden" "${owners[@]}"; then
        echo "[$LABEL] retired collection helper returned: $forbidden" >&2
        exit 1
    fi
done

for owner in \
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_resolved_call_type_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_queue_call_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_set_call_owner.pgy"; do
    for field in expected_arity receiver_argument_index value_argument_index return_kind; do
        grep -Fq "protocol.$field" "$owner" || {
            echo "[$LABEL] semantic consumer lost protocol field $field: $owner" >&2
            exit 1
        }
    done
done

for owner in \
    "$ROOT_DIR/src/self_hosted/codegen/emission/list_call_type_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/codegen/emission/queue_call_type_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/codegen/emission/set_call_type_owner.pgy"; do
    for field in expected_arity receiver_argument_index return_kind; do
        grep -Fq "protocol.$field" "$owner" || {
            echo "[$LABEL] codegen type consumer lost protocol field $field: $owner" >&2
            exit 1
        }
    done
done

for owner in \
    "$ROOT_DIR/src/self_hosted/codegen/emission/list_call_emit_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/codegen/emission/queue_call_emit_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/codegen/emission/set_call_emit_owner.pgy"; do
    for field in expected_arity receiver_argument_index value_argument_index return_kind operation; do
        grep -Fq "protocol.$field" "$owner" || {
            echo "[$LABEL] codegen emitter lost protocol field $field: $owner" >&2
            exit 1
        }
    done
done

DISPATCH="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"
grep -Fq 'let collection_protocol:' "$DISPATCH"
grep -Fq 'collection_protocol.family == "List"' "$DISPATCH"
grep -Fq 'collection_protocol.family == "Queue"' "$DISPATCH"
grep -Fq 'collection_protocol.family == "Set"' "$DISPATCH"
if grep -Eq 'source_name == "(List|Queue|Set)' "$DISPATCH"; then
    echo "[$LABEL] final direct-call dispatcher redeclared collection names" >&2
    exit 1
fi

echo "[$LABEL] shared collection call protocol owner and negative ratchet are wired"
