#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[domain-runtime-topology] $*" >&2
    exit 1
}

require_term() {
    local file="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$file" \
        || fail "missing '$term' in $file"
}

require_term "src/compiler/dir.h" "DIR_DOMAIN_TOPOLOGY_LINK_RELATION"
require_term "src/compiler/mir.c" "mir_domain_topology_project_from_dir"
require_term "src/compiler/driver_app.c" \
    "mir_lower_request_bind_dir(&mir_request, dir)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "pgy_codegen_zone_frontier_graph_pass_limit_from_mir"
require_term "src/codegen/llvm_domain_zone_sync.c" \
    "pgy_codegen_zone_frontier_graph_pass_limit_from_mir"

if rg -n \
    'propagation_graph_build_from_zone\(|pgy_codegen_zone_frontier_graph_pass_limit\(' \
    "$ROOT_DIR/src" >/dev/null; then
    fail "legacy backend AST zone graph entrypoint remains"
fi

if rg -n 'ast_zone_(refreshes|maintained_effects|links)' \
    "$ROOT_DIR/src/compiler/propagation_graph_build.c" \
    "$ROOT_DIR/src/codegen/domain_frontier_graph.c" >/dev/null; then
    fail "zone frontier graph reconstructs DIR-owned topology from AST"
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "domain-runtime-topology" "$PGY" \
    || fail "PGY_BIN is not runnable"

tmp_dir="$(mktemp -d)"
trap 'rm -rf -- "$tmp_dir"' EXIT

source_path="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
source_arg="$(pgy_path_for_compiler "$PGY" "$source_path")"

run_projection() {
    local backend="$1"
    local output="$tmp_dir/topology.${backend}"
    local output_arg
    local log="$tmp_dir/${backend}.log"
    output_arg="$(pgy_path_for_compiler "$PGY" "$output")"

    if [[ "$backend" == "c" ]]; then
        PGY_DUMP_PROPAGATION=1 "$PGY" "$source_arg" \
            --emit-c -o "$output_arg" >"$log" 2>&1 \
            || fail "C topology projection failed"
    else
        PGY_DUMP_PROPAGATION=1 "$PGY" "$source_arg" \
            --emit-llvm -o "$output_arg" >"$log" 2>&1 \
            || fail "LLVM topology projection failed"
    fi

    grep -Fq \
        '[propagation-graph] BattleZone: nodes=3 edges=2 acyclic depth=2 pass_limit=2' \
        "$log" || fail "$backend topology summary drifted"
    grep -Fq 'dep: trust <- player' "$log" \
        || fail "$backend topology lost player dependency"
    grep -Fq 'dep: trust <- enemy' "$log" \
        || fail "$backend topology lost enemy dependency"
    grep -E '^\[propagation-graph\]|^  propagation order:|^  dep:' \
        "$log" >"$tmp_dir/${backend}.trace"
}

run_projection c
run_projection llvm
cmp -s "$tmp_dir/c.trace" "$tmp_dir/llvm.trace" \
    || fail "C and LLVM consume different domain topology"

grep -Fq 'size_t _pgy_zone_frontier_pass_limit = 3;' \
    "$tmp_dir/topology.c" \
    || fail "C output did not preserve the count floor above the graph depth"

echo "[domain-runtime-topology] DIR -> MIR -> C/LLVM zone frontier topology is identical and AST bypasses are absent"
