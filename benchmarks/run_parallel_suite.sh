#!/usr/bin/env bash
#
# run_parallel_suite.sh -- reproduce the 3-axis same-machine parallel
# comparison in benchmarks/PARALLEL_RESULTS.md (throughput / nested
# fork-join / fine-grain). Manual runner, not a CI gate. Same-machine
# ratios only. Skips contenders whose toolchain is missing.
#
# Usage: PGY_BIN=bin/pgy.exe bash benchmarks/run_parallel_suite.sh
set -u

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
[[ -x "$PGY" ]] || { echo "[par-suite] missing pgy: $PGY"; exit 1; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/pgy_par_suite.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

bestof3() { # $@ = command; echoes best wall ms
    local best=999999999 s e ms
    for _ in 1 2 3; do
        s=$(date +%s%N)
        "$@" >/dev/null 2>&1
        e=$(date +%s%N)
        ms=$(( (e - s) / 1000000 ))
        (( ms < best )) && best=$ms
    done
    echo "$best"
}

pgy_build() { # $1 = stem
    "$PGY" "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/benchmarks/$1.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$WORK/$1")" \
        >/dev/null 2>&1 || { echo "[par-suite] pgy build failed: $1"; exit 1; }
    [[ -x "$WORK/$1.exe" ]] && echo "$WORK/$1.exe" || echo "$WORK/$1"
}

PGY_FINE="$(pgy_build perf_parallel_map_fine)"
PGY_CHUNK="$(pgy_build perf_parallel_map_chunked)"
PGY_SER="$(pgy_build perf_parallel_map_serial)"
PGY_FIB="$(pgy_build perf_parallel_fib)"

HAVE_GCC=0; command -v gcc >/dev/null 2>&1 && HAVE_GCC=1
HAVE_GO=0;  command -v go  >/dev/null 2>&1 && HAVE_GO=1
HAVE_FC=0;  command -v gfortran >/dev/null 2>&1 && HAVE_FC=1
if (( HAVE_GCC )); then
    gcc -O2 -fopenmp -o "$WORK/map_c" "$ROOT_DIR/benchmarks/baseline_par_map.c"
    gcc -O2 -fopenmp -o "$WORK/fib_c" "$ROOT_DIR/benchmarks/baseline_par_fib.c"
fi
if (( HAVE_GO )); then
    go build -o "$WORK/map_go" "$ROOT_DIR/benchmarks/baseline_par_map.go"
    go build -o "$WORK/fib_go" "$ROOT_DIR/benchmarks/baseline_par_fib.go"
fi
(( HAVE_FC )) && gfortran -O2 -fopenmp -o "$WORK/map_f" "$ROOT_DIR/benchmarks/baseline_par_map.f90"

echo "[par-suite] THROUGHPUT 32M (ms, best-of-3)"
echo "  pgy chunked x16 : $(bestof3 "$PGY_CHUNK")"
(( HAVE_GCC )) && echo "  C OpenMP for    : $(bestof3 "$WORK/map_c" ompfor 32000000)"
(( HAVE_GO ))  && echo "  Go chunked x16  : $(bestof3 "$WORK/map_go" chunked 32000000)"
(( HAVE_FC ))  && echo "  Fortran OMP do  : $(bestof3 "$WORK/map_f" 32000000)"
echo "  pgy serial      : $(bestof3 "$PGY_SER")"
(( HAVE_GCC )) && echo "  C serial        : $(bestof3 "$WORK/map_c" serial 32000000)"

echo "[par-suite] NESTED fib(38) cutoff 28 (ms, best-of-3)"
echo "  pgy nested      : $(bestof3 "$PGY_FIB")"
(( HAVE_GCC )) && echo "  C OpenMP task   : $(bestof3 "$WORK/fib_c")"
(( HAVE_GO ))  && echo "  Go recursion    : $(bestof3 "$WORK/fib_go")"
(( HAVE_GCC )) && echo "  C serial        : $(bestof3 "$WORK/fib_c" serial)"

echo "[par-suite] FINE 200k tasks (ms, best-of-3; pgy = task/index today)"
echo "  pgy task/index  : $(bestof3 "$PGY_FINE")"
(( HAVE_GCC )) && echo "  C omptask g(1)  : $(bestof3 "$WORK/map_c" omptask 200000)"
(( HAVE_GO ))  && echo "  Go goroutine/el : $(bestof3 "$WORK/map_go" perelem 200000)"
