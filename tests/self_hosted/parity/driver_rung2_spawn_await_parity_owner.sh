#!/usr/bin/env bash
# Owns the graph-carried direct spawn/await executable rung.

pgy_selfhost_verify_driver_rung2_spawn_await() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    if [[ "$base" == "async_spawn_await" ]]; then
        for fact in \
            '"kind":"spawn","text":"spawn Inc(4)"' \
            '"kind":"await","text":"await task"'; do
            grep -Fq "$fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend named Future graph fact drifted: $fact" >&2
                exit 1
            }
        done
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
            'pgy_selfhost_spawn_int1(Inc, 4)' \
            'pgy_await_take('; do
            grep -Fq "$term" "$emitted_c" || {
                echo "[self-host-parity:driver-rung2] $backend named Future C fact drifted: $term" >&2
                exit 1
            }
        done
        return 0
    fi
    [[ "$base" == "await_inline_spawn" ]] || return 0

    for term in \
        'pgy_lane_spawn_dispatch(PGY_LANE_WORKER_POOL' \
        'pgy_await_take(' \
        'pgy_pool_init(0)' \
        'pgy_selfhost_spawn_int1(Inc, 4)' \
        'pgy_selfhost_spawn_int1(Inc, 9)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend spawn/await C fact drifted: $term" >&2
            exit 1
        }
    done
}
