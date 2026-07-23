#!/usr/bin/env bash
# Owns graph-carried inline and named-Future spawn/await executable rungs.

pgy_selfhost_reject_named_future_scalar_mutation() {
    local backend="$1" driver_bin="$2" source="$3"
    local mutated="$BUILD_DIR/async_spawn_await_${backend}.scalar-task.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" \
        'let task: Future<Int> = spawn Inc(4);' \
        'let task: Int = spawn Inc(4);'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend named Future was accepted as a scalar" >&2
        exit 1
    fi
    grep -Fq "Code: let_type_mismatch" "$self_out" "$self_err" &&
    grep -Fq -- "- expected: Int" "$self_out" "$self_err" &&
    grep -Fq -- "- actual: Future<Int>" "$self_out" "$self_err" || {
        echo "[self-host-parity:driver-rung2] $backend named Future scalar diagnostic drifted" >&2
        cat "$self_out" "$self_err" >&2
        exit 1
    }
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted named Future as a scalar" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_spawn_await() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    if [[ "$base" == "async_spawn_await" ]]; then
        local source
        for fact in \
            '"kind":"spawn","text":"spawn Inc(4)"' \
            '"kind":"await","text":"await task"' \
            '"name":"task","type":"Future<Int>"'; do
            grep -Fq "$fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend named Future graph fact drifted: $fact" >&2
                exit 1
            }
        done
        source="$ROOT_DIR/tests/cases/backend_compare/async_spawn_await/main.pgy"
        pgy_selfhost_reject_named_future_scalar_mutation \
            "$backend" "$driver_bin" "$source"
        return 0
    fi
    [[ "$base" == "await_inline_spawn" ]] || return 0

    for fact in \
        '"kind":"spawn","text":"spawn Inc(4)"' \
        '"kind":"spawn","text":"spawn Inc(9)"' \
        '"kind":"await","text":"await spawn Inc(4)"' \
        '"kind":"await","text":"await spawn Inc(9)"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend spawn/await graph fact drifted: $fact" >&2
            exit 1
        }
    done
}

pgy_selfhost_verify_driver_rung2_spawn_await_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    if [[ "$base" == "async_spawn_await" ]]; then
        for term in \
            'PgyTaskHandle task' \
            'pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL' \
            '.unary = Inc }, 1, 4, 0)' \
            'pgy_await_take('; do
            grep -Fq "$term" "$emitted_c" || {
                echo "[self-host-parity:driver-rung2] $backend named Future C fact drifted: $term" >&2
                exit 1
            }
        done
        if grep -Fq 'long long task =' "$emitted_c" ||
            grep -Fq 'task = Inc(4)' "$emitted_c"; then
            echo "[self-host-parity:driver-rung2] $backend named Future used a scalar or sequential fallback" >&2
            exit 1
        fi
        return 0
    fi
    [[ "$base" == "await_inline_spawn" ]] || return 0

    for term in \
        'pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL' \
        'pgy_await_take(' \
        'pgy_pool_init(0)' \
        '.unary = Inc }, 1, 4, 0)' \
        '.unary = Inc }, 1, 9, 0)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend spawn/await C fact drifted: $term" >&2
            exit 1
        }
    done
}
