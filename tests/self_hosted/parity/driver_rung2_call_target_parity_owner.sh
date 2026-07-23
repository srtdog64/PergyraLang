#!/usr/bin/env bash
# Owns DRV-2 canonical call-target carriage and fail-closed mutation checks.

pgy_selfhost_verify_driver_rung2_call_target() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_target expected_member
    local -a expected_members=()

    if [[ "$base" == "class_recursive_factory" ]]; then
        pgy_selfhost_verify_driver_rung2_recursive_call_targets \
            "$backend" "$base" "$self_mir_json" "$driver_bin"
        return 0
    fi

    if [[ "$base" == "class_method_self_return" ||
        "$base" == "class_method_self_access" ||
        "$base" == "array_elem_class_literal" ||
        "$base" == "array_elem_class_method" ||
        "$base" == "class_factory_result_wrap" ||
        "$base" == "class_method_result_loop" ||
        "$base" == "class_bump_option_match" ||
        "$base" == "class_within_class_chain" ||
        "$base" == "class_method_short_circuit" ]]; then
        expected_members=(Stat_PromoteIf)
        if [[ "$base" == "class_method_self_access" ]]; then
            expected_members=(Account_Deposit)
        elif [[ "$base" == "array_elem_class_literal" ||
            "$base" == "array_elem_class_method" ]]; then
            expected_members=(P_V)
        elif [[ "$base" == "class_factory_result_wrap" ]]; then
            expected_members=(Tax_Compute)
        elif [[ "$base" == "class_method_result_loop" ]]; then
            expected_members=(Calc_DivBy)
        elif [[ "$base" == "class_bump_option_match" ]]; then
            expected_members=(Counter_Bump)
        elif [[ "$base" == "class_within_class_chain" ]]; then
            expected_members=(Outer_WithNewTag Outer_InnerId)
        elif [[ "$base" == "class_method_short_circuit" ]]; then
            expected_members=(Counter_IsAtLeast Counter_IsBetween)
        fi
        for expected_member in "${expected_members[@]}"; do
            grep -Fq "\"call_target_kind\":\"member\",\"call_target_name\":\"$expected_member\"" \
                "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend member target drifted: $expected_member" >&2
                exit 1
            }
            missing_target="$BUILD_DIR/${base}_${backend}.${expected_member}.missing-member-target.mir.json"
            pgy_replace_first_literal "$self_mir_json" "$missing_target" \
                "\"call_target_kind\":\"member\",\"call_target_name\":\"$expected_member\"" \
                '"call_target_kind":"none","call_target_name":""'
            if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
                "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
                >"$missing_target.out" 2>"$missing_target.err"); then
                echo "[self-host-parity:driver-rung2] $backend missing member target accepted: $expected_member" >&2
                exit 1
            fi
            grep -Fq "MIR instruction expression graph is missing or invalid" \
                "$missing_target.err" "$missing_target.out" || {
                echo "[self-host-parity:driver-rung2] $backend member-target diagnostic drifted: $expected_member" >&2
                cat "$missing_target.out" "$missing_target.err" >&2
                exit 1
            }
        done
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
