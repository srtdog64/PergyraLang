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

grep -a -q '\[type-res-stats\] metadata:' "$log" || {
  echo "missing graph-backed metadata inventory" >&2
  exit 1
}

graph_skips="$(
  grep -a '\[type-res-stats\] stage-graph-backed:' "$log" \
    | sed -E 's/.*skips=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_entries="$(
  grep -a '\[type-res-stats\] metadata:' "$log" \
    | sed -E 's/.*entries=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_hits="$(
  grep -a '\[type-res-stats\] metadata:' "$log" \
    | sed -E 's/.*hits=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_owned="$(
  grep -a '\[type-res-stats\] metadata:' "$log" \
    | sed -E 's/.*owned=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

if [ "$graph_skips" -le 0 ]; then
  echo "graph-backed stage skip inventory regressed to zero" >&2
  exit 1
fi

if [ "$metadata_entries" -le 0 ]; then
  echo "graph-backed metadata inventory regressed to zero entries" >&2
  exit 1
fi

if [ "$metadata_hits" -le 0 ]; then
  echo "graph-backed metadata reuse regressed to zero hits" >&2
  exit 1
fi

if [ "$metadata_owned" -le 0 ]; then
  echo "graph-backed stable constructed metadata regressed to zero owned entries" >&2
  exit 1
fi

grep -a -q 'topo_ok=1' "$log" || {
  echo "DAG topo validation did not report topo_ok=1" >&2
  exit 1
}

echo "[type-resolution-dag] graph stats and metadata reuse present (graph-backed skips=$graph_skips metadata_entries=$metadata_entries metadata_owned=$metadata_owned metadata_hits=$metadata_hits)"
