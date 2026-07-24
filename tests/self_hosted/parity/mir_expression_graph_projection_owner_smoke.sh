#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/expression_graph_fact_owner.pgy"
BODY="$ROOT_DIR/.tmp/mir-expression-graph-projection.smoke.body"

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:mir-expression-graph-projection] owner is missing" >&2
    exit 1
}

awk '
    /^func SelfMirExpressionGraphAttachLast\(/ { inside=1 }
    inside { print }
    inside && /^func / && !/SelfMirExpressionGraphAttachLast/ { exit }
' "$OWNER" >"$BODY"

for fact in \
    'SemanticExpressionGraphNodeKind(' \
    'SemanticExpressionGraphNodeText(' \
    'SemanticExpressionGraphLeftChild(' \
    'SemanticExpressionGraphRightChild('; do
    grep -Fq "$fact" "$BODY" || {
        echo "[self-host-parity:mir-expression-graph-projection] missing semantic fact: $fact" >&2
        exit 1
    }
done

for guard in \
    '!IsSome(kind_opt)' \
    '!IsSome(text_opt)' \
    '!IsSome(left_opt)' \
    '!IsSome(right_opt)'; do
    grep -Fq "$guard" "$BODY" || {
        echo "[self-host-parity:mir-expression-graph-projection] missing fail-closed guard: $guard" >&2
        exit 1
    }
done

for forbidden in \
    'view.graph.arena.topology.node_kinds' \
    'view.graph.arena.node_texts' \
    'view.graph.arena.topology.left_children' \
    'view.graph.arena.topology.right_children'; do
    if grep -Fq "$forbidden" "$BODY"; then
        echo "[self-host-parity:mir-expression-graph-projection] raw topology read reopened: $forbidden" >&2
        exit 1
    fi
done

echo "[self-host-parity:mir-expression-graph-projection] MIR projection consumes semantic graph facts"
