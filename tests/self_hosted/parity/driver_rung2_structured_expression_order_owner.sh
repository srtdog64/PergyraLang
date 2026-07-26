#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ORDER="$ROOT_DIR/src/self_hosted/mir_lower/structured_expression_emission_order_owner.pgy"
PROGRAM="$ROOT_DIR/src/self_hosted/mir_lower/program_lower.pgy"
FACTS="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_fact_owner.pgy"
OCCURRENCE="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_occurrence_owner.pgy"
DRIVER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"

for file in "$ORDER" "$PROGRAM" "$FACTS" "$OCCURRENCE" "$DRIVER"; do
    [[ -f "$file" ]] || {
        echo "[self-host-parity:structured-expression-order] missing owner: $file" >&2
        exit 1
    }
done

require_text() {
    local file="$1" term="$2"
    grep -Fq -- "$term" "$file" || {
        echo "[self-host-parity:structured-expression-order] missing fact: $file: $term" >&2
        exit 1
    }
}

reject_text() {
    local file="$1" term="$2"
    if grep -Fq -- "$term" "$file"; then
        echo "[self-host-parity:structured-expression-order] forbidden path: $file: $term" >&2
        exit 1
    fi
}

function_body() {
    local file="$1" name="$2"
    awk -v name="$name" '
        index($0, "func " name "(") { active=1; depth=0 }
        active {
            print
            opens=gsub(/\{/, "{")
            closes=gsub(/\}/, "}")
            depth += opens - closes
            if (depth == 0 && opens > 0) exit
        }
    ' "$file"
}

require_text "$ORDER" "global_instruction_rows: Array<Int>"
require_text "$ORDER" "derived_ordinals: Array<Int>"
require_text "$ORDER" "ArrayPush(rows, global_row)"
if function_body "$ORDER" MirStructuredExpressionEmissionOrderAppend |
    grep -Fq 'MirStructuredExpressionEmissionOrderReady('; then
    echo "[self-host-parity:structured-expression-order] append reopened whole-order validation" >&2
    exit 1
fi

require_text "$PROGRAM" "MirProgramTreeEmission"
require_text "$PROGRAM" "MirStructuredExpressionEmissionOrderReady(expression_order)"
require_text "$FACTS" "ref order: MirStructuredExpressionEmissionOrder"
require_text "$FACTS" "order.global_instruction_rows[order_i]"
require_text "$FACTS" "order.lanes[order_i] != lane"
require_text "$FACTS" "MirExpressionGraphOccurrenceGraphSlot("
require_text "$FACTS" "expr0_seen: Array<Bool>"
require_text "$FACTS" "expr1_seen: Array<Bool>"
require_text "$FACTS" "order_i != order_count"
require_text "$DRIVER" "emission.expression_order"

require_text "$OCCURRENCE" 'source == "AST_ASSIGNMENT" && kind == "def"'
require_text "$OCCURRENCE" 'return MirExpressionGraphSlotExpr1();'
require_text "$OCCURRENCE" 'UnwrapOption(arg0_opt) == "ArraySet"'
require_text "$OCCURRENCE" 'lane == AstExpressionLaneAuxiliary()'
require_text "$OCCURRENCE" 'source == "AST_FOR_LOOP"'

reject_text "$FACTS" "persisted_sequence"
reject_text "$FACTS" "sequence_i"
reject_text "$FACTS" "MirExpressionGraphSequenceAppendView("
reject_text "$FACTS" "MirExpressionGraphSequenceFromRoutineIndex("
reject_text "$FACTS" "BuildMirProgramRoutineIndex("
reject_text "$OCCURRENCE" "expected_text"
[[ ! -e "$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_sequence_view_owner.pgy" ]] || {
    echo "[self-host-parity:structured-expression-order] second graph view owner reopened" >&2
    exit 1
}

echo "[self-host-parity:structured-expression-order] structured occurrence identity owns one final graph arena and required-producer coverage"
