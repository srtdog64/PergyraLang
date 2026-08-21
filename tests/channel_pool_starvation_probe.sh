#!/usr/bin/env bash
#
# channel_pool_starvation_probe.sh -- HARD GATE for the channel edition of
# the WO-RT-3 pool-starvation class (WO-RT-5).
#
# Class: THREAD-model channel waits park the OS thread in cancellation-quantum
# pthread_cond_timedwait -- a parked task OCCUPIES its worker, and (pre-fix)
# there was no help-first drain: with every worker and the helping main parked
# on channel recv while the would-be sender was still QUEUED, nothing
# progressed. Witnessed RED on both backends 2026-07-17 (this file's original
# evidence-probe form, `79eb26b5`); help-first await alone could NOT close it
# because helping into a receiver parked the helper too.
#
# Fixed by capacity-bounded compensation workers. Before each 10ms park
# quantum, a blocked pool task calls pgy_pool_channel_blocked_tick(). If work
# is pending and no pool runner is idle, the lifecycle owner spawns a spare
# that runs the normal queue loop. The tick is idempotent and rechecks under
# the lifecycle mutex, so work arriving after an earlier empty observation
# self-heals on the next quantum. Beyond the spare cap the wait degrades to
# the plain quantum park; the fiber lane remains that residue's candidate.
#
# Gate contract: BOTH fixtures must complete with their exact totals within
# the timeout on both backends. A timeout here is the starvation class
# regressing -- fail closed.
#
# Deterministic pre-fix shape (PGY_WORKERS=2, arms form, 7 recv arms then 7
# send arms): workers and the helping main all picked receiver arms first and
# parked; the send arms sat queued behind them forever.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/channel_pool_starvation_probe.sh
set -euo pipefail

# Subject of this gate: native runtime channel starvation recovery.
# Delegating would turn a self-host coverage gap into a scheduler regression.
# This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="channel-pool-starvation"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"

require_runtime_term() {
    local rel="$1"
    local term="$2"
    if ! grep -Fq "$term" "$ROOT_DIR/$rel"; then
        echo "[$LABEL] missing runtime contract in $rel: $term" >&2
        exit 1
    fi
}

require_runtime_term "src/runtime/pgy_parallel_pool_lifecycle.h" \
    "pgy_pool_channel_blocked_tick(void)"
require_runtime_term "src/runtime/pgy_parallel_pool_lifecycle.h" \
    "atomic_init(&pool->spare_count, 0);"
require_runtime_term "src/runtime/pgy_parallel_pool_lifecycle.h" \
    "+ atomic_load_explicit(&g_pgy_pool.spare_count"
require_runtime_term "src/runtime/pgy_runtime.h" \
    "#include \"pgy_parallel.h\""
require_runtime_term "src/runtime/pgy_runtime_channel_inline.h" \
    "#include \"pgy_parallel.h\""
require_runtime_term "src/runtime/pgy_runtime_channel_inline.h" \
    "PGY_CHANNEL_BLOCKED_TICK();"
require_runtime_term "src/runtime/pgy_runtime_channel_string_inline.h" \
    "#include \"pgy_parallel.h\""
require_runtime_term "src/runtime/pgy_runtime_channel_string_inline.h" \
    "PGY_CHANNEL_BLOCKED_TICK();"
if grep -R -Fq '#define PGY_CHANNEL_BLOCKED_TICK() ((void)0)' \
        "$ROOT_DIR/src/runtime"; then
    echo "[$LABEL] silent no-op compensation fallback reappeared" >&2
    exit 1
fi
if grep -R -E -q "PGY_CHANNEL_POOL_HELP_ONE|pgy_pool_channel_wait_help_one" \
        "$ROOT_DIR/src/runtime"; then
    echo "[$LABEL] removed inline-help channel path reappeared" >&2
    exit 1
fi

if [[ ! -x "$PGY" ]]; then
    echo "[$LABEL] SKIP missing compiler binary: $PGY"
    exit 0
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[$LABEL] SKIP compiler binary is not runnable here: $PGY"
    exit 0
fi

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then PYTHON_BIN=python3
    elif command -v python >/dev/null 2>&1; then PYTHON_BIN=python
    else echo "[$LABEL] python is required" >&2; exit 1; fi
fi

TIMEOUT_SECONDS="${PGY_CHANNEL_STARVATION_TIMEOUT_SECONDS:-10}"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_channel_starvation.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

# Fixture note: channels are used DIRECTLY in the join bodies (recv/send at
# the capture site). Passing Channel<T> as a function parameter silently
# COPIES the descriptor and desyncs head/count from the shared buffer --
# discovered while writing this probe, filed separately (chip
# task_b4b2f972); a serial two-recv repro returns 1+1 instead of 1+2.

# Control: channel pre-filled before the fan-out -- no receiver ever parks.
cat >"$WORK_DIR/control.pgy" <<'PGYSRC'
func Main() -> Void {
    let ch: Channel<Int> = Channel(8);
    ch <- 1; ch <- 2; ch <- 3; ch <- 4;
    let total: Int = parallel (i in 0..4) join with sum {
        give <- ch;
    };
    Log("total=" + ToString(total));
}
PGYSRC

# Starvation (arms form; R2 forbids branch-around-give in the join form):
# 7 receiver arms spawned BEFORE 7 sender arms. With PGY_WORKERS=2 the two
# workers and the helping main all pick receiver arms and park in recv;
# every send arm sits queued behind parked receivers. Expected total on
# completion: 1+..+7 = 28.
cat >"$WORK_DIR/starve.pgy" <<'PGYSRC'
func Main() -> Void {
    let ch: Channel<Int> = Channel(8);
    let mut r1: Int = 0;
    let mut r2: Int = 0;
    let mut r3: Int = 0;
    let mut r4: Int = 0;
    let mut r5: Int = 0;
    let mut r6: Int = 0;
    let mut r7: Int = 0;
    parallel {
        r1 = <- ch;
        r2 = <- ch;
        r3 = <- ch;
        r4 = <- ch;
        r5 = <- ch;
        r6 = <- ch;
        r7 = <- ch;
        ch <- 1;
        ch <- 2;
        ch <- 3;
        ch <- 4;
        ch <- 5;
        ch <- 6;
        ch <- 7;
    }
    Log("total=" + ToString(r1 + r2 + r3 + r4 + r5 + r6 + r7));
}
PGYSRC

compile_case() {
    local backend="$1" fixture="$2"
    local src="$WORK_DIR/$fixture.pgy"
    local bin="$WORK_DIR/${fixture}_${backend}"

    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$src")" \
        "--backend=$backend" -o "$(pgy_path_for_compiler "$PGY" "$bin")") \
        >"$WORK_DIR/${fixture}_${backend}.compile.log" 2>&1; then
        echo "[$LABEL] $backend compile failed for $fixture" >&2
        cat "$WORK_DIR/${fixture}_${backend}.compile.log" >&2
        return 1
    fi
    [[ -x "$bin.exe" ]] && bin="$bin.exe"
    printf '%s\n' "$bin"
}

run_with_timeout() { # $1=binary $2=timeout; echoes COMPLETED:<lastline> or TIMEOUT
    "$PYTHON_BIN" - "$1" "$2" <<'PY'
import subprocess, sys
binary, timeout_s = sys.argv[1], float(sys.argv[2])
try:
    r = subprocess.run([binary], capture_output=True, text=True,
                       timeout=timeout_s, check=False,
                       env=None)
except subprocess.TimeoutExpired:
    print("TIMEOUT")
    raise SystemExit(0)
last = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""
print(f"COMPLETED rc={r.returncode} {last}")
PY
}

BACKENDS="${PGY_CHANNEL_STARVATION_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    if [[ "$backend" == "llvm" ]] \
        && ! "$PGY" --help 2>&1 | grep -q -- "--backend=llvm"; then
        echo "[$LABEL] SKIP llvm (compiler built without LLVM support)"
        continue
    fi

    control_bin="$(compile_case "$backend" control)" || exit 1
    starve_bin="$(compile_case "$backend" starve)" || exit 1

    control_res="$(PGY_WORKERS=2 run_with_timeout "$control_bin" "$TIMEOUT_SECONDS")"
    if [[ "$control_res" != "COMPLETED rc=0 total=10" ]]; then
        echo "[$LABEL] $backend control fixture broke: $control_res" >&2
        exit 1
    fi
    echo "[$LABEL] PASS $backend/control (total=10)"

    starve_res="$(PGY_WORKERS=2 run_with_timeout "$starve_bin" "$TIMEOUT_SECONDS")"
    case "$starve_res" in
        "COMPLETED rc=0 total=28")
            echo "[$LABEL] PASS $backend/starvation (total=28 within ${TIMEOUT_SECONDS}s)"
            ;;
        TIMEOUT)
            echo "[$LABEL] $backend/starvation REGRESSED: channel-parked pool" \
                 "starvation is back (no exit within ${TIMEOUT_SECONDS}s)" >&2
            exit 1
            ;;
        *)
            echo "[$LABEL] $backend/starvation unexpected outcome: $starve_res" >&2
            exit 1
            ;;
    esac
done

echo "[$LABEL] blocked channel chains unwind via bounded compensation on both backends"
