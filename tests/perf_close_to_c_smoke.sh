#!/usr/bin/env bash
#
# perf_close_to_c_smoke.sh
#
# Multi-language compute-speed comparison + "don't drift from C" gate.
#
# For each micro-benchmark it compiles, runs, and best-of-3 times the identical
# workload across every toolchain present:
#
#   hand-C     gcc -O2 -fwrapv                 (required; the gate baseline)
#   hand-C++   g++ -O2 -fwrapv -std=c++17      (optional; idiomatic std::array etc.)
#   Rust       rustc -O                        (optional; idiomatic iterators etc.)
#   pgy-C      pgy --backend=c   (transpile -> gcc)
#   pgy-LLVM   pgy --backend=llvm
#
# Correctness is cross-language OUTPUT EQUALITY: every present binary for a
# benchmark must print the same line, or it is a hard FAIL (this catches a
# baseline typo AND a pgy codegen regression -- neither can pass silently).
#
# The gate asserts pgy-C stays within a small factor of hand-C. C++/Rust are
# reported as reference points, not gated (they ride their own optimizers and
# are optional toolchains).
#
# Env knobs:
#   PGY                  path to the pgy binary (default: $BIN_DIR/pgy or bin/pgy)
#   ROOT_DIR             repo root (default: parent of this script's dir)
#   PERF_C_MAX_RATIO     pgy-C must be <= this x hand-C (default 2.0; fails)
#   PERF_LLVM_MAX_RATIO  pgy-LLVM warn threshold vs hand-C (default 4.0; warns)
#   PGY_RUSTC            path to rustc if not on PATH (rustc is not a build dep)
#
set -u

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "$0")/.." && pwd)}"
PGY="${PGY:-${BIN_DIR:-$ROOT_DIR/bin}/pgy}"
C_MAX="${PERF_C_MAX_RATIO:-2.0}"
LLVM_MAX="${PERF_LLVM_MAX_RATIO:-4.0}"

if [[ ! -x "$PGY" ]]; then echo "[perf] pgy not found at $PGY; skipping"; exit 0; fi
if ! command -v gcc >/dev/null 2>&1; then echo "[perf] gcc missing; skipping"; exit 0; fi
if ! command -v bc  >/dev/null 2>&1; then echo "[perf] bc missing; skipping"; exit 0; fi

# Optional reference toolchains. rustc is often installed but not on PATH.
HAVE_CPP=0
if command -v g++ >/dev/null 2>&1; then HAVE_CPP=1; fi
RUSTC=""
if [[ -n "${PGY_RUSTC:-}" && -x "${PGY_RUSTC:-}" ]]; then RUSTC="$PGY_RUSTC"
elif command -v rustc >/dev/null 2>&1; then RUSTC="rustc"; fi
echo "[perf] toolchains: hand-C=yes  hand-C++=$([[ $HAVE_CPP -eq 1 ]] && echo yes || echo no)" \
     " Rust=$([[ -n "$RUSTC" ]] && echo yes || echo no)  pgy-C=yes  pgy-LLVM=yes"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Map benchmark name -> benchmarks/<file>.pgy
pgy_src() {
    case "$1" in
        arith)     echo "$ROOT_DIR/benchmarks/perf_arith.pgy" ;;
        fib)       echo "$ROOT_DIR/benchmarks/perf_fib.pgy" ;;
        forloop)   echo "$ROOT_DIR/benchmarks/bench_forloop.pgy" ;;
        array)     echo "$ROOT_DIR/benchmarks/bench_array.pgy" ;;
        match)     echo "$ROOT_DIR/benchmarks/bench_match.pgy" ;;
        branchmix) echo "$ROOT_DIR/benchmarks/perf_branchmix.pgy" ;;
        nestloop)  echo "$ROOT_DIR/benchmarks/perf_nestloop.pgy" ;;
    esac
}

# Write the C / C++ / Rust baselines for a benchmark into $WORK/<name>.{c,cpp,rs}.
# A benchmark with no dedicated .cpp reuses its .c under g++. Each baseline must
# print the SAME line the .pgy prints; the equality check below enforces it.
write_sources() {
    local n="$1"
    case "$n" in
    arith)
        cat > "$WORK/arith.c" <<'EOF'
#include <stdio.h>
int main(void){int acc=1;int i=0;while(i<100000000){acc=acc*31+i;i++;}printf("acc=%d\n",acc);return 0;}
EOF
        cat > "$WORK/arith.rs" <<'EOF'
fn main(){let mut acc:i32=1;let mut i:i32=0;while i<100000000{acc=acc.wrapping_mul(31).wrapping_add(i);i+=1;}println!("acc={}",acc);}
EOF
        ;;
    fib)
        cat > "$WORK/fib.c" <<'EOF'
#include <stdio.h>
static int fib(int n){if(n<2)return n;return fib(n-1)+fib(n-2);}
int main(void){printf("fib=%d\n",fib(35));return 0;}
EOF
        cat > "$WORK/fib.rs" <<'EOF'
fn fib(n:i32)->i32{if n<2{n}else{fib(n-1)+fib(n-2)}}
fn main(){println!("fib={}",fib(35));}
EOF
        ;;
    forloop)
        cat > "$WORK/forloop.c" <<'EOF'
#include <stdio.h>
int main(void){int xs[5]={1,2,3,4,5};int total=0;int i=0;while(i<10000000){for(int k=0;k<5;k++)total+=xs[k];i++;}printf("for=%d\n",total);return 0;}
EOF
        cat > "$WORK/forloop.cpp" <<'EOF'
#include <cstdio>
#include <array>
int main(){std::array<int,5> xs{1,2,3,4,5};int total=0;int i=0;while(i<10000000){for(int n:xs)total+=n;i++;}printf("for=%d\n",total);return 0;}
EOF
        cat > "$WORK/forloop.rs" <<'EOF'
fn main(){let xs=[1,2,3,4,5];let mut total:i32=0;let mut i=0;while i<10000000{for &n in xs.iter(){total+=n;}i+=1;}println!("for={}",total);}
EOF
        ;;
    array)
        cat > "$WORK/array.c" <<'EOF'
#include <stdio.h>
int main(void){int xs[8]={1,2,3,4,5,6,7,8};int total=0;int i=0;int j=0;while(i<50000000){total+=xs[j];j++;if(j>=8)j=0;i++;}printf("array=%d\n",total);return 0;}
EOF
        cat > "$WORK/array.cpp" <<'EOF'
#include <cstdio>
#include <array>
int main(){std::array<int,8> xs{1,2,3,4,5,6,7,8};int total=0;int i=0;int j=0;while(i<50000000){total+=xs.at(j);j++;if(j>=8)j=0;i++;}printf("array=%d\n",total);return 0;}
EOF
        cat > "$WORK/array.rs" <<'EOF'
fn main(){let xs=[1,2,3,4,5,6,7,8];let mut total:i32=0;let mut i=0;let mut j:usize=0;while i<50000000{total+=xs[j];j+=1;if j>=8{j=0;}i+=1;}println!("array={}",total);}
EOF
        ;;
    match)
        cat > "$WORK/match.c" <<'EOF'
#include <stdio.h>
static int classify(int n){switch(n){case 0:return 1;case 1:return 3;case 2:return 5;default:return 0;}}
int main(void){int total=0;int i=0;int j=0;while(i<50000000){total+=classify(j);j++;if(j>=4)j=0;i++;}printf("match=%d\n",total);return 0;}
EOF
        cat > "$WORK/match.rs" <<'EOF'
fn classify(n:i32)->i32{match n{0=>1,1=>3,2=>5,_=>0}}
fn main(){let mut total:i32=0;let mut i=0;let mut j=0;while i<50000000{total+=classify(j);j+=1;if j>=4{j=0;}i+=1;}println!("match={}",total);}
EOF
        ;;
    branchmix)
        cat > "$WORK/branchmix.c" <<'EOF'
#include <stdio.h>
int main(void){int total=0;int i=0;int k=0;while(i<60000000){total+=1;if(k==0)total+=1;k++;if(k>=3)k=0;i++;}printf("branch=%d\n",total);return 0;}
EOF
        cat > "$WORK/branchmix.rs" <<'EOF'
fn main(){let mut total:i32=0;let mut i=0;let mut k=0;while i<60000000{total+=1;if k==0{total+=1;}k+=1;if k>=3{k=0;}i+=1;}println!("branch={}",total);}
EOF
        ;;
    nestloop)
        cat > "$WORK/nestloop.c" <<'EOF'
#include <stdio.h>
int main(void){int total=0;int i=0;while(i<8000){int j=0;while(j<8000){total+=1;j++;}i++;}printf("nest=%d\n",total);return 0;}
EOF
        cat > "$WORK/nestloop.rs" <<'EOF'
fn main(){let mut total:i32=0;let mut i=0;while i<8000{let mut j=0;while j<8000{total+=1;j+=1;}i+=1;}println!("nest={}",total);}
EOF
        ;;
    esac
}

timeit() { # $1 = executable; echoes best-of-3 wall seconds
    local best=999 r s e d
    for r in 1 2 3; do
        s=$(date +%s.%N); "$1" >/dev/null 2>&1; e=$(date +%s.%N)
        d=$(echo "$e - $s" | bc)
        best=$(echo "if ($d < $best) $d else $best" | bc)
    done
    echo "$best"
}

ratio() { # $1=time $2=base  -> time/base, base floored so near-zero can't blow up
    local base
    base=$(echo "if ($2 < 0.005) 0.005 else $2" | bc)
    echo "scale=2; $1 / $base" | bc
}

fail=0
echo "[perf] benchmark   hand-C     hand-C++   Rust       pgy-C          pgy-LLVM       out"
for name in arith fib forloop array match branchmix nestloop; do
    src="$(pgy_src "$name")"
    if [[ ! -f "$src" ]]; then echo "[perf] FAIL: missing benchmark $src"; fail=1; continue; fi
    write_sources "$name"

    csrc="$WORK/$name.c"
    cppsrc="$WORK/$name.cpp"; [[ -f "$cppsrc" ]] || cppsrc="$csrc"
    rssrc="$WORK/$name.rs"

    # Compile: hand-C (required) + pgy both backends (required) + optional refs.
    gcc -O2 -fwrapv -o "$WORK/${name}_cc" "$csrc" \
        || { echo "[perf] FAIL: hand-C gcc failed for $name"; fail=1; continue; }
    "$PGY" "$src" --backend=c    -o "$WORK/${name}_pc"  >/dev/null 2>&1 \
        || { echo "[perf] FAIL: pgy-C compile failed for $name"; fail=1; continue; }
    "$PGY" "$src" --backend=llvm -o "$WORK/${name}_pl"  >/dev/null 2>&1 \
        || { echo "[perf] FAIL: pgy-LLVM compile failed for $name"; fail=1; continue; }

    have_cpp_bin=0
    if [[ $HAVE_CPP -eq 1 ]]; then
        if g++ -O2 -fwrapv -std=c++17 -o "$WORK/${name}_cpp" "$cppsrc" 2>/dev/null; then
            have_cpp_bin=1
        else
            echo "[perf] FAIL: hand-C++ g++ failed for $name"; fail=1; continue
        fi
    fi
    have_rs_bin=0
    if [[ -n "$RUSTC" ]]; then
        if "$RUSTC" -O -o "$WORK/${name}_rs" "$rssrc" 2>/dev/null; then
            have_rs_bin=1
        else
            echo "[perf] FAIL: Rust rustc failed for $name"; fail=1; continue
        fi
    fi

    # Correctness: every present binary must print the SAME line.
    ref="$("$WORK/${name}_cc" 2>/dev/null)"
    for tag in pc pl $([[ $have_cpp_bin -eq 1 ]] && echo cpp) $([[ $have_rs_bin -eq 1 ]] && echo rs); do
        got="$("$WORK/${name}_${tag}" 2>/dev/null)"
        if [[ "$got" != "$ref" ]]; then
            echo "[perf] FAIL: $name output mismatch ($tag='$got' vs hand-C='$ref')"
            fail=1
        fi
    done

    t_cc=$(timeit "$WORK/${name}_cc")
    t_pc=$(timeit "$WORK/${name}_pc")
    t_pl=$(timeit "$WORK/${name}_pl")
    disp_cpp="-"; [[ $have_cpp_bin -eq 1 ]] && disp_cpp="$(timeit "$WORK/${name}_cpp")s"
    disp_rs="-";  [[ $have_rs_bin  -eq 1 ]] && disp_rs="$(timeit "$WORK/${name}_rs")s"

    r_pc=$(ratio "$t_pc" "$t_cc")
    r_pl=$(ratio "$t_pl" "$t_cc")
    printf "[perf] %-10s %-9s %-9s %-9s %ss(%sx) %ss(%sx) %s\n" \
        "$name" "${t_cc}s" "$disp_cpp" "$disp_rs" \
        "$t_pc" "$r_pc" "$t_pl" "$r_pl" "$ref"

    if [[ "$(echo "$r_pc > $C_MAX" | bc)" == "1" ]]; then
        echo "[perf] FAIL: $name pgy-C ${r_pc}x exceeds ${C_MAX}x of hand-C"
        fail=1
    fi
    if [[ "$(echo "$r_pl > $LLVM_MAX" | bc)" == "1" ]]; then
        echo "[perf] WARN: $name pgy-LLVM ${r_pl}x exceeds ${LLVM_MAX}x (LLVM opt-pipeline tuning)"
    fi
done

# generic stays a backend-equality construct check (its monomorphized 32-bit
# accumulator has no single-line C/C++/Rust idiom worth pinning here).
gsrc="$ROOT_DIR/benchmarks/bench_generic.pgy"
if [[ -f "$gsrc" ]]; then
    "$PGY" "$gsrc" --backend=c    -o "$WORK/gen_c"  >/dev/null 2>&1 \
        && "$PGY" "$gsrc" --backend=llvm -o "$WORK/gen_l" >/dev/null 2>&1 \
        && {
            og="$("$WORK/gen_c" 2>/dev/null)"; ol="$("$WORK/gen_l" 2>/dev/null)"
            if [[ "$og" != "$ol" ]]; then
                echo "[perf] FAIL: generic backend output mismatch ('$og' vs '$ol')"; fail=1
            else
                echo "[perf] generic (backend equality only): out=$og"
            fi
        } || { echo "[perf] FAIL: generic compile failed"; fail=1; }
fi

if [[ $fail -eq 0 ]]; then
    echo "[perf] ok: all languages agree per benchmark; pgy-C within ${C_MAX}x of hand-C"
else
    exit 1
fi
