#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[runtime-frontier-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing runtime frontier contract file: $rel"
}

require_term() {
    local label="$1"
    local path="$2"
    local term="$3"
    local normalized_term

    if grep -Fq -- "$term" "$path"; then
        return 0
    fi

    normalized_term="$(
        printf '%s' "$term" |
            tr '\n\r\t' '   ' |
            sed 's/[[:space:]][[:space:]]*/ /g'
    )"
    if ! tr '\n\r\t' '   ' < "$path" |
        sed 's/[[:space:]][[:space:]]*/ /g' |
        grep -Fq -- "$normalized_term"; then
        fail "$label missing frontier contract term: $term"
    fi
}

require_terms() {
    local label="$1"
    local path="$2"
    shift 2
    local term
    for term in "$@"; do
        require_term "$label" "$path" "$term"
    done
}

for rel in \
    "src/codegen/transpiler_domain_nominal_emit.h" \
    "src/codegen/transpiler_zone_decl_emit.h" \
    "src/codegen/transpiler_world_select_event_emit.h" \
    "src/codegen/transpiler_domain_role_ability_emit.h" \
    "src/codegen/llvm_domain.c" \
    "src/codegen/llvm_domain_zone_sync.c" \
    "src/codegen/llvm_domain_world_sync.c" \
    "src/codegen/llvm_domain_projection_sync_helpers.h" \
    "src/codegen/llvm_domain_projection_sync_body_helpers.h" \
    "tests/abi_pipeline_smoke.sh" \
    "tests/compare_backends.sh" \
    "docs/100_beta_readiness_checklist.md" \
    "TODO.md"; do
    require_file "$rel"
done

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

c_zone_contract="$tmp_dir/c_zone_contract.txt"
llvm_domain_contract="$tmp_dir/llvm_domain_contract.txt"
llvm_projection_contract="$tmp_dir/llvm_projection_contract.txt"
c_frontier_text="$tmp_dir/c_frontier_text.txt"

cat \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.h" \
    > "$c_zone_contract"

cat \
    "$ROOT_DIR/src/codegen/llvm_domain.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c" \
    > "$llvm_domain_contract"

cat \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_helpers.h" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_body_helpers.h" \
    > "$llvm_projection_contract"

cat \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.h" \
    > "$c_frontier_text"

require_terms "C zone frontier emitter" "$c_zone_contract" \
    "_pgy_zone_frontier_pass_limit" \
    "while (_pgy_zone_frontier_continue && _pgy_zone_frontier_pass < _pgy_zone_frontier_pass_limit)" \
    "_pgy_zone_frontier_continue = true" \
    "PGY_PANIC" \
    "zone frontier recompute exceeded bounded pass limit"

require_terms "C world frontier emitter" "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.h" \
    "_pgy_world_frontier_pass_limit" \
    "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit)" \
    "_pgy_world_frontier_continue = true" \
    "world frontier recompute exceeded bounded pass limit" \
    "_pgy_world_pass_limit" \
    "while (_pgy_world_continue && _pgy_world_pass < _pgy_world_pass_limit)" \
    "PGY_PANIC" \
    "world derived recompute exceeded bounded pass limit"

require_terms "C projection frontier emitter" "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h" \
    "_pgy_%s_pass_limit" \
    "while (_pgy_%s_continue && _pgy_%s_pass < _pgy_%s_pass_limit)" \
    "PGY_PANIC" \
    "projection recompute exceeded bounded pass limit"

require_terms "LLVM world/zone frontier emitter" "$llvm_domain_contract" \
    "zone.frontier.pass.addr" \
    "zone.frontier.continue.addr" \
    "zone.frontier.overflow" \
    "world.frontier.pass.addr" \
    "world.frontier.continue.addr" \
    "world.frontier.overflow" \
    "world.derived.overflow" \
    "llvm_lookup_or_create_function(ctx, \"abort\"" \
    "LLVMBuildUnreachable"

require_terms "LLVM projection frontier emitter" "$llvm_projection_contract" \
    "projection.loop.overflow" \
    "llvm_lookup_or_create_function(ctx, \"abort\"" \
    "LLVMBuildUnreachable"

require_terms "ABI pipeline frontier case registry" "$ROOT_DIR/tests/abi_pipeline_smoke.sh" \
    "world_fixpoint_abi" \
    "projection_chain_abi" \
    "zone_frontier_abi" \
    "world_embedded_projection_abi" \
    "world_embedded_method_projection_abi" \
    "world_embedded_branch_projection_abi" \
    "world_embedded_action_frontier_abi" \
    "world_embedded_action_pool_frontier_abi" \
    "handoff_projection_frontier_abi" \
    "handoff_world_state_frontier_abi" \
    "handoff_layer_state_frontier_abi"

require_terms "backend-compare frontier case registry" "$ROOT_DIR/tests/compare_backends.sh" \
    "tests/cases/backend_compare/world_embedded_branch_projection_visibility" \
    "tests/cases/backend_compare/world_embedded_action_frontier" \
    "tests/cases/backend_compare/world_embedded_action_pool_frontier" \
    "tests/cases/backend_compare/handoff_projection_frontier" \
    "tests/cases/backend_compare/handoff_world_state_frontier" \
    "tests/cases/backend_compare/handoff_layer_state_frontier"

for doc in "$ROOT_DIR/docs/100_beta_readiness_checklist.md" "$ROOT_DIR/TODO.md"; do
    require_terms "runtime frontier docs" "$doc" \
        "world derived-state bounded recompute" \
        "zone lifecycle bounded frontier loop" \
        "projection-chain bounded recompute" \
        "embedded world-zone action-caused layer/state freshness" \
        "full bounded fixpoint / transitive frontier scheduler" \
        "remaining authority/failure handoff family"
done

if grep -Eiq 'frontier.*single[- ]pass' "$c_frontier_text"; then
    fail "C emitter contains single-pass frontier wording"
fi
if grep -Eiq 'frontier.*single[- ]pass' "$llvm_domain_contract"; then
    fail "LLVM emitter contains single-pass frontier wording"
fi

echo "[runtime-frontier-contract] bounded C/LLVM frontier contracts are gated"
