#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-runtime-none.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY_BIN" ]]; then
    echo "[runtime-none-contract] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi

run_reject() {
    local name="$1"
    local source="$2"
    local expected="$3"
    local log="$WORK_DIR/${name}.json"

    if "$PGY_BIN" "$source" --runtime=none --error-format=json >"$WORK_DIR/${name}.out" 2>"$log"; then
        echo "[runtime-none-contract] expected rejection for $name" >&2
        exit 1
    fi
    for term in \
        "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED" \
        "\"stage\":\"driver\"" \
        "driver:runtime:none_unsupported" \
        "use-default-runtime-or-remove-runtime-surface" \
        "$expected"; do
        if ! grep -Fq "$term" "$log"; then
            echo "[runtime-none-contract] $name missing diagnostic term: $term" >&2
            cat "$log" >&2
            exit 1
        fi
    done
}

run_reject "parallel" "$ROOT_DIR/examples/concurrency_demo.pgy" "parallel"
run_reject "channel" "$ROOT_DIR/examples/channel_test.pgy" "channel"
run_reject "intent" "$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy" "intent"
run_reject "world" "$ROOT_DIR/tests/cases/backend_compare/world_zone_projection_visibility/main.pgy" "world"
run_reject "pure-freestanding-blocked" "$ROOT_DIR/tests/cases/backend_compare/basic/main.pgy" "freestanding backend lowering is not implemented yet"

echo "[runtime-none-contract] --runtime=none rejects runtime surfaces and blocks false freestanding claims"
