#!/usr/bin/env bash
#
# self_host_execution_lane_parity_smoke.sh
#
# The self-host SEA ExecutionLane classifier (src/self_hosted/sea/execution_lane.pgy)
# must make the SAME lane decision as the C policy (src/compiler/execution_lane.c)
# and the same boundary evidence -> capture-fact decisions as
# src/compiler/air_execution_lane.c, on both backends. The golden file is a
# named case-row artifact over the C decision table plus AIR evidence-shape
# proof output, so a match proves:
#   - self-host (Pergyra) policy/evidence shape == C policy/evidence shape
#   - C backend == LLVM backend

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"

fail() { echo "[sea-self-host-lane] FAIL: $*" >&2; exit 1; }

if { [ ! -x "$PGY" ] && [ ! -f "$PGY" ]; } || ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[sea-self-host-lane] SKIP: compiler binary not runnable"
    exit 0
fi

WORK="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/execution_lane}"
mkdir -p "$WORK"
HARNESS_PATHS="$WORK/execution_lane_harness_paths.txt"
pgy_selfhost_read_test_harness_manifest \
    "sea-self-host-lane" \
    "$WORK" \
    "execution-lane-parity-paths" \
    "$HARNESS_PATHS"
harness_paths=()
while IFS= read -r line; do
    [ -n "$line" ] || continue
    harness_paths+=("$line")
done < "$HARNESS_PATHS"
if [ "${#harness_paths[@]}" -ne 5 ]; then
    fail "TestHarness manifest expected 5 execution-lane paths, got ${#harness_paths[@]}"
fi
case "${harness_paths[0]}" in /*|[A-Za-z]:*|*\\*) fail "execution-lane source path must be repo-relative: ${harness_paths[0]}" ;; esac
case "${harness_paths[1]}" in /*|[A-Za-z]:*|*\\*) fail "execution-lane golden path must be repo-relative: ${harness_paths[1]}" ;; esac
case "${harness_paths[2]}" in /*|[A-Za-z]:*|*\\*) fail "lane executor source path must be repo-relative: ${harness_paths[2]}" ;; esac
case "${harness_paths[3]}" in /*|[A-Za-z]:*|*\\*) fail "lane executor golden path must be repo-relative: ${harness_paths[3]}" ;; esac
case "${harness_paths[4]}" in /*|[A-Za-z]:*|*\\*) fail "lane executor missing golden path must be repo-relative: ${harness_paths[4]}" ;; esac

SRC="$ROOT_DIR/${harness_paths[0]}"
GOLDEN="$ROOT_DIR/${harness_paths[1]}"
EXEC_SRC="$ROOT_DIR/${harness_paths[2]}"
EXEC_GOLDEN="$ROOT_DIR/${harness_paths[3]}"
EXEC_MISSING_GOLDEN="$ROOT_DIR/${harness_paths[4]}"
[ -f "$SRC" ]    || fail "missing classifier: $SRC"
[ -f "$GOLDEN" ] || fail "missing golden: $GOLDEN"
[ -f "$EXEC_SRC" ] || fail "missing lane executor contract probe: $EXEC_SRC"
[ -f "$EXEC_GOLDEN" ] || fail "missing lane executor contract golden: $EXEC_GOLDEN"
[ -f "$EXEC_MISSING_GOLDEN" ] || fail "missing lane executor missing-term golden: $EXEC_MISSING_GOLDEN"

grep -Fq "positive_movable_value_authority|MovableScheduler" "$GOLDEN" \
    || fail "golden must pin the MovableScheduler positive row"
grep -Fq "negative_pin_requires_movability|Reject" "$GOLDEN" \
    || fail "golden must pin pin+movability rejection"
grep -Fq "negative_raw_slot_requires_movability|Reject" "$GOLDEN" \
    || fail "golden must pin raw-slot+movability rejection"
grep -Fq "producer_rejects_resource_movable|Reject" "$GOLDEN" \
    || fail "golden must pin producer-side resource rejection"
grep -Fq "air_channel_raw_channel_pins|PinnedZone" "$GOLDEN" \
    || fail "golden must pin raw-channel materialization"
grep -Fq "lane|Reject|(rejected)|fail_closed" "$EXEC_GOLDEN" \
    || fail "executor golden must pin Reject fail-closed behavior"
grep -Fq "lane|MovableScheduler|MovableExecutor|worker_join_scaffold" "$EXEC_GOLDEN" \
    || fail "executor golden must honestly pin current MovableScheduler scaffold depth"
grep -Fq "missing_required|src/runtime/pgy_lane_scheduler.c|definitely_missing_lane_executor_contract_term" "$EXEC_MISSING_GOLDEN" \
    || fail "executor missing-term golden must pin fail-closed missing evidence"

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
    echo "[sea-self-host-lane] backend=$be matches named C policy/evidence shape (33/33)"
done

EXEC_ARG="$(pgy_path_for_compiler "$PGY" "$EXEC_SRC")"
EXEC_C_BIN="$WORK/lane_executor_contract_c.exe"
EXEC_C_OUT="$WORK/lane_executor_contract_c.out"
EXEC_EXPECTED_NORM="$WORK/lane_executor_contract_expected.norm"
EXEC_C_MISSING_OUT="$WORK/lane_executor_contract_c_missing.out"
EXEC_C_MISSING_ERR="$WORK/lane_executor_contract_c_missing.err"
EXEC_MISSING_EXPECTED_NORM="$WORK/lane_executor_contract_missing_expected.norm"
EXEC_C_COMPILE_OUT="$WORK/lane_executor_contract_c.compile.out"
EXEC_C_COMPILE_ERR="$WORK/lane_executor_contract_c.compile.err"
if ! "$PGY" "$EXEC_ARG" --backend=c -o "$EXEC_C_BIN" \
        >"$EXEC_C_COMPILE_OUT" 2>"$EXEC_C_COMPILE_ERR"; then
    cat "$EXEC_C_COMPILE_ERR" >&2
    fail "lane executor contract compile failed"
fi
"$EXEC_C_BIN" 2>/dev/null | pgy_selfhost_normalize_text_artifact > "$EXEC_C_OUT" \
    || fail "lane executor contract run failed"
pgy_selfhost_normalize_text_artifact < "$EXEC_GOLDEN" > "$EXEC_EXPECTED_NORM"
if ! diff -u "$EXEC_EXPECTED_NORM" "$EXEC_C_OUT"; then
    fail "lane executor contract drift (see diff)."
fi

set +e
"$EXEC_C_BIN" --self-test-missing-term 2>"$EXEC_C_MISSING_ERR" | \
    pgy_selfhost_normalize_text_artifact > "$EXEC_C_MISSING_OUT"
EXEC_MISSING_RC=$?
set -e
if [ "$EXEC_MISSING_RC" -ne 1 ]; then
    cat "$EXEC_C_MISSING_OUT" "$EXEC_C_MISSING_ERR" >&2
    fail "lane executor missing-term self-test should fail closed (rc=1), got rc=$EXEC_MISSING_RC"
fi
pgy_selfhost_normalize_text_artifact < "$EXEC_MISSING_GOLDEN" > "$EXEC_MISSING_EXPECTED_NORM"
if ! diff -u "$EXEC_MISSING_EXPECTED_NORM" "$EXEC_C_MISSING_OUT"; then
    fail "lane executor missing-term artifact drift (see diff)."
fi
assert_llvm_leg "sea-self-host-lane-executor" "$EXEC_ARG" "$WORK"

EXEC_LLVM_NEG_BIN="$WORK/lane_executor_contract_llvm_negative.exe"
EXEC_LLVM_NEG_COMPILE_OUT="$WORK/lane_executor_contract_llvm_negative.compile.out"
EXEC_LLVM_NEG_COMPILE_ERR="$WORK/lane_executor_contract_llvm_negative.compile.err"
EXEC_LLVM_MISSING_OUT="$WORK/lane_executor_contract_llvm_missing.out"
EXEC_LLVM_MISSING_ERR="$WORK/lane_executor_contract_llvm_missing.err"
if ! "$PGY" "$EXEC_ARG" --backend=llvm -o "$EXEC_LLVM_NEG_BIN" \
        >"$EXEC_LLVM_NEG_COMPILE_OUT" 2>"$EXEC_LLVM_NEG_COMPILE_ERR"; then
    if pgy_selfhost_log_reports_no_llvm "$EXEC_LLVM_NEG_COMPILE_ERR" 2>/dev/null; then
        echo "[sea-self-host-lane] lane executor missing-term llvm-leg skipped"
    else
        cat "$EXEC_LLVM_NEG_COMPILE_ERR" >&2
        fail "lane executor missing-term LLVM compile failed"
    fi
else
    set +e
    "$EXEC_LLVM_NEG_BIN" --self-test-missing-term 2>"$EXEC_LLVM_MISSING_ERR" | \
        pgy_selfhost_normalize_text_artifact > "$EXEC_LLVM_MISSING_OUT"
    EXEC_LLVM_MISSING_RC=$?
    set -e
    if [ "$EXEC_LLVM_MISSING_RC" -ne 1 ]; then
        cat "$EXEC_LLVM_MISSING_OUT" "$EXEC_LLVM_MISSING_ERR" >&2
        fail "lane executor missing-term LLVM self-test should fail closed (rc=1), got rc=$EXEC_LLVM_MISSING_RC"
    fi
    if ! diff -u "$EXEC_MISSING_EXPECTED_NORM" "$EXEC_LLVM_MISSING_OUT"; then
        fail "lane executor missing-term LLVM artifact drift (see diff)."
    fi
fi

echo "[sea-self-host-lane] PASS"
