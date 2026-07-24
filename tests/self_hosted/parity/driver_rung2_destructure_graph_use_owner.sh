#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_destructure_owner.pgy"

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:destructure-graph-use] owner is missing" >&2
    exit 1
}

require_text() {
    local term="$1"
    grep -Fq -- "$term" "$OWNER" || {
        echo "[self-host-parity:destructure-graph-use] missing owner fact: $term" >&2
        exit 1
    }
}

reject_text() {
    local term="$1"
    if grep -Fq -- "$term" "$OWNER"; then
        echo "[self-host-parity:destructure-graph-use] forbidden text fallback reopened: $term" >&2
        exit 1
    fi
}

require_text "let initializer_graph: SemanticExpressionGraphView"
require_text "node_id, AstExpressionLaneValue()"
require_text "if !initializer_graph.ok"
require_text "MIR destructure initializer expression graph is missing"
require_text "SelfMirExpressionGraphUses("
require_text "build, initializer_graph"
reject_text "SelfMirExpressionUses("
reject_text "SelfMirTextContainsIdentifier("

graph_line="$(grep -Fn 'let initializer_graph: SemanticExpressionGraphView' "$OWNER" | cut -d: -f1)"
uses_line="$(grep -Fn 'let uses: Array<String> = SelfMirExpressionGraphUses(' "$OWNER" | cut -d: -f1)"
binding_line="$(grep -Fn 'build = SelfMirRoutineAddLocal(build, binding' "$OWNER" | cut -d: -f1)"
if [[ -z "$graph_line" || -z "$uses_line" || -z "$binding_line" ||
    "$graph_line" -ge "$uses_line" || "$uses_line" -ge "$binding_line" ]]; then
    echo "[self-host-parity:destructure-graph-use] graph uses are not resolved before binding mutation" >&2
    exit 1
fi

echo "[self-host-parity:destructure-graph-use] destructure initializer uses are graph-owned"
