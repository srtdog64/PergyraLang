#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_iteration_owner.pgy"
FOR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_for_owner.pgy"

[[ -f "$OWNER" && -f "$FOR_OWNER" ]] || {
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
reject_text "$OWNER" "SelfMirExpressionUses("
reject_text "$OWNER" "SelfMirTextContainsIdentifier("
require_text "$FOR_OWNER" "let source_graph: SemanticExpressionGraphView"
require_text "$FOR_OWNER" "if !source_graph.ok"
require_text "$FOR_OWNER" "let branch_graph: SemanticExpressionGraphView"
require_text "$FOR_OWNER" "if iteration.is_foreach && !branch_graph.ok"
require_text "$FOR_OWNER" "SelfMirIterationBranchUses(build, iteration, branch_graph)"

echo "[self-host-parity:iteration-graph-use] iteration uses are graph-owned"
