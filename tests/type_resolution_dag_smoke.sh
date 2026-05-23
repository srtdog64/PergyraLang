#!/usr/bin/env bash
set -euo pipefail

bin="${SEMANTIC_TEST_BIN:-}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tests/pgy_binary_path_helpers.sh"
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"
if [ -z "$bin" ]; then
  if [ -x "$ROOT/bin/test_semantic" ]; then
    bin="$ROOT/bin/test_semantic"
  elif [ -x "$ROOT/bin/test_semantic.exe" ]; then
    bin="$ROOT/bin/test_semantic.exe"
  else
    echo "[type-resolution-dag] SEMANTIC_TEST_BIN not set; skipping executable DAG stats smoke"
    exit 0
  fi
fi

DAG_GRAPH_SKIPS_FLOOR=1900
DAG_METADATA_ENTRIES_FLOOR=3400
DAG_METADATA_HITS_FLOOR=7500
DAG_METADATA_OWNED_FLOOR=240

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
run_semantic_stats() {
  case "$bin" in
    *.exe)
      if command -v powershell.exe >/dev/null 2>&1; then
        local ps1="$work/run-semantic-stats.ps1"
        local win_bin win_work win_log win_ps1
        win_bin="$(pgy_path_for_compiler "$bin" "$bin")"
        win_work="$(pgy_path_for_compiler "$bin" "$work")"
        win_log="$(pgy_path_for_compiler "$bin" "$log")"
        win_ps1="$(pgy_path_for_compiler "$bin" "$ps1")"
cat >"$ps1" <<EOF
\$ErrorActionPreference = 'Continue'
Set-Location -LiteralPath '$win_work'
\$env:PATH = '$PGY_WINDOWS_PS_PATH_PREFIX' + \$env:PATH
\$env:PGY_TYPE_RES_STATS = '1'
& '$win_bin' 2>&1 | ForEach-Object { \$_.ToString() } | Set-Content -LiteralPath '$win_log' -Encoding utf8
exit \$LASTEXITCODE
EOF
        powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$win_ps1"
        return
      fi
      ;;
  esac

  (cd "$work" && PGY_TYPE_RES_STATS=1 "$bin" >"$log" 2>&1)
}

run_semantic_stats

grep -a -q '\[type-res-stats\] nodes=' "$log" || {
  echo "missing DAG node/edge stats" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] stage-metadata-materialize:' "$log" || {
  echo "missing stage metadata materialization inventory" >&2
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

grep -a -q '\[type-res-stats\] stage-materialize-family:' "$log" || {
  echo "missing stage metadata materialization family inventory" >&2
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

grep -a -q '\[type-res-stats\] metadata-unresolved-audit:' "$log" || {
  echo "missing graph-backed metadata unresolved audit family inventory" >&2
  exit 1
}

grep -a -q '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" || {
  echo "missing graph-backed metadata named unresolved audit inventory" >&2
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

metadata_dead_ends="$(
  grep -a '\[type-res-stats\] metadata:' "$log" \
    | sed -E 's/.*dead_ends=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_unresolved_sum="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit:' "$log" \
    | sed -E 's/.*named=([0-9]+).*generic_named=([0-9]+).*compound=([0-9]+).*other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $1 + $2 + $3 + $4 } END { print total + 0 }'
)"

metadata_unresolved_named="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit:' "$log" \
    | sed -E 's/.*metadata-unresolved-audit: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_unresolved_generic_named="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit:' "$log" \
    | sed -E 's/.*metadata-unresolved-audit: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $2 } END { print total + 0 }'
)"

metadata_unresolved_compound="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit:' "$log" \
    | sed -E 's/.*metadata-unresolved-audit: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $3 } END { print total + 0 }'
)"

metadata_unresolved_other="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit:' "$log" \
    | sed -E 's/.*metadata-unresolved-audit: named=([0-9]+) generic_named=([0-9]+) compound=([0-9]+) other=([0-9]+).*/\1 \2 \3 \4/' \
    | awk '{ total += $4 } END { print total + 0 }'
)"

metadata_named_detail_sum="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $1 + $2 + $3 + $4 + $5 } END { print total + 0 }'
)"

metadata_named_builtin_shell="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

metadata_named_generic_class="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $2 } END { print total + 0 }'
)"

metadata_named_alias="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $3 } END { print total + 0 }'
)"

metadata_named_non_class_symbol="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $4 } END { print total + 0 }'
)"

metadata_named_missing_symbol="$(
  grep -a '\[type-res-stats\] metadata-unresolved-audit-named:' "$log" \
    | sed -E 's/.*builtin_shell=([0-9]+).*generic_class=([0-9]+).*alias=([0-9]+).*non_class_symbol=([0-9]+).*missing_symbol=([0-9]+).*/\1 \2 \3 \4 \5/' \
    | awk '{ total += $5 } END { print total + 0 }'
)"

stage_materialize_alias="$(
  grep -a '\[type-res-stats\] stage-materialize-family:' "$log" \
    | sed -E 's/.*alias=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

stage_materialize_calls="$(
  grep -a '\[type-res-stats\] stage-metadata-materialize:' "$log" \
    | sed -E 's/.*calls=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

stage_materialize_failed="$(
  grep -a '\[type-res-stats\] stage-metadata-materialize:' "$log" \
    | sed -E 's/.*failed=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

stage_materialize_suppressed="$(
  grep -a '\[type-res-stats\] stage-metadata-materialize:' "$log" \
    | sed -E 's/.*suppressed_diagnostics=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

stage_materialize_non_alias="$(
  grep -a '\[type-res-stats\] stage-materialize-family:' "$log" \
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

generic_param_nodes="$(
  grep -a '\[type-res-stats\] kind:' "$log" \
    | sed -E 's/.*GENERIC_PARAM=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

dag_generic_contract_evidence="$(
  grep -a '\[type-res-stats\] dag-evidence:' "$log" \
    | sed -E 's/.*generic_contract=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

dag_ability_consumer_evidence="$(
  grep -a '\[type-res-stats\] dag-evidence:' "$log" \
    | sed -E 's/.*ability_consumer=([0-9]+).*/\1/' \
    | awk '{ total += $1 } END { print total + 0 }'
)"

if [ "$graph_skips" -le 0 ]; then
  echo "graph-backed stage skip inventory regressed to zero" >&2
  exit 1
fi

if [ "$graph_skips" -lt "$DAG_GRAPH_SKIPS_FLOOR" ]; then
  echo "graph-backed stage skip inventory regressed below beta floor: $graph_skips < $DAG_GRAPH_SKIPS_FLOOR" >&2
  exit 1
fi

if [ "$generic_param_nodes" -le 0 ]; then
  echo "generic parameter DAG evidence regressed to zero" >&2
  exit 1
fi

if [ "$dag_generic_contract_evidence" -le 0 ]; then
  echo "DAG generic contract evidence regressed to zero" >&2
  exit 1
fi

if [ "$dag_ability_consumer_evidence" -le 0 ]; then
  echo "DAG ability consumer evidence regressed to zero" >&2
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

if [ "$metadata_entries" -lt "$DAG_METADATA_ENTRIES_FLOOR" ]; then
  echo "graph-backed metadata inventory regressed below beta floor: $metadata_entries < $DAG_METADATA_ENTRIES_FLOOR" >&2
  exit 1
fi

if [ "$metadata_hits" -le 0 ]; then
  echo "graph-backed metadata reuse regressed to zero hits" >&2
  exit 1
fi

if [ "$metadata_hits" -lt "$DAG_METADATA_HITS_FLOOR" ]; then
  echo "graph-backed metadata reuse regressed below beta floor: $metadata_hits < $DAG_METADATA_HITS_FLOOR" >&2
  exit 1
fi

if [ "$metadata_owned" -le 0 ]; then
  echo "graph-backed stable constructed metadata regressed to zero owned entries" >&2
  exit 1
fi

if [ "$metadata_owned" -lt "$DAG_METADATA_OWNED_FLOOR" ]; then
  echo "graph-backed stable constructed metadata regressed below beta floor: $metadata_owned < $DAG_METADATA_OWNED_FLOOR" >&2
  exit 1
fi

if [ "$metadata_dead_ends" -ne 0 ]; then
  echo "metadata dead-end inventory regressed above beta cap: $metadata_dead_ends > 0" >&2
  exit 1
fi

if [ "$metadata_unresolved_sum" -ne "$metadata_dead_ends" ]; then
  echo "metadata unresolved audit family accounting mismatch: sum=$metadata_unresolved_sum dead_ends=$metadata_dead_ends" >&2
  exit 1
fi

if [ "$metadata_named_detail_sum" -ne "$metadata_unresolved_named" ]; then
  echo "metadata named unresolved audit detail accounting mismatch: sum=$metadata_named_detail_sum named=$metadata_unresolved_named" >&2
  exit 1
fi

if [ "$metadata_named_alias" -ne 0 ]; then
  echo "alias metadata materialization regressed into unresolved audit: $metadata_named_alias" >&2
  exit 1
fi

metadata_diagnostic_unresolved_sum=$((metadata_named_builtin_shell + metadata_unresolved_generic_named + metadata_named_missing_symbol))
if [ "$metadata_diagnostic_unresolved_sum" -ne "$metadata_dead_ends" ]; then
  echo "metadata unresolved audit contains non-diagnostic dead-end debt: diagnostic_sum=$metadata_diagnostic_unresolved_sum dead_ends=$metadata_dead_ends" >&2
  exit 1
fi

if [ "$metadata_unresolved_compound" -ne 0 ] || [ "$metadata_unresolved_other" -ne 0 ]; then
  echo "compound/other metadata unresolved audit regressed: compound=$metadata_unresolved_compound other=$metadata_unresolved_other" >&2
  exit 1
fi

if [ "$metadata_named_generic_class" -ne 0 ] || [ "$metadata_named_non_class_symbol" -ne 0 ]; then
  echo "non-diagnostic named metadata unresolved audit regressed: generic_class=$metadata_named_generic_class non_class_symbol=$metadata_named_non_class_symbol" >&2
  exit 1
fi

if [ "$metadata_named_builtin_shell" -ne 0 ]; then
  echo "bare builtin-shell unresolved audit regressed above cap: $metadata_named_builtin_shell > 0" >&2
  exit 1
fi

if [ "$metadata_unresolved_generic_named" -ne 0 ]; then
  echo "generic-named unresolved audit regressed above cap: $metadata_unresolved_generic_named > 0" >&2
  exit 1
fi

if [ "$metadata_named_missing_symbol" -ne 0 ]; then
  echo "missing-symbol unresolved audit regressed above cap: $metadata_named_missing_symbol > 0" >&2
  exit 1
fi

if [ "$stage_materialize_non_alias" -ne 0 ]; then
  echo "non-alias DAG stage metadata materialization regressed: $stage_materialize_non_alias" >&2
  exit 1
fi

if [ "$stage_materialize_calls" -ne 0 ]; then
  echo "DAG stage metadata materializer calls regressed above beta cap: $stage_materialize_calls > 0" >&2
  exit 1
fi

if [ "$stage_materialize_failed" -ne 0 ] || [ "$stage_materialize_suppressed" -ne 0 ]; then
  echo "DAG stage materializer diagnostic suppression regressed: failed=$stage_materialize_failed suppressed=$stage_materialize_suppressed" >&2
  exit 1
fi

if [ "$stage_materialize_alias" -ne 0 ]; then
  echo "alias DAG stage metadata materialization regressed above beta cap: $stage_materialize_alias > 0" >&2
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

echo "[type-resolution-dag] graph stats and metadata reuse present (graph-backed skips=$graph_skips generic_param_nodes=$generic_param_nodes dag_generic_contract_evidence=$dag_generic_contract_evidence dag_ability_consumer_evidence=$dag_ability_consumer_evidence retired_resolver_calls=$resolve_calls retired_resolver_unique_nodes=$resolve_unique_nodes retired_resolver_kind_sum=$resolve_kind_sum retired_resolver_kind_ast_type=$resolve_kind_ast_type retired_resolver_kind_compound_or_other=$resolve_kind_compound_or_other retired_resolver_body_fallbacks=$resolver_body_fallbacks metadata_entries=$metadata_entries metadata_owned=$metadata_owned metadata_hits=$metadata_hits metadata_dead_ends=$metadata_dead_ends metadata_unresolved_named=$metadata_unresolved_named metadata_unresolved_generic_named=$metadata_unresolved_generic_named metadata_unresolved_compound=$metadata_unresolved_compound metadata_unresolved_other=$metadata_unresolved_other metadata_unresolved_builtin_shell=$metadata_named_builtin_shell metadata_unresolved_generic_class=$metadata_named_generic_class metadata_unresolved_alias=$metadata_named_alias metadata_unresolved_non_class_symbol=$metadata_named_non_class_symbol metadata_unresolved_missing_symbol=$metadata_named_missing_symbol stage_materialize_calls=$stage_materialize_calls stage_materialize_failed=$stage_materialize_failed stage_materialize_suppressed=$stage_materialize_suppressed stage_materialize_alias=$stage_materialize_alias stage_materialize_non_alias=$stage_materialize_non_alias alias_materialized=$alias_materialized alias_diagnostic_unresolved=$alias_diagnostic_unresolved alias_diagnostic_resolver_calls=$alias_diagnostic_resolver_calls alias_diagnostic_resolved=$alias_diagnostic_resolved alias_diagnostic_cycle_unresolved=$alias_diagnostic_cycle_unresolved)"
