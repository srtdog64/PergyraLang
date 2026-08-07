#!/usr/bin/env bash
# Owns fail-closed mutations for concrete resource-call ABI consumers.

pgy_selfhost_verify_driver_rung2_resource_runtime_abi_negative() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local driver_bin="$5" machine_declaration="$6"
    local missing_row identity_row payload_row stray_row aux_identity_row aux_injected_row needle replacement
    local -a consumer_command

    if [[ "$base" != "device_slot_routine" &&
        "$base" != "bool_helper_while_slot" ]]; then
        return 0
    fi

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
    if [[ "$machine_fixture" -eq 1 ]]; then
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
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
    if [[ "$machine_fixture" -eq 1 ]]; then
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
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
    if [[ "$machine_fixture" -eq 1 ]]; then
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
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

    # A non-resource instruction may not carry an unowned runtime ABI value.
    # Wrong-kind row data is invalid rather than equivalent to absence.
    stray_row="$BUILD_DIR/${base}_${backend}.stray-consumer-runtime-row.mir.json"
    needle='"kind":"return","name":"return"'
    replacement='"kind":"return","name":"return","runtime_call_abi":null'
    if ! pgy_replace_first_literal \
        "$self_mir_json" "$stray_row" "$needle" "$replacement"; then
        echo "[self-host-parity:driver-rung2] $backend stray consumer runtime-row mutation did not apply: $base" >&2
        return 1
    fi

    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$stray_row")")
    if [[ "$machine_fixture" -eq 1 ]]; then
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$stray_row.out" 2>"$stray_row.err"); then
        echo "[self-host-parity:driver-rung2] $backend stray consumer runtime row was accepted: $base" >&2
        return 1
    fi
    grep -Fq \
        "resource instruction or consumer is missing its lowered runtime-call ABI row" \
        "$stray_row.err" "$stray_row.out" || {
        echo "[self-host-parity:driver-rung2] $backend stray consumer runtime-row diagnostic drifted: $base" >&2
        cat "$stray_row.out" "$stray_row.err" >&2
        return 1
    }

    # Inject an auxiliary row with a deliberately invalid stable identity. The
    # old primary-only reader would ignore this unknown array and accept the
    # document; the owner must reject it before reconstruction proceeds.
    if grep -Fq '"type":"DeviceSlot<Int>"' "$self_mir_json"; then
        aux_injected_row="$BUILD_DIR/${base}_${backend}.invalid-aux-runtime-row.mir.json"
        needle=',"runtime_call_abi":{"owner":"MIRResource","id":'
        replacement=',"runtime_call_abi_aux":[{"owner":"MIRResource","id":0,"domain":"native-resource","type":"DeviceSlot<Int>","operation":"Write","symbol":"pgy_device_write_Int","target_kind":"function","materialization":"mir_abi_resource_row","call_shape":"container_ptr_value_to_void"}],"runtime_call_abi":{"owner":"MIRResource","id":'
        if ! pgy_replace_first_literal \
            "$self_mir_json" "$aux_injected_row" "$needle" "$replacement"; then
            echo "[self-host-parity:driver-rung2] $backend auxiliary runtime-row injection did not apply: $base" >&2
            return 1
        fi

        consumer_command=("$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$aux_injected_row")")
        if [[ "$machine_fixture" -eq 1 ]]; then
            consumer_command+=(--machine-manifest-json "$machine_declaration")
        fi
        if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
            >"$aux_injected_row.out" 2>"$aux_injected_row.err"); then
            echo "[self-host-parity:driver-rung2] $backend invalid auxiliary runtime row was accepted: $base" >&2
            return 1
        fi
        grep -Fq \
            "resource instruction or consumer is missing its lowered runtime-call ABI row" \
            "$aux_injected_row.err" "$aux_injected_row.out" || {
            echo "[self-host-parity:driver-rung2] $backend invalid auxiliary runtime-row diagnostic drifted: $base" >&2
            cat "$aux_injected_row.out" "$aux_injected_row.err" >&2
            return 1
        }
    fi

    # A primary Claim row can own concrete auxiliary Read/Write or pin rows.
    # Mutate the first auxiliary identity as a separate negative leg so the
    # self-host validator cannot silently validate only the primary row.
    if grep -Fq '"runtime_call_abi_aux":[{"owner":"MIRResource","id":' \
        "$self_mir_json"; then
        aux_identity_row="$BUILD_DIR/${base}_${backend}.wrong-aux-runtime-row-id.mir.json"
        needle='"runtime_call_abi_aux":[{"owner":"MIRResource","id":'
        replacement='"runtime_call_abi_aux":[{"owner":"MIRResource","id":0,"id_original":'
        if ! pgy_replace_first_literal \
            "$self_mir_json" "$aux_identity_row" "$needle" "$replacement"; then
            echo "[self-host-parity:driver-rung2] $backend auxiliary runtime-row identity mutation did not apply: $base" >&2
            return 1
        fi

        consumer_command=("$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$aux_identity_row")")
        if [[ "$machine_fixture" -eq 1 ]]; then
            consumer_command+=(--machine-manifest-json "$machine_declaration")
        fi
        if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
            >"$aux_identity_row.out" 2>"$aux_identity_row.err"); then
            echo "[self-host-parity:driver-rung2] $backend wrong auxiliary runtime-row identity was accepted: $base" >&2
            return 1
        fi
        grep -Fq \
            "resource instruction or consumer is missing its lowered runtime-call ABI row" \
            "$aux_identity_row.err" "$aux_identity_row.out" || {
            echo "[self-host-parity:driver-rung2] $backend wrong auxiliary runtime-row identity diagnostic drifted: $base" >&2
            cat "$aux_identity_row.out" "$aux_identity_row.err" >&2
            return 1
        }
    fi
}
