#!/usr/bin/env bash
set -euo pipefail

bin="${SEMANTIC_TEST_BIN:-}"
if [ -z "$bin" ]; then
  echo "[semantic-fixture-isolation] SEMANTIC_TEST_BIN not set; skipping executable fixture-isolation smoke"
  exit 0
fi

case "$bin" in
  /*)
    ;;
  */*)
    bin="$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")"
    ;;
  *)
    resolved_bin="$(command -v "$bin" || true)"
    if [ -z "$resolved_bin" ]; then
      echo "SEMANTIC_TEST_BIN is not executable or not on PATH: $bin" >&2
      exit 2
    fi
    bin="$resolved_bin"
    ;;
esac

log_a="$(mktemp)"
log_b="$(mktemp)"
cleanup() {
  rm -f "$log_a" "$log_b"
}
trap cleanup EXIT

"$bin" >"$log_a" 2>&1 &
pid_a=$!
"$bin" >"$log_b" 2>&1 &
pid_b=$!

status_a=0
status_b=0
wait "$pid_a" || status_a=$?
wait "$pid_b" || status_b=$?

if [ "$status_a" -ne 0 ] || [ "$status_b" -ne 0 ]; then
  echo "[semantic-fixture-isolation] concurrent semantic binary run failed: a=$status_a b=$status_b" >&2
  tail -40 "$log_a" >&2 || true
  tail -40 "$log_b" >&2 || true
  exit 1
fi

grep -a -q '=== Results: .* 0 failed ===' "$log_a" || {
  echo "[semantic-fixture-isolation] first semantic run did not report zero failures" >&2
  tail -40 "$log_a" >&2 || true
  exit 1
}

grep -a -q '=== Results: .* 0 failed ===' "$log_b" || {
  echo "[semantic-fixture-isolation] second semantic run did not report zero failures" >&2
  tail -40 "$log_b" >&2 || true
  exit 1
}

echo "[semantic-fixture-isolation] concurrent semantic binary fixture isolation ok"
