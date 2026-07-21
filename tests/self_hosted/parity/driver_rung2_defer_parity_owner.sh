#!/usr/bin/env bash
# Typed cleanup-body facts for the bounded single-Log defer rung.
pgy_selfhost_verify_driver_rung2_defer() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"

    [[ "$base" == "defer_scope" || "$base" == "branch_defer_scope" ]] || return 0
    grep -Fq '"source_type":"AST_DEFER_STMT"' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend typed defer instruction was lost" >&2
        exit 1
    }
    grep -Fq '"arg0":"Log","arg1":null,"expr0":"{...}","expr0_graph":{' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend typed defer body graph was lost" >&2
        exit 1
    }
}
