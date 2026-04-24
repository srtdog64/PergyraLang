#!/usr/bin/env bash
set -euo pipefail

bin="${SEMANTIC_TEST_BIN:-}"
if [ -z "$bin" ]; then
  echo "SEMANTIC_TEST_BIN is required" >&2
  exit 2
fi

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

PGY_TYPE_RES_STATS=1 "$bin" >"$log" 2>&1

grep -a -q '\[type-res-stats\] nodes=' "$log" || {
  echo "missing DAG node/edge stats" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-legacy-resolve:' "$log" || {
  echo "missing stage legacy resolver inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-legacy-family:' "$log" || {
  echo "missing stage legacy resolver family inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-graph-backed:' "$log" || {
  echo "missing graph-backed stage skip inventory" >&2
  exit 1
}

graph_skips="$(
  grep -a '\[type-res-stats\] stage-graph-backed:' "$log" \
    | sed -E 's/.*skips=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

if [ "$graph_skips" -le 0 ]; then
  echo "graph-backed stage skip inventory regressed to zero" >&2
  exit 1
fi

grep -a -q 'topo_ok=1' "$log" || {
  echo "DAG topo validation did not report topo_ok=1" >&2
  exit 1
}

echo "[type-resolution-dag] graph stats and legacy fallback inventory present (graph-backed skips=$graph_skips)"
