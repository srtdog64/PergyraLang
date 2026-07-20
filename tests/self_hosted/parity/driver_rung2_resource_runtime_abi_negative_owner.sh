#!/usr/bin/env bash
# Owns fail-closed mutations for concrete resource-call ABI consumers.

pgy_selfhost_verify_driver_rung2_resource_runtime_abi_negative() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local driver_bin="$5" machine_declaration="$6"
    local missing_row needle replacement
    local -a consumer_command

    [[ "$machine_fixture" -eq 1 && "$base" == "device_slot_routine" ]] || return 0

    missing_row="$BUILD_DIR/${base}_${backend}.missing-consumer-runtime-row.mir.json"
    needle='"runtime_call_abi":{"owner":"MIRResource","domain":"native-resource","type":"DeviceSlot<Int>","operation":"Read"'
    replacement='"runtime_call_abi_removed":{"owner":"MIRResource","domain":"native-resource","type":"DeviceSlot<Int>","operation":"Read"'
    if ! pgy_replace_first_literal \
        "$self_mir_json" "$missing_row" "$needle" "$replacement"; then
        echo "[self-host-parity:driver-rung2] $backend consumer runtime-row mutation did not apply: $base" >&2
        return 1
    fi

    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_row")")
    consumer_command+=("$machine_declaration")
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$missing_row.out" 2>"$missing_row.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing consumer runtime row was accepted: $base" >&2
        return 1
    fi
    grep -Fq \
        "resource instruction or consumer is missing its lowered runtime-call ABI row" \
        "$missing_row.err" "$missing_row.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing consumer runtime-row diagnostic drifted: $base" >&2
        cat "$missing_row.out" "$missing_row.err" >&2
        return 1
    }
}
