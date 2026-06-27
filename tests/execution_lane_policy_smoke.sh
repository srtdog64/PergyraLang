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

# Naming-layer contract (docs/146 §1): SEA is the semantic model/contract, never
# the name of a scheduler. The runtime scheduler is PgyLaneScheduler. Forbid
# collapsing the layers by naming a scheduler "SEA*".
if git -C "$ROOT_DIR" grep -InE 'SEA[_ ]?[Ss]cheduler|sea_scheduler' \
        -- 'src/**' 'docs/**' >/dev/null 2>&1; then
    git -C "$ROOT_DIR" grep -InE 'SEA[_ ]?[Ss]cheduler|sea_scheduler' -- 'src/**' 'docs/**' >&2 || true
    fail "SEA must name the contract, not a scheduler (docs/146 §1). Use PgyLaneScheduler / *Executor for the runtime."
fi

echo "[execution-lane-policy] PASS"
