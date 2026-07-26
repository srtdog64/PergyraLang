#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PARSER_OWNER="$ROOT_DIR/src/self_hosted/parser/stmt_collection_graph_owner.pgy"
PARSER_DISPATCH="$ROOT_DIR/src/self_hosted/parser/stmt_owner.pgy"
SURFACE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_surface_fact_owner.pgy"
LANE_POLICY="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_lane_policy_owner.pgy"
USE_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_expression_use_owner.pgy"
EXPRESSION_FACT_OWNER="$ROOT_DIR/src/self_hosted/mir/expression_fact_owner.pgy"
MIR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_statement_owner.pgy"
DISPATCH="$ROOT_DIR/src/self_hosted/mir/routine_tracked_statement_owner.pgy"
VALIDATION="$ROOT_DIR/src/self_hosted/mir/instruction_validation_owner.pgy"
MIR_LOWER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_fact_owner.pgy"
MIR_SLOT_POLICY="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_instruction_policy_owner.pgy"
PARSER_BRIDGE="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_parser_bridge_owner.pgy"
OCCURRENCE_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_occurrence_owner.pgy"
ORDER_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/structured_expression_emission_order_owner.pgy"
RUNTIME_PARITY="$ROOT_DIR/tests/self_hosted/parity/driver_rung2_collection_mutation_graph_parity_owner.sh"
for file in "$PARSER_OWNER" "$PARSER_DISPATCH" "$SURFACE_OWNER" \
    "$LANE_POLICY" \
    "$USE_OWNER" "$EXPRESSION_FACT_OWNER" "$MIR_OWNER" "$DISPATCH" \
    "$VALIDATION" "$MIR_LOWER" "$MIR_SLOT_POLICY" "$PARSER_BRIDGE" \
    "$OCCURRENCE_OWNER" "$ORDER_OWNER" \
    "$RUNTIME_PARITY"; do
    [[ -f "$file" ]] || {
        echo "[self-host-parity:collection-mutation-graph-use] owner is missing: $file" >&2
        exit 1
    }
done
require_text() {
    local file="$1"
    local term="$2"
    grep -Fq -- "$term" "$file" || {
        echo "[self-host-parity:collection-mutation-graph-use] missing owner fact: $file: $term" >&2
        exit 1
    }
}
reject_text() {
    local file="$1"
    local term="$2"
    if grep -Fq -- "$term" "$file"; then
        echo "[self-host-parity:collection-mutation-graph-use] forbidden text fallback reopened: $file: $term" >&2
        exit 1
    fi
}
require_text "$PARSER_OWNER" "ParserExpressionNamedCallArgumentAt(expression, callee, 0)"
require_text "$PARSER_OWNER" "AstExpressionLaneAtom(), target"
require_text "$PARSER_OWNER" "if owner_kind == TypedAstKindArrayPopStmtTag()"
require_text "$PARSER_DISPATCH" "owner_kind == TypedAstKindArrayPopStmtTag() ||"
require_text "$SURFACE_OWNER" "producer_only_atom && atom_view.ok"
require_text "$SURFACE_OWNER" "SemanticAstExpressionGraphAtomLaneProducerOnly("
require_text "$LANE_POLICY" "func SemanticAstExpressionGraphAtomLaneProducerOnly("
require_text "$LANE_POLICY" "TypedAstKindArrayPopStmtTag()"
require_text "$LANE_POLICY" "TypedAstKindArrayPushStmtTag()"
require_text "$LANE_POLICY" "TypedAstKindArraySetStmtTag()"

require_text "$USE_OWNER" "func SelfMirExpressionGraphUsesAppend("
require_text "$USE_OWNER" "SelfMirExpressionGraphUses(build, view)"
require_text "$MIR_OWNER" "func SelfMirCollectionMutationKind("
require_text "$MIR_OWNER" "func SelfMirLowerCollectionMutationGraphOwnedSimpleStatement("
require_text "$MIR_OWNER" "SelfMirExpressionGraphUsesAppend(build, uses, target_graph)"
reject_text "$MIR_OWNER" "SelfMirExpressionUses("
reject_text "$USE_OWNER" "func SelfMirExpressionUses("
reject_text "$EXPRESSION_FACT_OWNER" "func SelfMirTextContainsIdentifier("

require_text "$DISPATCH" "if SelfMirCollectionMutationKind(kind)"
require_text "$DISPATCH" "let target_graph: SemanticExpressionGraphView"
require_text "$DISPATCH" "if !target_graph.ok"
require_text "$DISPATCH" "MIR collection mutation target expression graph is missing"
require_text "$DISPATCH" "target_graph, value_graph, auxiliary_graph"
reject_text "$VALIDATION" 'rows.arg0s[i] == "ArrayPop"'
reject_text "$MIR_SLOT_POLICY" 'UnwrapOption(arg0) == "ArrayPop"'
require_text "$MIR_LOWER" "MirExpressionGraphSequenceAppendParserBridge("
require_text "$PARSER_BRIDGE" "SemanticExpressionGraphBuildCompactBridgeFromText("
require_text "$OCCURRENCE_OWNER" "MirExpressionGraphProducerOnlyOccurrenceAllowed("
require_text "$OCCURRENCE_OWNER" 'operation == "ArrayPop"'
require_text "$OCCURRENCE_OWNER" 'operation == "ArrayPush"'
require_text "$OCCURRENCE_OWNER" 'operation == "ArraySet"'
require_text "$ORDER_OWNER" "global_instruction_rows: Array<Int>"
reject_text "$MIR_LOWER" "MirExpressionGraphSequenceAppendView("
reject_text "$MIR_LOWER" "persisted_sequence"
[[ ! -e "$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_sequence_view_owner.pgy" ]] || {
    echo "[self-host-parity:collection-mutation-graph-use] forbidden copied sequence view owner reopened" >&2
    exit 1
}
require_text "$RUNTIME_PARITY" "valid_array_builtins"
require_text "$RUNTIME_PARITY" "receiver SSA use was lost"
require_text "$RUNTIME_PARITY" "ArrayPop reopened a third persisted graph lane"
reject_text "$PARSER_BRIDGE" "SelfMirExpressionUses("
reject_text "$PARSER_BRIDGE" "SelfMirTextContainsIdentifier("

target_line="$(grep -Fn 'let target_graph: SemanticExpressionGraphView' "$DISPATCH" | cut -d: -f1)"
guard_line="$(grep -Fn 'if !target_graph.ok' "$DISPATCH" | cut -d: -f1)"
lower_line="$(grep -Fn 'SelfMirLowerCollectionMutationGraphOwnedSimpleStatement(' "$DISPATCH" | cut -d: -f1)"
if [[ -z "$target_line" || -z "$guard_line" || -z "$lower_line" ||
    "$target_line" -ge "$guard_line" || "$guard_line" -ge "$lower_line" ]]; then
    echo "[self-host-parity:collection-mutation-graph-use] target graph is not validated before lowering" >&2
    exit 1
fi

echo "[self-host-parity:collection-mutation-graph-use] collection receiver/value/index lanes are graph-owned, receiver absence fails closed, and no third MIR graph slot is required"
