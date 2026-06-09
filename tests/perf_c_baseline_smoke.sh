#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
PGY_BIN="${PGY_BIN:-${ROOT_DIR}/bin/pgy}"
CC="${CC:-cc}"
RUNS="${PGY_C_BASELINE_RUNS:-7}"
WARMUP="${PGY_C_BASELINE_WARMUP:-2}"
MAX_RATIO="${PGY_C_BASELINE_MAX_RATIO:-4.0}"

if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
  PGY_BIN="${PGY_BIN}.exe"
fi

if [[ ! -x "$PGY_BIN" ]]; then
  echo "[perf-c-baseline] PGY_BIN not executable: $PGY_BIN" >&2
  exit 1
fi
if ! pgy_binary_is_runnable_here "$PGY_BIN"; then
  echo "[perf-c-baseline] PGY_BIN is not runnable on this host: $PGY_BIN" >&2
  exit 1
fi

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "[perf-c-baseline] C compiler not found: $CC" >&2
  exit 1
fi

if ! command -v awk >/dev/null 2>&1; then
  echo "[perf-c-baseline] awk is required" >&2
  exit 1
fi

if (( RUNS <= WARMUP )); then
  echo "[perf-c-baseline] PGY_C_BASELINE_RUNS must be greater than PGY_C_BASELINE_WARMUP" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_c_baseline.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

PGY_SOURCE="${ROOT_DIR}/tests/perf/c_baseline_arith_loop.pgy"
C_SOURCE="${ROOT_DIR}/tests/perf/c_baseline_arith_loop.c"
PGY_EXE="${WORK_DIR}/arith_loop_pgy"
C_EXE="${WORK_DIR}/arith_loop_c"
PGY_C_OUT="${WORK_DIR}/arith_loop_pgy.c"
PGY_SOURCE_ARG="$(pgy_path_for_compiler "$PGY_BIN" "$PGY_SOURCE")"
PGY_EXE_ARG="$(pgy_path_for_compiler "$PGY_BIN" "$PGY_EXE")"
PGY_C_OUT_ARG="$(pgy_path_for_compiler "$PGY_BIN" "$PGY_C_OUT")"

"$PGY_BIN" "$PGY_SOURCE_ARG" --backend=c --emit-c -o "$PGY_C_OUT_ARG" >/dev/null
if grep -Fq "pgy_checked_mod_i32_export" "$PGY_C_OUT"; then
  echo "[perf-c-baseline] constant nonzero modulo regressed to checked helper" >&2
  exit 1
fi
if grep -Fq "pgy_checked_div_i32_export" "$PGY_C_OUT"; then
  echo "[perf-c-baseline] constant nonzero division regressed to checked helper" >&2
  exit 1
fi
"$PGY_BIN" "$PGY_SOURCE_ARG" --backend=c --opt=release -o "$PGY_EXE_ARG" >/dev/null
"$CC" -O3 -std=c11 "$C_SOURCE" -o "$C_EXE"

"$PGY_EXE" >"${WORK_DIR}/pgy.out"
"$C_EXE" >"${WORK_DIR}/c.out"

if ! cmp -s "${WORK_DIR}/pgy.out" "${WORK_DIR}/c.out"; then
  echo "[perf-c-baseline] output mismatch" >&2
  echo "pgy: $(cat "${WORK_DIR}/pgy.out")" >&2
  echo "c:   $(cat "${WORK_DIR}/c.out")" >&2
  exit 1
fi

pgy_now_ns() {
  local value
  value="$(date +%s%N 2>/dev/null || true)"
  if [[ "$value" =~ ^[0-9]+$ ]]; then
    printf '%s\n' "$value"
    return 0
  fi
  value="$(date +%s 2>/dev/null || true)"
  if [[ "$value" =~ ^[0-9]+$ ]]; then
    printf '%s000000000\n' "$value"
    return 0
  fi
  echo "[perf-c-baseline] date command cannot provide elapsed time" >&2
  return 1
}

measure_avg() {
  local exe="$1"
  local log="$2"
  local start_ns
  local end_ns
  : >"$log"
  for ((i = 0; i < RUNS; i++)); do
    start_ns="$(pgy_now_ns)"
    "$exe" >/dev/null
    end_ns="$(pgy_now_ns)"
    awk -v start="$start_ns" -v end="$end_ns" \
      'BEGIN { printf("%.9f\n", (end - start) / 1000000000.0) }' >>"$log"
  done
  awk -v warmup="$WARMUP" '
    NR > warmup { sum += $1; count += 1 }
    END {
      if (count == 0)
        exit 1
      printf("%.6f\n", sum / count)
    }
  ' "$log"
}

PGY_AVG="$(measure_avg "$PGY_EXE" "${WORK_DIR}/pgy.time")"
C_AVG="$(measure_avg "$C_EXE" "${WORK_DIR}/c.time")"
RATIO="$(awk -v pgy="$PGY_AVG" -v c="$C_AVG" 'BEGIN { if (c <= 0) exit 1; printf("%.3f\n", pgy / c) }')"
VERDICT="$(awk -v ratio="$RATIO" 'BEGIN { print (ratio < 1.0) ? "pgy-generated-c-faster" : "native-c-faster" }')"

echo "[perf-c-baseline] fixture=arith_loop output=$(tr -d '\r\n' <"${WORK_DIR}/pgy.out")"
echo "[perf-c-baseline] pgy_generated_c_avg_s=${PGY_AVG}"
echo "[perf-c-baseline] native_c_avg_s=${C_AVG}"
echo "[perf-c-baseline] pgy_over_c_ratio=${RATIO}"
echo "[perf-c-baseline] verdict=${VERDICT}"

awk -v ratio="$RATIO" -v max="$MAX_RATIO" '
  BEGIN {
    if (ratio > max) {
      printf("[perf-c-baseline] ratio %.3f exceeds max %.3f\n", ratio, max) > "/dev/stderr"
      exit 1
    }
  }
'
