#!/usr/bin/env bash
#
# lane_scheduler_smoke.sh — build + run the SEA runtime facade contract proof.
# Compiles the facade + the execution-lane policy + the test (pthread), and
# checks executor-invariance and fail-closed Reject.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
OUT="$(mktemp -d)/lane_scheduler_test"

fail() { echo "[lane-scheduler] FAIL: $*" >&2; exit 1; }

"$CC" -Wall -Wextra -Werror -std=c11 \
    -I"$ROOT_DIR/src/runtime" \
    "$ROOT_DIR/src/tests/lane_scheduler_test.c" \
    "$ROOT_DIR/src/runtime/pgy_lane_scheduler.c" \
    "$ROOT_DIR/src/compiler/execution_lane.c" \
    -lpthread \
    -o "$OUT" || fail "compile failed"

"$OUT" || fail "facade contract violated"

echo "[lane-scheduler] PASS"
