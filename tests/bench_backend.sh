#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: tests/bench_backend.sh <source.pgy> [dev|release]" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

SOURCE="$1"
PROFILE="${2:-dev}"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
PGY_BIN="${PGY_BIN:-${TMP_BASE%/}/pgy-PergyraLang-bin/pgy}"
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
  PGY_BIN="${PGY_BIN}.exe"
fi

if [[ ! -x "$PGY_BIN" ]]; then
  echo "bench: PGY_BIN not executable: $PGY_BIN" >&2
  exit 1
fi
if ! pgy_binary_is_runnable_here "$PGY_BIN"; then
  echo "bench: PGY_BIN is not runnable on this host: $PGY_BIN" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_bench.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

pgy_bench_now_ms() {
  local value=""

  value="$(date +%s%N 2>/dev/null || true)"
  if [[ "$value" =~ ^[0-9]+$ ]]; then
    printf '%s\n' "$((value / 1000000))"
    return 0
  fi

  value="$(date +%s 2>/dev/null || true)"
  if [[ "$value" =~ ^[0-9]+$ ]]; then
    printf '%s\n' "$((value * 1000))"
    return 0
  fi

  printf '0\n'
}

run_one() {
  local backend="$1"
  local label="$2"
  local source_arg=""
  local start_ms=0
  local end_ms=0
  local elapsed_ms=0
  local rc=0

  source_arg="$(pgy_path_for_compiler "$PGY_BIN" "$SOURCE")"
  start_ms="$(pgy_bench_now_ms)"
  set +e
  "$PGY_BIN" "$source_arg" "--backend=${backend}" "--opt=${PROFILE}" --run \
    >"$WORK_DIR/pgy_bench_${backend}.out" 2>"$WORK_DIR/pgy_bench_${backend}.err"
  rc=$?
  set -e
  end_ms="$(pgy_bench_now_ms)"
  elapsed_ms=$((end_ms - start_ms))
  printf '%s: %d.%03d sec\n' "$label" "$((elapsed_ms / 1000))" "$((elapsed_ms % 1000))"
  if [[ "$rc" -ne 0 ]]; then
    echo "bench: ${backend} backend failed with exit ${rc}" >&2
    cat "$WORK_DIR/pgy_bench_${backend}.err" >&2
    cat "$WORK_DIR/pgy_bench_${backend}.out" >&2
    exit "$rc"
  fi
}

run_one c "c ${PROFILE}"
run_one llvm "llvm ${PROFILE}"
