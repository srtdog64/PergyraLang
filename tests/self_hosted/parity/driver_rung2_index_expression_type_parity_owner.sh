#!/usr/bin/env bash
# Owns DRV-2 index receiver graph-type checks.

pgy_selfhost_verify_driver_rung2_index_expression_type() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local bad_receiver

    [[ "$base" == "member_array_index" ]] || return 0
    grep -Fq '"kind":"index","text":"holder.values[1]"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend member-array index graph drifted" >&2
        exit 1
    }
    bad_receiver="${self_mir_json%.json}.stale-index-provenance.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$bad_receiver" \
        '"kind":"member_access","text":"holder.values","call_target_kind":"none"' \
        '"kind":"member_access","text":"stale.provenance","call_target_kind":"none"'
    if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$bad_receiver")" \
        >"$bad_receiver.out" 2>"$bad_receiver.err"); then
        echo "[self-host-parity:driver-rung2] $backend index receiver reopened provenance text" >&2
        cat "$bad_receiver.out" "$bad_receiver.err" >&2
        exit 1
    fi
    grep -Fq "pgy_ai_get(holder.values, 1)" "$bad_receiver.out" || {
        echo "[self-host-parity:driver-rung2] $backend index receiver graph-type emission drifted" >&2
        cat "$bad_receiver.out" >&2
        exit 1
    }
}
