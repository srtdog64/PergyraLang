#!/usr/bin/env bash
#
# Coq-backed fixed-width multiplication probe.
#
# The Coq proof checks the current Int/CheckedMul contract and the executable
# leg measures the same safe workload through hand C, optional C++, Pergyra-C,
# and Pergyra-LLVM.  Timing is evidence about this checkout and host only; it
# is not a proof of the transpose lower-bound conjecture.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY_BIN="${PGY_BIN:-${BIN_DIR:-$ROOT_DIR/bin}/pgy}"
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
    PGY_BIN="${PGY_BIN}.exe"
fi

fail() { echo "[pergyra-mul-coq] FAIL: $*" >&2; exit 1; }

if [[ ! -x "$PGY_BIN" ]]; then
    fail "Pergyra compiler is not executable: $PGY_BIN"
fi
if ! command -v gcc >/dev/null 2>&1; then
    fail "gcc is required for the baseline"
fi
if ! command -v python >/dev/null 2>&1 && ! command -v python3 >/dev/null 2>&1; then
    fail "python or python3 is required for stable timing"
fi

if command -v rocq >/dev/null 2>&1; then
    COQ_COMPILE=(rocq compile)
    COQ_CHECK=rocqchk
elif command -v coqc >/dev/null 2>&1; then
    COQ_COMPILE=(coqc)
    COQ_CHECK=coqchk
else
    fail "rocq/coqc is required for the proof leg"
fi

PROOF_REL="docs/semantics/proofs/PergyraMulCost.v"
(
    cd "$ROOT_DIR"
    "${COQ_COMPILE[@]}" "$PROOF_REL"
)
(
    cd "$ROOT_DIR/docs/semantics/proofs"
    "$COQ_CHECK" -silent PergyraMulCost
)
echo "[pergyra-mul-coq] Coq compile + kernel check: ok"

WORK_DIR="$(mktemp -d "${TMPDIR:-${TEMP:-/tmp}}/pgy_mul_coq.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat >"$WORK_DIR/hand_mul.c" <<'EOF'
#include <stdio.h>
int main(void) {
    int acc = 0;
    int i = 0;
    while (i < 100000000) {
        int x = i % 1000;
        int product = x * 31;
        acc += product;
        if (acc >= 1000000000) acc -= 1000000000;
        i++;
    }
    printf("mul=%d\n", acc);
    return 0;
}
EOF

cat >"$WORK_DIR/hand_mul.cpp" <<'EOF'
#include <cstdio>
int main() {
    int acc = 0;
    int i = 0;
    while (i < 100000000) {
        int x = i % 1000;
        int product = x * 31;
        acc += product;
        if (acc >= 1000000000) acc -= 1000000000;
        i++;
    }
    std::printf("mul=%d\n", acc);
}
EOF

gcc -O2 -fwrapv -o "$WORK_DIR/hand_c.exe" "$WORK_DIR/hand_mul.c"
if command -v g++ >/dev/null 2>&1; then
    g++ -O2 -fwrapv -std=c++17 -o "$WORK_DIR/hand_cpp.exe" "$WORK_DIR/hand_mul.cpp"
fi

SOURCE="$ROOT_DIR/benchmarks/bench_checked_mul.pgy"
[[ -f "$SOURCE" ]] || fail "missing benchmark source: $SOURCE"
SOURCE_ARG="$(pgy_path_for_compiler "$PGY_BIN" "$SOURCE")"
"$PGY_BIN" "$SOURCE_ARG" --backend=c -o "$(pgy_path_for_compiler "$PGY_BIN" "$WORK_DIR/pgy_c.exe")" \
    >/dev/null
"$PGY_BIN" "$SOURCE_ARG" --backend=llvm -o "$(pgy_path_for_compiler "$PGY_BIN" "$WORK_DIR/pgy_llvm.exe")" \
    >/dev/null

PYTHON_BIN=python
command -v "$PYTHON_BIN" >/dev/null 2>&1 || PYTHON_BIN=python3
measure() {
    local exe="$1"
    local out="$2"
    "$PYTHON_BIN" - "$exe" "$out" <<'PY'
import pathlib
import subprocess
import sys
import time

exe, out = sys.argv[1:]
start = time.perf_counter_ns()
completed = subprocess.run([exe], capture_output=True, text=True)
elapsed_ms = (time.perf_counter_ns() - start) / 1_000_000
pathlib.Path(out).write_text(completed.stdout, encoding="utf-8")
if completed.returncode != 0:
    sys.stderr.write(completed.stderr)
    raise SystemExit(completed.returncode)
print(f"{elapsed_ms:.3f}")
PY
}

declare -a LABELS=(hand-c pgy-c pgy-llvm)
declare -a EXES=("$WORK_DIR/hand_c.exe" "$WORK_DIR/pgy_c.exe" "$WORK_DIR/pgy_llvm.exe")
declare -a TIMES=()
if [[ -x "$WORK_DIR/hand_cpp.exe" ]]; then
    LABELS+=(hand-cpp)
    EXES+=("$WORK_DIR/hand_cpp.exe")
fi

reference=""
hand_ms=""
for index in "${!LABELS[@]}"; do
    label="${LABELS[$index]}"
    exe="${EXES[$index]}"
    out="$WORK_DIR/$label.out"
    # Best-of-three avoids treating one scheduler tick as a compiler result.
    best=999999999
    for _ in 1 2 3; do
        elapsed="$(measure "$exe" "$out")"
        best="$(awk -v a="$best" -v b="$elapsed" 'BEGIN { print (b < a) ? b : a }')"
    done
    got="$(tr -d '\r\n' <"$out")"
    if [[ -z "$reference" ]]; then
        reference="$got"
    elif [[ "$got" != "$reference" ]]; then
        fail "$label output '$got' != reference '$reference'"
    fi
    TIMES+=("$best")
    printf '[pergyra-mul-coq] %-10s %sms output=%s\n' "$label" "$best" "$got"
    if [[ "$label" == hand-c ]]; then hand_ms="$best"; fi
done

echo "[pergyra-mul-coq] fixed-width result: current Int has no n-bit asymptotic family"
echo "[pergyra-mul-coq] timing is an executable observation, not a proof of O(n log n)"
if [[ -n "$hand_ms" ]]; then
    for index in "${!LABELS[@]}"; do
        label="${LABELS[$index]}"
        [[ "$label" == hand-c ]] && continue
        elapsed="${TIMES[$index]}"
        ratio="$(awk -v a="$elapsed" -v b="$hand_ms" 'BEGIN { printf "%.2fx", a / b }')"
        echo "[pergyra-mul-coq] ratio $label/hand-c = $ratio"
    done
fi
