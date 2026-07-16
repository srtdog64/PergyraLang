#!/usr/bin/env bash
#
# run_bn_compare.sh -- reproduce the same-machine B_n cycle-tally comparison.
#
# This is a MANUAL benchmark runner, not a CI gate: each run is minutes long at
# n=10 (3,715,891,200 signed permutations), so it does not belong in the quick
# best-of-3 micro-benchmark smoke. It times, on THIS machine only, the same
# single-method Loops workload three ways and prints a table:
#
#     hand-C serial   (gcc -O2, benchmarks/baseline_bn_loops.c)
#     pgy-C serial    (benchmarks/perf_bn_cycles_loops.pgy)
#     pgy-C parallel  (benchmarks/perf_bn_cycles_parallel.pgy)
#
# Same-machine ratios are the only meaningful comparison -- do NOT compare these
# to another machine's numbers (e.g. an Apple M4 Max reference suite).
#
# Usage:
#   PGY=bin/pgy bash benchmarks/run_bn_compare.sh [n]      # default n=10
#   PGY=bin/pgy PGY_BN_N=8 bash benchmarks/run_bn_compare.sh   # quicker check
#
# Env:
#   PGY   path to the pgy binary (default bin/pgy)
#   n     first positional arg or PGY_BN_N (default 10)
set -u

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PGY="${PGY:-$ROOT_DIR/bin/pgy}"
N="${1:-${PGY_BN_N:-10}}"

if [[ ! -x "$PGY" && ! -x "$PGY.exe" ]]; then echo "[bn] pgy not found at $PGY; skipping"; exit 0; fi
if ! command -v gcc >/dev/null 2>&1; then echo "[bn] gcc missing; skipping"; exit 0; fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# The benchmark .pgy files default to n=10; rewrite the single N line to $N.
sed "s/let n: Int = 10;/let n: Int = $N;/" \
    "$ROOT_DIR/benchmarks/perf_bn_cycles_loops.pgy"    > "$WORK/loops.pgy"
sed "s/let n: Int = 10;/let n: Int = $N;/" \
    "$ROOT_DIR/benchmarks/perf_bn_cycles_parallel.pgy" > "$WORK/par.pgy"

timeit() { # $@ = command; echoes wall seconds (single run -- these are long)
    local s e
    s=$(date +%s%N 2>/dev/null || python -c 'import time;print(int(time.time()*1e9))')
    "$@" >/dev/null 2>&1
    e=$(date +%s%N 2>/dev/null || python -c 'import time;print(int(time.time()*1e9))')
    awk -v a="$s" -v b="$e" 'BEGIN{printf "%.2f", (b-a)/1e9}'
}

echo "[bn] n=$N  (this machine only; same-machine ratios only)"

gcc -O2 -o "$WORK/handc" "$ROOT_DIR/benchmarks/baseline_bn_loops.c" \
    || { echo "[bn] hand-C gcc failed"; exit 1; }
t_handc=$(timeit "$WORK/handc" "$N")

"$PGY" "$WORK/loops.pgy" --backend=c -o "$WORK/loops" >/dev/null 2>&1 \
    || { echo "[bn] pgy-C serial compile failed"; exit 1; }
t_serial=$(timeit "$WORK/loops")

"$PGY" "$WORK/par.pgy" --backend=c -o "$WORK/par" >/dev/null 2>&1 \
    || { echo "[bn] pgy-C parallel compile failed"; exit 1; }
t_par=$(timeit "$WORK/par")

r_serial=$(awk -v a="$t_serial" -v b="$t_handc" 'BEGIN{printf "%.2f", a/b}')
r_par=$(awk -v a="$t_par" -v b="$t_handc" 'BEGIN{printf "%.2f", a/b}')
echo "[bn]   hand-C serial (gcc -O2)   ${t_handc}s   1.00x"
echo "[bn]   pgy-C  serial (Loops)     ${t_serial}s   ${r_serial}x hand-C"
echo "[bn]   pgy-C  parallel           ${t_par}s   ${r_par}x hand-C"
