#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
CC="${CC:-cc}"
RUNS="${PGY_GUARD_AMORTIZATION_RUNS:-7}"
WARMUP="${PGY_GUARD_AMORTIZATION_WARMUP:-1}"
MAX_RATIO="${PGY_GUARD_AMORTIZATION_MAX_RATIO:-0.85}"
CACHE_MAX_RATIO="${PGY_GUARD_CACHE_MAX_RATIO:-0.85}"
ITERATIONS="${PGY_GUARD_ITERATIONS:-50000000}"
UNAME_S="$(uname -s 2>/dev/null || echo unknown)"

pgy_path_for_c_compiler() {
  local path="$1"
  case "$UNAME_S" in
    MINGW*|MSYS*|CYGWIN*)
      pgy_path_for_windows_tool "$path"
      ;;
    *)
      printf '%s\n' "$path"
      ;;
  esac
}

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "[guard-amortization] C compiler not found: $CC" >&2
  exit 1
fi
if ! command -v awk >/dev/null 2>&1; then
  echo "[guard-amortization] awk is required" >&2
  exit 1
fi
if (( RUNS <= WARMUP )); then
  echo "[guard-amortization] RUNS must be greater than WARMUP" >&2
  exit 1
fi

require_source_term() {
  local path="$1"
  local term="$2"
  if ! grep -Fq "$term" "$ROOT_DIR/$path"; then
    echo "[guard-amortization] missing source gate: $path :: $term" >&2
    exit 1
  fi
}

require_source_term "src/compiler/mir_cfg_contract_pin.h" \
  "mir_block_has_pin_guard_amortization_region"
require_source_term "src/compiler/mir_cfg_contract_pin.c" \
  "mir_block_has_pin_cleanup_edge(block)"
require_source_term "src/codegen/transpiler_mir_pin_emit.c" \
  "transpiler_emit_mir_plain_pin_preflight_local"
require_source_term "src/codegen/transpiler_mir_pin_emit.c" \
  "PgySlot_%s *%s = %s;"
require_source_term "src/codegen/llvm_mir_pin_region.c" \
  "mir_block_has_pin_guard_amortization_region(block)"
require_source_term "benchmarks/perf_guard_amortization.c" \
  "run_repeated_preflight_view"
require_source_term "benchmarks/perf_guard_amortization.c" \
  "repeat-pre"
require_source_term "benchmarks/perf_guard_amortization.c" \
  "time-repeat-pre"

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_guard_amortization.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ -n "${PGY_BIN:-}" ]]; then
  PGY_BIN="$(pgy_select_optional_exe_binary "$PGY_BIN")"
  if [[ ! -x "$PGY_BIN" ]]; then
    echo "[guard-amortization] PGY_BIN is not executable: $PGY_BIN" >&2
    exit 1
  fi
  pgy_require_runnable_binary_here "guard-amortization" "$PGY_BIN"
  PLAIN_C="$WORK_DIR/pin_read_view_block.c"
  SECURE_C="$WORK_DIR/pin_secure_param_read_view_block.c"
  PLAIN_SRC_ARG="$(pgy_path_for_compiler "$PGY_BIN" \
    "$ROOT_DIR/tests/cases/backend_compare/pin_read_view_block/main.pgy")"
  SECURE_SRC_ARG="$(pgy_path_for_compiler "$PGY_BIN" \
    "$ROOT_DIR/tests/cases/backend_compare/pin_secure_param_read_view_block/main.pgy")"
  PLAIN_C_ARG="$(pgy_path_for_compiler "$PGY_BIN" "$PLAIN_C")"
  SECURE_C_ARG="$(pgy_path_for_compiler "$PGY_BIN" "$SECURE_C")"
  "$PGY_BIN" "$PLAIN_SRC_ARG" \
    --backend=c --emit-c -o "$PLAIN_C_ARG" >/dev/null
  grep -Fq "__pgy_mir_pin_slot_" "$PLAIN_C"
  grep -Fq "PgyPinnedSlotView_Int" "$PLAIN_C"
  if grep -Eq "pgy_pin_(read|write)_[A-Za-z0-9_]+\\(|pgy_unpin_[A-Za-z0-9_]+\\(" "$PLAIN_C"; then
    echo "[guard-amortization] plain MIR pin emitted runtime pin/unpin call" >&2
    exit 1
  fi
  "$PGY_BIN" "$SECURE_SRC_ARG" \
    --backend=c --emit-c -o "$SECURE_C_ARG" >/dev/null
  grep -Fq "pgy_secure_pin_read_Int" "$SECURE_C"
  grep -Fq "pgy_secure_unpin_Int" "$SECURE_C"
fi

case "$UNAME_S" in
  MINGW*|MSYS*|CYGWIN*) EXE="$WORK_DIR/guard_amortization.exe" ;;
  *) EXE="$WORK_DIR/guard_amortization" ;;
esac
BENCH_SRC_ARG="$(pgy_path_for_c_compiler \
  "$ROOT_DIR/benchmarks/perf_guard_amortization.c")"
EXE_ARG="$(pgy_path_for_c_compiler "$EXE")"
"$CC" -O3 -std=c11 -DPGY_GUARD_ITERATIONS="$ITERATIONS" \
  "$BENCH_SRC_ARG" -o "$EXE_ARG"

"$EXE" >"$WORK_DIR/check.out"
grep -Fq "same=1" "$WORK_DIR/check.out"
grep -Fq "repeated_same=1" "$WORK_DIR/check.out"
grep -Fq "invalid=-1" "$WORK_DIR/check.out"

measure_mode_seconds() {
  local mode="$1"
  local output
  local seconds
  output="$("$EXE" "time-$mode")"
  seconds="$(printf '%s\n' "$output" \
    | awk -F= '$1 == "seconds" { print $2; found = 1; exit }
               END { if (!found) exit 1 }')"
  if [[ ! "$seconds" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    echo "[guard-amortization] invalid internal timing for mode $mode" >&2
    printf '%s\n' "$output" >&2
    exit 1
  fi
  printf '%s\n' "$seconds"
}

MEASURE_LOG="$WORK_DIR/paired.time"
: >"$MEASURE_LOG"
for ((i = 0; i < RUNS; i++)); do
  per_seconds="$(measure_mode_seconds per)"
  repeated_preflight_seconds="$(measure_mode_seconds repeat-pre)"
  preflight_seconds="$(measure_mode_seconds pre)"
  printf '%s %s %s\n' "$per_seconds" "$repeated_preflight_seconds" \
    "$preflight_seconds" >>"$MEASURE_LOG"
done

read -r PER_ACCESS_AVG REPEATED_PREFLIGHT_AVG PREFLIGHT_AVG RATIO \
  CACHE_RATIO RATIO_BEST CACHE_RATIO_BEST < <(
  awk -v warmup="$WARMUP" '
    NR > warmup {
      per = $1
      repeat = $2
      pre = $3
      if (per <= 0 || repeat <= 0)
        exit 1
      ratio = pre / per
      cache_ratio = pre / repeat
      sum_per += per
      sum_repeat += repeat
      sum_pre += pre
      sum_ratio += ratio
      sum_cache_ratio += cache_ratio
      count += 1
      if (count == 1 || ratio < best_ratio)
        best_ratio = ratio
      if (count == 1 || cache_ratio < best_cache_ratio)
        best_cache_ratio = cache_ratio
    }
    END {
      if (count == 0)
        exit 1
      printf("%.6f %.6f %.6f %.3f %.3f %.3f %.3f\n",
             sum_per / count,
             sum_repeat / count,
             sum_pre / count,
             sum_ratio / count,
             sum_cache_ratio / count,
             best_ratio,
             best_cache_ratio)
    }
  ' "$MEASURE_LOG"
)

if [[ -z "$PER_ACCESS_AVG" || -z "$CACHE_RATIO_BEST" ]]; then
  echo "[guard-amortization] failed to compute paired timing metrics" >&2
  exit 1
fi

echo "[guard-amortization] fixture=slot_read iterations=${ITERATIONS}"
echo "[guard-amortization] per_access_guard_avg_s=${PER_ACCESS_AVG}"
echo "[guard-amortization] repeated_preflight_avg_s=${REPEATED_PREFLIGHT_AVG}"
echo "[guard-amortization] preflight_view_avg_s=${PREFLIGHT_AVG}"
echo "[guard-amortization] preflight_over_per_access_ratio=${RATIO}"
echo "[guard-amortization] cached_preflight_over_repeated_preflight_ratio=${CACHE_RATIO}"
echo "[guard-amortization] preflight_over_per_access_best_ratio=${RATIO_BEST}"
echo "[guard-amortization] cached_preflight_over_repeated_preflight_best_ratio=${CACHE_RATIO_BEST}"

awk -v ratio="$RATIO_BEST" -v max="$MAX_RATIO" '
  BEGIN {
    if (ratio > max) {
      printf("[guard-amortization] best ratio %.3f exceeds max %.3f\n", ratio, max) > "/dev/stderr"
      exit 1
    }
  }
'

awk -v ratio="$CACHE_RATIO_BEST" -v max="$CACHE_MAX_RATIO" '
  BEGIN {
    if (ratio > max) {
      printf("[guard-amortization] best cache ratio %.3f exceeds max %.3f\n", ratio, max) > "/dev/stderr"
      exit 1
    }
  }
'
