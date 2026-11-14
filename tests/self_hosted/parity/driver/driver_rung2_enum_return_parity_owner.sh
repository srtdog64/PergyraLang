#!/usr/bin/env bash
# Owns DRV-2 payload-free enum return graph checks.

pgy_selfhost_verify_driver_rung2_enum_return() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local bad_member

    [[ "$base" == "enum_return" ]] || return 0
    grep -Fq '"kind":"member_access","text":"Choice.B"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend enum return member graph drifted" >&2
        exit 1
    }
    bad_member="${self_mir_json%.json}.bad-enum-return-member.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$bad_member" \
        '"kind":"leaf","text":"B","call_target_kind":"none"' \
        '"kind":"leaf","text":"Missing","call_target_kind":"none"'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$bad_member")" \
        >"$bad_member.out" 2>"$bad_member.err"); then
        echo "[self-host-parity:driver-rung2] $backend invalid enum return member was accepted" >&2
        exit 1
    fi
    grep -Fq "semantic member-access receiver type fact is missing" \
        "$bad_member.err" "$bad_member.out" || {
        echo "[self-host-parity:driver-rung2] $backend enum return member diagnostic drifted" >&2
        cat "$bad_member.out" "$bad_member.err" >&2
        exit 1
    }
}
