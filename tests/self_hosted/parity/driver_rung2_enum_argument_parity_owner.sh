#!/usr/bin/env bash
# Owns DRV-2 qualified enum call-argument graph checks.

pgy_selfhost_verify_driver_rung2_enum_argument() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_graph

    [[ "$base" == "enum_call_argument" ]] || return 0
    grep -Fq '"kind":"member_access","text":"Direction.East"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend enum argument member graph drifted" >&2
        exit 1
    }
    grep -Fq '"kind":"call_argument","text":"IsEast(Direction.East)"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend enum argument call spine drifted" >&2
        exit 1
    }
    missing_graph="${self_mir_json%.json}.missing-enum-argument.mir.json"
    sed 's/"expr0":"IsEast(Direction.East)","expr0_graph"/"expr0":"IsEast(Direction.East)","expr0_graph_removed"/g' \
        "$self_mir_json" >"$missing_graph"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
        >"$missing_graph.out" 2>"$missing_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing enum argument graph was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_graph.err" "$missing_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend enum argument diagnostic drifted" >&2
        cat "$missing_graph.out" "$missing_graph.err" >&2
        exit 1
    }
}
