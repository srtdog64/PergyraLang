#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: tests/bench_backend.sh <source.pgy> [dev|release]" >&2
  exit 1
fi

SOURCE="$1"
PROFILE="${2:-dev}"
PGY_BIN="${PGY_BIN:-/tmp/pgy-PergyraLang-bin/pgy}"
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
  PGY_BIN="${PGY_BIN}.exe"
fi

if [[ ! -x "$PGY_BIN" ]]; then
  echo "bench: PGY_BIN not executable: $PGY_BIN" >&2
  exit 1
fi

run_one() {
  local backend="$1"
  local label="$2"
  /usr/bin/time -f "${label}: %e sec %M KB" \
    "$PGY_BIN" "$SOURCE" "--backend=${backend}" "--opt=${PROFILE}" --run \
    >/tmp/pgy_bench_${backend}.out 2>/tmp/pgy_bench_${backend}.time
  cat /tmp/pgy_bench_${backend}.time
}

run_one c "c ${PROFILE}"
run_one llvm "llvm ${PROFILE}"
