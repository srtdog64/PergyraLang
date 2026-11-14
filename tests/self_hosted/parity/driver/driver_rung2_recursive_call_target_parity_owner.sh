#!/usr/bin/env bash
# Owns the recursive class-state call spine and missing-target rejection.

pgy_selfhost_verify_driver_rung2_recursive_call_targets() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local target_row target_kind target_name target_fact missing_target
    local -a target_rows=(
        "direct:Train"
        "direct:LevelUp"
        "direct:Charge"
        "member:State_Power"
    )

    for target_row in "${target_rows[@]}"; do
        target_kind="${target_row%%:*}"
        target_name="${target_row#*:}"
        target_fact="\"call_target_kind\":\"$target_kind\",\"call_target_name\":\"$target_name\""
        grep -Fq "$target_fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend recursive call target drifted: $target_name" >&2
            exit 1
        }
        missing_target="$BUILD_DIR/${base}_${backend}.${target_name}.missing-target.mir.json"
        pgy_replace_first_literal "$self_mir_json" "$missing_target" \
            "$target_fact" '"call_target_kind":"none","call_target_name":""'
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
            >"$missing_target.out" 2>"$missing_target.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing recursive target accepted: $target_name" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$missing_target.err" "$missing_target.out" || {
            echo "[self-host-parity:driver-rung2] $backend recursive target diagnostic drifted: $target_name" >&2
            cat "$missing_target.out" "$missing_target.err" >&2
            exit 1
        }
    done
}
