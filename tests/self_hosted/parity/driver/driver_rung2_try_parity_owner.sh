#!/usr/bin/env bash
# Owns DRV-2 Option/Result postfix-try graph preservation checks.

pgy_selfhost_verify_driver_rung2_try_graph() {
    local backend="$1"
    local base="$2"
    local graph_json="$3"
    local stage="$4"
    local expected_try_graph=""
    local expected_try_call=""

    if [[ "$base" == "option_try" ]]; then
        expected_try_graph='"kind":"try","text":"?Pick(x)"'
        expected_try_call='"kind":"call_argument","text":"Pick(x)"'
    elif [[ "$base" == "result_try" ]]; then
        expected_try_graph='"kind":"try","text":"?Validate(doubled)"'
        expected_try_call='"kind":"call_argument","text":"Validate(doubled)"'
    else
        return 0
    fi

    grep -Fq "$expected_try_graph" "$graph_json" || {
        echo "[self-host-parity:driver-rung2] $backend $stage try graph was lost" >&2
        exit 1
    }
    grep -Fq "$expected_try_call" "$graph_json" || {
        echo "[self-host-parity:driver-rung2] $backend $stage try operand call spine was lost" >&2
        exit 1
    }
}
