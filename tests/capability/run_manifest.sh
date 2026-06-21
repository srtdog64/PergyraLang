#!/usr/bin/env bash
# Capability manifest + declared>=used gate. CI-portable harness.
#
# Capability is a first-class, interprocedurally-inferred refinement of effects.
# `pgy --capability-manifest` prints the program's inferred capability set; the
# per-function `with caps` check (declared >= used) is enforced during normal
# semantic analysis and fails the run.
#
#   manifest_clean.pgy        -> exit 0; used set has IO_WRITE/CLOCK/RANDOM.
#   manifest_declared_ok.pgy  -> exit 0; `with caps clock` covers Now().
#   manifest_violation.pgy    -> exit 1; GetTimestamp uses CLOCK, declared io_read.
#   manifest_interproc.pgy    -> exit 1; entry declares io_read but calls helper()
#                                which uses CLOCK (capability propagates through
#                                the call graph -- the interprocedural proof).
#
# PGY_BIN must point at the built compiler.
set -u

PGY_BIN="${PGY_BIN:-bin/pgy}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$HERE" || exit 2

if [ ! -x "$PGY_BIN" ] && [ ! -f "$PGY_BIN" ]; then
    echo "[SKIP] pgy not built at $PGY_BIN"
    exit 0
fi

fail=0

run_manifest() { "$PGY_BIN" --capability-manifest "$1" 2>&1; }

# --- expect-clean cases ----------------------------------------------------
for f in manifest_clean manifest_declared_ok; do
    out="$(run_manifest "tests/capability/$f.pgy")"; rc=$?
    if [ "$rc" -eq 127 ]; then
        echo "[SKIP] could not launch pgy (exit 127; is LLVM-C.dll on PATH?)"; exit 0
    fi
    if [ "$rc" -ne 0 ]; then echo "[FAIL] $f exit=$rc (want 0)"; fail=1
    elif printf '%s' "$out" | grep -q "missing declared capabilities"; then
        echo "[FAIL] $f reported a capability violation"; fail=1
    else echo "[PASS] $f clean (exit 0)"; fi
done

# clean fixture must enumerate the expected capabilities
clean_out="$(run_manifest tests/capability/manifest_clean.pgy)"
for cap in IO_WRITE CLOCK RANDOM; do
    printf '%s' "$clean_out" | grep -q "$cap" \
        || { echo "[FAIL] clean manifest missing $cap"; fail=1; }
done

# --- expect-violation cases (declared >= used must fire) -------------------
check_violation() { # file  function-name
    out="$(run_manifest "tests/capability/$1.pgy")"; rc=$?
    if [ "$rc" -eq 0 ]; then echo "[FAIL] $1 exit=0 (gate did not fire)"; fail=1; return; fi
    printf '%s' "$out" | grep -q "missing declared capabilities" \
        || { echo "[FAIL] $1 missing capability-violation message"; fail=1; }
    printf '%s' "$out" | grep -q "$2" \
        || { echo "[FAIL] $1 violation should name '$2'"; fail=1; }
    printf '%s' "$out" | grep -q "clock" \
        || { echo "[FAIL] $1 violation should name the missing 'clock' capability"; fail=1; }
    if [ "$rc" -ne 0 ] \
       && printf '%s' "$out" | grep -q "missing declared capabilities" \
       && printf '%s' "$out" | grep -q "$2"; then
        echo "[PASS] $1 under-declaration gate fired ($2 -> clock, exit $rc)"
    fi
}
check_violation manifest_violation GetTimestamp
check_violation manifest_interproc entry

if [ "$fail" -eq 0 ]; then echo "ALL PASS (0 failures)"; exit 0; fi
echo "FAILED"; exit 1
