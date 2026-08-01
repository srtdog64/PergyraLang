#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:mir-expression-graph-read] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "mir-expression-graph-read" "$PGY" \
    || fail "PGY_BIN is not runnable"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/mir_expression_graph_read"
PROBE="$BUILD_DIR/probe.exe"
mkdir -p "$BUILD_DIR"

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/tools/expression_graph_persisted_read_probe/main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$PROBE")" \
    >"$BUILD_DIR/compile.log" 2>&1) \
    || { cat "$BUILD_DIR/compile.log" >&2; fail "probe build failed"; }

positive="$($PROBE)"
[[ "$positive" == *"persisted-graph-read=one-pass-exact"* ]] \
    || fail "positive graph reconstruction drifted"

for mode in \
    --duplicate-node-field \
    --unknown-header-field \
    --unreachable-node; do
    negative="$($PROBE "$mode")"
    [[ "$negative" == *"persisted-graph-negative=closed"* ]] \
        || fail "negative graph was not rejected: $mode"
done

echo "[self-host-parity:mir-expression-graph-read] one-pass persisted graph owner ok"
