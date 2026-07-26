#!/usr/bin/env bash
# Owns assignment binding-mode carriage and missing-fact rejection.

pgy_selfhost_verify_driver_rung2_assignment_binding_mode() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local invalid_mode
    local owner="$ROOT_DIR/src/self_hosted/mir_lower/assignment_binding_mode_fact_owner.pgy"
    local driver_owner="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
    [[ "$base" == "bubble_sort_basic" ]] || return 0

    for required in \
        'ref routines: MirProgramRoutineIndex' \
        'ref order: MirStructuredExpressionEmissionOrder' \
        'MirStructuredExpressionEmissionOrderReady(order)' \
        'order.global_instruction_rows[order_i]' \
        'global_row < 0 || global_row >= instruction_count' \
        'routines.instruction_kinds[global_row]' \
        'routines.instruction_source_types[global_row]' \
        'order.lanes[order_i] != AstExpressionLaneAtom()' \
        'order.derived_ordinals[order_i] != 0' \
        'order.global_instruction_rows[value_i] != global_row' \
        'order.lanes[value_i] != AstExpressionLaneValue()' \
        'order.derived_ordinals[value_i] != 0' \
        'routines.instruction_starts[global_row]' \
        'routines.instruction_ends[global_row]' \
        '!scalar.valid || scalar.arg1 == ""' \
        'assignment_i = assignment_i + 1' \
        'return assignment_i == semantic_count;' \
        'scalar.arg1 != facts.target_binding_modes[assignment_i]'; do
        grep -Fq "$required" "$owner" || {
            echo "[self-host-parity:driver-rung2] structured assignment occurrence owner drifted: $required" >&2
            exit 1
        }
    done
    for forbidden in \
        'BuildMirProgramRoutineIndex(' 'BuildMirRoutineFactIndex(' \
        'index.instruction_facts' 'AstTreeArtifactFromText(' \
        'JsonObjectFactTableFromBounds(' 'JsonCollect' \
        'while routine_i' 'while instruction_i' 'Array<' \
        'seen_rows' 'previous_global_row' 'skip_duplicate'; do
        if grep -Fq "$forbidden" "$owner"; then
            echo "[self-host-parity:driver-rung2] assignment owner restored raw scan/cache fallback: $forbidden" >&2
            exit 1
        fi
    done
    [[ "$(grep -Fc 'MirAssignmentBindingModesMatchSemanticFacts(' "$driver_owner")" == "2" ]] || {
        echo "[self-host-parity:driver-rung2] assignment occurrence verifier call count drifted" >&2
        exit 1
    }
    grep -Fq 'routines, emission.expression_order,' "$driver_owner" &&
        grep -Fq 'admitted.routines, tree_emission.expression_order,' "$driver_owner" || {
        echo "[self-host-parity:driver-rung2] driver stopped forwarding prebuilt occurrence owners" >&2
        exit 1
    }

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
