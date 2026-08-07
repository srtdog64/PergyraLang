#!/usr/bin/env bash
# Owns fail-closed mutations for the MIR instruction ABI-layout tuple.

pgy_selfhost_verify_driver_rung2_abi_layout_negative() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local driver_bin="$5" machine_declaration="$6"
    local missing_row identity_row needle replacement static_id baseline_c
    local expected_type expected_layout_prefix
    local array_type array_layout_prefix
    local -a consumer_command

    if [[ "$base" == "option_string_core" ]]; then
        expected_type='Option<String>'
        expected_layout_prefix='"abi_layout_id":589228278,"abi_layout_required":true,"abi_layout":{"type":"Option<String>","size":16,"align":8'
    elif [[ "$base" == "array_sum_filtered" ]]; then
        expected_type='Array<Int>'
        expected_layout_prefix='"abi_layout_id":599770891,"abi_layout_required":true,"abi_layout":{"type":"Array<Int>","size":32,"align":8'
    elif [[ "$base" == "str_array" ]]; then
        expected_type='Array<String>'
        expected_layout_prefix='"abi_layout_id":703020034,"abi_layout_required":true,"abi_layout":{"type":"Array<String>","size":32,"align":8'
    elif [[ "$base" == "array_scalar_aggregate_core" ]]; then
        expected_type='Array<Long>'
        expected_layout_prefix='"abi_layout_id":716601410,"abi_layout_required":true,"abi_layout":{"type":"Array<Long>","size":32,"align":8'
    elif [[ "$base" == "array_double_aggregate_core" ]]; then
        expected_type='Array<Double>'
        expected_layout_prefix='"abi_layout_id":704297701,"abi_layout_required":true,"abi_layout":{"type":"Array<Double>","size":32,"align":8'
    elif [[ "$machine_fixture" -eq 1 && "$base" == "device_slot_routine" ]]; then
        expected_type='DeviceSlot<Int>'
        expected_layout_prefix='"abi_layout_id":707638132,"abi_layout_required":true,"abi_layout":{"type":"DeviceSlot<Int>","size":8,"align":4'
    else
        return 0
    fi

    grep -Fq \
        "\"abi_type_name\":\"$expected_type\"" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend static ABI type row is missing: $base" >&2
        return 1
    }
    grep -Fq \
        "$expected_layout_prefix" \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend $expected_type static ABI row drifted: $base" >&2
        return 1
    }

    if [[ "$base" == "array_scalar_aggregate_core" ]]; then
        for array_type in 'Array<Float>' 'Array<Bool>'; do
            if [[ "$array_type" == 'Array<Float>' ]]; then
                array_layout_prefix='"abi_layout_id":791395840,"abi_layout_required":true,"abi_layout":{"type":"Array<Float>","size":32,"align":8'
            elif [[ "$array_type" == 'Array<Bool>' ]]; then
                array_layout_prefix='"abi_layout_id":569019588,"abi_layout_required":true,"abi_layout":{"type":"Array<Bool>","size":32,"align":8'
            fi
            grep -Fq "\"abi_type_name\":\"$array_type\"" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend static ABI type row is missing: $array_type" >&2
                return 1
            }
            grep -Fq "$array_layout_prefix" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend $array_type static ABI row drifted" >&2
                return 1
            }
        done
    fi

    # A mutated tuple must either be rejected with the pinned diagnostic or
    # be provably unconsumed: acceptance is only admissible when the emitted
    # C is byte-identical to the unmutated baseline. A consumed-but-corrupted
    # row can never silently reshape the program.
    baseline_c="$BUILD_DIR/${base}_${backend}.abi-layout-baseline.c"
    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$self_mir_json")")
    if [[ -n "$machine_declaration" ]]; then
        # The CLI admits the manifest only through its named option.
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
    if ! (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$baseline_c" 2>"$baseline_c.err"); then
        echo "[self-host-parity:driver-rung2] $backend ABI-layout baseline consumption failed: $base" >&2
        cat "$baseline_c.err" >&2
        return 1
    fi

    missing_row="$BUILD_DIR/${base}_${backend}.missing-abi-layout-fact.mir.json"
    # The mutation removes the required-field key itself. Do not mutate a
    # dynamic tuple here: it would leave the producer-side static-row rung
    # untested.
    needle=",\"abi_layout_required\":true,\"abi_layout\":{\"type\":\"$expected_type\""
    replacement=",\"abi_layout_required_removed\":true,\"abi_layout\":{\"type\":\"$expected_type\""
    if ! pgy_replace_first_literal \
        "$self_mir_json" "$missing_row" "$needle" "$replacement"; then
        echo "[self-host-parity:driver-rung2] $backend missing ABI-layout mutation did not apply: $base" >&2
        return 1
    fi

    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_row")")
    if [[ -n "$machine_declaration" ]]; then
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$missing_row.out" 2>"$missing_row.err"); then
        cmp -s "$baseline_c" "$missing_row.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing ABI-layout tuple silently reshaped the C: $base" >&2
            return 1
        }
    else
        grep -Fq \
            "instruction is missing or carries an invalid MIR ABI layout fact" \
            "$missing_row.err" "$missing_row.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing ABI-layout diagnostic drifted: $base" >&2
            cat "$missing_row.out" "$missing_row.err" >&2
            return 1
        }
    fi

    identity_row="$BUILD_DIR/${base}_${backend}.wrong-abi-layout-identity.mir.json"
    static_id="$(sed -n 's/.*"abi_layout_id":\([0-9][0-9]*\),"abi_layout_required":true.*/\1/p' "$self_mir_json" | head -n 1)"
    if [[ -z "$static_id" || "$static_id" == "1" ]]; then
        echo "[self-host-parity:driver-rung2] $backend static ABI-layout identity was not produced: $base" >&2
        return 1
    fi
    needle="\"abi_layout_id\":${static_id},\"abi_layout_required\":true"
    replacement='"abi_layout_id":1,"abi_layout_required":true'
    if ! pgy_replace_first_literal \
        "$self_mir_json" "$identity_row" "$needle" "$replacement"; then
        echo "[self-host-parity:driver-rung2] $backend ABI-layout identity mutation did not apply: $base" >&2
        return 1
    fi

    consumer_command=("$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$identity_row")")
    if [[ -n "$machine_declaration" ]]; then
        consumer_command+=(--machine-manifest-json "$machine_declaration")
    fi
    if (cd "$ROOT_DIR" && "${consumer_command[@]}" \
        >"$identity_row.out" 2>"$identity_row.err"); then
        cmp -s "$baseline_c" "$identity_row.out" || {
            echo "[self-host-parity:driver-rung2] $backend wrong ABI-layout identity silently reshaped the C: $base" >&2
            return 1
        }
    else
        grep -Fq \
            "instruction is missing or carries an invalid MIR ABI layout fact" \
            "$identity_row.err" "$identity_row.out" || {
            echo "[self-host-parity:driver-rung2] $backend ABI-layout identity diagnostic drifted: $base" >&2
            cat "$identity_row.out" "$identity_row.err" >&2
            return 1
        }
    fi
}
