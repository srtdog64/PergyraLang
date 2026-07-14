#!/usr/bin/env bash
# Owns DRV-2 typed array-literal call-argument graph checks.

pgy_selfhost_verify_driver_rung2_array_argument() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local malformed_graph

    [[ "$base" == "array_literal_call_argument" ]] || return 0

    grep -Fq '"kind":"array_literal","text":"[]"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend array literal root drifted" >&2
        exit 1
    }
    if [[ "$(grep -Fo '"kind":"array_element"' "$self_mir_json" | wc -l | tr -d ' ')" -lt 2 ]]; then
        echo "[self-host-parity:driver-rung2] $backend array element graph drifted" >&2
        exit 1
    fi
    grep -Fq '"kind":"call_argument","text":"Double(4)"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend nested array element call graph drifted" >&2
        exit 1
    }

    malformed_graph="${self_mir_json%.json}.malformed-array-spine.mir.json"
    sed 's/"kind":"array_literal"/"kind":"leaf"/g' \
        "$self_mir_json" >"$malformed_graph"
    if cmp -s "$self_mir_json" "$malformed_graph"; then
        echo "[self-host-parity:driver-rung2] $backend array-spine mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_graph")" \
        >"$malformed_graph.out" 2>"$malformed_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed array spine was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_graph.err" "$malformed_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend array-spine diagnostic drifted" >&2
        cat "$malformed_graph.out" "$malformed_graph.err" >&2
        exit 1
    }
}
