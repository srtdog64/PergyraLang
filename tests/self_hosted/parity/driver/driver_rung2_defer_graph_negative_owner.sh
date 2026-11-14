#!/usr/bin/env bash
# Owns the direct-call target falsifier for typed defer expression graphs.
pgy_selfhost_verify_driver_rung2_defer_graph_negative() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local invalid_target

    [[ "$base" == "allocator_defer_cleanup" ]] || return 0
    invalid_target="$BUILD_DIR/${base}_${backend}.invalid-defer-target.mir.json"
    sed 's/"call_target_name":"AllocatorDestroy"/"call_target_name":"AllocatorRelease"/g' \
        "$self_mir_json" >"$invalid_target"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$invalid_target")" \
        >"$invalid_target.out" 2>"$invalid_target.err"); then
        echo "[self-host-parity:driver-rung2] $backend defer target mutation was accepted" >&2
        exit 1
    fi
    if ! grep -Fq "ast_artifact_invalid" \
        "$invalid_target.err" "$invalid_target.out" &&
        ! grep -Fq "defer body graph disagrees with direct call target fact" \
        "$invalid_target.err" "$invalid_target.out"; then
        echo "[self-host-parity:driver-rung2] $backend invalid defer target did not fail at the graph/artifact owner" >&2
        cat "$invalid_target.out" "$invalid_target.err" >&2
        exit 1
    fi
}
