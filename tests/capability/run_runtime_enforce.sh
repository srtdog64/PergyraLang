#!/usr/bin/env bash
# Runtime sandbox-enforcement gate (the *dynamic* half; run_manifest.sh covers
# the static declared>=used half). Proves the host-imposed gates actually
# fail-close at run time and -- critically -- do so IDENTICALLY on the C and LLVM
# backends. This is the regression guard for the single-instance gate-state fix
# (g_pgy_budget / g_pgy_cap_granted were each compiled into multiple objects on
# the LLVM path, so the LLVM gate silently never fired until unified).
#
#   capability: cap_random_demo.pgy calls Random() (gates PGY_CAP_RANDOM).
#     PGY_CAP_GRANT unset / "random" -> runs; "clock" / "none" -> capability-denied.
#   stdout: print_demo.pgy calls Print() (gates PGY_CAP_IO_WRITE).
#     PGY_CAP_GRANT unset / "io_write" -> exact marker; "io_read" -> denial
#     before the marker is emitted.
#   budget:     budget_alloc_demo4.pgy while-pushes a List (forced heap).
#     no limit -> runs; PGY_BUDGET_ALLOC_BYTES=64 -> budget-exceeded.
#
# PGY_BIN must point at the built compiler. LLVM cases self-skip if the backend
# cannot launch (no clang / LLVM-C.dll), so the gate still protects the C path.
set -u

# Subject of this gate:
#   a host-imposed runtime gate stopped failing closed, or stopped
#   doing so identically on the C and LLVM backends.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

PGY_BIN="${PGY_BIN:-bin/pgy}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$HERE" || exit 2

if [ ! -x "$PGY_BIN" ] && [ ! -f "$PGY_BIN" ]; then
    echo "[SKIP] pgy not built at $PGY_BIN"; exit 0
fi

# Probe: if the compiler cannot launch at all (e.g. a dev box where the shell
# cannot exec the native pgy / LLVM-C.dll is off PATH), skip the whole gate
# cleanly rather than reporting false failures -- the same contract as
# run_manifest.sh. CI launches pgy natively, so the gate is load-bearing there.
probe_rc=0
"$PGY_BIN" --backend=c --run tests/capability/cap_random_demo.pgy >/dev/null 2>&1 || probe_rc=$?
if [ "$probe_rc" -eq 127 ]; then
    echo "[SKIP] could not launch pgy (exit 127; is LLVM-C.dll on PATH?)"; exit 0
fi

fail=0
llvm_ok=1
io_root="$(mktemp -d "${TMPDIR:-/tmp}/pgy-capability-io.XXXXXX")" || exit 2
trap 'rm -rf -- "$io_root"' EXIT
printf 'file handle read ok\n' > "$io_root/capability_handle_read.txt"

# run <backend> <file> <env-assignments...> -- prints program output+stderr.
# Guarded by `timeout` where available so a wall-clock-deadline regression (a
# program that should be killed by the watchdog but is not) cannot hang the gate.
PGY_TIMEOUT=""
if command -v timeout >/dev/null 2>&1; then PGY_TIMEOUT="timeout 25"; fi
run_prog() {
    local be="$1" file="$2"; shift 2
    env "$@" $PGY_TIMEOUT "$PGY_BIN" "--backend=$be" --run "$file" 2>&1
}

# expect <label> <backend> <file>
#   <deny:CLASS|deny-clean:CLASS:MARKER|run:MARKER> <env...>
expect() {
    local label="$1" be="$2" file="$3" want="$4"; shift 4
    local out rc
    out="$(run_prog "$be" "$file" "$@")"; rc=$?
    # LLVM --run reports a failing child as 127 too. A typed runtime denial is
    # evidence, not backend unavailability; only a marker-free 127 may skip.
    if [ "$be" = "llvm" ] && [ "$rc" -eq 127 ] \
       && ! printf '%s' "$out" | grep -q "class=capability-denied\|class=budget-exceeded"; then
        [ "$llvm_ok" -eq 1 ] && echo "[SKIP] llvm backend unavailable (exit 127)"
        llvm_ok=0; return
    fi
    case "$want" in
        deny-clean:*)
            local denial="${want#deny-clean:}"
            local cls="${denial%%:*}"
            local mk="${denial#*:}"
            if printf '%s' "$out" | grep -q "class=$cls" \
               && ! printf '%s' "$out" | grep -q "$mk"; then
                echo "[PASS] $label ($be) fail-closed class=$cls before '$mk'"
            else
                echo "[FAIL] $label ($be) expected class=$cls without '$mk', got: $(printf '%s' "$out" | tail -1)"; fail=1
            fi ;;
        deny:*)
            local cls="${want#deny:}"
            if printf '%s' "$out" | grep -q "class=$cls"; then
                echo "[PASS] $label ($be) fail-closed class=$cls"
            else
                echo "[FAIL] $label ($be) expected class=$cls, got: $(printf '%s' "$out" | tail -1)"; fail=1
            fi ;;
        run:*)
            local mk="${want#run:}"
            if printf '%s' "$out" | grep -q "$mk" \
               && ! printf '%s' "$out" | grep -q "class=capability-denied\|class=budget-exceeded"; then
                echo "[PASS] $label ($be) ran ($mk)"
            else
                echo "[FAIL] $label ($be) expected to run/'$mk', got: $(printf '%s' "$out" | tail -1)"; fail=1
            fi ;;
    esac
}

for be in c llvm; do
    [ "$be" = "llvm" ] && [ "$llvm_ok" -eq 0 ] && continue
    # capability: Random gated on PGY_CAP_RANDOM
    expect "cap default-grant"  "$be" tests/capability/cap_random_demo.pgy run:"random ok"
    expect "cap grant=random"   "$be" tests/capability/cap_random_demo.pgy run:"random ok"   PGY_CAP_GRANT=random
    expect "cap grant=clock"    "$be" tests/capability/cap_random_demo.pgy deny:capability-denied PGY_CAP_GRANT=clock
    expect "cap grant=none"     "$be" tests/capability/cap_random_demo.pgy deny:capability-denied PGY_CAP_GRANT=none
    # a second cap type + distinct require site (Now -> CLOCK, op="now-ms"):
    # catches op-specific gate-wiring bugs the RANDOM case alone cannot.
    expect "clock default"      "$be" tests/capability/cap_clock_demo.pgy run:"clock ok"
    expect "clock denied"       "$be" tests/capability/cap_clock_demo.pgy deny:capability-denied PGY_CAP_GRANT=random
    # a third cap type (Args -> ENV, op="args"): the process-args fingerprinting
    # surface; closes the previously-dead PGY_CAP_ENV bit. (Verifies on both
    # backends now that the LLVM Args sret-call crash is fixed.)
    expect "env default"        "$be" tests/capability/cap_env_demo.pgy run:"args ok"
    expect "env denied"         "$be" tests/capability/cap_env_demo.pgy deny:capability-denied PGY_CAP_GRANT=random
    # Print owns byte-exact stdout, so its IO_WRITE gate must run before the
    # first byte on both runtime twins.
    expect "print default"       "$be" tests/capability/print_demo.pgy run:"print ok"
    expect "print granted"       "$be" tests/capability/print_demo.pgy run:"print ok" PGY_CAP_GRANT=io_write
    expect "print denied clean"  "$be" tests/capability/print_demo.pgy deny-clean:capability-denied:"print ok" PGY_CAP_GRANT=io_read
    # FileOpen/FileRead/FileWrite used to bypass both static inference and the
    # runtime grant. Pin literal-mode refinement and mode-specific fail-closed
    # enforcement on both backends.
    rm -f -- "$io_root/capability_handle_write.txt"
    expect "file handle write default" "$be" tests/capability/file_handle_write_demo.pgy run:"file handle write ok" PGY_IO_ROOT="$io_root"
    rm -f -- "$io_root/capability_handle_write.txt"
    expect "file handle write granted" "$be" tests/capability/file_handle_write_demo.pgy run:"file handle write ok" PGY_IO_ROOT="$io_root" PGY_CAP_GRANT=io_write
    rm -f -- "$io_root/capability_handle_write.txt"
    expect "file handle write denied" "$be" tests/capability/file_handle_write_demo.pgy deny:capability-denied PGY_IO_ROOT="$io_root" PGY_CAP_GRANT=io_read
    if [ -e "$io_root/capability_handle_write.txt" ]; then
        echo "[FAIL] file handle write denial created an artifact ($be)"; fail=1
    else
        echo "[PASS] file handle write denial left no artifact ($be)"
    fi
    expect "file handle read granted" "$be" tests/capability/file_handle_read_demo.pgy run:"file handle read ok" PGY_IO_ROOT="$io_root" PGY_CAP_GRANT=io_read
    expect "file handle read denied" "$be" tests/capability/file_handle_read_demo.pgy deny:capability-denied PGY_IO_ROOT="$io_root" PGY_CAP_GRANT=io_write
    expect "file exists granted" "$be" tests/capability/file_exists_demo.pgy run:"file exists ok" PGY_IO_ROOT="$io_root" PGY_CAP_GRANT=io_read
    expect "file exists denied" "$be" tests/capability/file_exists_demo.pgy deny:capability-denied PGY_IO_ROOT="$io_root" PGY_CAP_GRANT=io_write
    # budget: forced-heap List
    expect "budget no-limit"    "$be" tests/capability/budget_alloc_demo4.pgy run:"pushed"
    expect "budget limit=64"    "$be" tests/capability/budget_alloc_demo4.pgy deny:budget-exceeded PGY_BUDGET_ALLOC_BYTES=64
    # budget: spawn fork-bomb bound (4 spawns vs ceiling 3)
    expect "spawn no-limit"     "$be" tests/capability/budget_spawn_demo.pgy run:"spawned all"
    expect "spawn limit=3"      "$be" tests/capability/budget_spawn_demo.pgy deny:budget-exceeded PGY_BUDGET_SPAWN_COUNT=3
    # budget: channel count bound (4 channels vs ceiling 3)
    expect "channel no-limit"   "$be" tests/capability/budget_channel_demo.pgy run:"channels made"
    expect "channel limit=3"    "$be" tests/capability/budget_channel_demo.pgy deny:budget-exceeded PGY_BUDGET_CHANNEL_COUNT=3
    # budget: wall-clock deadline bounds an otherwise-infinite tight loop (the
    # case the per-kind counters cannot see). 300ms watchdog vs a forever loop.
    expect "wall deadline"      "$be" tests/capability/budget_wall_demo.pgy deny:budget-exceeded PGY_BUDGET_WALL_MS=300
    # ...and a generous deadline must NOT kill a program that finishes in time
    # (the armed watchdog dies with the process). No-false-positive for the time
    # axis -- finishes in ms, well under the 5s deadline.
    expect "wall no-kill"       "$be" tests/capability/budget_alloc_demo4.pgy run:"pushed" PGY_BUDGET_WALL_MS=5000
    # composition: several budgets imposed at once (the real-world host profile).
    # ALLOC is tight, the siblings generous -> ALLOC fires and the others must not
    # interfere. Guards the per-kind independence of the budget state.
    expect "budget compose"     "$be" tests/capability/budget_alloc_demo4.pgy deny:budget-exceeded \
        PGY_BUDGET_ALLOC_BYTES=64 PGY_BUDGET_SPAWN_COUNT=999 PGY_BUDGET_CHANNEL_COUNT=999 PGY_BUDGET_WALL_MS=60000
done

if [ "$fail" -eq 0 ]; then echo "ALL PASS (0 failures)"; exit 0; fi
echo "FAILED"; exit 1
