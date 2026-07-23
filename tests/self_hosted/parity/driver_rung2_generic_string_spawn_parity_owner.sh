#!/usr/bin/env bash
# Owns String generic specialization carriage through one tagged spawn ABI.

pgy_selfhost_generic_string_spawn_reject_int_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/generic_future_spawn_string_${backend}.int.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"
    pgy_replace_first_literal "$source" "$mutated" \
        'spawn Echo("hi")' 'spawn Echo(42)'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend generic String spawn payload mismatch was accepted" >&2
        exit 1
    fi
    grep -Fq "Code: let_type_mismatch" "$self_out" "$self_err" &&
    grep -Fq -- "- expected: String" "$self_out" "$self_err" &&
    grep -Fq -- "- actual: Int" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend generic String spawn diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted generic String spawn payload mismatch" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_generic_string_spawn() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4" fact
    [[ "$base" == "generic_future_spawn_string" ]] || return 0
    for fact in \
        '"kind":"spawn","text":"spawn Echo(\"hi\")"' \
        '"call_target_kind":"direct","call_target_name":"Echo"' \
        '"kind":"await","text":"await task"' \
        '"name":"task","type":"Future<String>"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend generic String spawn fact drifted: $fact" >&2
            exit 1
        }
    done
    pgy_selfhost_generic_string_spawn_reject_int_mutation "$backend" \
        "$driver_bin" "$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
}

pgy_selfhost_verify_driver_rung2_generic_string_spawn_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3" term
    [[ "$base" == "generic_future_spawn_string" ]] || return 0
    for term in \
        'const char* Echo_String(const char* x)' \
        'PgyTaskHandle task = pgy_selfhost_spawn((PgySelfHostSpawnFunction){ .string_unary = Echo_String }, PGY_SELFHOST_SPAWN_STRING1, (PgySelfHostSpawnValue){ .string_value = "hi" }, (PgySelfHostSpawnValue){0})' \
        'pgy_await_take(task, const char*)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend generic String spawn C fact drifted: $term" >&2
            exit 1
        }
    done
    if grep -Fq 'pgy_selfhost_spawn_string' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend String spawn reopened a payload-specific helper" >&2
        exit 1
    fi
}
