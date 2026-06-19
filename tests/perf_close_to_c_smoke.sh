#!/usr/bin/env bash
#
# perf_close_to_c_smoke.sh
#
# Guards that Pergyra's compute speed does not drift far from hand-written C.
# For each micro-benchmark it compiles the .pgy on both backends and a matching
# hand-C baseline (gcc -O2), times best-of-3, and asserts the C backend stays
# within a small factor of hand-C. The LLVM backend is reported and warned on a
# looser threshold (its own optimization pipeline is not yet at -O2 parity).
#
# Env knobs:
#   PGY                  path to the pgy binary (default: $BIN_DIR/pgy or bin/pgy)
#   ROOT_DIR             repo root (default: parent of this script's dir)
#   PERF_C_MAX_RATIO     pgy-C must be <= this x hand-C (default 2.0; fails)
#   PERF_LLVM_MAX_RATIO  pgy-LLVM warn threshold (default 4.0; warns only)
#
set -u

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
PGY="${PGY:-${BIN_DIR:-$ROOT_DIR/bin}/pgy}"
C_MAX="${PERF_C_MAX_RATIO:-2.0}"
LLVM_MAX="${PERF_LLVM_MAX_RATIO:-4.0}"

if [[ ! -x "$PGY" ]]; then echo "[perf] pgy not found at $PGY; skipping"; exit 0; fi
if ! command -v gcc >/dev/null 2>&1; then echo "[perf] gcc missing; skipping"; exit 0; fi
if ! command -v bc  >/dev/null 2>&1; then echo "[perf] bc missing; skipping"; exit 0; fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Hand-C baselines (must mirror the .pgy semantics exactly).
cat > "$WORK/arith.c" <<'EOF'
#include <stdio.h>
int main(void){int acc=1,i=0;while(i<100000000){acc=acc*31+i;i++;}printf("acc=%d\n",acc);return 0;}
EOF
cat > "$WORK/fib.c" <<'EOF'
#include <stdio.h>
int fib(int n){if(n<2)return n;return fib(n-1)+fib(n-2);}
int main(void){printf("fib=%d\n",fib(35));return 0;}
EOF

timeit() { # $1 = executable; echoes best-of-3 wall seconds
    local best=999 r s e d
    for r in 1 2 3; do
        s=$(date +%s.%N); "$1" >/dev/null 2>&1; e=$(date +%s.%N)
        d=$(echo "$e - $s" | bc)
        best=$(echo "if ($d < $best) $d else $best" | bc)
    done
    echo "$best"
}

fail=0
for name in arith fib; do
    src="$ROOT_DIR/benchmarks/perf_${name}.pgy"
    if [[ ! -f "$src" ]]; then echo "[perf] missing benchmark $src"; exit 1; fi

    gcc -O2 -o "$WORK/${name}_cc" "$WORK/${name}.c" \
        || { echo "[perf] baseline gcc failed for $name"; exit 1; }
    "$PGY" "$src" --backend=c -o "$WORK/${name}_c" >/dev/null 2>&1 \
        || { echo "[perf] pgy-C compile failed for $name"; exit 1; }
    "$PGY" "$src" --backend=llvm -o "$WORK/${name}_llvm" >/dev/null 2>&1 \
        || { echo "[perf] pgy-LLVM compile failed for $name"; exit 1; }

    t_cc=$(timeit "$WORK/${name}_cc")
    t_c=$(timeit "$WORK/${name}_c")
    t_llvm=$(timeit "$WORK/${name}_llvm")

    # Floor the baseline so a near-zero hand-C time cannot blow up the ratio.
    base=$(echo "if ($t_cc < 0.005) 0.005 else $t_cc" | bc)
    r_c=$(echo "scale=2; $t_c / $base" | bc)
    r_llvm=$(echo "scale=2; $t_llvm / $base" | bc)

    printf "[perf] %-6s hand-C=%ss  pgy-C=%ss (%sx)  pgy-LLVM=%ss (%sx)\n" \
        "$name" "$t_cc" "$t_c" "$r_c" "$t_llvm" "$r_llvm"

    if [[ "$(echo "$r_c > $C_MAX" | bc)" == "1" ]]; then
        echo "[perf] FAIL: $name pgy-C ${r_c}x exceeds ${C_MAX}x of hand-C"
        fail=1
    fi
    if [[ "$(echo "$r_llvm > $LLVM_MAX" | bc)" == "1" ]]; then
        echo "[perf] WARN: $name pgy-LLVM ${r_llvm}x exceeds ${LLVM_MAX}x (LLVM opt-pipeline tuning needed)"
    fi
done

# --- Keyword / construct timings ---------------------------------------------
# These have no hand-C baseline (array/match/generic/for-in do not map to one
# line of C), so correctness is asserted by backend output equality and speed is
# reported with a loose backend-ratio sanity guard. A mismatch is a hard FAIL.
echo "[perf] keyword/construct timings (correctness = backend output equality):"
for name in array forloop generic match; do
    src="$ROOT_DIR/benchmarks/bench_${name}.pgy"
    if [[ ! -f "$src" ]]; then echo "[perf] missing $src"; exit 1; fi

    "$PGY" "$src" --backend=c -o "$WORK/k_${name}_c" >/dev/null 2>&1 \
        || { echo "[perf] construct $name pgy-C compile failed"; exit 1; }
    "$PGY" "$src" --backend=llvm -o "$WORK/k_${name}_llvm" >/dev/null 2>&1 \
        || { echo "[perf] construct $name pgy-LLVM compile failed"; exit 1; }

    out_c="$("$WORK/k_${name}_c" 2>/dev/null)"
    out_llvm="$("$WORK/k_${name}_llvm" 2>/dev/null)"
    if [[ "$out_c" != "$out_llvm" ]]; then
        echo "[perf] FAIL: construct $name backend output mismatch ('$out_c' vs '$out_llvm')"
        fail=1
    fi

    t_c=$(timeit "$WORK/k_${name}_c")
    t_llvm=$(timeit "$WORK/k_${name}_llvm")
    base=$(echo "if ($t_c < 0.002) 0.002 else $t_c" | bc)
    r=$(echo "scale=2; $t_llvm / $base" | bc)
    printf "[perf] %-8s pgy-C=%ss  pgy-LLVM=%ss (%sx)  out=%s\n" \
        "$name" "$t_c" "$t_llvm" "$r" "$out_c"

    if [[ "$(echo "$r > $LLVM_MAX" | bc)" == "1" ]]; then
        echo "[perf] WARN: construct $name pgy-LLVM ${r}x of pgy-C (backend codegen)"
    fi
done

if [[ $fail -eq 0 ]]; then
    echo "[perf] ok: C backend within ${C_MAX}x of hand-C; constructs agree across backends"
else
    exit 1
fi
