#!/usr/bin/env bash
# Owns generic-call specialization carriage through spawn and await.

pgy_selfhost_generic_spawn_reject_string_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/generic_future_spawn_int_${backend}.string.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" \
        'spawn Identity(42)' 'spawn Identity("bad")'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend generic spawn payload mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: let_type_mismatch" "$self_out" "$self_err" &&
    grep -Fq -- "- expected: Int" "$self_out" "$self_err" &&
    grep -Fq -- "- actual: String" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend generic spawn diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted generic spawn payload mismatch" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_generic_spawn() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact source
    [[ "$base" == "generic_future_spawn_int" ]] || return 0

    for fact in \
        '"kind":"spawn","text":"spawn Identity(42)"' \
        '"call_target_kind":"direct","call_target_name":"Identity"' \
        '"kind":"await","text":"await task"' \
        '"name":"task","type":"Future<Int>"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend generic spawn fact drifted: $fact" >&2
            exit 1
        }
    done
    source="$ROOT_DIR/tests/cases/backend_compare/generic_future_spawn_int/main.pgy"
    pgy_selfhost_generic_spawn_reject_string_mutation \
        "$backend" "$driver_bin" "$source"
}

pgy_selfhost_verify_driver_rung2_generic_spawn_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "generic_future_spawn_int" ]] || return 0

    for term in \
        'long long Identity_Int(long long x)' \
        'PgyTaskHandle task = pgy_selfhost_spawn_int1(Identity_Int, 42)' \
        'pgy_await_take(task, long long)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend generic spawn C fact drifted: $term" >&2
            exit 1
        }
    done
    if grep -Fq 'pgy_selfhost_spawn_int1(Identity, ' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend generic spawn lost its carried specialization" >&2
        exit 1
    fi
}
