#!/usr/bin/env bash
#
# sea_execution_lane_golden_smoke.sh — golden test for the SEA ExecutionLane
# fact, end to end through real compilation.
#
# The policy unit test (execution_lane_policy_smoke.sh) proves the decision
# table in isolation. This proves the fact actually FLOWS: a real program is
# compiled, AIR is synthesised, each concurrency boundary is classified in the
# finalization pass, and the lane is emitted in `--air-json`. It also guards the
# regression where boundaries were left at the fail-closed zero value (Reject)
# because the classifier never ran.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"

fail() { echo "[sea-lane-golden] FAIL: $*" >&2; exit 1; }

if [ ! -x "$PGY" ] && [ ! -f "$PGY" ]; then
    echo "[sea-lane-golden] SKIP: no compiler binary at $PGY"
    exit 0
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[sea-lane-golden] SKIP: compiler binary not runnable on this host"
    exit 0
fi

FIXTURE="$ROOT_DIR/tests/cases/backend_compare/intent_cross_world_transfer/main.pgy"
GOLDEN="$ROOT_DIR/tests/cases/sea_execution_lanes/expected_lanes.txt"
[ -f "$FIXTURE" ] || fail "missing fixture: $FIXTURE"
[ -f "$GOLDEN" ]  || fail "missing golden: $GOLDEN"

WORK="$(mktemp -d)"
JSON="$WORK/air.json"
GOT="$WORK/got.txt"

"$PGY" --air-json "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" > "$JSON" 2>"$WORK/err" \
    || { cat "$WORK/err" >&2; fail "pgy --air-json failed"; }

# Extract "<kind> <lane>" per boundary, in document order. The non-greedy class
# keeps each kind paired with the execution_lane inside the same boundary object.
grep -oE '"kind":"[a-z]+"[^}]*?"execution_lane":"[A-Za-z]+"' "$JSON" \
    | sed -E 's/.*"kind":"([a-z]+)".*"execution_lane":"([A-Za-z]+)".*/\1 \2/' \
    > "$GOT" || true

# Regression guard: a classified boundary must never be the fail-closed zero
# value. If every lane is Reject the finalization pass did not run.
if [ -s "$GOT" ] && ! grep -qvE ' Reject$' "$GOT"; then
    fail "every boundary is Reject — the SEA finalization classifier did not run"
fi

if ! diff -u "$GOLDEN" "$GOT"; then
    fail "ExecutionLane golden drift (see diff above). If the fixture or the lane policy changed on purpose, update $GOLDEN."
fi

echo "[sea-lane-golden] PASS ($(wc -l < "$GOT" | tr -d ' ') boundaries)"
