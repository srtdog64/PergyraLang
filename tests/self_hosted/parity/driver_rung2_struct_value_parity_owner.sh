#!/usr/bin/env bash
# Owns DRV-2 general struct-value graph checks.

pgy_selfhost_verify_driver_rung2_struct_value() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local malformed_graph

    [[ "$base" == "struct_literal_value_flow" ]] || return 0

    if [[ "$(grep -Fo '"kind":"struct_literal","text":"Pair { }"' \
        "$self_mir_json" | wc -l | tr -d ' ')" -lt 3 ]]; then
        echo "[self-host-parity:driver-rung2] $backend struct value graph coverage drifted" >&2
        exit 1
    fi
    for carried_value in \
        '"expr0":"Pair { left: (base + 1), right: (base + 2) }","expr0_graph"' \
        '"expr0":"Pair { left: 1, right: 2 }","expr0_graph"' \
        '"expr0":"Pair { left: 3, right: 4 }","expr0_graph"'; do
        grep -Fq "$carried_value" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend struct value graph was lost: $carried_value" >&2
            exit 1
        }
    done

    malformed_graph="${self_mir_json%.json}.malformed-struct-value.mir.json"
    sed 's/"kind":"struct_literal"/"kind":"leaf"/g' \
        "$self_mir_json" >"$malformed_graph"
    if cmp -s "$self_mir_json" "$malformed_graph"; then
        echo "[self-host-parity:driver-rung2] $backend struct-value mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_graph")" \
        >"$malformed_graph.out" 2>"$malformed_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed struct value was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_graph.err" "$malformed_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend struct-value diagnostic drifted" >&2
        cat "$malformed_graph.out" "$malformed_graph.err" >&2
        exit 1
    }
}
