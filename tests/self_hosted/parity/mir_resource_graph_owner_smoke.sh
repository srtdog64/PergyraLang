#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_build_owner.pgy"
BODY="$ROOT_DIR/.tmp/mir-resource-graph-owner.smoke.body"

[[ -f "$OWNER" ]] || {
    echo "[self-host-parity:mir-resource-graph-owner] missing owner: $OWNER" >&2
    exit 1
}

awk '
    /^func SelfMirRoutineResourceTypeForInstruction\(/ { inside=1 }
    inside { print }
    inside && /^func / && !/SelfMirRoutineResourceTypeForInstruction/ { exit }
' "$OWNER" >"$BODY"

grep -Fq 'view: SemanticExpressionGraphView' "$BODY" || {
    echo "[self-host-parity:mir-resource-graph-owner] consumer lost semantic graph view" >&2
    exit 1
}

for fact in \
    'SelfMirExpressionGraphSubtreeStart(' \
    'SemanticExpressionGraphNodeKind(' \
    'SemanticExpressionGraphLeftChild(' \
    'SemanticExpressionGraphRightChild(' \
    'SemanticExpressionGraphNodeText('; do
    grep -Fq "$fact" "$BODY" || {
        echo "[self-host-parity:mir-resource-graph-owner] missing owner fact: $fact" >&2
        exit 1
    }
done

for forbidden in \
    'SelfMirExpressionGraphRows' \
    'cfg.instructions.expr0_graphs' \
    'graphs.node_starts' \
    'graphs.node_counts' \
    'graphs.node_kinds' \
    'graphs.node_texts' \
    'graphs.left_children' \
    'graphs.right_children'; do
    if grep -Fq "$forbidden" "$BODY"; then
        echo "[self-host-parity:mir-resource-graph-owner] MIR graph copy read reopened: $forbidden" >&2
        exit 1
    fi
done

for guard in \
    '!IsSome(start_opt)' \
    '!IsSome(wrapper_kind)' \
    '!IsSome(receiver_opt)' \
    '!IsSome(receiver_name_opt)'; do
    grep -Fq "$guard" "$BODY" || {
        echo "[self-host-parity:mir-resource-graph-owner] missing fail-closed guard: $guard" >&2
        exit 1
    }
done

grep -Fq \
    'build, view, cfg, instruction_index, operation' \
    "$OWNER" || {
    echo "[self-host-parity:mir-resource-graph-owner] caller did not pass semantic view" >&2
    exit 1
}

echo "[self-host-parity:mir-resource-graph-owner] resource type consumes semantic graph view"
