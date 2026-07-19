#!/usr/bin/env bash
# Owns DRV-2 loop-header phi omission and malformed-header rejection.

pgy_selfhost_verify_driver_rung2_loop_phi() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local header_prefix forged_header_phi forged

    [[ "$base" == "nested_if_in_loop" ]] || return 0
    header_prefix='{"id":1,"reachable":true,"instructions":[{"id":0,"kind":"branch"'
    grep -Fq "$header_prefix" "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend no-backedge loop retained a header phi" >&2
        exit 1
    }
    grep -Fq '"kind":"phi","name":"largest","result":"largest.8"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend live branch phi was lost" >&2
        exit 1
    }
    forged_header_phi='{"id":1,"reachable":true,"instructions":[{"id":99,"kind":"phi","name":"largest","result":"largest.99","arg0":"phi","arg1":null,"expr0":null,"expr0_graph":null,"expr1":null,"expr1_graph":null,"source_type":null,"machine_contact_kind":"","machine_layer":null,"match_patterns":[],"match_variant":null,"match_bindings":[],"destructure_bindings":[],"uses":["largest.1"]},{"id":0,"kind":"branch"'
    forged="$BUILD_DIR/${base}_${backend}.forged-header-phi.mir.json"
    pgy_replace_first_literal \
        "$self_mir_json" "$forged" "$header_prefix" "$forged_header_phi"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$forged")" \
        >"$forged.out" 2>"$forged.err"); then
        echo "[self-host-parity:driver-rung2] $backend one-predecessor header phi was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR phi facts are missing or inconsistent" \
        "$forged.err" "$forged.out" || {
        echo "[self-host-parity:driver-rung2] $backend forged header-phi diagnostic drifted" >&2
        cat "$forged.out" "$forged.err" >&2
        exit 1
    }
}
