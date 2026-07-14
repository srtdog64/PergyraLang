#!/usr/bin/env bash
# Owns DRV-2 Option<struct> constructor and payload graph checks.

pgy_selfhost_verify_driver_rung2_option_struct_value() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local malformed_graph bad_fixture bad_out bad_err oracle_bin oracle_log

    [[ "$base" == "option_struct_value_flow" ]] || return 0

    if [[ "$(grep -Fo '"kind":"call_argument","text":"Some(Pair' \
        "$self_mir_json" | wc -l | tr -d ' ')" -lt 3 ]]; then
        echo "[self-host-parity:driver-rung2] $backend Option<struct> call spine coverage drifted" >&2
        exit 1
    fi
    grep -Fq '"expr0":"None","expr0_graph":{"root":0,"nodes":[{"kind":"leaf","text":"None"' \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend Option<struct> None assignment graph was lost" >&2
        exit 1
    }
    for carried_value in \
        '"expr0":"Some(Pair { left: base, right: (base + 1) })","expr0_graph"' \
        '"expr0":"Some(Pair { left: 1, right: 2 })","expr0_graph"' \
        '"expr0":"Some(Pair { left: 3, right: 4 })","expr0_graph"'; do
        grep -Fq "$carried_value" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Option<struct> graph was lost: $carried_value" >&2
            exit 1
        }
    done

    malformed_graph="${self_mir_json%.json}.malformed-option-struct.mir.json"
    sed 's/"kind":"call_argument","text":"Some(Pair/"kind":"leaf","text":"Some(Pair/g' \
        "$self_mir_json" >"$malformed_graph"
    if cmp -s "$self_mir_json" "$malformed_graph"; then
        echo "[self-host-parity:driver-rung2] $backend Option<struct> mutation did not apply" >&2
        exit 1
    fi
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$malformed_graph")" \
        >"$malformed_graph.out" 2>"$malformed_graph.err"); then
        echo "[self-host-parity:driver-rung2] $backend malformed Option<struct> call spine was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$malformed_graph.err" "$malformed_graph.out" || {
        echo "[self-host-parity:driver-rung2] $backend Option<struct> diagnostic drifted" >&2
        cat "$malformed_graph.out" "$malformed_graph.err" >&2
        exit 1
    }

    bad_fixture="src/self_hosted/mir_lower/fixture/option_struct_bad_field_type.pgy"
    bad_out="${self_mir_json%.json}.bad-field.out"
    bad_err="${self_mir_json%.json}.bad-field.err"
    if (cd "$ROOT_DIR" && "$driver_bin" "$bad_fixture" --emit-c-verified \
        >"$bad_out" 2>"$bad_err"); then
        echo "[self-host-parity:driver-rung2] $backend Option<struct> field type mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arg_type_mismatch" "$bad_out" "$bad_err" || {
        echo "[self-host-parity:driver-rung2] $backend Option<struct> field diagnostic drifted" >&2
        cat "$bad_out" "$bad_err" >&2
        exit 1
    }
    oracle_bin="${self_mir_json%.json}.bad-field.oracle.exe"
    oracle_log="${self_mir_json%.json}.bad-field.oracle.log"
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/$bad_fixture")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted Option<struct> field type mismatch" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_option_struct_emitted_c() {
    local backend="$1"
    local base="$2"
    local emitted_c="$3"

    [[ "$base" == "option_struct_value_flow" ]] || return 0

    grep -Fq 'pgy_option_none_Pair()' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend Option<struct> None was not emitted from the MIR graph" >&2
        exit 1
    }
}
