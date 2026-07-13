#!/usr/bin/env bash
# Owns DRV-2 non-identifier foreach normalization checks.

pgy_selfhost_verify_driver_rung2_foreach_call_type() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_local

    [[ "$base" == "for_each_call" ]] || return 0

    local ordinal
    for ordinal in 0 1 2; do
        grep -Fq "{\"name\":\"__pgy_forin_$ordinal\",\"type\":\"Array<Int>\"}" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach synthetic local $ordinal drifted" >&2
            exit 1
        }
        grep -Fq "\"kind\":\"def\",\"name\":\"ssa-def\",\"result\":\"__pgy_forin_$ordinal.1\",\"arg0\":\"__pgy_forin_$ordinal\",\"arg1\":null,\"expr0\":\"MakeValues()\"" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach synthetic def $ordinal drifted" >&2
            exit 1
        }
        grep -Fq "\"uses\":[\"__pgy_forin_$ordinal.1\"]" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach branch use $ordinal drifted" >&2
            exit 1
        }
    done
    if grep -Fq '"kind":"loop-init","name":"loop-init","result":null,"arg0":"value","arg1":null,"expr0":"MakeValues()"' \
        "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend retained direct-call foreach consumption" >&2
        exit 1
    fi

    missing_local="${self_mir_json%.json}.missing-synthetic-local.mir.json"
    sed 's/,{"name":"__pgy_forin_0","type":"Array<Int>"}//' \
        "$self_mir_json" >"$missing_local"
    if cmp -s "$self_mir_json" "$missing_local"; then
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
