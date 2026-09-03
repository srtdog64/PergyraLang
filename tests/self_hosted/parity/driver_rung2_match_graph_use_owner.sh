#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MIR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_match_owner.pgy"
CODEGEN_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/stmt_emit.pgy"
OPTION_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/option_match_owner.pgy"
TAGGED_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/tagged_enum_match_owner.pgy"

for owner in "$MIR_OWNER" "$CODEGEN_OWNER" "$OPTION_OWNER" "$TAGGED_OWNER"; do
    [[ -f "$owner" ]] || {
        echo "[self-host-parity:match-graph-use] owner is missing: $owner" >&2
        exit 1
    }
done

require_text() {
    local owner="$1"
    local term="$2"
    grep -Fq -- "$term" "$owner" || {
        echo "[self-host-parity:match-graph-use] missing owner fact: $owner: $term" >&2
        exit 1
    }
}

reject_text() {
    local owner="$1"
    local term="$2"
    if grep -Fq -- "$term" "$owner"; then
        echo "[self-host-parity:match-graph-use] forbidden text fallback reopened: $owner: $term" >&2
        exit 1
    fi
}

require_text "$MIR_OWNER" "let match_graph: SemanticExpressionGraphView"
require_text "$MIR_OWNER" "if !match_graph.ok"
require_text "$MIR_OWNER" "SelfMirExpressionGraphUses(build, match_graph)"
require_text "$MIR_OWNER" "build, match_graph, input.machine_declaration"
reject_text "$MIR_OWNER" "SelfMirExpressionUses("
reject_text "$MIR_OWNER" "SelfMirTextContainsIdentifier("

require_text "$CODEGEN_OWNER" "let match_graph: SemanticExpressionGraphView"
require_text "$CODEGEN_OWNER" "let rendered_match_subject: String = RewriteExprWithSemanticGraph("
reject_text "$CODEGEN_OWNER" "IntEval(match_subject, env)"
reject_text "$CODEGEN_OWNER" "RewriteExpr(match_subject, env)"
require_text "$OPTION_OWNER" "rendered_match_value: String"
reject_text "$OPTION_OWNER" "RewriteExpr("
require_text "$TAGGED_OWNER" "rendered_match_value: String"
reject_text "$TAGGED_OWNER" "RewriteExpr("

echo "[self-host-parity:match-graph-use] match subject uses are graph-owned; codegen is graph-owned"
