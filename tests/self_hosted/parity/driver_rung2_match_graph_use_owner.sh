#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_match_owner.pgy"

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:match-graph-use] owner is missing" >&2
    exit 1
}

require_text() {
    local term="$1"
    grep -Fq -- "$term" "$OWNER" || {
        echo "[self-host-parity:match-graph-use] missing owner fact: $term" >&2
        exit 1
    }
}

reject_text() {
    local term="$1"
    if grep -Fq -- "$term" "$OWNER"; then
        echo "[self-host-parity:match-graph-use] forbidden text fallback reopened: $term" >&2
        exit 1
    fi
}

require_text "let match_graph: SemanticExpressionGraphView"
require_text "if !match_graph.ok"
require_text "SelfMirExpressionGraphUses(build, match_graph)"
require_text "build, match_graph, input.machine_declaration"
reject_text "SelfMirExpressionUses("
reject_text "SelfMirTextContainsIdentifier("

echo "[self-host-parity:match-graph-use] match subject uses are graph-owned"
