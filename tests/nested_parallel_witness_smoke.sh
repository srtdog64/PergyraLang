#!/usr/bin/env bash
#
# nested_parallel_witness_smoke.sh -- nested `parallel ... join` must complete,
# not deadlock (docs/186 P-A1, WO-RT-3).
#
# Class under test: a pool worker that awaits subtasks spawned into the SAME
# worker pool. Before help-first await landed, a worker parked in
# pthread_cond_wait; with every worker parked on outer tasks, the inner tasks
# queued behind the remaining outers could never run -- pool-starvation
# deadlock. Witnessed on this machine pre-fix: the forced fixture below timed
# out 3/3 at 25s on the C backend; post-fix it completes in well under a
# second per run on both backends.
#
# Two fixtures:
#   mild   -- 32 outers x 4 inners, no burn. Pre-fix this PASSED by luck
#             (workers could pop inners before parking): the trap shape that
#             survives tests and deadlocks under production load.
#   forced -- 64 outers (> workers) burn first so every worker holds an outer
#             before any inner is enqueued: deterministic starvation pre-fix.
#
# Wiring note: not yet in the Makefile/test-all (Makefile concurrently owned
# at landing time). Run directly:
#   PGY_BIN=bin/pgy.exe bash tests/nested_parallel_witness_smoke.sh
set -euo pipefail

# Subject of this gate: native runtime nested-pool progress on both backends.
# Delegating would turn a self-host coverage gap into a scheduler regression.
# This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    echo "[nested-parallel] SKIP missing compiler binary: $PGY"
    exit 0
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[nested-parallel] SKIP compiler binary is not runnable here: $PGY"
    exit 0
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then PYTHON_BIN=python3
    elif command -v python >/dev/null 2>&1; then PYTHON_BIN=python
    else echo "[nested-parallel] python is required" >&2; exit 1; fi
fi

TIMEOUT_SECONDS="${PGY_NESTED_PARALLEL_TIMEOUT_SECONDS:-30}"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_nested_parallel.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat >"$WORK_DIR/mild.pgy" <<'PGYSRC'
func Inner(base: Int) -> Int {
    let s: Int = parallel (i in 0..4) join with sum { give base + i; };
    return s;
}
func Main() -> Void {
    let total: Int = parallel (p in 0..32) join with sum { give Inner(p); };
    Log("total=" + ToString(total));
}
PGYSRC

cat >"$WORK_DIR/forced.pgy" <<'PGYSRC'
func Burn(n: Int) -> Int {
    let acc: Int = 0;
    let i: Int = 0;
    while i < n { acc = acc + 1; i = i + 1; }
    return acc;
}
func Inner(base: Int) -> Int {
    let warm: Int = Burn(20000000);
    let s: Int = parallel (i in 0..4) join with sum { give base + i; };
    return s + warm - warm;
}
func Main() -> Void {
    let total: Int = parallel (p in 0..64) join with sum { give Inner(p); };
    Log("total=" + ToString(total));
}
PGYSRC

# expected: mild = sum_{p<32} (4p+6) = 2176 ; forced = sum_{p<64} (4p+6) = 8448
run_case() {
    local backend="$1" fixture="$2" expected="$3"
    local src="$WORK_DIR/$fixture.pgy"
    local bin="$WORK_DIR/${fixture}_${backend}"
    local bin_arg src_arg

    src_arg="$(pgy_path_for_compiler "$PGY" "$src")"
    bin_arg="$(pgy_path_for_compiler "$PGY" "$bin")"
    if ! (cd "$ROOT_DIR" && "$PGY" "$src_arg" "--backend=$backend" -o "$bin_arg") \
        >"$WORK_DIR/${fixture}_${backend}.compile.log" 2>&1; then
        echo "[nested-parallel] $backend compile failed for $fixture" >&2
        cat "$WORK_DIR/${fixture}_${backend}.compile.log" >&2
        return 1
    fi
    if [[ -x "$bin.exe" ]]; then bin="$bin.exe"; fi

    "$PYTHON_BIN" - "$bin" "$backend" "$fixture" "$expected" "$TIMEOUT_SECONDS" <<'PY'
import subprocess
import sys

binary, backend, fixture, expected, timeout_s = (
    sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5]))
try:
    result = subprocess.run([binary], capture_output=True, text=True,
                            timeout=timeout_s, check=False)
except subprocess.TimeoutExpired:
    raise SystemExit(
        f"[nested-parallel] {backend}/{fixture} DEADLOCK: no exit within "
        f"{timeout_s}s -- worker awaits are parking instead of helping")
line = result.stdout.strip().splitlines()[-1] if result.stdout.strip() else ""
if result.returncode != 0 or line != f"total={expected}":
    raise SystemExit(
        f"[nested-parallel] {backend}/{fixture} rc={result.returncode} "
        f"stdout={result.stdout!r} stderr={result.stderr[-300:]!r}")
PY
    echo "[nested-parallel] PASS $backend/$fixture (total=$expected within ${TIMEOUT_SECONDS}s)"
}

BACKENDS="${PGY_NESTED_PARALLEL_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    if [[ "$backend" == "llvm" ]] \
        && ! "$PGY" --help 2>&1 | grep -q -- "--backend=llvm"; then
        echo "[nested-parallel] SKIP llvm (compiler built without LLVM support)"
        continue
    fi
    run_case "$backend" mild 2176
    run_case "$backend" forced 8448
done

echo "[nested-parallel] nested parallel fan-out completes on every worker-parked shape"
