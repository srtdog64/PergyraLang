#!/usr/bin/env bash
# Owns DRV-2 inferred-generic aggregate value and emission checks.

pgy_selfhost_verify_driver_rung2_inferred_generic_value() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local drifted

    [[ "$base" == "generic_struct_field_inferred_value_flow" ]] || return 0

    grep -Fq 'Identity(41)' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend inferred generic text was lost" >&2
        exit 1
    }
    if grep -Fq '"kind":"generic_type_actual"' "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend inferred call gained explicit actuals" >&2
        exit 1
    fi

    drifted="${self_mir_json%.json}.inferred-actual-drift.mir.json"
    sed 's/"text":"41"/"text":"\\"bad\\""/g' \
        "$self_mir_json" >"$drifted"
    if cmp -s "$self_mir_json" "$drifted"; then
        echo "[self-host-parity:driver-rung2] $backend inferred actual mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$drifted")" \
        >"$drifted.out" 2>"$drifted.err"); then
        echo "[self-host-parity:driver-rung2] $backend inferred actual drift was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arg_type_mismatch" \
        "$drifted.out" "$drifted.err" || {
        echo "[self-host-parity:driver-rung2] $backend inferred drift diagnostic changed" >&2
        cat "$drifted.out" "$drifted.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_inferred_generic_emitted_c() {
    local backend="$1"
    local base="$2"
    local emitted_c="$3"

    [[ "$base" == "generic_struct_field_inferred_value_flow" ]] || return 0
    grep -Fq 'long long Identity_Int(long long value)' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend inferred generic symbol was not emitted" >&2
        exit 1
    }
    grep -Fq 'Identity_Int(41)' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend inferred call was not specialized" >&2
        exit 1
    }
    if grep -Fq 'long long Identity(long long value)' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend generic template leaked into C" >&2
        exit 1
    fi
}
