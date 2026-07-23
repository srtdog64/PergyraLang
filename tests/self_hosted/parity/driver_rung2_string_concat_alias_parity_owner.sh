#!/usr/bin/env bash
# Owns the stable StringConcat source alias projection to the concat runtime ABI.

pgy_selfhost_string_concat_alias_reject_argument_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/string_utility_aliases_${backend}.bad-arg.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" \
        'StringConcat("ok:", replaced)' 'StringConcat("ok:", 7)'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend StringConcat argument mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: call_arg_type_mismatch" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend StringConcat diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted StringConcat argument mismatch" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_string_concat_alias() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact source
    [[ "$base" == "string_utility_aliases" ]] || return 0

    for fact in \
        '"call_target_kind":"direct","call_target_name":"StringConcat"' \
        '"kind":"call_argument","text":"StringConcat(\"ok:\", replaced)"' \
        '"name":"replaced","type":"String"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend StringConcat fact drifted: $fact" >&2
            exit 1
        }
    done

    source="$ROOT_DIR/tests/cases/backend_compare/string_utility_aliases/main.pgy"
    pgy_selfhost_string_concat_alias_reject_argument_mutation \
        "$backend" "$driver_bin" "$source"
}

pgy_selfhost_verify_driver_rung2_string_concat_alias_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "string_utility_aliases" ]] || return 0

    grep -Fq 'pgy_concat("ok:", replaced)' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend StringConcat ABI projection drifted" >&2
        exit 1
    }
    if grep -Fq 'StringConcat(' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend retained the source alias as a second C ABI" >&2
        exit 1
    fi
}
