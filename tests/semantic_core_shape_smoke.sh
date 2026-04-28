#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fail() {
    echo "[semantic-core-shape] $*" >&2
    exit 1
}

type_checker_loc="$(wc -l < src/semantic/type_checker.c | tr -d '[:space:]')"
if [ "$type_checker_loc" -gt 600 ]; then
    fail "src/semantic/type_checker.c is ${type_checker_loc} LOC; expected <= 600"
fi

if [ -e src/semantic/type_checker_resolution_graph_inventory.inc ]; then
    fail "DAG inventory must stay in type_checker_resolution_graph_inventory.c, not .inc"
fi

for path in \
    src/semantic/type_checker_event.c \
    src/semantic/type_checker_generic_validation.c \
    src/semantic/type_checker_qubit.c \
    src/semantic/type_checker_domain_role_lookup.c \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_metadata_diagnostics.c \
    src/semantic/type_checker_resolution_graph_inventory.c \
    src/semantic/type_checker_resolution_stage.c \
    src/semantic/type_checker_resolution_stage_alias.c \
    src/semantic/type_checker_resolution_stage_nominal.c \
    src/semantic/type_checker_resolution_stage_systemic.c \
    src/semantic/type_checker_resolution_stage_domain_decl.c \
    src/semantic/type_checker_resolution_stage_lookup.c \
    src/semantic/type_checker_resolution_stage_stats.c \
    src/semantic/type_checker_resolution_stage_domain.c
do
    [ -f "$path" ] || fail "missing semantic owner TU: $path"
done

[ -f src/semantic/type_checker_flow_match.c ] \
    || fail "missing CFG match-flow owner TU: src/semantic/type_checker_flow_match.c"

for path in \
    src/semantic/type_checker_resolution_metadata.c \
    src/semantic/type_checker_resolution_metadata_alias.c \
    src/semantic/type_checker_resolution_stage.c \
    src/semantic/type_checker_resolution_stage_nominal.c \
    src/semantic/type_checker_resolution_stage_systemic.c \
    src/semantic/type_checker_resolution_stage_domain_decl.c \
    src/semantic/type_checker_flow.c \
    src/semantic/type_checker_flow_match.c
do
    loc="$(wc -l < "$path" | tr -d '[:space:]')"
    if [ "$loc" -gt 600 ]; then
        fail "$path is ${loc} LOC; expected <= 600"
    fi
done

if grep -q '#include "type_checker_resolution_graph_inventory.inc"' src/semantic/type_checker.c; then
    fail "type_checker.c must not include graph inventory body"
fi

if grep -R "resolve_type_node(" src/semantic/type_checker_resolution_graph_*.c src/semantic/type_checker_resolution_graph_core.h >/dev/null; then
    fail "DAG graph core/precollect layer must not call resolve_type_node directly"
fi

echo "[semantic-core-shape] semantic owner boundaries ok"
