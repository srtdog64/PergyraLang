#!/usr/bin/env bash
#
# execution_lane_policy_smoke.sh — build + run the SEA ExecutionLane decision
# table proof. The policy is a pure function (no runtime), so this compiles just
# execution_lane.c + the test and runs it. Keeps the lane assignment contract
# (evidence -> lane, fail-closed) from drifting.

set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="$(cd "${SCRIPT_PATH%/*}" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
CC_CMD=($CC)

fail() { echo "[execution-lane-policy] FAIL: $*" >&2; exit 1; }

OUT_DIR="$ROOT_DIR/build"
if [[ ! -d "$OUT_DIR" ]]; then
    mkdir -p "$OUT_DIR" || fail "could not create build output dir"
fi
OUT="$OUT_DIR/lane_policy_test_$$.exe"
ROOT_FOR_CC="$ROOT_DIR"
OUT_FOR_CC="$OUT"
COMPILE_PATH="$PATH"
if [[ "${CC_CMD[0]}" = "gcc" && -x /c/ProgramData/mingw64/mingw64/bin/gcc ]]; then
    COMPILE_PATH="/c/ProgramData/mingw64/mingw64/bin:/c/Windows/system32:/c/Windows"
fi

COMPILE_OUT="$OUT.compile.out"
COMPILE_ERR="$OUT.compile.err"
ORIGINAL_PATH="$PATH"
PATH="$COMPILE_PATH"
if ! "${CC_CMD[@]}" -Wall -Wextra -Werror -std=c11 \
        -I"$ROOT_FOR_CC/src/compiler" \
        "$ROOT_FOR_CC/src/tests/execution_lane_policy_test.c" \
        "$ROOT_FOR_CC/src/compiler/execution_lane.c" \
        -o "$OUT_FOR_CC" >"$COMPILE_OUT" 2>"$COMPILE_ERR"; then
    PATH="$ORIGINAL_PATH"
    cat "$COMPILE_OUT" >&2 || true
    cat "$COMPILE_ERR" >&2 || true
    echo "[execution-lane-policy] CC=$CC" >&2
    echo "[execution-lane-policy] MAKEFLAGS=${MAKEFLAGS:-}" >&2
    echo "[execution-lane-policy] PATH=$PATH" >&2
    command -v "${CC_CMD[0]}" >&2 || true
    "${CC_CMD[0]}" --version >&2 || true
    echo "[execution-lane-policy] OUT=$OUT" >&2
    echo "[execution-lane-policy] ROOT_FOR_CC=$ROOT_FOR_CC" >&2
    echo "[execution-lane-policy] OUT_FOR_CC=$OUT_FOR_CC" >&2
    fail "compile failed"
fi
PATH="$ORIGINAL_PATH"

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
