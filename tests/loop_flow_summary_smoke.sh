#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_BIN="$(pgy_select_optional_exe_binary "$PGY_BIN")"
pgy_require_runnable_binary_here "loop-flow-summary" "$PGY_BIN"

PYTHON_BIN="${PYTHON_BIN:-python}"
FIXTURE_DIR="$ROOT_DIR/tests/cases/semantic_loop_flow"
TMP_DIR="${TMPDIR:-$ROOT_DIR/.tmp}/loop_flow_summary_smoke"
mkdir -p "$TMP_DIR"

for count in 1 2 3 4; do
    fixture="$FIXTURE_DIR/loop_${count}.pgy"
    stderr_file="$TMP_DIR/loop_${count}.stderr"
    "$PYTHON_BIN" - "$PGY_BIN" "$fixture" "$stderr_file" "$count" <<'PY'
import os
import subprocess
import sys
import time

pgy, fixture, stderr_path, loop_count = sys.argv[1:]
env = os.environ.copy()
env["PGY_DEBUG_LOOP_FLOW"] = "1"
started = time.monotonic()
try:
    run = subprocess.run(
        [pgy, "--hir", fixture],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=env,
        timeout=5.0 if loop_count == "4" else 10.0,
        check=False,
    )
except subprocess.TimeoutExpired:
    raise SystemExit(f"{loop_count}-loop fixture exceeded its deadline")
elapsed = time.monotonic() - started
with open(stderr_path, "wb") as out:
    out.write(run.stderr)
if run.returncode != 0:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit(f"{loop_count}-loop fixture failed with {run.returncode}")
if loop_count == "4" and elapsed >= 5.0:
    raise SystemExit(f"4-loop fixture took {elapsed:.3f}s, expected <5s")
print(f"[loop-flow-summary] loops={loop_count} elapsed={elapsed:.3f}s")
PY

    trace_count="$(grep -c 'pgy: loop-flow kind=' "$stderr_file" || true)"
    if [[ "$trace_count" -ne "$count" ]]; then
        echo "expected $count loop-flow traces, got $trace_count" >&2
        cat "$stderr_file" >&2
        exit 1
    fi
    if grep -Ev 'body_reentry_count=1([[:space:]]|$)' "$stderr_file" \
        | grep 'pgy: loop-flow kind=' >/dev/null; then
        echo "loop body re-entry count must remain exactly 1" >&2
        cat "$stderr_file" >&2
        exit 1
    fi
done

grep -Eq 'kind=(for|while).*summary_record_count=1' \
    "$TMP_DIR/loop_4.stderr" || {
    echo "4-loop fixture did not record LoopFlowSummary facts" >&2
    cat "$TMP_DIR/loop_4.stderr" >&2
    exit 1
}

HIT_FIXTURE="$FIXTURE_DIR/summary_hit.pgy"
HIT_STDERR="$TMP_DIR/summary_hit.stderr"
PGY_DEBUG_LOOP_FLOW=1 "$PGY_BIN" --hir "$HIT_FIXTURE" \
    >/dev/null 2>"$HIT_STDERR"
grep -Eq 'summary_hit_count=[1-9][0-9]*' "$HIT_STDERR" || {
    echo "summary-hit fixture did not apply a cached loop transfer" >&2
    cat "$HIT_STDERR" >&2
    exit 1
}

echo "[loop-flow-summary] permanent 1..4 loop re-entry and 5s gates passed"
