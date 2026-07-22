#!/usr/bin/env bash
# Owns fail-closed mutations for self-produced MIR expression graphs.
pgy_selfhost_verify_driver_rung2_mir_graph_negatives() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local machine_declaration="${5:-}" missing_graph invalid_graph invalid_cast_target
    local -a consumer_command

    if ! grep -Fq '"expr0_graph":{' "$self_mir_json"; then
        return 0
    fi

    missing_graph="$BUILD_DIR/${base}_${backend}.missing-graph.mir.json"
    sed 's/"expr0_graph"/"expr0_graph_removed"/g' \
        "$self_mir_json" >"$missing_graph"
    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_graph")")
    if [[ -n "$machine_declaration" ]]; then
        consumer_command+=("$machine_declaration")
    fi
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$missing_graph.out" 2>"$missing_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing expression graph was accepted: $base" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_graph.err" "$missing_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing expression graph diagnostic drifted: $base" >&2
        cat "$missing_graph.out" "$missing_graph.err" >&2
        exit 1
    }
    pgy_selfhost_verify_driver_rung2_array_set_graph_negative \
        "$backend" "$base" "$self_mir_json" "$driver_bin"
    pgy_selfhost_verify_driver_rung2_operator_kind_negative "$backend" "$base" "$self_mir_json" "$driver_bin"
    if [[ "$base" == "result_int_core" ]]; then
        if (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
            >"$missing_graph.canonical.out" 2>"$missing_graph.canonical.err"); then
            echo "[self-host-parity:driver-rung2] $backend canonicalizer accepted a missing expression graph" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$missing_graph.canonical.err" "$missing_graph.canonical.out" || {
            echo "[self-host-parity:driver-rung2] $backend canonicalizer missing-graph diagnostic drifted" >&2
            cat "$missing_graph.canonical.out" "$missing_graph.canonical.err" >&2
            exit 1
        }
    fi

    invalid_graph="$BUILD_DIR/${base}_${backend}.invalid-graph.mir.json"
    sed 's/"root":[0-9][0-9]*/"root":999/g' \
        "$self_mir_json" >"$invalid_graph"
    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$invalid_graph")")
    if [[ -n "$machine_declaration" ]]; then
        consumer_command+=("$machine_declaration")
    fi
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$invalid_graph.out" 2>"$invalid_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend invalid expression graph was accepted: $base" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$invalid_graph.err" "$invalid_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend invalid expression graph diagnostic drifted: $base" >&2
        cat "$invalid_graph.out" "$invalid_graph.err" >&2
        exit 1
    }
    if [[ "$base" == "cast_numeric" ]]; then
        invalid_cast_target="$BUILD_DIR/${base}_${backend}.invalid-cast-target.mir.json"
        sed 's/"kind":"type_name"/"kind":"leaf"/g' \
            "$self_mir_json" >"$invalid_cast_target"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$invalid_cast_target")" \
            >"$invalid_cast_target.out" 2>"$invalid_cast_target.err"); then
            echo "[self-host-parity:driver-rung2] $backend value leaf was accepted as a cast target type" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$invalid_cast_target.err" "$invalid_cast_target.out" || {
            echo "[self-host-parity:driver-rung2] $backend invalid cast target diagnostic drifted" >&2
            cat "$invalid_cast_target.out" "$invalid_cast_target.err" >&2
            exit 1
        }
    fi
    if [[ "$base" == "result_int_core" ]]; then
        if (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$(pgy_selfhost_path_relative_to_root "$invalid_graph")" \
            >"$invalid_graph.canonical.out" 2>"$invalid_graph.canonical.err"); then
            echo "[self-host-parity:driver-rung2] $backend canonicalizer accepted an invalid expression graph" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$invalid_graph.canonical.err" "$invalid_graph.canonical.out" || {
            echo "[self-host-parity:driver-rung2] $backend canonicalizer invalid-graph diagnostic drifted" >&2
            cat "$invalid_graph.canonical.out" "$invalid_graph.canonical.err" >&2
            exit 1
        }
    fi
}
