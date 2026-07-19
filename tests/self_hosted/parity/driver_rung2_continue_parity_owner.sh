#!/usr/bin/env bash
# Owns DRV-2 continue-edge carriage and loop-summary rejection.

pgy_selfhost_verify_driver_rung2_continue() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local missing_summary

    [[ "$base" == "for_continue" ]] || return 0
    grep -Fq '"kind":"branch","name":"continue"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend continue edge was lost" >&2
        exit 1
    }
    grep -Fq '"loop_flow_summary_count":1' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend continue loop summary was lost" >&2
        exit 1
    }

    missing_summary="$BUILD_DIR/${base}_${backend}.missing-loop-summary.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_summary" \
        '"loop_flow_summary_count":1' '"loop_flow_summary_count":0'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_summary")" \
        >"$missing_summary.out" 2>"$missing_summary.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing continue loop summary was accepted" >&2
        exit 1
    fi
    grep -Fq "routine LoopFlowSummary facts are incomplete" \
        "$missing_summary.err" "$missing_summary.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing loop-summary diagnostic drifted" >&2
        cat "$missing_summary.out" "$missing_summary.err" >&2
        exit 1
    }
}
