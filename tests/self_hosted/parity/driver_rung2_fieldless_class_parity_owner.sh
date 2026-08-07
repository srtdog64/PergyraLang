#!/usr/bin/env bash
# Owns zero-field nominal inventory and its standard-C unit representation.

pgy_selfhost_fieldless_class_reject_argument_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/fieldless_class_method_${backend}.bad-arg.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" 'Calc()' 'Calc(1)'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend fieldless constructor argument was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arity_mismatch" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend fieldless constructor diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted fieldless constructor argument" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_fieldless_class() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact source
    [[ "$base" == "fieldless_class_method" ]] || return 0

    for fact in \
        '"kind":"class","nominal_kind":"class","name":"Calc","source_syntax_id":1,"fields":[]' \
        '"call_target_kind":"direct","call_target_name":"Calc"' \
        '"call_target_kind":"member","call_target_name":"Calc_Add"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend fieldless class fact drifted: $fact" >&2
            exit 1
        }
    done

    source="$ROOT_DIR/tests/cases/backend_compare/fieldless_class_method/main.pgy"
    pgy_selfhost_fieldless_class_reject_argument_mutation \
        "$backend" "$driver_bin" "$source"
}

pgy_selfhost_verify_driver_rung2_fieldless_class_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "fieldless_class_method" ]] || return 0

    grep -Fq 'char _pgy_reserved;' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend fieldless class storage drifted" >&2
        exit 1
    }
    grep -Fq 'Calc c = (Calc){ 0 };' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend fieldless constructor initializer drifted" >&2
        exit 1
    }
}
