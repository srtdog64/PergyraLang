#!/usr/bin/env bash
set -euo pipefail

bin="${SEMANTIC_TEST_BIN:-}"
if [ -z "$bin" ]; then
  echo "SEMANTIC_TEST_BIN is required" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

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

mkdir -p "$ROOT/.tmp"
log="$(mktemp)"
work="$(mktemp -d "$ROOT/.tmp/pgy-type-resolution-dag.XXXXXX")"
cleanup() {
  rm -f "$log"
  if [ -n "${work:-}" ] && [ -d "$work" ]; then
    case "$work" in
      "$ROOT"/.tmp/pgy-type-resolution-dag.*)
        rm -rf "$work"
        ;;
      *)
        echo "refusing to remove unexpected DAG smoke work dir: $work" >&2
        ;;
    esac
  fi
}
trap cleanup EXIT

# The semantic suite writes temporary import fixtures with fixed filenames.
# Run the DAG stats pass from an isolated cwd so it can coexist with a direct
# make test-semantic run without corrupting those fixtures.
(cd "$work" && PGY_TYPE_RES_STATS=1 "$bin" >"$log" 2>&1)

grep -a -q '\[type-res-stats\] nodes=' "$log" || {
  echo "missing DAG node/edge stats" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-compat-resolve:' "$log" || {
  echo "missing stage compatibility inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] retired-compatibility-resolver:' "$log" || {
  echo "missing retired compatibility resolver call inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] retired-compatibility-resolver-kind:' "$log" || {
  echo "missing retired compatibility resolver AST-kind inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-compat-family:' "$log" || {
  echo "missing stage compatibility family inventory" >&2
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

grep -a -q '\[type-res-stats\] retired-compatibility-cache:' "$log" || {
  echo "missing retired compatibility resolver body-fallback/cache inventory" >&2
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

grep -a -q '\[type-res-stats\] stage-alias-diagnostic:' "$log" || {
  echo "missing alias diagnostic inventory" >&2
  exit 1
}

graph_skips="$(
  grep -a '\[type-res-stats\] stage-graph-backed:' "$log" \
    | sed -E 's/.*skips=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

resolve_calls="$(
  grep -a '\[type-res-stats\] retired-compatibility-resolver:' "$log" \
    | sed -E 's/.*calls=([0-9]+).*/\1/' \
    | awk '{ if ($1 > max) max = $1 } END { print max + 0 }'
)"

resolve_unique_nodes="$(
  grep -a '\[type-res-stats\] retired-compatibility-resolver:' "$log" \
    | sed -E 's/.*unique_nodes=([0-9]+).*/\1/' \
    | awk '{ if ($1 > max) max = $1 } END { print max + 0 }'
)"

resolve_kind_sum="$(
  grep -a '\[type-res-stats\] retired-compatibility-resolver-kind:' "$log" \
    | sed -E 's/.*ast_type=([0-9]+).*channel=([0-9]+).*future=([0-9]+).*event_handler=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ v = $1 + $2 + $3 + $4 + $5; if (v > max) max = v } END { print max + 0 }'
)"

resolve_kind_ast_type="$(
  grep -a '\[type-res-stats\] retired-compatibility-resolver-kind:' "$log" \
    | sed -E 's/.*ast_type=([0-9]+).*channel=([0-9]+).*future=([0-9]+).*event_handler=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ if ($1 > max) max = $1 } END { print max + 0 }'
)"

resolve_kind_compound_or_other="$(
  grep -a '\[type-res-stats\] retired-compatibility-resolver-kind:' "$log" \
    | sed -E 's/.*ast_type=([0-9]+).*channel=([0-9]+).*future=([0-9]+).*event_handler=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ v = $2 + $3 + $4 + $5; if (v > max) max = v } END { print max + 0 }'
)"

resolver_body_fallbacks="$(
  grep -a '\[type-res-stats\] retired-compatibility-cache:' "$log" \
    | sed -E 's/.*misses=([0-9]+).*/\1/' \
    | awk '{ if ($1 > max) max = $1 } END { print max + 0 }'
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

compat_alias="$(
  grep -a '\[type-res-stats\] stage-compat-family:' "$log" \
    | sed -E 's/.*alias=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

compat_non_alias="$(
  grep -a '\[type-res-stats\] stage-compat-family:' "$log" \
    | sed -E 's/.*generic_contract=([0-9]+).*signature=([0-9]+).*ability_consumer=([0-9]+).*domain_contract=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $1 + $2 + $3 + $4 + $5 } END { print total + 0 }'
)"

alias_materialized="$(
  grep -a '\[type-res-stats\] stage-alias:' "$log" \
    | sed -E 's/.*materialized=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_diagnostic_unresolved="$(
  grep -a '\[type-res-stats\] stage-alias:' "$log" \
    | sed -E 's/.*diagnostic_unresolved=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_diagnostic_resolver_calls="$(
  grep -a '\[type-res-stats\] stage-alias-diagnostic:' "$log" \
    | sed -E 's/.*resolver_calls=([0-9]+) resolved=.*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_diagnostic_resolved="$(
  grep -a '\[type-res-stats\] stage-alias-diagnostic:' "$log" \
    | sed -E 's/.* resolved=([0-9]+) cycle_unresolved=.*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

alias_diagnostic_cycle_unresolved="$(
  grep -a '\[type-res-stats\] stage-alias-diagnostic:' "$log" \
    | sed -E 's/.* cycle_unresolved=([0-9]+).*/\1/' \
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

if [ "$resolve_calls" -ne 0 ]; then
  echo "retired compatibility resolver calls regressed above beta cap: $resolve_calls > 0" >&2
  exit 1
fi

if [ "$resolve_kind_sum" -ne "$resolve_calls" ]; then
  echo "retired compatibility resolver AST-kind accounting mismatch: kind_sum=$resolve_kind_sum calls=$resolve_calls" >&2
  exit 1
fi

if [ "$resolve_kind_ast_type" -ne "$resolve_calls" ]; then
  echo "retired compatibility resolver non-AST_TYPE calls regressed: ast_type=$resolve_kind_ast_type calls=$resolve_calls" >&2
  exit 1
fi

if [ "$resolve_kind_compound_or_other" -ne 0 ]; then
  echo "compound/other retired compatibility resolver calls regressed: $resolve_kind_compound_or_other > 0" >&2
  exit 1
fi

if [ "$resolver_body_fallbacks" -ne 0 ]; then
  echo "retired compatibility resolver body fallback regressed: $resolver_body_fallbacks > 0" >&2
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

if [ "$compat_non_alias" -ne 0 ]; then
  echo "non-alias DAG stage compatibility fallback regressed: $compat_non_alias" >&2
  exit 1
fi

if [ "$compat_alias" -ne 0 ]; then
  echo "alias DAG stage compatibility fallback regressed above beta cap: $compat_alias > 0" >&2
  exit 1
fi

if [ "$alias_materialized" -le 0 ]; then
  echo "alias stage materialization inventory regressed to zero" >&2
  exit 1
fi

if [ "$alias_diagnostic_unresolved" -le 0 ]; then
  echo "alias diagnostic unresolved inventory regressed to zero" >&2
  exit 1
fi

if [ "$alias_diagnostic_resolver_calls" -ne 0 ]; then
  echo "alias diagnostic inventory reintroduced recursive resolver calls: $alias_diagnostic_resolver_calls" >&2
  exit 1
fi

if [ "$alias_diagnostic_resolved" -ne 0 ]; then
  echo "valid alias materialization leaked into diagnostic inventory: $alias_diagnostic_resolved" >&2
  exit 1
fi

if [ "$alias_diagnostic_cycle_unresolved" -ne "$alias_diagnostic_unresolved" ]; then
  echo "alias diagnostic accounting mismatch: unresolved=$alias_diagnostic_unresolved cycle_unresolved=$alias_diagnostic_cycle_unresolved" >&2
  exit 1
fi

grep -a -q 'topo_ok=1' "$log" || {
  echo "DAG topo validation did not report topo_ok=1" >&2
  exit 1
}

echo "[type-resolution-dag] graph stats and metadata reuse present (graph-backed skips=$graph_skips resolve_calls=$resolve_calls resolve_unique_nodes=$resolve_unique_nodes resolve_kind_sum=$resolve_kind_sum resolve_kind_ast_type=$resolve_kind_ast_type resolve_kind_compound_or_other=$resolve_kind_compound_or_other resolver_body_fallbacks=$resolver_body_fallbacks metadata_entries=$metadata_entries metadata_owned=$metadata_owned metadata_hits=$metadata_hits materializer_fallbacks=$materializer_fallbacks metadata_fallback_named=$metadata_fallback_named metadata_fallback_generic_named=$metadata_fallback_generic_named metadata_fallback_compound=$metadata_fallback_compound metadata_fallback_other=$metadata_fallback_other metadata_named_builtin_shell=$metadata_named_builtin_shell metadata_named_generic_class=$metadata_named_generic_class metadata_named_alias=$metadata_named_alias metadata_named_non_class_symbol=$metadata_named_non_class_symbol metadata_named_missing_symbol=$metadata_named_missing_symbol compat_alias=$compat_alias compat_non_alias=$compat_non_alias alias_materialized=$alias_materialized alias_diagnostic_unresolved=$alias_diagnostic_unresolved alias_diagnostic_resolver_calls=$alias_diagnostic_resolver_calls alias_diagnostic_resolved=$alias_diagnostic_resolved alias_diagnostic_cycle_unresolved=$alias_diagnostic_cycle_unresolved)"
