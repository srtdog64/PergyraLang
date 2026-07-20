#!/usr/bin/env bash
# Owns DRV-2 canonical call-target carriage and fail-closed mutation checks.

pgy_selfhost_verify_driver_rung2_call_target() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_target

    if [[ "$base" == "class_method_self_return" ||
        "$base" == "class_method_self_access" ]]; then
        local expected_member="Stat_PromoteIf"
        if [[ "$base" == "class_method_self_access" ]]; then
            expected_member="Account_Deposit"
        fi
        grep -Fq "\"call_target_kind\":\"member\",\"call_target_name\":\"$expected_member\"" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend chained member call target fact drifted" >&2
            exit 1
        }
        missing_target="${self_mir_json%.json}.missing-member-target.mir.json"
        sed "s/\"call_target_kind\":\"member\",\"call_target_name\":\"$expected_member\"/\"call_target_kind\":\"none\",\"call_target_name\":\"\"/g" \
            "$self_mir_json" >"$missing_target"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
            >"$missing_target.out" 2>"$missing_target.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing chained member target was accepted" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$missing_target.err" "$missing_target.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing member-target diagnostic drifted" >&2
            cat "$missing_target.out" "$missing_target.err" >&2
            exit 1
        }
        return 0
    fi

    [[ "$base" == "namespace_call" ]] || return 0
    grep -Fq '"kind":"call","text":"Math.Add()","call_target_kind":"namespace","call_target_name":"Math_Add"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend namespace call target fact drifted" >&2
        exit 1
    }
    missing_target="${self_mir_json%.json}.missing-call-target.mir.json"
    sed 's/"call_target_kind":"namespace","call_target_name":"Math_Add"/"call_target_kind":"none","call_target_name":""/g' \
        "$self_mir_json" >"$missing_target"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
        >"$missing_target.out" 2>"$missing_target.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing namespace call target was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_target.err" "$missing_target.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing call-target diagnostic drifted" >&2
        cat "$missing_target.out" "$missing_target.err" >&2
        exit 1
    }
}
