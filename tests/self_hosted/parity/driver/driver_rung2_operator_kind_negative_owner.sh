#!/usr/bin/env bash
# Owns fail-closed mutations of typed expression-graph operator identity.
pgy_selfhost_verify_driver_rung2_operator_kind_negative() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local invalid_kind logical_kind
    if [[ "$base" == "class_method_short_circuit" ]]; then
        for logical_kind in logical_or logical_and logical_not; do
            grep -Fq "\"kind\":\"$logical_kind\"" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend logical graph kind was lost: $logical_kind" >&2
                exit 1
            }
        done
        invalid_kind="$BUILD_DIR/${base}_${backend}.invalid-logical-kind.mir.json"
        pgy_replace_first_literal "$self_mir_json" "$invalid_kind" \
            '"kind":"logical_or"' '"kind":"add"'
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$invalid_kind")" \
            >"$invalid_kind.out" 2>"$invalid_kind.err"); then
            echo "[self-host-parity:driver-rung2] $backend logical operator mutation was accepted" >&2
            exit 1
        fi
        grep -Eq "(semantic additive operand type fact is invalid|MIR instruction expression graph is missing or invalid|statement_type_unresolved)" \
            "$invalid_kind.err" "$invalid_kind.out" || {
            echo "[self-host-parity:driver-rung2] $backend logical operator diagnostic drifted" >&2
            cat "$invalid_kind.out" "$invalid_kind.err" >&2
            exit 1
        }
        return 0
    fi
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
