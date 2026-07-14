#!/usr/bin/env bash
# Owns DRV-2 array-literal graph consumption assertions.

pgy_selfhost_verify_driver_rung2_array_literal_graph() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"

    [[ "$base" == "ast_node_array_literal" ]] || return

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

    [[ "$base" == "ast_node_array_literal" ]] || return

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
