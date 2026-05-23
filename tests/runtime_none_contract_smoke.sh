#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_BIN_WAS_DEFAULT=0
if [[ -z "${PGY_BIN:-}" ]]; then
    PGY_BIN="$ROOT_DIR/bin/pgy"
    PGY_BIN_WAS_DEFAULT=1
fi
if [[ "$PGY_BIN" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY_BIN}.exe"; then
    PGY_BIN="${PGY_BIN}.exe"
fi
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-runtime-none.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

require_term() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        { echo "[runtime-none-contract] $rel missing term: $term" >&2; exit 1; }
}

require_term "src/pgy_driver.c" "--runtime=none"
require_term "src/compiler/driver_app.c" "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED"
require_term "src/compiler/driver_app.c" "freestanding backend lowering is not implemented yet"
require_term "src/compiler/runtime_none_contract.c" "runtime-dependent surface"
require_term "src/compiler/driver_diag.c" "PGY_CAUSE_DRIVER_RUNTIME_NONE_UNSUPPORTED"
require_term "src/semantic/diag_codes.h" "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED"
require_term "src/semantic/diag_codes.h" "driver:runtime:none_unsupported"

if [[ ! -x "$PGY_BIN" ]]; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 || "${PGY_RUNTIME_NONE_ALLOW_MISSING_BIN:-0}" == "1" ]]; then
        echo "[runtime-none-contract] SKIP executable probe; source contract is gated"
        exit 0
    fi
    echo "[runtime-none-contract] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi
if ! "$PGY_BIN" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_BIN_WAS_DEFAULT" -eq 1 || "${PGY_RUNTIME_NONE_ALLOW_MISSING_BIN:-0}" == "1" ]]; then
        echo "[runtime-none-contract] SKIP executable probe; source contract is gated"
        exit 0
    fi
    echo "[runtime-none-contract] compiler binary is not runnable: $PGY_BIN" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

pgy_path_arg() {
    pgy_path_for_compiler "$PGY_BIN" "$1"
}

run_reject() {
    local name="$1"
    local source="$2"
    local expected="$3"
    local log="$WORK_DIR/${name}.json"
    local source_arg

    source_arg="$(pgy_path_arg "$source")"

    if "$PGY_BIN" "$source_arg" --runtime=none --error-format=json >"$WORK_DIR/${name}.out" 2>"$log"; then
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
