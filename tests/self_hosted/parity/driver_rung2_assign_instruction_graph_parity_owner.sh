#!/usr/bin/env bash
# Owns residual MIR ASSIGN target/value graph carriage and fail-closed checks.
pgy_selfhost_verify_driver_rung2_assign_instruction_graph() {
    local backend="$1"
    local base="$2"
    local native_mir_json="$3"
    local self_mir_json="$4"
    local self_emitted_c="$5"
    local driver_bin="$6"
    local lane missing_graph native_emitted_c
    [[ "$base" == "array_index_assign" ]] || return 0

    grep -Fq '"kind":"assign"' "$native_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend native residual assignment fact was lost" >&2
        exit 1
    }
    grep -Fq '"expr0":"nums[1]","expr0_graph":{' "$native_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend native assignment target graph was lost" >&2
        exit 1
    }
    grep -Fq '"expr1":"9","expr1_graph":{' "$native_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend native assignment value graph was lost" >&2
        exit 1
    }
    grep -Fq '"kind":"def"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend self assignment SSA definition was lost" >&2
        exit 1
    }
    grep -Fq '"expr0":"9","expr0_graph":{' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend self assignment value graph was lost" >&2
        exit 1
    }
    grep -Fq '"expr1":"nums[1]","expr1_graph":{' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend self assignment target graph was lost" >&2
        exit 1
    }

    native_emitted_c="$BUILD_DIR/${base}_${backend}.native-direct.c"
    if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$native_mir_json")" \
        >"$native_emitted_c.raw" 2>"$native_emitted_c.err"); then
        echo "[self-host-parity:driver-rung2] $backend native assignment hard consumer failed" >&2
        cat "$native_emitted_c.raw" "$native_emitted_c.err" >&2
        exit 1
    fi
    tr -d '\r' <"$native_emitted_c.raw" >"$native_emitted_c"
    rm -f "$native_emitted_c.raw"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "driver-rung2:$backend:$base:native-assign-c" "$BUILD_DIR" \
        "$self_emitted_c" "$native_emitted_c" "emitted_c"

    for lane in expr0 expr1; do
        missing_graph="$BUILD_DIR/${base}_${backend}.native-missing-${lane}-graph.mir.json"
        sed "s/\"${lane}_graph\"/\"${lane}_graph_removed\"/g" \
            "$native_mir_json" >"$missing_graph"
        if (cd "$ROOT_DIR" && "$driver_bin" --canonicalize-mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_graph")" \
            >"$missing_graph.out" 2>"$missing_graph.err"); then
            echo "[self-host-parity:driver-rung2] $backend native assignment accepted missing $lane graph" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$missing_graph.err" "$missing_graph.out" || {
            echo "[self-host-parity:driver-rung2] $backend native assignment $lane diagnostic drifted" >&2
            cat "$missing_graph.out" "$missing_graph.err" >&2
            exit 1
        }
    done
}
