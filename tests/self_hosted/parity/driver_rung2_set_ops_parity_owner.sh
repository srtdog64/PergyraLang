#!/usr/bin/env bash
# Owns Set<T> runtime calls and fail-closed ABI rows for the first executable
# Set rung. Set literals remain a separate parser/graph surface until observed.
# Set MIR/runtime ABI parity and fail-closed negatives.
# Forbidden fallbacks: set_receiver_type_guess, set_element_type_guess,
# source Set spelling as ABI, missing_set_runtime_fact_success,
# Queue_runtime_fallback.

pgy_selfhost_verify_driver_rung2_set_ops() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_type bad_source source out err
    [[ "$base" == "set_ops" ]] || return 0

    for fact in \
        '"result":"seen.1","arg0":"seen","arg1":null,"slot_anchor":null,"abi_type_name":"Set<Int>"' \
        '"call_target_name":"SetNew"' \
        '"call_target_name":"SetAdd"' \
        '"call_target_name":"SetHas"' \
        '"call_target_name":"SetRemove"' \
        '"call_target_name":"SetSize"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Set fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_type="$BUILD_DIR/${base}_${backend}.missing-set-abi-type.mir.json"
    out="$missing_type.out"
    err="$missing_type.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_type" \
        '"abi_type_name":"Set<Int>"' '"abi_type_name":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_type")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing Set ABI type was accepted" >&2
        exit 1
    fi
    grep -Fq 'local declaration is missing its MIR ABI type fact' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend missing Set ABI diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    bad_source="$BUILD_DIR/${base}_${backend}.bad-set-element.pgy"
    pgy_replace_first_literal "$source" "$bad_source" \
        'SetAdd(seen, 7);' 'SetAdd(seen, "bad");'
    out="$bad_source.out"
    err="$bad_source.err"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_source")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend mismatched Set element was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: call_arg_type_mismatch' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend Set element mismatch diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_set_ops_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "set_ops" ]] || return 0
    for term in \
        'PgySet_int seen = pgy_set_new_int()' \
        'pgy_set_add_int(&seen, 7)' \
        'pgy_set_has_int(&seen, 7)' \
        'pgy_set_remove_int(&seen, 7)' \
        'pgy_set_size_int(&seen)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Set ABI missing: $term" >&2
            exit 1
        }
    done
}
