#!/usr/bin/env bash
# Owns the missing target-projection fact mutation at the DRV-2 emitter.

pgy_selfhost_verify_driver_rung2_target_projection_negative() {
    local machine_fixture="$1" backend="$2" base="$3" self_mir_json="$4"
    local driver_bin="$5" machine_declaration="$6"
    local -a command

    [[ "$machine_fixture" -eq 1 && "$base" == "device_slot_routine" ]] || return 0

    command=("$driver_bin" --self-test-missing-target-projection \
        "$(pgy_selfhost_path_relative_to_root "$self_mir_json")")
    command+=(--machine-manifest-json "$machine_declaration")
    if (cd "$ROOT_DIR" && "${command[@]}" \
        >"$self_mir_json.missing-target.out" \
        2>"$self_mir_json.missing-target.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing target projection was accepted: $base" >&2
        return 1
    fi
    grep -Fq \
        "self-host C emission target projection fact is missing or invalid" \
        "$self_mir_json.missing-target.err" \
        "$self_mir_json.missing-target.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing target projection diagnostic drifted: $base" >&2
        cat "$self_mir_json.missing-target.out" \
            "$self_mir_json.missing-target.err" >&2
        return 1
    }
}
