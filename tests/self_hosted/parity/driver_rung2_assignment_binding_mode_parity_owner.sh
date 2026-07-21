#!/usr/bin/env bash
# Owns assignment binding-mode carriage and missing-fact rejection.

pgy_selfhost_verify_driver_rung2_assignment_binding_mode() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local invalid_mode
    [[ "$base" == "bubble_sort_basic" ]] || return 0

    grep -Fq '"arg0":"arr","arg1":"inout_param"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend assignment parameter mode was lost" >&2
        exit 1
    }
    invalid_mode="$BUILD_DIR/${base}_${backend}.invalid-assignment-mode.mir.json"
    sed 's/"arg1":"inout_param"/"arg1":"local"/g' \
        "$self_mir_json" >"$invalid_mode"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$invalid_mode")" \
        >"$invalid_mode.out" 2>"$invalid_mode.err"); then
        echo "[self-host-parity:driver-rung2] $backend invalid assignment mode was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR assignment binding-mode fact is missing or invalid" \
        "$invalid_mode.err" "$invalid_mode.out" || {
        echo "[self-host-parity:driver-rung2] $backend assignment-mode diagnostic drifted" >&2
        cat "$invalid_mode.out" "$invalid_mode.err" >&2
        exit 1
    }
}
