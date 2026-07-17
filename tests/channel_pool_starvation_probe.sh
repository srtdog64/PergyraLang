#!/usr/bin/env bash
#
# channel_pool_starvation_probe.sh -- EVIDENCE PROBE (not a pass/fail feature
# gate) for the channel edition of the WO-RT-3 pool-starvation class, and the
# measured basis for the B4 decision memo (docs/187 memo 1).
#
# Class: THREAD-model channel waits park the OS thread in cancellation-quantum
# pthread_cond_timedwait (pgy_runtime_channel_inline.h) -- a parked task
# OCCUPIES its worker, and unlike pgy_await there is no help-first drain. If
# every worker (and main, via help running INTO a parking task) is parked on
# channel recv while the would-be sender is still QUEUED, nothing progresses.
# Help-first await does NOT close this class: help runs a queued task to
# completion on the helping thread, so helping into a receiver parks the
# helper too.
#
# Probe contract (asserts the DOCUMENTED state, fail-closed on change):
#   control fixture completes  -> required GREEN (harness validity);
#   starvation fixture times out -> documented gap holds, exit 0;
#   starvation fixture COMPLETES -> exit 1: the class got fixed -- flip this
#     probe into a hard gate (expect total=28) and update the board WO.
#
# Deterministic shape (PGY_WORKERS=2, arms form, 7 recv arms then 7 send
# arms): the two workers and the helping main all pick receiver arms first
# (receivers occupy every shard head) and park; the send arms sit queued
# behind them. No cancellation is in flight, so the cancellation quanta
# re-arm forever.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/channel_pool_starvation_probe.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="channel-pool-starvation"
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
        TIMEOUT)
            echo "[$LABEL] $backend/starvation: DOCUMENTED GAP holds (parked" \
                 "channel waits starve the pool within ${TIMEOUT_SECONDS}s)"
            ;;
        "COMPLETED rc=0 total=28")
            echo "[$LABEL] $backend/starvation COMPLETED: the class got fixed." >&2
            echo "[$LABEL] Flip this probe into a hard gate (expect total=28)" \
                 "and close the board WO." >&2
            exit 1
            ;;
        *)
            echo "[$LABEL] $backend/starvation unexpected outcome: $starve_res" >&2
            exit 1
            ;;
    esac
done

echo "[$LABEL] evidence recorded: channel-parked pool starvation is reachable;"
echo "[$LABEL] fix candidates live in docs/187 memo 1 (help-in-channel-wait vs fiber lane)"
