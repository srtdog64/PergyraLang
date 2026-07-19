#!/usr/bin/env bash
# Owns DRV-2 inferred-generic aggregate value and emission checks.
pgy_selfhost_verify_driver_rung2_inferred_generic_value() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local drifted drift_code return_drifted missing_rows
    if [[ "$base" != "generic_struct_field_inferred_value_flow" &&
        "$base" != "generic_return_assignment_inferred_flow" ]]; then
        return 0
    fi
    grep -Fq 'Identity(41)' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend inferred generic text was lost" >&2
        exit 1
    }
    if grep -Fq '"kind":"generic_type_actual"' "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend inferred call gained explicit actuals" >&2
        exit 1
    fi
    grep -Fq '"target_kind":"direct","owner":"","callable":"Identity","specialized_symbol":"Identity_Int"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend inferred generic MIR row was lost" >&2
        exit 1
    }
    missing_rows="${self_mir_json%.json}.missing-generic-rows.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_rows" \
        '"generic_method_specializations":' '"generic_method_specializations_missing":'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_rows")" \
        >"$missing_rows.out" 2>"$missing_rows.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing direct generic MIR row was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR generic specialization facts are incomplete" \
        "$missing_rows.err" "$missing_rows.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing direct generic row diagnostic drifted" >&2
        cat "$missing_rows.out" "$missing_rows.err" >&2
        exit 1
    }

    drifted="${self_mir_json%.json}.inferred-actual-drift.mir.json"
    sed 's/"kind":"integer_literal","text":"41"/"kind":"string_literal","text":"\\"bad\\""/g' \
        "$self_mir_json" >"$drifted"
    if [[ "$(<"$self_mir_json")" == "$(<"$drifted")" ]]; then
        echo "[self-host-parity:driver-rung2] $backend inferred actual mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$drifted")" \
        >"$drifted.out" 2>"$drifted.err"); then
        echo "[self-host-parity:driver-rung2] $backend inferred actual drift was accepted" >&2
        exit 1
    fi
    drift_code="call_arg_type_mismatch"
    if [[ "$base" == "generic_return_assignment_inferred_flow" ]]; then
        drift_code="assign_type_mismatch"
    fi
    grep -Fq "Code: $drift_code" \
        "$drifted.out" "$drifted.err" || {
        echo "[self-host-parity:driver-rung2] $backend inferred drift diagnostic changed" >&2
        cat "$drifted.out" "$drifted.err" >&2
        exit 1
    }

    [[ "$base" == "generic_return_assignment_inferred_flow" ]] || return 0
    grep -Fq 'Identity(return_value)' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend inferred generic return text was lost" >&2
        exit 1
    }
    return_drifted="${self_mir_json%.json}.inferred-return-drift.mir.json"
    sed 's/"text":"return_value"/"text":"missing_return_value"/g' \
        "$self_mir_json" >"$return_drifted"
    if [[ "$(<"$self_mir_json")" == "$(<"$return_drifted")" ]]; then
        echo "[self-host-parity:driver-rung2] $backend inferred return mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$return_drifted")" \
        >"$return_drifted.out" 2>"$return_drifted.err"); then
        echo "[self-host-parity:driver-rung2] $backend inferred return drift was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arg_type_mismatch" \
        "$return_drifted.out" "$return_drifted.err" || {
        echo "[self-host-parity:driver-rung2] $backend inferred return diagnostic changed" >&2
        cat "$return_drifted.out" "$return_drifted.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_inferred_generic_emitted_c() {
    local backend="$1"
    local base="$2"
    local emitted_c="$3"

    if [[ "$base" != "generic_struct_field_inferred_value_flow" &&
        "$base" != "generic_return_assignment_inferred_flow" ]]; then
        return 0
    fi
    grep -Eq 'long long Identity_Int\(long long [A-Za-z_][A-Za-z0-9_]*\)' \
        "$emitted_c" || {
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
    if [[ "$base" == "generic_return_assignment_inferred_flow" ]]; then
        grep -Fq 'Identity_Int(return_value)' "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend inferred generic return was not specialized" >&2
            exit 1
        }
    fi
}
