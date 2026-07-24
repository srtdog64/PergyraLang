#!/usr/bin/env bash
# Owns DRV-2 non-identifier foreach normalization checks.

pgy_selfhost_verify_driver_rung2_foreach_call_type() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_local

    [[ "$base" == "for_each_call" ]] || return 0

    local ordinal call_graph_count synthetic_graph_count
    for ordinal in 0 1 2; do
        grep -Fq "{\"name\":\"__pgy_forin_$ordinal\",\"type\":\"Array<Int>\"}" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach synthetic local $ordinal drifted" >&2
            exit 1
        }
        grep -Fq "\"kind\":\"def\",\"name\":\"ssa-def\",\"result\":\"__pgy_forin_$ordinal.1\",\"arg0\":\"__pgy_forin_$ordinal\",\"arg1\":null,\"slot_anchor\":null,\"abi_type_name\":\"Array<Int>\"" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach synthetic def $ordinal drifted" >&2
            exit 1
        }
        grep -Fq "\"uses\":[\"__pgy_forin_$ordinal.1\"]" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach branch use $ordinal drifted" >&2
            exit 1
        }
        synthetic_graph_count="$(
            { grep -oF \
                "\"expr0_graph\":{\"root\":0,\"nodes\":[{\"kind\":\"leaf\",\"text\":\"__pgy_forin_$ordinal\"" \
                "$self_mir_json" || true; } | wc -l | tr -d ' '
        )"
        if [[ "$synthetic_graph_count" -ne 2 ]]; then
            echo "[self-host-parity:driver-rung2] $backend foreach shared synthetic graph $ordinal drifted: $synthetic_graph_count != 2" >&2
            exit 1
        fi
    done
    call_graph_count="$(
        { grep -oF \
            '"kind":"call","text":"MakeValues()","call_target_kind":"direct","call_target_name":"MakeValues"' \
            "$self_mir_json" || true; } | wc -l | tr -d ' '
    )"
    if [[ "$call_graph_count" -ne 3 ]]; then
        echo "[self-host-parity:driver-rung2] $backend foreach call graph count drifted: $call_graph_count != 3" >&2
        exit 1
    fi
    if grep -Eq '"kind":"loop-init"[^}]*"name":"loop-init"[^}]*"result":null[^}]*"arg0":"value"[^}]*"expr0":"MakeValues\(\)"' \
        "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend retained direct-call foreach consumption" >&2
        exit 1
    fi

    missing_local="${self_mir_json%.json}.missing-synthetic-local.mir.json"
    sed 's/,{"name":"__pgy_forin_0","type":"Array<Int>"}//' \
        "$self_mir_json" >"$missing_local"
    if [[ "$(<"$self_mir_json")" == "$(<"$missing_local")" ]]; then
        echo "[self-host-parity:driver-rung2] $backend synthetic-local mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_local")" \
        >"$missing_local.out" 2>"$missing_local.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing foreach synthetic local was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_local.err" "$missing_local.out" || {
        echo "[self-host-parity:driver-rung2] $backend foreach missing-fact diagnostic drifted" >&2
        cat "$missing_local.out" "$missing_local.err" >&2
        exit 1
    }
}
