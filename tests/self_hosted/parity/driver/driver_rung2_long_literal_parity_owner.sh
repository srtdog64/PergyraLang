#!/usr/bin/env bash
# Owns DRV-2 Long-literal identity and text-fallback rejection.

pgy_selfhost_verify_driver_rung2_long_literal() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local misclassified_long malformed_long

    [[ "$base" == "long_scalar" ]] || return 0
    grep -Fq '"kind":"long_literal","text":"42000000000L"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend Long literal fact was lost" >&2
        exit 1
    }
    misclassified_long="${self_mir_json%.json}.misclassified-long.mir.json"
    sed 's/"kind":"long_literal","text":"42000000000L"/"kind":"leaf","text":"42000000000L"/g' \
        "$self_mir_json" >"$misclassified_long"
    grep -Fq '"kind":"leaf","text":"42000000000L"' \
        "$misclassified_long" || {
        echo "[self-host-parity:driver-rung2] $backend Long-kind mutation did not apply" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$misclassified_long")" \
        >"$misclassified_long.out" 2>"$misclassified_long.err"); then
        echo "[self-host-parity:driver-rung2] $backend misclassified Long literal was accepted" >&2
        exit 1
    fi
    grep -Fq "ToString argument type fact is missing" \
        "$misclassified_long.err" "$misclassified_long.out" || {
        echo "[self-host-parity:driver-rung2] $backend Long-kind diagnostic drifted" >&2
        cat "$misclassified_long.out" "$misclassified_long.err" >&2
        exit 1
    }

    malformed_long="${self_mir_json%.json}.malformed-long.mir.json"
    sed 's/"kind":"long_literal","text":"42000000000L"/"kind":"long_literal","text":"4200000000xL"/g' \
        "$self_mir_json" >"$malformed_long"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_long")" \
        >"$malformed_long.out" 2>"$malformed_long.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed Long payload was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_long.err" "$malformed_long.out" || {
        echo "[self-host-parity:driver-rung2] $backend malformed Long diagnostic drifted" >&2
        cat "$malformed_long.out" "$malformed_long.err" >&2
        exit 1
    }
}
