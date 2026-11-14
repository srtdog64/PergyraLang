#!/usr/bin/env bash
# Owns the typed condition-graph contract for nested else-if branches.
pgy_selfhost_verify_driver_rung2_else_if_graph() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local condition

    if [[ "$base" != "else_if_chain" ]]; then
        return 0
    fi

    for condition in '(n < 0)' '(n == 0)' '(n < 10)' '(n == 31)' '(n == 47)'; do
        grep -Fq "\"expr0\":\"$condition\",\"expr0_graph\":{" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend else-if condition graph drifted: $condition" >&2
            exit 1
        }
    done
}
