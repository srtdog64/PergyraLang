#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
CC="${CC:-cc}"
RUNS="${PGY_GUARD_AMORTIZATION_RUNS:-7}"
WARMUP="${PGY_GUARD_AMORTIZATION_WARMUP:-1}"
MAX_RATIO="${PGY_GUARD_AMORTIZATION_MAX_RATIO:-0.85}"
ITERATIONS="${PGY_GUARD_ITERATIONS:-50000000}"

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

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_guard_amortization.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ -n "${PGY_BIN:-}" ]]; then
  if [[ ! -x "$PGY_BIN" ]]; then
    echo "[guard-amortization] PGY_BIN is not executable: $PGY_BIN" >&2
    exit 1
  fi
  PLAIN_C="$WORK_DIR/pin_read_view_block.c"
  SECURE_C="$WORK_DIR/pin_secure_param_read_view_block.c"
  "$PGY_BIN" "$ROOT_DIR/tests/cases/backend_compare/pin_read_view_block/main.pgy" \
    --backend=c --emit-c -o "$PLAIN_C" >/dev/null
  grep -Fq "__pgy_mir_pin_slot_" "$PLAIN_C"
  grep -Fq "PgyPinnedSlotView_Int" "$PLAIN_C"
  if grep -Eq "pgy_pin_(read|write)_[A-Za-z0-9_]+\\(|pgy_unpin_[A-Za-z0-9_]+\\(" "$PLAIN_C"; then
    echo "[guard-amortization] plain MIR pin emitted runtime pin/unpin call" >&2
    exit 1
  fi
  "$PGY_BIN" "$ROOT_DIR/tests/cases/backend_compare/pin_secure_param_read_view_block/main.pgy" \
    --backend=c --emit-c -o "$SECURE_C" >/dev/null
  grep -Fq "pgy_secure_pin_read_Int" "$SECURE_C"
  grep -Fq "pgy_secure_unpin_Int" "$SECURE_C"
fi

EXE="$WORK_DIR/guard_amortization"
"$CC" -O3 -std=c11 -DPGY_GUARD_ITERATIONS="$ITERATIONS" \
  "$ROOT_DIR/benchmarks/perf_guard_amortization.c" -o "$EXE"

"$EXE" >"$WORK_DIR/check.out"
grep -Fq "same=1" "$WORK_DIR/check.out"
grep -Fq "invalid=-1" "$WORK_DIR/check.out"

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
  echo "[guard-amortization] date command cannot provide elapsed time" >&2
  return 1
}

measure_avg() {
  local mode="$1"
  local log="$2"
  local start_ns
  local end_ns
  : >"$log"
  for ((i = 0; i < RUNS; i++)); do
    start_ns="$(pgy_now_ns)"
    "$EXE" "$mode" >/dev/null
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

PER_ACCESS_AVG="$(measure_avg per "$WORK_DIR/per.time")"
PREFLIGHT_AVG="$(measure_avg pre "$WORK_DIR/pre.time")"
RATIO="$(awk -v pre="$PREFLIGHT_AVG" -v per="$PER_ACCESS_AVG" \
  'BEGIN { if (per <= 0) exit 1; printf("%.3f\n", pre / per) }')"

echo "[guard-amortization] fixture=slot_read iterations=${ITERATIONS}"
echo "[guard-amortization] per_access_guard_avg_s=${PER_ACCESS_AVG}"
echo "[guard-amortization] preflight_view_avg_s=${PREFLIGHT_AVG}"
echo "[guard-amortization] preflight_over_per_access_ratio=${RATIO}"

awk -v ratio="$RATIO" -v max="$MAX_RATIO" '
  BEGIN {
    if (ratio > max) {
      printf("[guard-amortization] ratio %.3f exceeds max %.3f\n", ratio, max) > "/dev/stderr"
      exit 1
    }
  }
'
