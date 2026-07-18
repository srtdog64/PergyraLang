#!/usr/bin/env bash
# Owns DRV-2 String-literal identity and text-fallback rejection.

pgy_selfhost_verify_driver_rung2_string_literal() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local misclassified_string malformed_string

    [[ "$base" == "str_array" ]] || return 0
    grep -Fq '"kind":"string_literal","text":"\"BOB\""' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend String literal fact was lost" >&2
        exit 1
    }
    misclassified_string="${self_mir_json%.json}.misclassified-string.mir.json"
    sed 's/"kind":"string_literal","text":"\\\"BOB\\\""/"kind":"leaf","text":"\\\"BOB\\\""/g' \
        "$self_mir_json" >"$misclassified_string"
    grep -Fq '"kind":"leaf","text":"\"BOB\""' \
        "$misclassified_string" || {
        echo "[self-host-parity:driver-rung2] $backend String-kind mutation did not apply" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$misclassified_string")" \
        >"$misclassified_string.out" 2>"$misclassified_string.err"); then
        echo "[self-host-parity:driver-rung2] $backend misclassified String literal was accepted" >&2
        exit 1
    fi
    grep -Fq 'CODEGEN ERROR: unsupported semantic leaf expression: "BOB"' \
        "$misclassified_string.err" "$misclassified_string.out" || {
        echo "[self-host-parity:driver-rung2] $backend String-kind diagnostic drifted" >&2
        cat "$misclassified_string.out" "$misclassified_string.err" >&2
        exit 1
    }

    malformed_string="${self_mir_json%.json}.malformed-string.mir.json"
    sed 's/"kind":"string_literal","text":"\\\"BOB\\\""/"kind":"string_literal","text":"BOB"/g' \
        "$self_mir_json" >"$malformed_string"
    grep -Fq '"kind":"string_literal","text":"BOB"' \
        "$malformed_string" || {
        echo "[self-host-parity:driver-rung2] $backend String-payload mutation did not apply" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_string")" \
        >"$malformed_string.out" 2>"$malformed_string.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed String payload was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_string.err" "$malformed_string.out" || {
        echo "[self-host-parity:driver-rung2] $backend malformed String diagnostic drifted" >&2
        cat "$malformed_string.out" "$malformed_string.err" >&2
        exit 1
    }
}
