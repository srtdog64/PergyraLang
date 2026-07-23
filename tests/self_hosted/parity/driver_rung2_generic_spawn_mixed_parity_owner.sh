#!/usr/bin/env bash
# Owns the mixed scalar Future capstone over one tagged spawn ABI.

pgy_selfhost_verify_driver_rung2_generic_spawn_mixed() {
    local backend="$1" base="$2" self_mir_json="$3" fact
    [[ "$base" == "generic_future_spawn_mixed" ]] || return 0
    for fact in \
        '"kind":"spawn","text":"spawn Identity(42)"' \
        '"kind":"spawn","text":"spawn PickSecond(10, 77)"' \
        '"kind":"spawn","text":"spawn Identity(\"hi\")"' \
        '"name":"first","type":"Future<Int>"' \
        '"name":"second","type":"Future<Int>"' \
        '"name":"text","type":"Future<String>"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend mixed generic spawn fact drifted: $fact" >&2
            exit 1
        }
    done
}

pgy_selfhost_verify_driver_rung2_generic_spawn_mixed_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3" term
    [[ "$base" == "generic_future_spawn_mixed" ]] || return 0
    for term in \
        '.int_unary = Identity_Int }, PGY_SELFHOST_SPAWN_INT1' \
        '.int_binary = PickSecond_Int }, PGY_SELFHOST_SPAWN_INT2' \
        '.string_unary = Identity_String }, PGY_SELFHOST_SPAWN_STRING1' \
        'pgy_await_take(first, long long)' \
        'pgy_await_take(second, long long)' \
        'pgy_await_take(text, const char*)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend mixed generic spawn C fact drifted: $term" >&2
            exit 1
        }
    done
    if grep -Eq 'pgy_selfhost_spawn_(int|string)' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend mixed generic spawn reopened payload-specific helpers" >&2
        exit 1
    fi
}
