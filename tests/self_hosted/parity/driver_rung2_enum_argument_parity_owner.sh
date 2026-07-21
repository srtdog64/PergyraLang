#!/usr/bin/env bash
# Owns DRV-2 payload-free enum value projection checks.

pgy_selfhost_verify_driver_rung2_enum_argument() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_graph

    if [[ "$base" == "array_enum" ]]; then
        grep -Fq '"kind":"enum","nominal_kind":"enum","name":"Color"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend enum local owner fact drifted" >&2
            exit 1
        }
        missing_graph="${self_mir_json%.json}.missing-enum-local-owner.mir.json"
        pgy_replace_first_literal "$self_mir_json" "$missing_graph" \
            '"kind":"enum","nominal_kind":"enum","name":"Color"' \
            '"kind":"enum","nominal_kind":"enum","name":"MissingColor"'
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
            >"$missing_graph.out" 2>"$missing_graph.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing enum local owner was accepted" >&2
            exit 1
        fi
        grep -Fq "Code: let_type_mismatch" \
            "$missing_graph.err" "$missing_graph.out" || {
            echo "[self-host-parity:driver-rung2] $backend enum local owner diagnostic drifted" >&2
            cat "$missing_graph.out" "$missing_graph.err" >&2
            exit 1
        }
        return 0
    fi

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
