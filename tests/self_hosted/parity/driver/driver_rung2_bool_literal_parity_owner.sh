#!/usr/bin/env bash
# Owns DRV-2 Bool-literal identity and text-fallback rejection.

pgy_selfhost_verify_driver_rung2_bool_literal() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local misclassified_bool malformed_bool

    [[ "$base" == "if_else_assign" ]] || return 0
    grep -Fq '"kind":"bool_literal","text":"false"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend Bool literal fact was lost" >&2
        exit 1
    }
    misclassified_bool="${self_mir_json%.json}.misclassified-bool.mir.json"
    sed 's/"kind":"bool_literal","text":"false"/"kind":"leaf","text":"false"/g' \
        "$self_mir_json" >"$misclassified_bool"
    grep -Fq '"kind":"leaf","text":"false"' \
        "$misclassified_bool" || {
        echo "[self-host-parity:driver-rung2] $backend Bool-kind mutation did not apply" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$misclassified_bool")" \
        >"$misclassified_bool.out" 2>"$misclassified_bool.err"); then
        echo "[self-host-parity:driver-rung2] $backend misclassified Bool literal was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: statement_type_unresolved" \
        "$misclassified_bool.err" "$misclassified_bool.out" || {
        echo "[self-host-parity:driver-rung2] $backend Bool-kind diagnostic drifted" >&2
        cat "$misclassified_bool.out" "$misclassified_bool.err" >&2
        exit 1
    }

    malformed_bool="${self_mir_json%.json}.malformed-bool.mir.json"
    sed 's/"kind":"bool_literal","text":"false"/"kind":"bool_literal","text":"truth"/g' \
        "$self_mir_json" >"$malformed_bool"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_bool")" \
        >"$malformed_bool.out" 2>"$malformed_bool.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed Bool payload was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_bool.err" "$malformed_bool.out" || {
        echo "[self-host-parity:driver-rung2] $backend malformed Bool diagnostic drifted" >&2
        cat "$malformed_bool.out" "$malformed_bool.err" >&2
        exit 1
    }
}
