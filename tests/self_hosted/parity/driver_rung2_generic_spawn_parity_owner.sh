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
    local fact source call_text target_name
    if [[ "$base" == "generic_future_spawn_int" ]]; then
        call_text='spawn Identity(42)'
        target_name=Identity
    elif [[ "$base" == "generic_future_spawn_multi_arg" ]]; then
        call_text='spawn PickSecond(10, 77)'
        target_name=PickSecond
    else
        return 0
    fi

    for fact in \
        "\"kind\":\"spawn\",\"text\":\"$call_text\"" \
        "\"call_target_kind\":\"direct\",\"call_target_name\":\"$target_name\"" \
        '"kind":"await","text":"await task"' \
        '"name":"task","type":"Future<Int>"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend generic spawn fact drifted: $fact" >&2
            exit 1
        }
    done
    if [[ "$base" == "generic_future_spawn_int" ]]; then
        source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
        pgy_selfhost_generic_spawn_reject_string_mutation \
            "$backend" "$driver_bin" "$source"
    fi
}

pgy_selfhost_verify_driver_rung2_generic_spawn_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    local signature spawn_call
    if [[ "$base" == "generic_future_spawn_int" ]]; then
        signature='long long Identity_Int(long long x)'
        spawn_call='PgyTaskHandle task = pgy_selfhost_spawn_int((PgySelfHostSpawnIntFunction){ .unary = Identity_Int }, 1, 42, 0)'
    elif [[ "$base" == "generic_future_spawn_multi_arg" ]]; then
        signature='long long PickSecond_Int(long long left, long long right)'
        spawn_call='PgyTaskHandle task = pgy_selfhost_spawn_int((PgySelfHostSpawnIntFunction){ .binary = PickSecond_Int }, 2, 10, 77)'
    else
        return 0
    fi

    for term in \
        "$signature" \
        "$spawn_call" \
        'pgy_await_take(task, long long)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend generic spawn C fact drifted: $term" >&2
            exit 1
        }
    done
    if grep -Fq 'pgy_selfhost_spawn_int1' "$emitted_c" ||
        grep -Fq 'pgy_selfhost_spawn_int2' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend generic spawn fragmented by arity" >&2
        exit 1
    fi
}
