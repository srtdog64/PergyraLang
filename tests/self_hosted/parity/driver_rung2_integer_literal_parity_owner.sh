#!/usr/bin/env bash
# Owns DRV-2 integer-literal identity and text-fallback rejection.

pgy_selfhost_verify_driver_rung2_integer_literal_kind() {
    local backend="$1"
    local self_mir_json="$2"
    local driver_bin="$3"
    local misclassified_integer

    misclassified_integer="${self_mir_json%.json}.misclassified-integer.mir.json"
    sed 's/"kind":"integer_literal","text":"0"/"kind":"leaf","text":"0"/g' \
        "$self_mir_json" >"$misclassified_integer"
    grep -Fq '"kind":"leaf","text":"0"' "$misclassified_integer" || {
        echo "[self-host-parity:driver-rung2] $backend integer-kind mutation did not apply" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$misclassified_integer")" \
        >"$misclassified_integer.out" 2>"$misclassified_integer.err"); then
        echo "[self-host-parity:driver-rung2] $backend misclassified integer literal was accepted" >&2
        exit 1
    fi
    grep -Fq "unsupported semantic leaf expression: 0" \
        "$misclassified_integer.err" "$misclassified_integer.out" || {
        echo "[self-host-parity:driver-rung2] $backend integer-kind diagnostic drifted" >&2
        cat "$misclassified_integer.out" "$misclassified_integer.err" >&2
        exit 1
    }
}
