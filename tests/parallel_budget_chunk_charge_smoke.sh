#!/usr/bin/env bash
# SPAWN_COUNT charge semantics golden (docs/188 R3 ruling, docs/187 memo 3).
#
# RULING (2026-07-17, delegated): PGY_BUDGET_SPAWN_COUNT meters the pool
# tasks the runtime actually creates. Since auto-chunking (WO-RT-4 B3,
# `6f5f29c0`) a `parallel (i in lo..hi) join` fan-out creates
# chunk_count(n) = min(n, workers x PGY_PARALLEL_CHUNK_FACTOR) driver tasks,
# NOT n -- so the spawn ceiling bounds real task creation (threads, task
# structs, queue pressure), and no longer doubles as an accidental
# iteration-count ceiling. Bounding iteration work is a SEPARATE future
# budget axis (ITER/TIME, board-registered); using SPAWN for it would make
# every legitimate large loop budget-hostile.
#
# This gate pins that ruling on both backends, deterministically via
# PGY_WORKERS=4 (chunk_count(1000) = 16):
#   charge case  -- budget 100 >= 16 chunk charges: completes, total=1000.
#                   (Under the PRE-chunking semantics this same program
#                   charged 1000 and would fail-close -- this passing line
#                   IS the semantic pin.)
#   ceiling case -- budget 8 < 16: the 9th chunk spawn fail-closes with
#                   panic class `budget-exceeded` (the ceiling still bites
#                   on what it now meters).
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/parallel_budget_chunk_charge_smoke.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="parallel-budget-chunk-charge"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    echo "[$LABEL] SKIP missing compiler binary: $PGY"
    exit 0
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[$LABEL] SKIP compiler binary is not runnable here: $PGY"
    exit 0
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_budget_chunk.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat >"$WORK_DIR/main.pgy" <<'PGYSRC'
func Main() -> Void {
    let total: Int = parallel (i in 0..1000) join with sum { give 1; };
    Log("total=" + ToString(total));
}
PGYSRC

BACKENDS="${PGY_BUDGET_CHUNK_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    if [[ "$backend" == "llvm" ]] \
        && ! "$PGY" --help 2>&1 | grep -q -- "--backend=llvm"; then
        echo "[$LABEL] SKIP llvm (compiler built without LLVM support)"
        continue
    fi

    bin="$WORK_DIR/charge_$backend"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/main.pgy")" \
        "--backend=$backend" -o "$(pgy_path_for_compiler "$PGY" "$bin")") \
        >"$WORK_DIR/$backend.compile.log" 2>&1; then
        echo "[$LABEL] $backend compile failed" >&2
        cat "$WORK_DIR/$backend.compile.log" >&2
        exit 1
    fi
    [[ -x "$bin.exe" ]] && bin="$bin.exe"

    # charge case: chunk-count charging fits; N-charging would not.
    set +e
    charge_out="$(PGY_WORKERS=4 PGY_BUDGET_SPAWN_COUNT=100 "$bin" 2>"$WORK_DIR/$backend.charge.err")"
    charge_rc=$?
    set -e
    if [[ "$charge_rc" -ne 0 || "$(printf '%s\n' "$charge_out" | tr -d '\r' | tail -1)" != "total=1000" ]]; then
        echo "[$LABEL] $backend charge case broke (rc=$charge_rc): a 1000-index join" >&2
        echo "[$LABEL] must charge chunk_count (16 under PGY_WORKERS=4), not N" >&2
        printf '%s\n' "$charge_out" >&2
        cat "$WORK_DIR/$backend.charge.err" >&2
        exit 1
    fi

    # ceiling case: the meter still fail-closes on what it now counts.
    set +e
    PGY_WORKERS=4 PGY_BUDGET_SPAWN_COUNT=8 "$bin" \
        >"$WORK_DIR/$backend.ceiling.out" 2>"$WORK_DIR/$backend.ceiling.err"
    ceiling_rc=$?
    set -e
    if [[ "$ceiling_rc" -eq 0 ]]; then
        echo "[$LABEL] $backend ceiling case broke: 16 chunk spawns under budget 8" >&2
        echo "[$LABEL] must fail-close, but the program completed" >&2
        exit 1
    fi
    if ! grep -q "budget-exceeded" "$WORK_DIR/$backend.ceiling.err" \
        && ! grep -q "budget-exceeded" "$WORK_DIR/$backend.ceiling.out"; then
        echo "[$LABEL] $backend ceiling case died without the budget-exceeded panic class" >&2
        cat "$WORK_DIR/$backend.ceiling.out" "$WORK_DIR/$backend.ceiling.err" >&2
        exit 1
    fi
    echo "[$LABEL] PASS $backend (budget 100: total=1000; budget 8: budget-exceeded)"
done

echo "[$LABEL] SPAWN_COUNT meters created tasks (chunk drivers), and the ceiling still bites"
