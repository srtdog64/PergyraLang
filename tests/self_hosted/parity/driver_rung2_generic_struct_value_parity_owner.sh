#!/usr/bin/env bash
# Owns DRV-2 explicit-generic struct-value graph and emission checks.

pgy_selfhost_verify_driver_rung2_generic_struct_value() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_formal missing_actual

    [[ "$base" == "generic_struct_field_value_flow" ]] || return 0

    grep -Fq '"generics":["T"]' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend generic formal MIR fact was lost" >&2
        exit 1
    }
    if [[ "$(grep -Fo '"kind":"generic_type_actual","text":"Int"' \
        "$self_mir_json" | wc -l | tr -d ' ')" -ne 4 ]]; then
        echo "[self-host-parity:driver-rung2] $backend generic actual graph coverage drifted" >&2
        exit 1
    fi
    grep -Fq 'Identity<Int>' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend generic canonical text was lost" >&2
        exit 1
    }

    missing_formal="${self_mir_json%.json}.missing-generic-formal.mir.json"
    sed 's/"generics":\["T"\]/"generics":[]/g' "$self_mir_json" >"$missing_formal"
    if [[ "$(<"$self_mir_json")" == "$(<"$missing_formal")" ]]; then
        echo "[self-host-parity:driver-rung2] $backend generic formal mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_formal")" \
        >"$missing_formal.out" 2>"$missing_formal.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing generic formal was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: generic_argument_count_mismatch" \
        "$missing_formal.out" "$missing_formal.err" || {
        echo "[self-host-parity:driver-rung2] $backend missing generic formal diagnostic drifted" >&2
        cat "$missing_formal.out" "$missing_formal.err" >&2
        exit 1
    }

    missing_actual="${self_mir_json%.json}.missing-generic-actual.mir.json"
    sed 's/"kind":"generic_type_actual"/"kind":"leaf"/g' \
        "$self_mir_json" >"$missing_actual"
    if [[ "$(<"$self_mir_json")" == "$(<"$missing_actual")" ]]; then
        echo "[self-host-parity:driver-rung2] $backend generic actual mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_actual")" \
        >"$missing_actual.out" 2>"$missing_actual.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing generic actual was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_actual.out" "$missing_actual.err" || {
        echo "[self-host-parity:driver-rung2] $backend missing generic actual diagnostic drifted" >&2
        cat "$missing_actual.out" "$missing_actual.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_generic_struct_emitted_c() {
    local backend="$1"
    local base="$2"
    local emitted_c="$3"

    [[ "$base" == "generic_struct_field_value_flow" ]] || return 0
    grep -Fq 'int32_t Identity_Int(int32_t value)' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend generic symbol was not emitted" >&2
        exit 1
    }
    if grep -Eq '(long long|int32_t) Identity\((long long|int32_t) value\)' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend generic template leaked into C" >&2
        exit 1
    fi
}
