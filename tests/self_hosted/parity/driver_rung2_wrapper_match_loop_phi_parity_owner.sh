#!/usr/bin/env bash
# Owns wrapper-match loop-carried state phi completeness and rejection.

pgy_selfhost_verify_driver_rung2_wrapper_match_loop_phi() {
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
        missing_label="Result class state"
    elif [[ "$base" == "class_method_result_loop" ]]; then
        symbol="acc"
        header_result="acc.4"
        header_uses='"uses":["acc.1","acc.13"]'
        merge_result="acc.13"
        merge_uses='"uses":["acc.8","acc.12","acc.4"]'
        missing_uses='"uses":["acc.12","acc.4"]'
        missing_label="Result accumulator"
    elif [[ "$base" == "class_bump_option_match" ]]; then
        symbol="c"
        header_result="c.3"
        header_uses='"uses":["c.1","c.10"]'
        merge_result="c.10"
        merge_uses='"uses":["c.7","c.3"]'
        missing_uses='"uses":["c.3"]'
        missing_label="Option class state"
    else
        return 0
    fi

    for fact in \
        "\"kind\":\"phi\",\"name\":\"$symbol\",\"result\":\"$header_result\"" \
        "$header_uses" \
        "\"kind\":\"phi\",\"name\":\"$symbol\",\"result\":\"$merge_result\"" \
        "$merge_uses"; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend wrapper-loop $missing_label phi fact drifted: $fact" >&2
            exit 1
        }
    done
    # The predecessor-resolved phi plan (92c38472) admits deduplicated phi
    # inputs, so a dropped operand can be legitimately absorbed; acceptance
    # is admissible only when the emitted C is byte-identical to the
    # unmutated baseline. A consumed operand can never silently reshape
    # the program.
    wrapper_baseline_c="$BUILD_DIR/${base}_${backend}.wrapper-loop-baseline.c"
    if ! (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$self_mir_json")" \
        >"$wrapper_baseline_c.raw" 2>"$wrapper_baseline_c.err"); then
        echo "[self-host-parity:driver-rung2] $backend wrapper-loop baseline consumption failed" >&2
        cat "$wrapper_baseline_c.err" >&2
        exit 1
    fi
    # Strip CR before comparing: the Windows runner's stdout text mode may
    # vary CR placement between invocations (same fix as the ABI-layout
    # negative owner).
    tr -d '\r' <"$wrapper_baseline_c.raw" >"$wrapper_baseline_c"
    missing_state_input="$BUILD_DIR/${base}_${backend}.missing-wrapper-loop-state-input.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_state_input" \
        "$merge_uses" "$missing_uses"
    missing_state_accepted=1
    (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_state_input")" \
        >"$missing_state_input.out.raw" 2>"$missing_state_input.err") \
        || missing_state_accepted=0
    # Normalize on both branches: the reject-path grep reads the same .out.
    tr -d '\r' <"$missing_state_input.out.raw" >"$missing_state_input.out"
    if [[ "$missing_state_accepted" -eq 1 ]]; then
        cmp -s "$wrapper_baseline_c" "$missing_state_input.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing wrapper-loop $missing_label phi input silently reshaped the C" >&2
            exit 1
        }
    else
        grep -Fq "MIR phi facts are missing or inconsistent" \
            "$missing_state_input.err" "$missing_state_input.out" || {
            echo "[self-host-parity:driver-rung2] $backend wrapper-loop $missing_label phi diagnostic drifted" >&2
            cat "$missing_state_input.out" "$missing_state_input.err" >&2
            exit 1
        }
    fi
}
