#!/usr/bin/env bash
#
# execution_lane_policy_smoke.sh — build + run the SEA ExecutionLane decision
# table proof. The policy is a pure function (no runtime), so this compiles just
# execution_lane.c + the test and runs it. Keeps the lane assignment contract
# (evidence -> lane, fail-closed) from drifting.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
OUT="$(mktemp -d)/lane_policy_test"

fail() { echo "[execution-lane-policy] FAIL: $*" >&2; exit 1; }

"$CC" -Wall -Wextra -Werror -std=c11 \
    -I"$ROOT_DIR/src/compiler" \
    "$ROOT_DIR/src/tests/execution_lane_policy_test.c" \
    "$ROOT_DIR/src/compiler/execution_lane.c" \
    -o "$OUT" || fail "compile failed"

"$OUT" || fail "decision table mismatch"

echo "[execution-lane-policy] PASS"
