#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_tracked_statement_owner.pgy"

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:return-graph-use] owner is missing" >&2
    exit 1
}

require_text() {
    local term="$1"
    grep -Fq -- "$term" "$OWNER" || {
        echo "[self-host-parity:return-graph-use] missing owner fact: $term" >&2
        exit 1
    }
}

reject_text() {
    local term="$1"
    if grep -Fq -- "$term" "$OWNER"; then
        echo "[self-host-parity:return-graph-use] forbidden text fallback reopened: $term" >&2
        exit 1
    fi
}

require_text "if kind == TypedAstKindBareReturnStmtTag()"
require_text "AST_RETURN_VOID"
require_text "SelfMirNoUses()"
require_text "let return_graph: SemanticExpressionGraphView"
require_text "if !return_graph.ok"
require_text "SelfMirExpressionGraphUses(build, return_graph)"
require_text "build, return_graph, input.machine_declaration"
reject_text "SelfMirExpressionUses("
reject_text "SelfMirTextContainsIdentifier("

echo "[self-host-parity:return-graph-use] value returns are graph-owned"
