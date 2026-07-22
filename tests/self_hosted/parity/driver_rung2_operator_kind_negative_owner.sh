#!/usr/bin/env bash
# Owns fail-closed mutations of typed expression-graph operator identity.
pgy_selfhost_verify_driver_rung2_operator_kind_negative() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local invalid_kind
    [[ "$base" == "class_method_coalesce_call" ]] || return 0

    invalid_kind="$BUILD_DIR/${base}_${backend}.invalid-operator-kind.mir.json"
    sed 's/"kind":"coalesce"/"kind":"logical_or"/g' \
        "$self_mir_json" >"$invalid_kind"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$invalid_kind")" \
        >"$invalid_kind.out" 2>"$invalid_kind.err"); then
        echo "[self-host-parity:driver-rung2] $backend operator kind mutation was accepted: $base" >&2
        exit 1
    fi
    grep -Fq "semantic logical operand type fact is invalid" \
        "$invalid_kind.err" "$invalid_kind.out" || {
        echo "[self-host-parity:driver-rung2] $backend operator kind mutation diagnostic drifted: $base" >&2
        cat "$invalid_kind.out" "$invalid_kind.err" >&2
        exit 1
    }
}
