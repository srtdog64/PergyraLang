#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi

fail() {
    echo "[runtime-frontier-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing runtime frontier contract file: $rel"
}

normalized_file_for() {
    local path="$1"
    local key
    local normalized_path

    key="$(printf '%s' "$path" | sed 's#[^A-Za-z0-9_]#_#g')"
    normalized_path="$tmp_dir/norm_${key}"
    if [[ ! -f "$normalized_path" ]]; then
        tr '\n\r\t' '   ' < "$path" |
            sed 's/[[:space:]][[:space:]]*/ /g' > "$normalized_path"
    fi
    printf '%s\n' "$normalized_path"
}

require_term() {
    local label="$1"
    local path="$2"
    local term="$3"
    local normalized_term
    local normalized_path

    if grep -Fq -- "$term" "$path"; then
        return 0
    fi

    normalized_term="$(
        printf '%s' "$term" |
            tr '\n\r\t' '   ' |
            sed 's/[[:space:]][[:space:]]*/ /g'
    )"
    normalized_path="$(normalized_file_for "$path")"
    if ! grep -Fq -- "$normalized_term" "$normalized_path"; then
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

require_generated_frontier_limit() {
    local pgy_bin="${PGY_BIN:-$DEFAULT_PGY}"
    local source="$ROOT_DIR/tests/cases/backend_compare/world_embedded_action_frontier/main.pgy"
    local out_c="$tmp_dir/world_embedded_action_frontier.c"
    local log="$tmp_dir/world_embedded_action_frontier.log"

    [[ -n "$pgy_bin" && -x "$pgy_bin" ]] \
        || fail "PGY_BIN must point to an executable pgy for generated frontier check"
    "$pgy_bin" "$source" --emit-c -o "$out_c" >"$log" 2>&1 \
        || fail "failed to emit generated frontier fixture with $pgy_bin"

    require_terms "generated embedded world frontier C" "$out_c" \
        "_pgy_world_frontier_pass_limit = 7;" \
        "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit)"

    if grep -Fq "_pgy_world_frontier_pass_limit = 5;" "$out_c"; then
        fail "generated embedded world frontier still uses outer-only pass limit"
    fi
}

for rel in \
    "src/codegen/domain_frontier_policy.h" \
    "src/runtime/pgy_frontier_policy.h" \
    "src/codegen/transpiler_domain_nominal_emit.h" \
    "src/codegen/transpiler_domain_provenance_emit.h" \
    "src/codegen/transpiler_zone_decl_emit.h" \
    "src/codegen/transpiler_world_select_event_emit.h" \
    "src/codegen/transpiler_domain_role_ability_emit.h" \
    "src/codegen/llvm_domain.c" \
    "src/codegen/llvm_domain_sync_frontier.c" \
    "src/codegen/llvm_domain_zone_sync.c" \
    "src/codegen/llvm_domain_world_frontier.c" \
    "src/codegen/llvm_domain_world_sync.c" \
    "src/codegen/llvm_domain_projection_sync_helpers.h" \
    "src/codegen/llvm_domain_projection_sync_body_helpers.h" \
    "tests/abi_pipeline_smoke.sh" \
    "tests/compare_backends.sh" \
    "tests/runtime_frontier_policy_smoke.sh" \
    "Makefile" \
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
    "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.h" \
    > "$c_zone_contract"

cat \
    "$ROOT_DIR/src/codegen/llvm_domain.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_sync_frontier.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_frontier.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c" \
    > "$llvm_domain_contract"

cat \
    "$ROOT_DIR/src/codegen/llvm_domain_sync_frontier.c" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_helpers.h" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_body_helpers.h" \
    > "$llvm_projection_contract"

cat \
    "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.h" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.h" \
    > "$c_frontier_text"

require_terms "C zone frontier emitter" "$c_zone_contract" \
    "pgy_domain_zone_frontier_pass_limit" \
    "_pgy_zone_frontier_pass_limit" \
    "while (_pgy_zone_frontier_continue && _pgy_zone_frontier_pass < _pgy_zone_frontier_pass_limit)" \
    "_pgy_zone_frontier_continue = true" \
    "PGY_PANIC" \
    "zone frontier recompute exceeded bounded pass limit"

require_terms "C world frontier emitter" "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.h" \
    "transpiler_frontier_lookup_zone" \
    "pgy_domain_world_embedded_frontier_count" \
    "embedded_frontier_count" \
    "pgy_domain_world_transitive_frontier_pass_limit" \
    "pgy_domain_world_derived_frontier_pass_limit" \
    "_pgy_world_frontier_pass_limit" \
    "_pgy_world_derived_changed_any" \
    "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit)" \
    "_pgy_world_frontier_continue = true" \
    "world frontier recompute exceeded bounded pass limit" \
    "_pgy_world_pass_limit" \
    "while (_pgy_world_continue && _pgy_world_pass < _pgy_world_pass_limit)" \
    "PGY_PANIC" \
    "world derived recompute exceeded bounded pass limit"

require_terms "C projection frontier emitter" "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.h" \
    "pgy_domain_projection_frontier_pass_limit" \
    "_pgy_%s_pass_limit" \
    "while (_pgy_%s_continue && _pgy_%s_pass < _pgy_%s_pass_limit)" \
    "PGY_PANIC" \
    "projection recompute exceeded bounded pass limit"

require_terms "LLVM world/zone frontier emitter" "$llvm_domain_contract" \
    "llvm_world_frontier_lookup_zone" \
    "pgy_domain_world_embedded_frontier_count" \
    "pgy_domain_zone_frontier_pass_limit" \
    "pgy_domain_world_transitive_frontier_pass_limit" \
    "pgy_domain_world_derived_frontier_pass_limit" \
    "zone.frontier.pass.addr" \
    "zone.frontier.continue.addr" \
    "zone.frontier.overflow" \
    "world.frontier.pass.addr" \
    "world.frontier.continue.addr" \
    "world.derived.changed_any.addr" \
    "world.frontier.overflow" \
    "world.derived.overflow" \
    "pgy_runtime_panic_internal_invariant_export" \
    "zone frontier recompute exceeded bounded pass limit" \
    "world frontier recompute exceeded bounded pass limit" \
    "world derived recompute exceeded bounded pass limit" \
    "LLVMBuildUnreachable"

require_terms "LLVM projection frontier emitter" "$llvm_projection_contract" \
    "pgy_domain_projection_frontier_pass_limit" \
    "projection.loop.overflow" \
    "pgy_runtime_panic_internal_invariant_export" \
    "projection recompute exceeded bounded pass limit" \
    "LLVMBuildUnreachable"

require_terms "frontier policy source of truth" "$ROOT_DIR/src/runtime/pgy_frontier_policy.h" \
    "UINT32_MAX" \
    "pgy_frontier_pass_limit_clamp" \
    "pgy_frontier_pass_limit_add" \
    "pgy_frontier_pass_limit_add_one" \
    "pgy_frontier_projection_pass_limit" \
    "pgy_frontier_zone_pass_limit" \
    "pgy_frontier_world_pass_limit" \
    "pgy_frontier_world_transitive_pass_limit" \
    "embedded_zone_frontier_count" \
    "pgy_frontier_world_derived_pass_limit"

require_terms "codegen frontier policy compatibility wrapper" "$ROOT_DIR/src/codegen/domain_frontier_policy.h" \
    "../runtime/pgy_frontier_policy.h" \
    "PgyDomainZoneLookupFn" \
    "pgy_domain_zone_frontier_pass_limit" \
    "pgy_domain_projection_frontier_pass_limit" \
    "pgy_domain_world_derived_frontier_pass_limit" \
    "pgy_domain_world_embedded_frontier_count" \
    "pgy_domain_world_transitive_frontier_pass_limit" \
    "zone_decl->data.zone_decl.state_count" \
    "zone_decl->data.zone_decl.layer_slot_count"

require_terms "frontier policy arithmetic smoke" "$ROOT_DIR/tests/runtime_frontier_policy_smoke.sh" \
    "pgy_frontier_pass_limit_cap" \
    "pgy_frontier_pass_limit_add" \
    "pgy_frontier_pass_limit_add_one" \
    "pgy_frontier_world_transitive_pass_limit" \
    "pgy_domain_zone_frontier_pass_limit" \
    "pgy_domain_projection_frontier_pass_limit" \
    "pgy_domain_world_transitive_frontier_pass_limit" \
    "world-transitive-embedded-limit" \
    "UINT32_MAX"

require_terms "frontier policy Makefile wiring" "$ROOT_DIR/Makefile" \
    "runtime-frontier-policy-test-smoke:" \
    "tests/runtime_frontier_policy_smoke.sh" \
    "runtime-frontier-policy-test-smoke"

require_generated_frontier_limit

require_terms "ABI pipeline frontier case registry" "$ROOT_DIR/tests/abi_pipeline_smoke.sh" \
    "world_fixpoint_abi" \
    "projection_chain_abi" \
    "zone_frontier_abi" \
    "intent_authority_snapshot_abi" \
    "authority_failure_abi" \
    "world_embedded_projection_abi" \
    "world_embedded_method_projection_abi" \
    "world_embedded_branch_projection_abi" \
    "world_embedded_action_frontier_abi" \
    "world_embedded_action_pool_frontier_abi" \
    "handoff_projection_frontier_abi" \
    "handoff_world_state_frontier_abi" \
    "handoff_layer_state_frontier_abi"

require_terms "backend-compare frontier case registry" "$ROOT_DIR/tests/compare_backends.sh" \
    "tests/cases/backend_compare/intent_authority_snapshot" \
    "tests/cases/backend_compare/authority_failure_surface" \
    "tests/cases/backend_compare/world_embedded_branch_projection_visibility" \
    "tests/cases/backend_compare/world_embedded_action_frontier" \
    "tests/cases/backend_compare/world_embedded_action_pool_frontier" \
    "tests/cases/backend_compare/handoff_projection_frontier" \
    "tests/cases/backend_compare/handoff_world_state_frontier" \
    "tests/cases/backend_compare/handoff_layer_state_frontier"

require_terms "C authority/failure frontier surface" "$ROOT_DIR/src/codegen/transpiler_block_intent_helpers.h" \
    "pgy_zone_authority_validate_flags_export" \
    "__intent_failed = true" \
    "authority:%s" \
    "goto __intent_cleanup"

require_terms "LLVM authority/failure frontier surface" "$ROOT_DIR/src/codegen/llvm_intent_flow.c" \
    "pgy_zone_authority_validate_flags_export" \
    "return LLVMConstInt(ctx->type_i1, 0, 0)" \
    "llvm_emit_intent_presence_flag(ctx, zone_alias)" \
    "llvm_emit_intent_presence_flag(ctx, alias)" \
    "authority:%s" \
    "LLVMBuildCondBr(ctx->builder, ok, ok_bb, fail_bb)"

require_terms "runtime authority queryable failure surface" "$ROOT_DIR/src/runtime/pgy_runtime_authority_contract.h" \
    "PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE" \
    "PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT" \
    "PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH"

require_terms "runtime authority queryable failure exports" "$ROOT_DIR/src/runtime/pgy_runtime_lib_authority_file_core.h" \
    "pgy_zone_authority_validate_flags_export" \
    "pgy_zone_authority_last_ok_rt_export" \
    "pgy_zone_authority_last_zone_rt_export" \
    "pgy_zone_authority_last_participant_rt_export" \
    "pgy_zone_authority_last_code_rt_export" \
    "pgy_zone_authority_last_reason_rt_export"

for doc in "$ROOT_DIR/docs/100_beta_readiness_checklist.md" "$ROOT_DIR/TODO.md"; do
    require_terms "runtime frontier docs" "$doc" \
        "world derived-state bounded recompute" \
        "zone lifecycle bounded frontier loop" \
        "projection-chain bounded recompute" \
        "embedded world-zone action-caused layer/state freshness" \
        "authority/failure handoff queryable baseline" \
        "full bounded fixpoint / transitive frontier scheduler" \
        "broader world-zone propagation family"
done

if grep -Eiq 'frontier.*single[- ]pass' "$c_frontier_text"; then
    fail "C emitter contains single-pass frontier wording"
fi
if grep -Eiq 'frontier.*single[- ]pass' "$llvm_domain_contract"; then
    fail "LLVM emitter contains single-pass frontier wording"
fi
if grep -Fq 'llvm_lookup_or_create_function(ctx, "abort"' "$llvm_domain_contract"; then
    fail "LLVM frontier emitter must use runtime panic export, not raw abort"
fi
if grep -Fq 'llvm_lookup_or_create_function(ctx, "abort"' "$llvm_projection_contract"; then
    fail "LLVM projection frontier emitter must use runtime panic export, not raw abort"
fi

echo "[runtime-frontier-contract] bounded C/LLVM frontier contracts are gated"
