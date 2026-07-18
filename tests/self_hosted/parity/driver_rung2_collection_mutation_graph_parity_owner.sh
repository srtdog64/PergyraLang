#!/usr/bin/env bash
# Owns DRV-2 collection mutator value-graph preservation assertions.

pgy_selfhost_verify_driver_rung2_collection_mutation_graph() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local operation

    if [[ "$base" == "ast_node_array_push" ]]; then
        operation="ArrayPush"
        grep -Fq '"expr0":"ArrayPush(nodes, CodegenAstTextNode(2, \"Let\", 0, 8))","expr0_graph":{' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend ArrayPush value graph was lost" >&2
            exit 1
        }
    elif [[ "$base" == "ast_node_array_set" ]]; then
        operation="ArraySet"
        grep -Fq '"expr0":"ArraySet(nodes, 0, CodegenAstTextNode(2, \"Let\", 0, 8))","expr0_graph":{' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend ArraySet value graph was lost" >&2
            exit 1
        }
    elif [[ "$base" == "str_array" ]]; then
        operation="ArraySet"
        grep -Fq '"expr0":"ArraySet(names, 1, \"BOB\")","expr0_graph":{"root":0,"nodes":[{"kind":"string_literal","text":"\"BOB\""' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend String ArraySet value graph was lost" >&2
            exit 1
        }
    else
        return 0
    fi

    if [[ "$base" != "str_array" ]]; then
        grep -Fq '"kind":"call_argument","text":"CodegenAstTextNode(2, \"Let\", 0, 8)"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend $operation constructor graph was lost" >&2
            exit 1
        }
    fi
}
