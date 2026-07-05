#!/usr/bin/env bash
#
# self_host_execution_lane_parity_smoke.sh
#
# The self-host SEA ExecutionLane classifier (src/self_hosted/sea/execution_lane.pgy)
# must make the SAME lane decision as the C policy (src/compiler/execution_lane.c)
# and the same boundary evidence -> capture-fact decisions as
# src/compiler/air_execution_lane.c, on both backends. The golden file is the C
# decision table plus AIR evidence-shape proof output, so a match proves:
#   - self-host (Pergyra) policy/evidence shape == C policy/evidence shape
#   - C backend == LLVM backend

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
SRC="$ROOT_DIR/src/self_hosted/sea/execution_lane.pgy"
GOLDEN="$ROOT_DIR/src/self_hosted/sea/expected_lanes.txt"

fail() { echo "[sea-self-host-lane] FAIL: $*" >&2; exit 1; }

if { [ ! -x "$PGY" ] && [ ! -f "$PGY" ]; } || ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[sea-self-host-lane] SKIP: compiler binary not runnable"
    exit 0
fi
[ -f "$SRC" ]    || fail "missing classifier: $SRC"
[ -f "$GOLDEN" ] || fail "missing golden: $GOLDEN"

grep -Fq "struct BoundaryLaneInputFact" "$SRC" \
    || fail "self-host lane classifier must consume a typed BoundaryLaneInputFact"
grep -Fq "func CaptureFactFromBoundaryFact" "$SRC" \
    || fail "self-host lane classifier must derive capture facts from typed input"
grep -Fq "func BoundaryFactWithMirValueCaptureEvidence" "$SRC" \
    || fail "self-host lane classifier must mirror MIR value-capture evidence production"
grep -Fq "func BoundaryHasResourceCaptureEvidence" "$SRC" \
    || fail "self-host lane classifier must reject resource captures before value-only promotion"
grep -Fq "has_rir_zone_pin_evidence" "$SRC" \
    || fail "self-host lane classifier must consume RIR zone-pin evidence"
if grep -Fq "BoundarySourceKind" "$SRC" || grep -Fq "source_kind" "$SRC"; then
    fail "self-host lane classifier reintroduced source-kind lane evidence"
fi
if grep -Fq "CaptureFactFromBoundarySource" "$SRC"; then
    fail "self-host lane classifier reintroduced source-string capture fact API"
fi
if grep -Fq "LaneFromBoundarySource" "$SRC"; then
    fail "self-host lane classifier reintroduced source-string lane API"
fi

WORK="$(mktemp -d)"
backends="c"
# Only attempt LLVM if this build advertises it.
if "$PGY" --help 2>/dev/null | grep -qiE 'llvm'; then
    backends="c llvm"
fi

for be in $backends; do
    exe="$WORK/lane_$be.exe"
    if ! "$PGY" "$(pgy_path_for_compiler "$PGY" "$SRC")" --backend="$be" -o "$exe" \
            >"$WORK/compile_$be.out" 2>"$WORK/compile_$be.err"; then
        if [ "$be" = "llvm" ] && pgy_selfhost_log_reports_no_llvm "$WORK/compile_$be.err" 2>/dev/null; then
            echo "[sea-self-host-lane] LLVM unavailable; skipping llvm backend"
            continue
        fi
        cat "$WORK/compile_$be.err" >&2
        fail "compile failed (backend=$be)"
    fi
    "$exe" 2>/dev/null | tr -d '\r' > "$WORK/got_$be.txt" || fail "run failed (backend=$be)"
    if ! diff -u "$GOLDEN" "$WORK/got_$be.txt"; then
        fail "lane decision drift (backend=$be) vs the C policy golden (see diff)."
    fi
    echo "[sea-self-host-lane] backend=$be matches C policy/evidence shape (31/31)"
done

echo "[sea-self-host-lane] PASS"
