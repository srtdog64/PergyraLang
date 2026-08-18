#!/usr/bin/env bash
# Owns contextual Result<T, E> assignment into nominal constructor fields.

pgy_selfhost_result_field_reject_payload_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/result_as_class_field_${backend}.bad-payload.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" \
        'Ok(100)' 'Ok("bad")'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend Result field payload mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arg_type_mismatch" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend Result field payload diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted Result field payload mismatch" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_result_field() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact source
    [[ "$base" == "result_as_class_field" ]] || return 0

    for fact in \
        '"kind":"class","nominal_kind":"class","name":"Wallet","source_syntax_id":2,' \
        '"fields":[{"name":"balance","type":"Result<Int, CardErr>"' \
        '"call_target_kind":"direct","call_target_name":"Wallet"' \
        '"kind":"call_argument","text":"Ok(100)"' \
        '"kind":"call_argument","text":"Err(Empty)"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Result field fact drifted: $fact" >&2
            exit 1
        }
    done

    source="$ROOT_DIR/tests/cases/backend_compare/result_as_class_field/main.pgy"
    pgy_selfhost_result_field_reject_payload_mutation \
        "$backend" "$driver_bin" "$source"
}

pgy_selfhost_verify_driver_rung2_result_field_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    local fact
    [[ "$base" == "result_as_class_field" ]] || return 0

    for fact in \
        'pgy_result_Int_CardErr balance;' \
        'pgy_result_ok_Int_CardErr(100)' \
        'pgy_result_err_Int_CardErr(CardErr_Empty)' \
        'pgy_result_err_Int_CardErr(CardErr_Damaged)'; do
        grep -Fq "$fact" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Result field emission drifted: $fact" >&2
            exit 1
        }
    done
}
