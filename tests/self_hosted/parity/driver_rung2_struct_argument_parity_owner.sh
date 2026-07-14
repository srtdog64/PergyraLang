#!/usr/bin/env bash
# Owns DRV-2 typed struct-literal call-argument graph checks.

pgy_selfhost_verify_driver_rung2_struct_argument() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local malformed_graph

    [[ "$base" == "struct_literal_call_argument" ]] || return 0

    grep -Fq '"kind":"struct_literal","text":"Line { }"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend struct literal root drifted" >&2
        exit 1
    }
    if [[ "$(grep -Fo '"kind":"struct_field_binding"' "$self_mir_json" | wc -l | tr -d ' ')" -lt 6 ]]; then
        echo "[self-host-parity:driver-rung2] $backend struct field binding graph drifted" >&2
        exit 1
    fi
    grep -Fq '"kind":"call_argument","text":"Twice(1)"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend nested struct field call graph drifted" >&2
        exit 1
    }

    malformed_graph="${self_mir_json%.json}.malformed-struct-spine.mir.json"
    sed 's/"kind":"struct_literal"/"kind":"leaf"/g' \
        "$self_mir_json" >"$malformed_graph"
    if cmp -s "$self_mir_json" "$malformed_graph"; then
        echo "[self-host-parity:driver-rung2] $backend struct-spine mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_graph")" \
        >"$malformed_graph.out" 2>"$malformed_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed struct spine was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_graph.err" "$malformed_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend struct-spine diagnostic drifted" >&2
        cat "$malformed_graph.out" "$malformed_graph.err" >&2
        exit 1
    }
}
