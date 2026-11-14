#!/usr/bin/env bash
# Typed cleanup-body graph facts for bounded Log and direct-call defer rungs.
pgy_selfhost_verify_driver_rung2_defer() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"

    local body_kind_marker='"arg0":"Log"'
    if [[ "$base" == "allocator_defer_cleanup" ]]; then
        body_kind_marker='"arg0":"Call"'
    elif [[ "$base" != "defer_scope" && "$base" != "branch_defer_scope" ]]; then
        return 0
    fi
    grep -Fq '"source_type":"AST_DEFER_STMT"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend typed defer instruction was lost" >&2
        exit 1
    }
    grep -Fq "$body_kind_marker" "$self_mir_json" &&
        grep -Fq '"expr0":"{...}","expr0_graph":{' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend typed defer body graph was lost" >&2
        exit 1
    }
    if [[ "$base" == "allocator_defer_cleanup" ]]; then
        grep -Fq '"call_target_kind":"direct","call_target_name":"AllocatorDestroy"' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend allocator defer target fact was lost" >&2
            exit 1
        }
        grep -Fq '"expr0":"BoxArray(3, scratch)","expr0_graph":{' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend scratch BoxArray graph was lost" >&2
            exit 1
        }
        grep -Fq '"expr0":"BoxArray(2, pool)","expr0_graph":{' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend pool BoxArray graph was lost" >&2
            exit 1
        }
        grep -Fq '"expr1":"Box<Array<Int>>"' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend BoxArray value type fact was lost" >&2
            exit 1
        }
    fi
}
