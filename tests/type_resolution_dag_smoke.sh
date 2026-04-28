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

grep -a -q '\[type-res-stats\] metadata-fallback:' "$log" || {
  echo "missing graph-backed metadata fallback family inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] metadata-fallback-named:' "$log" || {
  echo "missing graph-backed metadata named fallback inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-alias-fallback:' "$log" || {
  echo "missing alias fallback resolution inventory" >&2
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

materializer_fallbacks="$(
  grep -a '\[type-res-stats\] metadata:' "$log" \
    | sed -E 's/.*materializer_fallbacks=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_fallback_sum="$(
  grep -a '\[type-res-stats\] metadata-fallback:' "$log" \
    | sed -E 's/.*named=([0-9]+).*generic_named=([0-9]+).*compound=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $1 + $2 + $3 + $4 } END { print total + 0 }'
)"

metadata_fallback_named="$(
  grep -a '\[type-res-stats\] metadata-fallback:' "$log" \
    | sed -E 's/.*metadata-fallback: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_fallback_generic_named="$(
  grep -a '\[type-res-stats\] metadata-fallback:' "$log" \
    | sed -E 's/.*metadata-fallback: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $2 } END { print total + 0 }'
)"

metadata_fallback_compound="$(
  grep -a '\[type-res-stats\] metadata-fallback:' "$log" \
    | sed -E 's/.*metadata-fallback: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $3 } END { print total + 0 }'
)"

metadata_fallback_other="$(
  grep -a '\[type-res-stats\] metadata-fallback:' "$log" \
    | sed -E 's/.*metadata-fallback: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $4 } END { print total + 0 }'
)"

metadata_named_detail_sum="$(
  grep -a '\[type-res-stats\] metadata-fallback-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $1 + $2 + $3 + $4 + $5 } END { print total + 0 }'
)"

metadata_named_builtin_shell="$(
  grep -a '\[type-res-stats\] metadata-fallback-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_named_generic_class="$(
  grep -a '\[type-res-stats\] metadata-fallback-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $2 } END { print total + 0 }'
)"

metadata_named_alias="$(
  grep -a '\[type-res-stats\] metadata-fallback-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $3 } END { print total + 0 }'
)"

metadata_named_non_class_symbol="$(
  grep -a '\[type-res-stats\] metadata-fallback-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $4 } END { print total + 0 }'
)"

metadata_named_missing_symbol="$(
  grep -a '\[type-res-stats\] metadata-fallback-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $5 } END { print total + 0 }'
)"

legacy_alias="$(
  grep -a '\[type-res-stats\] stage-legacy-family:' "$log" \
    | sed -E 's/.*alias=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

legacy_non_alias="$(
  grep -a '\[type-res-stats\] stage-legacy-family:' "$log" \
    | sed -E 's/.*generic_contract=([0-9]+).*signature=([0-9]+).*ability_consumer=([0-9]+).*domain_contract=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $1 + $2 + $3 + $4 + $5 } END { print total + 0 }'
)"

alias_materialized="$(
  grep -a '\[type-res-stats\] stage-alias:' "$log" \
    | sed -E 's/.*materialized=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_diagnostic_fallback="$(
  grep -a '\[type-res-stats\] stage-alias:' "$log" \
    | sed -E 's/.*diagnostic_fallback=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_fallback_resolver_calls="$(
  grep -a '\[type-res-stats\] stage-alias-fallback:' "$log" \
    | sed -E 's/.*resolver_calls=([0-9]+) resolved=.*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_fallback_resolved="$(
  grep -a '\[type-res-stats\] stage-alias-fallback:' "$log" \
    | sed -E 's/.* resolved=([0-9]+) unresolved=.*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_fallback_unresolved="$(
  grep -a '\[type-res-stats\] stage-alias-fallback:' "$log" \
    | sed -E 's/.* unresolved=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

if [ "$graph_skips" -le 0 ]; then
  echo "graph-backed stage skip inventory regressed to zero" >&2
  exit 1
fi

if [ "$graph_skips" -lt 3000 ]; then
  echo "graph-backed stage skip inventory regressed below beta floor: $graph_skips < 3000" >&2
  exit 1
fi

if [ "$metadata_entries" -le 0 ]; then
  echo "graph-backed metadata inventory regressed to zero entries" >&2
  exit 1
fi

if [ "$metadata_entries" -lt 3300 ]; then
  echo "graph-backed metadata inventory regressed below beta floor: $metadata_entries < 3300" >&2
  exit 1
fi

if [ "$metadata_hits" -le 0 ]; then
  echo "graph-backed metadata reuse regressed to zero hits" >&2
  exit 1
fi

if [ "$metadata_hits" -lt 4900 ]; then
  echo "graph-backed metadata reuse regressed below beta floor: $metadata_hits < 4900" >&2
  exit 1
fi

if [ "$metadata_owned" -le 0 ]; then
  echo "graph-backed stable constructed metadata regressed to zero owned entries" >&2
  exit 1
fi

if [ "$metadata_owned" -lt 200 ]; then
  echo "graph-backed stable constructed metadata regressed below beta floor: $metadata_owned < 200" >&2
  exit 1
fi

if [ "$materializer_fallbacks" -ne 0 ]; then
  echo "metadata materializer fallback inventory regressed above beta cap: $materializer_fallbacks > 0" >&2
  exit 1
fi

if [ "$metadata_fallback_sum" -ne "$materializer_fallbacks" ]; then
  echo "metadata materializer fallback family accounting mismatch: sum=$metadata_fallback_sum total=$materializer_fallbacks" >&2
  exit 1
fi

if [ "$metadata_named_detail_sum" -ne "$metadata_fallback_named" ]; then
  echo "metadata named fallback detail accounting mismatch: sum=$metadata_named_detail_sum named=$metadata_fallback_named" >&2
  exit 1
fi

if [ "$metadata_named_alias" -ne 0 ]; then
  echo "alias metadata materialization regressed into fallback: $metadata_named_alias" >&2
  exit 1
fi

metadata_diagnostic_fallback_sum=$((metadata_named_builtin_shell + metadata_fallback_generic_named + metadata_named_missing_symbol))
if [ "$metadata_diagnostic_fallback_sum" -ne "$materializer_fallbacks" ]; then
  echo "metadata fallback contains non-diagnostic materializer debt: diagnostic_sum=$metadata_diagnostic_fallback_sum total=$materializer_fallbacks" >&2
  exit 1
fi

if [ "$metadata_fallback_compound" -ne 0 ] || [ "$metadata_fallback_other" -ne 0 ]; then
  echo "compound/other metadata fallback regressed: compound=$metadata_fallback_compound other=$metadata_fallback_other" >&2
  exit 1
fi

if [ "$metadata_named_generic_class" -ne 0 ] || [ "$metadata_named_non_class_symbol" -ne 0 ]; then
  echo "non-diagnostic named metadata fallback regressed: generic_class=$metadata_named_generic_class non_class_symbol=$metadata_named_non_class_symbol" >&2
  exit 1
fi

if [ "$metadata_named_builtin_shell" -ne 0 ]; then
  echo "bare builtin-shell diagnostic fallback regressed above cap: $metadata_named_builtin_shell > 0" >&2
  exit 1
fi

if [ "$metadata_fallback_generic_named" -ne 0 ]; then
  echo "generic-named diagnostic fallback regressed above cap: $metadata_fallback_generic_named > 0" >&2
  exit 1
fi

if [ "$metadata_named_missing_symbol" -ne 0 ]; then
  echo "missing-symbol diagnostic fallback regressed above cap: $metadata_named_missing_symbol > 0" >&2
  exit 1
fi

if [ "$legacy_non_alias" -ne 0 ]; then
  echo "non-alias DAG stage legacy fallback regressed: $legacy_non_alias" >&2
  exit 1
fi

if [ "$legacy_alias" -le 0 ]; then
  echo "alias-only legacy diagnostic fallback inventory regressed to zero" >&2
  exit 1
fi

if [ "$alias_materialized" -le 0 ]; then
  echo "alias stage materialization inventory regressed to zero" >&2
  exit 1
fi

if [ "$alias_diagnostic_fallback" -le 0 ]; then
  echo "alias diagnostic fallback inventory regressed to zero" >&2
  exit 1
fi

if [ "$alias_fallback_resolver_calls" -ne 0 ]; then
  echo "alias diagnostic fallback reintroduced recursive resolver calls: $alias_fallback_resolver_calls" >&2
  exit 1
fi

if [ "$alias_fallback_resolved" -ne 0 ]; then
  echo "valid alias materialization leaked into diagnostic fallback: $alias_fallback_resolved" >&2
  exit 1
fi

if [ "$alias_fallback_unresolved" -ne "$alias_diagnostic_fallback" ]; then
  echo "alias fallback accounting mismatch: fallback=$alias_diagnostic_fallback unresolved=$alias_fallback_unresolved" >&2
  exit 1
fi

grep -a -q 'topo_ok=1' "$log" || {
  echo "DAG topo validation did not report topo_ok=1" >&2
  exit 1
}

echo "[type-resolution-dag] graph stats and metadata reuse present (graph-backed skips=$graph_skips metadata_entries=$metadata_entries metadata_owned=$metadata_owned metadata_hits=$metadata_hits materializer_fallbacks=$materializer_fallbacks metadata_fallback_named=$metadata_fallback_named metadata_fallback_generic_named=$metadata_fallback_generic_named metadata_fallback_compound=$metadata_fallback_compound metadata_fallback_other=$metadata_fallback_other metadata_named_builtin_shell=$metadata_named_builtin_shell metadata_named_generic_class=$metadata_named_generic_class metadata_named_alias=$metadata_named_alias metadata_named_non_class_symbol=$metadata_named_non_class_symbol metadata_named_missing_symbol=$metadata_named_missing_symbol legacy_alias=$legacy_alias legacy_non_alias=$legacy_non_alias alias_materialized=$alias_materialized alias_diagnostic_fallback=$alias_diagnostic_fallback alias_fallback_resolver_calls=$alias_fallback_resolver_calls alias_fallback_resolved=$alias_fallback_resolved alias_fallback_unresolved=$alias_fallback_unresolved)"
