#!/usr/bin/env bash
# Owns Queue<T> sequence construction, runtime calls, and fail-closed ABI rows.
# Queue MIR/runtime ABI parity and fail-closed negatives.
# Forbidden fallbacks: queue_receiver_type_guess, queue_element_type_guess,
# source Queue spelling as ABI, missing_queue_runtime_fact_success,
# List_runtime_fallback.

pgy_selfhost_verify_driver_rung2_queue_ops() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_type bad_source source out err
    [[ "$base" == "sequence_literal_list_queue" ]] || return 0

    for fact in \
        '"result":"q.1","arg0":"q","arg1":null,"slot_anchor":null,"abi_type_name":"Queue<Int>"' \
        '"result":"empty.1","arg0":"empty","arg1":null,"slot_anchor":null,"abi_type_name":"Queue<String>"' \
        '"expr0":"[7, 8]"' \
        '"expr0":"Log(QueueSize(q))"' \
        '"call_target_name":"QueuePop"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Queue fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_type="$BUILD_DIR/${base}_${backend}.missing-queue-abi-type.mir.json"
    out="$missing_type.out"
    err="$missing_type.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_type" \
        '"abi_type_name":"Queue<Int>"' '"abi_type_name":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_type")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing Queue ABI type was accepted" >&2
        exit 1
    fi
    grep -Fq 'local declaration is missing its MIR ABI type fact' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend missing Queue ABI diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    bad_source="$BUILD_DIR/${base}_${backend}.bad-queue-element.pgy"
    pgy_replace_first_literal "$source" "$bad_source" \
        'let q: Queue<Int> = [7, 8];' \
        'let q: Queue<Int> = ["bad"];'
    out="$bad_source.out"
    err="$bad_source.err"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_source")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend mismatched Queue literal was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: let_type_mismatch' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend Queue literal mismatch diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_queue_ops_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "sequence_literal_list_queue" ]] || return 0
    for term in \
        'PgyQueue_Int q = ({' \
        'pgy_queue_push_int(&_pgy_queue_value, 7)' \
        'pgy_queue_size_int(&q)' \
        'pgy_queue_pop_int(&q)' \
        'PgyQueue_String empty = ({' \
        'pgy_queue_size_string(&empty)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Queue ABI missing: $term" >&2
            exit 1
        }
    done
}
