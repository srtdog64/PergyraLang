#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OWNER="$ROOT_DIR/src/self_hosted/mir/routine_statement_owner.pgy"
DISPATCH="$ROOT_DIR/src/self_hosted/mir/routine_tracked_statement_owner.pgy"

[[ -f "$OWNER" && -f "$DISPATCH" ]] || {
    echo "[self-host-parity:simple-statement-graph-use] owner is missing" >&2
    exit 1
}

require_text() {
    local file="$1"
    local term="$2"
    grep -Fq -- "$term" "$file" || {
        echo "[self-host-parity:simple-statement-graph-use] missing owner fact: $file: $term" >&2
        exit 1
    }
}

reject_graph_function_text_fallback() {
    awk '
        /^func SelfMirLowerGraphOwnedSimpleStatement\(/ {
            in_graph_function = 1
            found_graph_function = 1
        }
        in_graph_function && /^func / &&
            $0 !~ /^func SelfMirLowerGraphOwnedSimpleStatement\(/ {
            in_graph_function = 0
        }
        in_graph_function && /SelfMirExpressionUses\(/ { exit 1 }
        END {
            if (!found_graph_function) { exit 1 }
        }
    ' "$OWNER" || {
        echo "[self-host-parity:simple-statement-graph-use] graph-owned path reopened text fallback" >&2
        exit 1
    }
}

require_text "$OWNER" "func SelfMirSimpleStatementGraphOwnedKind"
require_text "$OWNER" "func SelfMirLowerGraphOwnedSimpleStatement"
require_text "$OWNER" "SelfMirExpressionGraphUses(build, graph)"
require_text "$DISPATCH" "if SelfMirSimpleStatementGraphOwnedKind(kind)"
require_text "$DISPATCH" "let simple_graph: SemanticExpressionGraphView"
require_text "$DISPATCH" "if !simple_graph.ok"
require_text "$DISPATCH" "build, kind, payload, simple_graph"
require_text "$DISPATCH" "build, simple_graph, input.machine_declaration"
reject_graph_function_text_fallback

echo "[self-host-parity:simple-statement-graph-use] Log/call/Exit uses are graph-owned; collection bridge remains bounded"
