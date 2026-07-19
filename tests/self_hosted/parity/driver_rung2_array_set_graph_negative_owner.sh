#!/usr/bin/env bash
# Owns the ArraySet index-graph fail-closed mutation for DRV-2.

pgy_selfhost_verify_driver_rung2_array_set_graph_negative() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_secondary

    [[ "$base" == "ast_node_array_set" ]] || return 0
    missing_secondary="$BUILD_DIR/${base}_${backend}.missing-secondary-graph.mir.json"
    sed 's/"expr1_graph"/"expr1_graph_removed"/g' \
        "$self_mir_json" >"$missing_secondary"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_secondary")" \
        >"$missing_secondary.out" 2>"$missing_secondary.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing ArraySet index graph was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_secondary.err" "$missing_secondary.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing ArraySet index graph diagnostic drifted" >&2
        cat "$missing_secondary.out" "$missing_secondary.err" >&2
        exit 1
    }
}
