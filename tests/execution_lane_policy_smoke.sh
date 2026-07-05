#!/usr/bin/env bash
#
# execution_lane_policy_smoke.sh — build + run the SEA ExecutionLane decision
# table proof. The policy is a pure function (no runtime), so this compiles just
# execution_lane.c + the test and runs it. Keeps the lane assignment contract
# (evidence -> lane, fail-closed) from drifting.

set -euo pipefail

if ! command -v cat >/dev/null 2>&1 \
    || ! command -v mkdir >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

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

AIR_OUT="$OUT_DIR/air_lane_capture_test_$$.exe"
AIR_COMPILE_OUT="$AIR_OUT.compile.out"
AIR_COMPILE_ERR="$AIR_OUT.compile.err"
PATH="$COMPILE_PATH"
if ! "${CC_CMD[@]}" -Wall -Wextra -Werror -std=c11 \
        -I"$ROOT_FOR_CC/src/compiler" \
        "$ROOT_FOR_CC/src/tests/air_execution_lane_source_test.c" \
        "$ROOT_FOR_CC/src/compiler/air_execution_lane.c" \
        "$ROOT_FOR_CC/src/compiler/execution_lane.c" \
        -o "$AIR_OUT" >"$AIR_COMPILE_OUT" 2>"$AIR_COMPILE_ERR"; then
    PATH="$ORIGINAL_PATH"
    cat "$AIR_COMPILE_OUT" >&2 || true
    cat "$AIR_COMPILE_ERR" >&2 || true
    fail "AIR boundary evidence lane compile failed"
fi
PATH="$ORIGINAL_PATH"

"$AIR_OUT" || fail "AIR boundary evidence lane mismatch"

if grep -Fq "air_boundary_source_kind(boundary)" \
        "$ROOT_DIR/src/compiler/air_execution_lane.c"; then
    fail "AIR lane capture must not consume source-kind as lane evidence"
fi
grep -Fq "boundary->has_rir_deterministic_fork_join_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR fork-join lane capture must consume RIR fork-join evidence"
grep -Fq "boundary->has_rir_await_local_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR async/await lane capture must consume RIR await-local evidence"
grep -Fq "boundary->has_rir_movability_requirement_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR spawn lane capture must consume RIR movability-requirement evidence"
grep -Fq "boundary->has_rir_raw_channel_capture_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR channel lane capture must consume RIR raw-channel evidence"
grep -Fq "boundary->has_rir_raw_slot_capture_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR resource lane capture must consume RIR raw-slot evidence"
grep -Fq "boundary->has_rir_live_view_capture_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR resource lane capture must consume RIR live-view evidence"
grep -Fq "boundary->has_mir_pin_cleanup_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR pin lane capture must consume MIR pin-cleanup evidence"
grep -Fq "boundary->has_mir_value_capture_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR value-only lane capture must consume MIR value-capture evidence"

# Naming-layer contract (docs/146 §1): SEA is the semantic model/contract, never
# the name of a scheduler. The runtime scheduler is PgyLaneScheduler. Forbid
# collapsing the layers by naming a scheduler "SEA*".
grep -Fq "air_collect_mir_value_capture_evidence" \
    "$ROOT_DIR/src/compiler/air_evidence_mir.c" ||
    fail "AIR MIR evidence must produce value-only capture evidence"
grep -Fq "boundary->has_rir_movability_requirement_evidence" \
    "$ROOT_DIR/src/compiler/air_evidence_mir.c" ||
    fail "AIR MIR value-capture producer must require movability evidence"
grep -Fq "air_boundary_has_resource_capture_evidence" \
    "$ROOT_DIR/src/compiler/air_evidence_mir.c" ||
    fail "AIR MIR value-capture producer must reject resource captures"
grep -Fq "air_refresh_execution_lane_facts(air)" \
    "$ROOT_DIR/src/compiler/air_evidence_mir.c" ||
    fail "AIR MIR evidence must refresh boundary capture and lane facts"
grep -Fq "AIR collects MIR value-capture lane evidence" \
    "$ROOT_DIR/src/test_air.c" ||
    fail "AIR tests must cover MIR value-capture lane evidence"

if git -C "$ROOT_DIR" grep -InE 'SEA[_ ]?[Ss]cheduler|sea_scheduler' \
        -- 'src/**' 'docs/**' >/dev/null 2>&1; then
    git -C "$ROOT_DIR" grep -InE 'SEA[_ ]?[Ss]cheduler|sea_scheduler' -- 'src/**' 'docs/**' >&2 || true
    fail "SEA must name the contract, not a scheduler (docs/146 §1). Use PgyLaneScheduler / *Executor for the runtime."
fi

grep -Fq "boundary->has_rir_zone_pin_evidence" \
    "$ROOT_DIR/src/compiler/air_execution_lane.c" ||
    fail "AIR zone lane capture must consume RIR zone-pin evidence"
if awk '
    /case AIR_BOUNDARY_ZONE:/ { in_zone = 1 }
    in_zone && /break;/ { in_zone = 0 }
    in_zone && /captures_pin[[:space:]]*=[[:space:]]*true/ { found = 1 }
    END { exit(found ? 0 : 1) }
' "$ROOT_DIR/src/compiler/air_execution_lane.c"; then
    fail "AIR zone lane capture must not derive pin directly from boundary kind"
fi

echo "[execution-lane-policy] PASS"
