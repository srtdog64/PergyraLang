#!/usr/bin/env bash
# Owns Result-match loop-carried state phi completeness and rejection.

pgy_selfhost_verify_driver_rung2_result_loop_phi() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local symbol header_result header_uses merge_result merge_uses
    local missing_uses missing_label missing_state_input fact

    if [[ "$base" == "class_result_chain_loop" ]]; then
        symbol="w"
        header_result="w.3"
        header_uses='"uses":["w.1","w.13"]'
        merge_result="w.13"
        merge_uses='"uses":["w.7","w.3","w.3"]'
        missing_uses='"uses":["w.3","w.3"]'
        missing_label="class state"
    elif [[ "$base" == "class_method_result_loop" ]]; then
        symbol="acc"
        header_result="acc.4"
        header_uses='"uses":["acc.1","acc.13"]'
        merge_result="acc.13"
        merge_uses='"uses":["acc.8","acc.12","acc.4"]'
        missing_uses='"uses":["acc.12","acc.4"]'
        missing_label="accumulator"
    else
        return 0
    fi

    for fact in \
        "\"kind\":\"phi\",\"name\":\"$symbol\",\"result\":\"$header_result\"" \
        "$header_uses" \
        "\"kind\":\"phi\",\"name\":\"$symbol\",\"result\":\"$merge_result\"" \
        "$merge_uses"; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Result-loop $missing_label phi fact drifted: $fact" >&2
            exit 1
        }
    done
    missing_state_input="$BUILD_DIR/${base}_${backend}.missing-result-loop-state-input.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_state_input" \
        "$merge_uses" "$missing_uses"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_state_input")" \
        >"$missing_state_input.out" 2>"$missing_state_input.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing Result-loop $missing_label phi input was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR phi facts are missing or inconsistent" \
        "$missing_state_input.err" "$missing_state_input.out" || {
        echo "[self-host-parity:driver-rung2] $backend Result-loop $missing_label phi diagnostic drifted" >&2
        cat "$missing_state_input.out" "$missing_state_input.err" >&2
        exit 1
    }
}
