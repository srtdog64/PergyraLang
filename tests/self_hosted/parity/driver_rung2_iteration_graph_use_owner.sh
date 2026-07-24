#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_iteration_owner.pgy"
FOR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_for_owner.pgy"
INPUT_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_input_owner.pgy"
LOCAL_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_local_inventory_owner.pgy"
SEMANTIC_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_iteration_graph_root_owner.pgy"
PROGRAM_OWNER="$ROOT_DIR/src/self_hosted/hir/program_graph_owner.pgy"

[[ -f "$OWNER" && -f "$FOR_OWNER" && -f "$INPUT_OWNER" &&
    -f "$LOCAL_OWNER" && -f "$SEMANTIC_OWNER" && -f "$PROGRAM_OWNER" ]] || {
    echo "[self-host-parity:iteration-graph-use] owner is missing" >&2
    exit 1
}

require_text() {
    local file="$1"
    local term="$2"
    grep -Fq -- "$term" "$file" || {
        echo "[self-host-parity:iteration-graph-use] missing owner fact: $file: $term" >&2
        exit 1
    }
}

reject_text() {
    local file="$1"
    local term="$2"
    if grep -Fq -- "$term" "$file"; then
        echo "[self-host-parity:iteration-graph-use] forbidden text fallback reopened: $file: $term" >&2
        exit 1
    fi
}

require_text "$OWNER" "if !source_graph.ok"
require_text "$OWNER" "SelfMirExpressionGraphUses("
require_text "$OWNER" "branch_graph: SemanticExpressionGraphView"
require_text "$OWNER" "SelfMirIterationSyntheticGraphView("
require_text "$OWNER" "expression_surfaces.expression_graph"
reject_text "$OWNER" "SelfMirExpressionUses("
reject_text "$OWNER" "SelfMirTextContainsIdentifier("
reject_text "$OWNER" "SelfMirSyntheticLocalExpressionGraph("
require_text "$FOR_OWNER" "let source_graph: SemanticExpressionGraphView"
require_text "$FOR_OWNER" "if !source_graph.ok"
require_text "$FOR_OWNER" "let branch_graph: SemanticExpressionGraphView"
require_text "$FOR_OWNER" "if iteration.is_foreach && !branch_graph.ok"
require_text "$FOR_OWNER" "SelfMirIterationBranchUses(build, iteration, branch_graph)"
require_text "$FOR_OWNER" "input.analysis.expression_surfaces, iteration"
reject_text "$FOR_OWNER" "SelfMirSyntheticLocalExpressionGraph("
require_text "$SEMANTIC_OWNER" "SemanticAstIterationSyntheticGraphRootsAttach("
require_text "$PROGRAM_OWNER" "ProgramExpressionGraphAppendIsolatedNode("
require_text "$SEMANTIC_OWNER" "ProgramExpressionGraphAppendIsolatedNode("
reject_text "$SEMANTIC_OWNER" "SemanticExpressionGraphAppendNode("
reject_text "$SEMANTIC_OWNER" "topology.node_kinds"
require_text "$SEMANTIC_OWNER" "facts.synthetic_graph_root_ids = synthetic_roots"
require_text "$INPUT_OWNER" "facts.synthetic_graph_root_ids"
require_text "$LOCAL_OWNER" "input.iterations.synthetic_names[iteration_row]"
reject_text "$INPUT_OWNER" "SelfMirForEachSyntheticOrdinal("

echo "[self-host-parity:iteration-graph-use] iteration uses share one semantic program graph"
