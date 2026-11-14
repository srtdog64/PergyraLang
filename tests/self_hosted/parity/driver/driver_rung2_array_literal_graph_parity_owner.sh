#!/usr/bin/env bash
# Owns DRV-2 array-literal graph consumption assertions.

pgy_selfhost_verify_driver_rung2_array_literal_graph() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"

    if [[ "$base" == "array_return_literal" ]]; then
        grep -Fq '"kind":"return","name":"return"' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend array return instruction was lost" >&2
            exit 1
        }
        grep -Fq '"expr0":"[1, 2, 3, 4]","expr0_graph":{' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend array return graph was lost" >&2
            exit 1
        }
        grep -Fq '"kind":"array_literal"' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend array return root was lost" >&2
            exit 1
        }
        grep -Fq '"kind":"array_element","text":"[1, 2, 3, 4]"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend final array return edge was lost" >&2
            exit 1
        }
        return 0
    fi

    [[ "$base" == "ast_node_array_literal" ]] || return 0

    grep -Fq '"expr0":"[CodegenAstTextNode(2, \"Let\", 0, 8)]","expr0_graph":{' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend array literal graph was lost" >&2
        exit 1
    }
    grep -Fq '"kind":"array_literal"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend array literal root was lost" >&2
        exit 1
    }
    grep -Fq '"kind":"array_element"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend array element edge was lost" >&2
        exit 1
    }
    grep -Fq '"kind":"call_argument","text":"CodegenAstTextNode(2, \"Let\", 0, 8)"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend array element constructor graph was lost" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_array_literal_emitted_c() {
    local backend="$1"
    local base="$2"
    local emitted_c="$3"

    if [[ "$base" == "array_return_literal" ]]; then
        grep -Fq 'pgy_ai _pgy_array_value = pgy_ai_new();' "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend array return storage was lost" >&2
            exit 1
        }
        grep -Fq 'pgy_ai_push(&_pgy_array_value, 4); _pgy_array_value;' \
            "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend array return emission drifted" >&2
            exit 1
        }
        return 0
    fi

    [[ "$base" == "ast_node_array_literal" ]] || return 0

    grep -Fq 'pgy_CodegenAstTextNode_array_push(&nodes, (CodegenAstTextNode){' \
        "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend array literal did not emit a typed push" >&2
        exit 1
    }
    grep -Fq '.indent = (2), .text = ("Let"), .parent = (0), .kind = (8)' \
        "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend array literal struct fields drifted" >&2
        exit 1
    }
}
