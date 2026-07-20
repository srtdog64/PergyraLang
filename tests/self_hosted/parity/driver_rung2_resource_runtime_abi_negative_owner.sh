#!/usr/bin/env bash
# Owns fail-closed mutations for concrete resource-call ABI consumers.

pgy_selfhost_verify_driver_rung2_resource_runtime_abi_negative() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local driver_bin="$5" machine_declaration="$6"
    local missing_row identity_row payload_row needle replacement
    local -a consumer_command

    [[ "$machine_fixture" -eq 1 && "$base" == "device_slot_routine" ]] || return 0

    missing_row="$BUILD_DIR/${base}_${backend}.missing-consumer-runtime-row.mir.json"
    needle='"runtime_call_abi":{"owner":"MIRResource","id":'
    replacement='"runtime_call_abi_removed":{"owner":"MIRResource","id":'
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

    identity_row="$BUILD_DIR/${base}_${backend}.wrong-consumer-runtime-row-id.mir.json"
    needle='"runtime_call_abi":{"owner":"MIRResource","id":'
    replacement='"runtime_call_abi":{"owner":"MIRResource","id":0,"id_original":'
    if ! pgy_replace_first_literal \
        "$self_mir_json" "$identity_row" "$needle" "$replacement"; then
        echo "[self-host-parity:driver-rung2] $backend consumer runtime-row identity mutation did not apply: $base" >&2
        return 1
    fi

    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$identity_row")")
    consumer_command+=("$machine_declaration")
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$identity_row.out" 2>"$identity_row.err"); then
        echo "[self-host-parity:driver-rung2] $backend wrong consumer runtime-row identity was accepted: $base" >&2
        return 1
    fi
    grep -Fq \
        "resource instruction or consumer is missing its lowered runtime-call ABI row" \
        "$identity_row.err" "$identity_row.out" || {
        echo "[self-host-parity:driver-rung2] $backend wrong consumer runtime-row identity diagnostic drifted: $base" >&2
        cat "$identity_row.out" "$identity_row.err" >&2
        return 1
    }

    payload_row="$BUILD_DIR/${base}_${backend}.wrong-consumer-runtime-row-payload.mir.json"
    needle='"symbol":"'
    replacement='"symbol":"pgy_wrong_'
    if ! pgy_replace_first_literal \
        "$self_mir_json" "$payload_row" "$needle" "$replacement"; then
        echo "[self-host-parity:driver-rung2] $backend consumer runtime-row payload mutation did not apply: $base" >&2
        return 1
    fi

    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$payload_row")")
    consumer_command+=("$machine_declaration")
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$payload_row.out" 2>"$payload_row.err"); then
        echo "[self-host-parity:driver-rung2] $backend wrong consumer runtime-row payload was accepted: $base" >&2
        return 1
    fi
    grep -Fq \
        "resource instruction or consumer is missing its lowered runtime-call ABI row" \
        "$payload_row.err" "$payload_row.out" || {
        echo "[self-host-parity:driver-rung2] $backend wrong consumer runtime-row payload diagnostic drifted: $base" >&2
        cat "$payload_row.out" "$payload_row.err" >&2
        return 1
    }
}
