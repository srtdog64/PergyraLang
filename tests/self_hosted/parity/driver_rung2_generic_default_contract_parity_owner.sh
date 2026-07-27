#!/usr/bin/env bash
# Owns generic-default substitution through nominal typing and emitted C.

pgy_selfhost_generic_default_reject_string_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/generic_default_contracts_${backend}.string.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" \
        'Box<T = Int>' 'Box<T = String>'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend generic default mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arg_type_mismatch" "$self_out" "$self_err" &&
    grep -Fq -- "- expected: String" "$self_out" "$self_err" &&
    grep -Fq -- "- actual: Int" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend generic default diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted generic default mismatch" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_generic_default_contract() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact return_void_count
    [[ "$base" == "generic_default_contracts" ]] || return 0

    for fact in \
        '"kind":"class","nominal_kind":"class","name":"Box","fields":[{"name":"value","type":"Int","field_kind":"field","source_syntax_id":' \
        '"name":"Put","kind":"method","owner":"IntBuffer"' \
        '"call_target_kind":"member","call_target_name":"Bag_Save"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend generic default fact drifted: $fact" >&2
            exit 1
        }
    done
    return_void_count="$(grep -Fo '"source_type":"AST_RETURN_VOID"' \
        "$self_mir_json" | wc -l | tr -d ' ')"
    [[ "$return_void_count" -ge 2 ]] || {
        echo "[self-host-parity:driver-rung2] $backend void return facts were lost" >&2
        exit 1
    }
    pgy_selfhost_generic_default_reject_string_mutation "$backend" \
        "$driver_bin" "$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
}

pgy_selfhost_verify_driver_rung2_generic_default_contract_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3" term
    [[ "$base" == "generic_default_contracts" ]] || return 0

    for term in \
        'long long value;' \
        'void IntBuffer_Put(void *_pgy_raw_self, long long value)' \
        'void Bag_Save(Bag self)' \
        'Bag_Save(bag);'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend generic default C fact drifted: $term" >&2
            exit 1
        }
    done
    if grep -Eq '\bT[[:space:]]+value;' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend unresolved generic default leaked into C" >&2
        exit 1
    fi
}
