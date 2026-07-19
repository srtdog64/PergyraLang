#!/usr/bin/env bash
# Owns DRV-2 destructure type/binding carriage and missing-fact rejection.

pgy_selfhost_verify_driver_rung2_destructure() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_type

    [[ "$base" == "array_destructure" ]] || return 0
    for fact in \
        '"destructure_element_type":"String"' \
        '"destructure_bindings":["id_str","name","active_str"]' \
        '"name":"id_str","type":"String"' \
        '"name":"name","type":"String"' \
        '"name":"active_str","type":"String"' \
        '"uses":["csv.1"]'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend destructure fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_type="$BUILD_DIR/${base}_${backend}.missing-destructure-type.mir.json"
    pgy_replace_first_literal \
        "$self_mir_json" "$missing_type" \
        '"destructure_element_type":"String"' \
        '"destructure_element_type":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_type")" \
        >"$missing_type.out" 2>"$missing_type.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing destructure type was accepted" >&2
        exit 1
    fi
    grep -Fq "destructure initializer element type is unknown" \
        "$missing_type.err" "$missing_type.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing destructure type diagnostic drifted" >&2
        cat "$missing_type.out" "$missing_type.err" >&2
        exit 1
    }
}
