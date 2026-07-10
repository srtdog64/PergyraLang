#!/usr/bin/env bash
set -euo pipefail

case ":$PATH:" in
    *:/usr/bin:*) ;;
    *) if [ -d /usr/bin ]; then PATH="/usr/bin:$PATH"; fi ;;
esac
case ":$PATH:" in
    *:/bin:*) ;;
    *) if [ -d /bin ]; then PATH="/bin:$PATH"; fi ;;
esac
export PATH

SCRIPT_PATH="${BASH_SOURCE[0]}"
case "$SCRIPT_PATH" in
    */*) SCRIPT_DIR="${SCRIPT_PATH%/*}" ;;
    *) SCRIPT_DIR="." ;;
esac
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"
TMPDIR="${TMPDIR:-/tmp}"
WORK_DIR="$(mktemp -d "$TMPDIR/pgy_perf_contract.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
SRC_INDEX="$WORK_DIR/src-index.txt"
CODEGEN_INDEX="$WORK_DIR/codegen-index.txt"
SEMANTIC_INDEX="$WORK_DIR/semantic-index.txt"
RUNTIME_INDEX="$WORK_DIR/runtime-index.txt"
AIR_TEST_INDEX="$WORK_DIR/air-test-index.txt"

grep -RIn -e '.' "$ROOT_DIR/src" > "$SRC_INDEX"
grep -F "/src/codegen/" "$SRC_INDEX" > "$CODEGEN_INDEX" || true
grep -F "/src/semantic/" "$SRC_INDEX" > "$SEMANTIC_INDEX" || true
grep -F "/src/runtime/" "$SRC_INDEX" > "$RUNTIME_INDEX" || true
grep -F "/src/tests/air/" "$SRC_INDEX" > "$AIR_TEST_INDEX" || true

c_fail_open_zero="$WORK_DIR/c-fail-open-zero.txt"
grep -F 'return pergyra_strdup("0")' "$CODEGEN_INDEX" \
    | grep -Fv 'src/codegen/transpiler_let_box_emit.c:' \
    > "$c_fail_open_zero" || true
if [ -s "$c_fail_open_zero" ]; then
    echo "[perf-contract] C backend reintroduced silent zero fail-open fallback" >&2
    cat "$c_fail_open_zero" >&2
    exit 1
fi
if grep -Fq 'return pergyra_strdup("false")' "$CODEGEN_INDEX"; then
    echo "[perf-contract] C backend reintroduced silent false fail-open fallback" >&2
    grep -F 'return pergyra_strdup("false")' "$CODEGEN_INDEX" >&2
    exit 1
fi
if grep -Fq 'return pergyra_strdup("/*' "$CODEGEN_INDEX"; then
    echo "[perf-contract] C backend reintroduced comment-only fail-open fallback" >&2
    grep -F 'return pergyra_strdup("/*' "$CODEGEN_INDEX" >&2
    exit 1
fi

LOG="$WORK_DIR/test-abi-perf.log"
SUMMARY="$WORK_DIR/perf-summary.txt"

cat > "$LOG" <<'EOF'
  c/projection_abi: wrote test source /tmp/projection_abi.pgy
metrics: compile=0.500s run=0.002s
  c/intent_authority_snapshot_abi: wrote test source /tmp/intent_authority_snapshot_abi.pgy
metrics: compile=1.750s run=0.004s
  llvm/projection_abi: wrote test source /tmp/projection_abi.pgy
metrics: compile=0.250s run=0.003s
EOF

bash "$ROOT_DIR/tests/perf_summary.sh" "$LOG" > "$SUMMARY"

grep -Fq "backend cases compile_avg_s compile_max_s compile_max_case run_avg_s run_max_s run_max_case" "$SUMMARY"
grep -Fq "c 2 1.125 1.750 intent_authority_snapshot_abi 0.003 0.004 intent_authority_snapshot_abi" "$SUMMARY"
grep -Fq "llvm 1 0.250 0.250 projection_abi 0.003 0.003 projection_abi" "$SUMMARY"

pgy_beta_checklist_contains "test-abi-perf"
pgy_beta_checklist_contains "perf-summary"
pgy_beta_checklist_contains "perf-c-baseline-test-smoke"
pgy_beta_checklist_contains "pgy_over_c_ratio"
grep -Fq "perf-c-baseline-test-smoke" "$ROOT_DIR/TODO.md"
grep -Fq "pgy_over_c_ratio" "$ROOT_DIR/TODO.md"
grep -Fq "perf-c-baseline-test-smoke" "$ROOT_DIR/Makefile"
grep -Fq "tests/perf_c_baseline_smoke.sh" "$ROOT_DIR/Makefile"
grep -Fq "Invoke-CheckedNative" "$ROOT_DIR/tests/perf_c_baseline_smoke.ps1"
grep -Fq "ArgList" "$ROOT_DIR/tests/perf_c_baseline_smoke.ps1"
grep -Fq "c_baseline_arith_loop.pgy" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "c_baseline_arith_loop.c" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "constant safe divisor modulo regressed to checked helper" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "constant safe divisor division regressed to checked helper" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "codegen_scalar_arithmetic_policy.c" "$ROOT_DIR/Makefile"
grep -Fq "pgy_codegen_ast_number_is_safe_divisor_i32_literal" "$ROOT_DIR/src/codegen/codegen_scalar_arithmetic_policy.c"
grep -Fq "pgy_codegen_ast_number_is_safe_divisor_i32_literal" "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c"
grep -Fq "pgy_codegen_ast_number_is_safe_divisor_i32_literal" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "transpiler_binary_emit_operand" "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c"
grep -Fq "C backend: binary expression could not lower %s operand" "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c"
grep -Fq "transpiler_control_flow_emit_expr" "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq "C backend: %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq '"if", "condition"' "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq '"for-in", "iterable"' "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq '"for range", "start"' "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq '"while", "condition"' "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq "P10" "$ROOT_DIR/TODO.md"
grep -Fq "trace_len" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_active_count" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "count = pgy_intent_active_count" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_exports.h"
grep -Fq "pgy_intent_active_free_cursor" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_active_free_cursor" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "pgy_intent_find_free_active_slot" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_find_free_active_slot_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "subject_count > 0 && subjects == NULL" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "subject_count > 0 && subjects == NULL" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "subject_fingerprint" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_subject_fingerprint" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "entry->subject_fingerprint & subject_fingerprint" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "subject_fingerprint" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "pgy_intent_subject_fingerprint_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "entry->subject_fingerprint & subject_fingerprint" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "pgy_intent_active_count_value" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "count = pgy_intent_active_count_value" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_exports.h"
grep -Fq "pgy_intent_append_line_len" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_events_inline.h"
grep -Fq "pgy_intent_append_line_len_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
grep -Fq "pgy_runtime_lib_mir_trace_exports.c" "$ROOT_DIR/Makefile"
grep -Fq "pgy_runtime_lib_mir_trace_exports.c" "$ROOT_DIR/src/compiler/compiler_runtime_cache.c"
grep -Fq '#include "pgy_runtime_lib_mir_trace_exports.c"' "$ROOT_DIR/src/runtime/pgy_runtime_lib.c"
grep -Fq "pgy_mir_resource_op_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_mir_trace_exports.c"
grep -Fq "pgy_mir_cleanup_op_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_mir_trace_exports.c"
grep -Fq "g_schedulerByTag" "$ROOT_DIR/src/runtime/party_runtime_scheduler.c"
grep -Fq "g_schedulerRegistryMutex" "$ROOT_DIR/src/runtime/party_runtime_scheduler.c"
grep -A12 -F "GetSchedulerForTag(SchedulerTag tag)" \
    "$ROOT_DIR/src/runtime/party_runtime_scheduler.c" | \
    grep -Fq "pthread_mutex_lock(&g_schedulerRegistryMutex);"
grep -A16 -F "GetSchedulerForTag(SchedulerTag tag)" \
    "$ROOT_DIR/src/runtime/party_runtime_scheduler.c" | \
    grep -Fq "pthread_mutex_unlock(&g_schedulerRegistryMutex);"
grep -Fq "party_runtime_stats.c" "$ROOT_DIR/Makefile"
grep -Fq "indexHashes" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "indexHealthy" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "fiber_stats_lookup(roleId)" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "fiber_stats_index_insert(stats->roleId" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "return fiber_stats_lookup_linear(roleId);" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "g_fiberStatsMutex" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "pthread_mutex_lock(&g_fiberStatsMutex);" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "pthread_mutex_unlock(&g_fiberStatsMutex);" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -A10 -F "GetFiberStats(const char* roleId)" \
    "$ROOT_DIR/src/runtime/party_runtime_stats.c" | \
    grep -Fq "pthread_mutex_lock(&g_fiberStatsMutex);"
grep -A30 -F "party_runtime_dump_fiber_stats(void)" \
    "$ROOT_DIR/src/runtime/party_runtime_stats.c" | \
    grep -Fq "snapshot_count = g_fiberStats.count;"
grep -A30 -F "party_runtime_dump_fiber_stats(void)" \
    "$ROOT_DIR/src/runtime/party_runtime_stats.c" | \
    grep -Fq "pthread_mutex_unlock(&g_fiberStatsMutex);"
grep -A45 -F "party_runtime_dump_fiber_stats(void)" \
    "$ROOT_DIR/src/runtime/party_runtime_stats.c" | \
    grep -Fq "party_runtime_free_fiber_stats_snapshot(snapshot, snapshot_count);"
grep -Fq "party_runtime_copy_fiber_stats_snapshot_role" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "memcpy(roleId, source->roleId" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
if grep -A4 -F "if (g_fiberStats.indexHashes[slot] == 0)" \
    "$ROOT_DIR/src/runtime/party_runtime_stats.c" | \
    grep -Fq "fiber_stats_lookup_linear(roleId)"; then
    echo "[perf-contract] fiber stat indexed miss regressed to linear fallback" >&2
    exit 1
fi
if grep -A12 -F "pgy_intent_active_count_export(void)" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_exports.h" | \
    grep -Fq "PGY_INTENT_ACTIVE_MAX"; then
    echo "[perf-contract] exported active-count query regressed to registry scan" >&2
    exit 1
fi
if grep -A14 -F "GetSchedulerForTag(SchedulerTag tag)" \
    "$ROOT_DIR/src/runtime/party_runtime_scheduler.c" | \
    grep -Fq "g_schedulerCount"; then
    echo "[perf-contract] scheduler tag lookup regressed to registry scan" >&2
    exit 1
fi
if grep -A10 -F "GetFiberStats(const char* roleId)" \
    "$ROOT_DIR/src/runtime/party_runtime_stats.c" | \
    grep -Fq "g_fiberStats.count"; then
    echo "[perf-contract] fiber stat lookup regressed to stats-count scan" >&2
    exit 1
fi
if grep -A80 -F "ContextFindRoles(PartyContext* context, const char* requiredAbility)" \
    "$ROOT_DIR/src/runtime/party_runtime.c" | \
    grep -Fq "matches"; then
    echo "[perf-contract] context role query regressed to two-pass match counting" >&2
    exit 1
fi
grep -Fq "symbol_ptr_array_contains" "$ROOT_DIR/src/semantic/slot_analyzer.c"
grep -Fq "bsearch(&needle" "$ROOT_DIR/src/semantic/slot_analyzer.c"
grep -Fq "symbol_hash_name" "$ROOT_DIR/src/semantic/symbol_table.c"
grep -Fq "symbol_index_insert" "$ROOT_DIR/src/semantic/symbol_table.c"
grep -Fq "scope_ensure_symbol_index_capacity" "$ROOT_DIR/src/semantic/symbol_table.c"
grep -Fq "scope_lookup_current_linear" "$ROOT_DIR/src/semantic/symbol_table.c"
grep -Fq "last_lookup_name;" "$ROOT_DIR/src/codegen/llvm_internal.h"
grep -Fq "LLVMVarEntry *last_lookup;" "$ROOT_DIR/src/codegen/llvm_internal.h"
grep -Fq "frame->last_lookup_name" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "frame->last_lookup = &frame->entries" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -A30 -F "llvm_scope_push(LLVMGenCtx *ctx)" \
    "$ROOT_DIR/src/codegen/llvm_registry.c" | \
    grep -Fq "llvm_scope_cache_invalidate(ctx)"
grep -A30 -F "llvm_scope_pop(LLVMGenCtx *ctx)" \
    "$ROOT_DIR/src/codegen/llvm_registry.c" | \
    grep -Fq "llvm_scope_cache_invalidate(ctx)"
grep -A40 -F "llvm_scope_declare(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry.c" | \
    grep -Fq "llvm_scope_cache_invalidate(ctx)"
grep -Fq "LLVM scope registry capacity overflow" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "LLVM scope registry allocation overflow" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "static LLVMVarEntry *" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_scope_lookup(LLVMGenCtx *ctx, const char *name)" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
! grep -Fq "LLVMVarEntry *llvm_scope_lookup" \
    "$ROOT_DIR/src/codegen/llvm_internal_api.h"
grep -Fq "llvm_scope_lookup_snapshot" \
    "$ROOT_DIR/src/codegen/llvm_internal_api.h"
grep -Fq "llvm_scope_contains" \
    "$ROOT_DIR/src/codegen/llvm_internal_api.h"
grep -Fq "llvm_scope_lookup_snapshot(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_scope_contains(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "*out = *entry;" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, alloca_name, &entry)" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, token_name, &token_entry)" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, block->pin_source_name" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, pin_name, &pin_entry)" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_base, &source_entry)" \
    "$ROOT_DIR/src/codegen/llvm_mir_resource_view.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_token_name, &token_entry)" \
    "$ROOT_DIR/src/codegen/llvm_mir_resource_view.c"
grep -Fq 'Do not keep a borrowed `LLVMVarEntry *`' \
    "$ROOT_DIR/AGENTS.md"
grep -Fq "Do not pass growable runtime container storage" \
    "$ROOT_DIR/AGENTS.md"
grep -Fq "Rehash/grow plus concurrent read is UB" \
    "$ROOT_DIR/AGENTS.md"
grep -Fq "list_alloca = list_snapshot.alloca" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "loop_alloca = loop_snapshot.alloca" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, idx_name, &idx_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, iter_name, &iter_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "LLVMValueRef binding_alloca = entry.alloca" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "LLVMValueRef list_alloca = list_snapshot.alloca" \
    "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, iter_name" \
    "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, var_name" \
    "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, alloca_name, &loop_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "LLVMValueRef source_alloca = source.alloca" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_name, &source)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, ast_identifier_name(init)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, token_name, &token_var)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "source_alloca = has_source_entry && source_entry.alloca" \
    "$ROOT_DIR/src/codegen/llvm_mir_resource_view.c"
grep -Fq "LLVMValueRef token_alloca = token_entry.alloca" \
    "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -A16 -F "LLVMVerifyModule(ctx->module" \
    "$ROOT_DIR/src/codegen/llvm_api.c" | \
    grep -Fq "if (verify_error != NULL)"
! grep -A16 -F "LLVMVerifyModule(ctx->module" \
    "$ROOT_DIR/src/codegen/llvm_api.c" | \
    grep -Fxq "    LLVMDisposeMessage(verify_error);"
grep -Fq "llvm_scope_declare(ctx, name, NULL, target_cls->struct_type)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "llvm_register_projection_borrow(ctx, name, type_name, source_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, base_name, &source)" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "type_ann = mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "ASTNode *init = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "!mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, \"self\", &self_entry)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, inst->arg0, &target_var)" \
    "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_scope_contains(ctx, target_name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, vname, &var)" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, party_var, &party_entry)" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_name," \
    "$ROOT_DIR/src/codegen/llvm_expr_host_spawn_literal_helpers.c"
! grep -Fq "llvm_scope_lookup(ctx, source_name)" \
    "$ROOT_DIR/src/codegen/llvm_expr_host_spawn_literal_helpers.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, var_name, &entry)" \
    "$ROOT_DIR/src/codegen/llvm_registry_arrays.c"
! grep -Fq "llvm_scope_lookup(ctx, var_name)" \
    "$ROOT_DIR/src/codegen/llvm_registry_arrays.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_nominal.c"
! grep -Fq "llvm_scope_lookup(ctx, name)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_nominal.c"
grep -A10 -F "llvm_collection_active_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_collections.c" | \
    grep -Fq "llvm_scope_lookup_snapshot(ctx, var_name, &entry)"
! grep -A10 -F "llvm_collection_active_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_collections.c" | \
    grep -Fq "llvm_scope_lookup(ctx, var_name)"
grep -A10 -F "llvm_resource_active_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "llvm_scope_lookup_snapshot(ctx, var_name, &entry)"
! grep -A10 -F "llvm_resource_active_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "llvm_scope_lookup(ctx, var_name)"
grep -A10 -F "llvm_registry_active_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" | \
    grep -Fq "llvm_scope_lookup_snapshot(ctx, var_name, &entry)"
! grep -A10 -F "llvm_registry_active_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" | \
    grep -Fq "llvm_scope_lookup(ctx, var_name)"
grep -A8 -F "llvm_lookup_projection_borrow(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_aux.c" | \
    grep -Fq "llvm_scope_contains(ctx, var_name)"
grep -A8 -F "llvm_lookup_callable_entry(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_aux.c" | \
    grep -Fq "llvm_scope_contains(ctx, var_name)"
grep -Fq 'llvm_scope_contains(ctx, "self")' \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq 'llvm_scope_lookup(ctx, "self") != NULL' \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "bool has_var = llvm_scope_lookup_snapshot(ctx, name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, target_name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_scope_contains(ctx, name)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq 'llvm_scope_lookup_snapshot(ctx, "self", &self_var)' \
    "$ROOT_DIR/src/codegen/llvm_expr_common.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_common.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_common.c"
grep -Fq "llvm_array_required_receiver_binding" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "LLVMValueRef args[] = { arr_alloca" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
! grep -Fq "llvm_array_required_receiver_var" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq "bool has_arr_var = llvm_scope_lookup_snapshot(ctx, name, &arr_var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "args[0] = arr_var.alloca" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "args[1] = index64" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, recv_name, &arr_var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "LLVMValueRef args[] = { arr_var.alloca, sep }" \
    "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "bool has_var = llvm_scope_lookup_snapshot(ctx, var_name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "llvm_rc_load_handle(ctx, &var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, callee_name, &ev)" \
    "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, evt_name, &ev)" \
    "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, party_var, &pvar)" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"
grep -Fq "pvar.alloca" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, arg_name, &arg_var)" \
    "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq "arg_var.alloca" \
    "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq 'llvm_scope_lookup_snapshot(ctx, "self", &self_var)' \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"
grep -Fq "self_var.alloca" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_projection.c"
grep -Fq 'llvm_scope_lookup_snapshot(ctx, "self", &self_var)' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_projection_sync.c"
grep -Fq "self_var.alloca" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_projection_sync.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_call_projection_sync.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_name, &slot_var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_boundary_slot_runtime_arg(ctx, &slot_var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_scope_contains(ctx, idx_name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_scope_contains(ctx, alloca_name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "llvm_scope_contains(ctx, inst->arg1)" \
    "$ROOT_DIR/src/codegen/llvm_mir_resource_view.c"
scope_mixed_files=$(
    grep -R -l "llvm_scope_lookup(" "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' | \
    while IFS= read -r file; do
        if ! grep -Eq "llvm_scope_(declare|push|pop)\(" "$file"; then
            continue
        fi
        rel=${file#"$ROOT_DIR"/}
        if [ "$rel" = "src/codegen/llvm_registry.c" ] \
            || [ "$rel" = "src/codegen/llvm_internal_api.h" ] \
            || [ "$rel" = "src/codegen/llvm_stmt_parallel_async.c" ]; then
            continue
        fi
        printf '%s\n' "$rel"
    done
)
if [ -n "$scope_mixed_files" ]; then
    echo "[perf-contract] LLVM borrowed scope lookup mixed with scope mutation outside approved owners:" >&2
    printf '%s\n' "$scope_mixed_files" >&2
    exit 1
fi
scope_lookup_sites="$(
    grep -RIn --include='*.c' --include='*.h' 'llvm_scope_lookup(ctx,' \
        "$ROOT_DIR/src/codegen" \
        | sed -E 's#^.*src/codegen/#src/codegen/#; s#:[0-9]+:#:#' \
        | sort
)"
expected_scope_lookup_sites="$(cat <<'SCOPELOOKUP'
src/codegen/llvm_registry.c:    entry = llvm_scope_lookup(ctx, frame->entries[index].name);
src/codegen/llvm_registry.c:    entry = llvm_scope_lookup(ctx, name);
src/codegen/llvm_registry.c:    return llvm_scope_lookup(ctx, name) != NULL;
SCOPELOOKUP
)"
if [ "$scope_lookup_sites" != "$expected_scope_lookup_sites" ]; then
    echo "[perf-contract] unexpected LLVM borrowed scope lookup site(s):" >&2
    diff -u <(printf '%s\n' "$expected_scope_lookup_sites") \
        <(printf '%s\n' "$scope_lookup_sites") >&2 || true
    exit 1
fi
grep -Fq "entry == &frame->entries[index]" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_scope_frame_entry_is_current(ctx, frame, index)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "source.alloca == NULL" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "llvm_lookup_projection_borrow(ctx, base_name) != NULL" "$ROOT_DIR/src/codegen/llvm_mir_source_def_copy.c"
grep -Fq "llvm_intent_current_handle_or_error" "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "silent zero handle fallback is not allowed" "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq 'llvm_scope_lookup_snapshot(ctx, "__intent_handle", &handle_entry)' \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, zone_alias, &zone_var)" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, from_alias, &from_zone_var)" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, alias, &participant_var)" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "participant_var.alloca" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
! grep -Fq "return LLVMConstInt(ctx->type_i32, 0, 0)" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
! grep -Fq "llvm_scope_lookup(ctx, \"__intent_handle\")->alloca" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "ScaffoldKindSpec" "$ROOT_DIR/src/compiler/driver_scaffold.c"
grep -Fq "scaffold_find_kind" "$ROOT_DIR/src/compiler/driver_scaffold.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/compiler/driver_scaffold.c"
grep -Fq "TranspilerTypeNameMap" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_lookup_type_name_map" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_is_box_array" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
if grep -E '/transpiler_[^/]*\.c:.*strncmp\([^"]*"(Channel|Future|RemoteFuture|Result|Option|Array|Slice|List|Queue|Set|HashMap|Box|Rc|Weak)(<|")' \
    "$CODEGEN_INDEX" | grep -v "transpiler_type_mapping.c"; then
    echo "[perf-contract] C backend type-family classification bypassed transpiler_type_mapping" >&2
    exit 1
fi
grep -Fq "transpiler_forward_type_name_is_allowed" "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
grep -Fq "transpiler_forward_allowed_type_compare" "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
grep -Fq "bsearch(&name" "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*name[[:space:]]*,[[:space:]]*"(Int|Float|Bool|String|Char|Byte|Void|Qubit|Result|Option|Slot|SecureSlot|DeviceSlot|RemoteFuture|Array|Slice|Channel|Box|Rc|Weak|Future)"' \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
    echo "[perf-contract] function forward policy reintroduced direct type branch" >&2
    exit 1
fi
forward_allowed_type_names="$(
    sed -n '/static const char \*allowed_names\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
forward_allowed_type_names_sorted="$(
    printf '%s\n' "$forward_allowed_type_names" | sort
)"
if [[ "$forward_allowed_type_names" != "$forward_allowed_type_names_sorted" ]]; then
    echo "function forward allowed type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$forward_allowed_type_names_sorted") \
        <(printf '%s\n' "$forward_allowed_type_names") >&2 || true
    exit 1
fi
grep -Fq "lexer_keywords.c" "$ROOT_DIR/Makefile"
grep -Fq "lexer_lookup_keyword" "$ROOT_DIR/src/lexer/lexer.c"
grep -Fq "keyword_compare_slice" "$ROOT_DIR/src/lexer/lexer_keywords.c"
lexer_keyword_names="$(
    sed -n '/static const KeywordEntry kKeywords\[\]/,/^};/p' \
        "$ROOT_DIR/src/lexer/lexer_keywords.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
lexer_keyword_names_sorted="$(
    printf '%s\n' "$lexer_keyword_names" | sort
)"
if [[ "$lexer_keyword_names" != "$lexer_keyword_names_sorted" ]]; then
    echo "lexer keyword names must stay sorted for binary lookup" >&2
    diff -u <(printf '%s\n' "$lexer_keyword_names_sorted") \
        <(printf '%s\n' "$lexer_keyword_names") >&2 || true
    exit 1
fi
grep -Fq "kEffectWordSpecs" "$ROOT_DIR/src/semantic/type_checker_helpers_effects.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/semantic/type_checker_helpers_effects.c"
if grep -A28 -F "scope_lookup_current(Scope *scope" \
    "$ROOT_DIR/src/semantic/symbol_table.c" | \
    grep -Fq "scope->symbol_count"; then
    echo "[perf-contract] scope_lookup_current regressed to symbol-count linear lookup" >&2
    exit 1
fi
grep -Fq "air_collect_mir_pin_block_evidence" "$ROOT_DIR/src/compiler/air_evidence_mir.c"
grep -Fq "air_boundary_has_evidence_kind_provider" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
! grep -Fq "air_has_mir_pin_cleanup_evidence" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
! grep -Fq "air_has_boundary_evidence_provider" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
grep -Fq "type_resolution_dag_generic_contract_evidence_count" "$ROOT_DIR/src/semantic/semantic.h"
grep -Fq "type_resolution_dag_ability_consumer_evidence_count" "$ROOT_DIR/src/semantic/semantic.h"
grep -Fq "type_resolution_dag_ability_consumer_evidence_count" "$ROOT_DIR/src/semantic/type_checker.h"
grep -Fq "semantic_result_type_resolution_metadata_hits" "$ROOT_DIR/src/semantic/semantic.h"
grep -Fq "semantic_result_type_resolution_metadata_hits(" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "ctx->type_resolution_dag_generic_contract_evidence_count" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "ctx->type_resolution_dag_ability_consumer_evidence_count" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "semantic_record_dag_generic_contract_evidence" "$ROOT_DIR/src/semantic/type_checker_resolution_stage_signature.c"
grep -Fq "[type-res-stats] dag-evidence:" "$ROOT_DIR/src/semantic/type_checker_program_stats.c"
grep -Fq "dag_generic_contract_evidence" "$ROOT_DIR/tests/type_resolution_dag_smoke.sh"
grep -Fq "dag_ability_consumer_evidence" "$ROOT_DIR/tests/type_resolution_dag_smoke.sh"
grep -Fq "semantic_result_dag_generic_contract_evidence_count(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_dag_ability_consumer_evidence_count(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_type_resolution_metadata_entries(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_type_resolution_metadata_hits(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_type_resolution_metadata_dead_ends(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "air_publish_dag_evidence(" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "metadata hits without metadata inventory" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
! grep -Fq "sem->type_resolution_" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
! grep -Fq "type_resolution_stage_compat_generic_contract_count" "$ROOT_DIR/src/semantic/semantic.c"
! grep -Fq "type_resolution_stage_compat_generic_contract_count" "$ROOT_DIR/src/semantic/semantic.h"
! grep -Fq "sem->type_resolution_stage_compat_generic_contract_count" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
! grep -Fq "sem->type_resolution_dag_ability_evidence_count" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
! grep -Fq "type_resolution_dag_ability_evidence_count" "$ROOT_DIR/src/semantic/semantic.h"
! grep -Fq "type_resolution_dag_ability_evidence_count" "$ROOT_DIR/src/semantic/type_checker.h"
grep -Fq "realloc(q->data, nc * sizeof" "$ROOT_DIR/src/runtime/pgy_runtime_queue_inline.h"
grep -Fq "transpiler_mir_effective_type.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_render_effective_local_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_effective_type.c"
grep -Fq "transpiler_mir_effective_type.h" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
! grep -Fq "find_class_decl(ctx, ast_type_name(type_node))" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_find_local_type_name(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
! grep -Fq "transpiler_find_local_type_name(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_ssa_emitters.h"
! grep -Fq "transpiler_lookup_current_owner_member_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_ssa_emitters.h"
! test -e "$ROOT_DIR/src/codegen/transpiler_mir_ssa_emit.h"
grep -Fq "HIRRoutineSourceIndex" "$ROOT_DIR/src/compiler/hir_callgraph.c"
grep -Fq "hir_build_routine_source_index" "$ROOT_DIR/src/compiler/hir_callgraph.c"
grep -Fq "hir_lookup_routine_id_by_source" "$ROOT_DIR/src/compiler/hir_callgraph.c"
grep -Fq "evidence_capacity" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "drift_capacity" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "owned_name_capacity" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "air_ensure_owned_name_capacity" "$ROOT_DIR/src/compiler/air_names.c"
grep -Fq "air_intent_storage_valid" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_boundary_storage_valid" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_drift_storage_valid" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_intent_node_count" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_intent_node_at" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_boundary_node_count" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_boundary_node_at" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_boundary_node_mut_at" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_drift_count" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_drift_at" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "air_intent_storage_valid(air)" "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "air_boundary_storage_valid(air)" "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "air_drift_storage_valid(air)" "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "air_has_hir_input" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_has_rir_input" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_has_mir_input" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_requires_strict_evidence" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_mark_hir_input" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_mark_rir_input" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_mark_mir_input" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_mark_rir_input(air)" "$ROOT_DIR/src/compiler/air_evidence_rir.c"
grep -Fq "air_mark_mir_input(air)" "$ROOT_DIR/src/compiler/air_evidence_mir.c"
grep -Fq "air_has_hir_input(air)" "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "air_has_rir_input(air)" "$ROOT_DIR/src/compiler/air_dump_json.c"
grep -Fq "air_has_mir_input(air)" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_requires_strict_evidence(air)" "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "air_requires_strict_evidence(air)" "$ROOT_DIR/src/compiler/air_verify_global.c"
for air_input_consumer in \
    "$ROOT_DIR/src/compiler/air_dump.c" \
    "$ROOT_DIR/src/compiler/air_dump_json.c" \
    "$ROOT_DIR/src/compiler/air_evidence_mir.c" \
    "$ROOT_DIR/src/compiler/air_evidence_rir.c" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c" \
    "$ROOT_DIR/src/compiler/air_validate_evidence.c" \
    "$ROOT_DIR/src/compiler/air_validate_summary_counters.c" \
    "$ROOT_DIR/src/compiler/air_verify.c" \
    "$ROOT_DIR/src/compiler/air_verify_global.c"; do
    ! grep -Fq "air->has_hir_input" "$air_input_consumer"
    ! grep -Fq "air->has_rir_input" "$air_input_consumer"
    ! grep -Fq "air->has_mir_input" "$air_input_consumer"
    ! grep -Fq "air->strict_evidence" "$air_input_consumer"
done
grep -Fq "air_intent_node_at(air, i)" "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "air_intent_node_at" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "air_boundary_node_at" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "air_drift_at" "$ROOT_DIR/src/compiler/air.h"
! grep -Fq '#include "air_internal.h"' "$ROOT_DIR/src/compiler/driver_app.c"
! grep -Fq '#include "air_internal.h"' "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "air_boundary_node_at(air, i)" "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "air_drift_at(air, i)" "$ROOT_DIR/src/compiler/air_dump_json.c"
grep -Fq "air_drift_count(air) > 0" "$ROOT_DIR/src/compiler/driver_app.c"
grep -Fq "air_drift_at(air, 0)" "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "air_boundary_node_mut_at(air, j)" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
grep -Fq "air_boundary_node_mut_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "air_boundary_node_mut_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
grep -Fq "air_boundary_node_mut_at(air, i)" "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "air_boundary_node_count(air) > 0" "$ROOT_DIR/src/compiler/air_verify_global.c"
for air_graph_consumer in \
    "$ROOT_DIR/src/compiler/air_dump.c" \
    "$ROOT_DIR/src/compiler/air_dump_json.c" \
    "$ROOT_DIR/src/compiler/air_validate.c" \
    "$ROOT_DIR/src/compiler/air_evidence_hir.c" \
    "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c" \
    "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c" \
    "$ROOT_DIR/src/compiler/air_verify.c" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c" \
    "$ROOT_DIR/src/compiler/air_validate_evidence.c" \
    "$ROOT_DIR/src/compiler/air_verify_global.c" \
    "$ROOT_DIR/src/compiler/driver_app.c" \
    "$ROOT_DIR/src/compiler/driver_diag.c"; do
    ! grep -Fq "air->intent_count" "$air_graph_consumer"
    ! grep -Fq "air->intents[" "$air_graph_consumer"
    ! grep -Fq "air->boundary_count" "$air_graph_consumer"
    ! grep -Fq "air->boundaries[" "$air_graph_consumer"
    ! grep -Fq "air->drift_count" "$air_graph_consumer"
    ! grep -Fq "air->drifts[" "$air_graph_consumer"
done
grep -Fq "air_evidence_inventory_storage_valid" "$ROOT_DIR/src/compiler/air_internal.h"
grep -Fq "air_evidence_inventory_storage_valid" "$ROOT_DIR/src/compiler/air_evidence_node.c"
grep -Fq "air_evidence_inventory_storage_valid(air)" "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "air_evidence_node_count" "$ROOT_DIR/src/compiler/air_evidence_node.c"
grep -Fq "air_evidence_node_at" "$ROOT_DIR/src/compiler/air_evidence_node.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_dump.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_dump_json.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_dump_json.c"
grep -Fq "air_boundary_has_evidence_kind_provider" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
! grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
! grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "air_boundary_has_evidence_kind_provider" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "air_global_evidence_node_provider_subject" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
! grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
! grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
grep -Fq "air_boundary_has_evidence_kind_provider" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "air_evidence_node_at(air, evidence_index)" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_dump.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_dump.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_dump_json.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_dump_json.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
! grep -Fq "air->evidence_nodes[i]" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
! grep -Fq "air->evidence_count" "$ROOT_DIR/src/compiler/air_validate.c"
! grep -Fq "air->evidence_nodes" "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "air_evidence_kind_is_known(kind)" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_evidence_node_kind(evidence)" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_boundary_mark_summary_flag" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_boundary_mark_summary_flag(" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
grep -Fq "air_boundary_mark_summary_flag(" "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
! grep -Fq "has_hir_routine_evidence = true" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
! grep -Fq "has_hir_cfg_evidence = true" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
! grep -Fq "has_rir_boundary_evidence = true" "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
! grep -Fq "has_rir_authority_evidence = true" "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
grep -Fq "air_evidence_kind_is_known(kind)" "$ROOT_DIR/src/compiler/air_validate_global_evidence.c"
grep -Fq "!air_evidence_kind_is_boundary_scoped(kind)" "$ROOT_DIR/src/compiler/air_validate_global_evidence.c"
grep -Fq "air_boundary_authority_name_count" "$ROOT_DIR/src/compiler/air_boundary.c"
grep -Fq "air_boundary_authority_name_at" "$ROOT_DIR/src/compiler/air_boundary.c"
grep -Fq "air_boundary_authority_storage_valid" "$ROOT_DIR/src/compiler/air_boundary.c"
grep -Fq "air_boundary_authority_name_count(boundary)" "$ROOT_DIR/src/compiler/air_dump_json.c"
grep -Fq "air_boundary_authority_name_at(boundary, j)" "$ROOT_DIR/src/compiler/air_dump_json.c"
grep -Fq "air_boundary_authority_storage_valid(boundary)" \
    "$ROOT_DIR/src/compiler/air_validate.c"
grep -Fq "air_boundary_declares_authority_name(boundary, fact->name)" \
    "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
grep -Fq "air_boundary_authority_name_count(boundary)" \
    "$ROOT_DIR/src/compiler/air_verify_provenance.c"
grep -Fq "air_boundary_authority_name_count(boundary)" \
    "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "air_boundary_authority_name_at(boundary, i)" \
    "$ROOT_DIR/src/compiler/driver_diag.c"
! grep -Fq "boundary->authority_name_count" "$ROOT_DIR/src/compiler/air_dump_json.c"
! grep -Fq "boundary->authority_names[" "$ROOT_DIR/src/compiler/air_dump_json.c"
! grep -Fq "boundary->authority_name_count" "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
! grep -Fq "boundary->authority_names[" "$ROOT_DIR/src/compiler/air_evidence_rir_boundary.c"
! grep -Fq "boundary->authority_name_count" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
! grep -Fq "boundary->authority_names[" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
! grep -Fq "boundary->authority_name_count" "$ROOT_DIR/src/compiler/air_verify_provenance.c"
! grep -Fq "boundary->authority_names[" "$ROOT_DIR/src/compiler/air_verify_provenance.c"
! grep -Fq "boundary->authority_name_count" "$ROOT_DIR/src/compiler/driver_diag.c"
! grep -Fq "boundary->authority_names[" "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "air_boundary_has_summary_flag(boundary, AIR_EVIDENCE_HIR_ROUTINE)" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
grep -Fq "air_boundary_has_summary_flag(boundary, AIR_EVIDENCE_RIR_AUTHORITY)" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
! grep -Fq "boundary->has_hir_routine_evidence" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
! grep -Fq "boundary->has_hir_cfg_evidence" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
! grep -Fq "boundary->has_rir_boundary_evidence" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
! grep -Fq "boundary->has_rir_authority_evidence" \
    "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
grep -Fq "AIR boundary node %zu has %s summary without evidence node" "$ROOT_DIR/src/compiler/air_validate_boundary_summary.c"
grep -Fq "AIR boundary evidence node %zu has no matching boundary summary flag" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "HIR CFG evidence summary without evidence node" \
    "$ROOT_DIR/src/tests/air/test_air_core_part_a_1.cases.h"
grep -Fq "no matching boundary summary flag" \
    "$ROOT_DIR/src/tests/air/test_air_core_evidence_part_k.cases.h"
grep -Fq "AIR strict evidence rejects stale boundary summary flags" "$ROOT_DIR/src/test_air.c"
grep -Fq "DriverDiagCodeMap" "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "driver_diag_code_map_find" "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "driver_diag_code_map_compare" "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "bsearch(&key" "$ROOT_DIR/src/compiler/driver_diag.c"
grep -Fq "pgy_compiler_io_boundary_builtin_is_stable" "$ROOT_DIR/src/compiler/io_boundary_builtin.c"
grep -Fq "pgy_compiler_io_boundary_builtin_is_stable(name)" "$ROOT_DIR/src/compiler/air_boundary.c"
grep -Fq "pgy_compiler_io_boundary_builtin_is_stable(name)" "$ROOT_DIR/src/compiler/rir_builder_walk.c"
grep -Fq "C backend: %s could not lower %s argument" "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c"
! grep -Fq "io_names[]" "$ROOT_DIR/src/compiler/air_boundary.c"
! grep -Fq "io_names[]" "$ROOT_DIR/src/compiler/rir_builder_walk.c"
for outputter_builtin in Print Log LogRaw LogBanner; do
    if grep -Fq "\"$outputter_builtin\"" "$ROOT_DIR/src/compiler/io_boundary_builtin.c"; then
        echo "[perf-contract] outputter builtin must not become AIR/RIR resource-boundary inputter: $outputter_builtin" >&2
        exit 1
    fi
done
grep -Fq "pgy_codegen_claim_slot_spec_compare" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "bsearch(name" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "PgyCodegenSlotCallSpec specs[]" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "pgy_codegen_slot_call_spec_compare" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "pgy_hashmap_key_spec_compare" "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c"
grep -Fq "if (name == NULL || name[0] == '\\0')" \
    "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c"
grep -Fq "bsearch(name" "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c"
grep -Fq "codegen_match_variant_policy.c" "$ROOT_DIR/Makefile"
grep -Fq "pgy_codegen_match_variant_lookup" "$ROOT_DIR/src/codegen/codegen_match_variant_policy.c"
grep -Fq "pgy_match_variant_lookup(name)" "$ROOT_DIR/src/codegen/codegen_match_variant_policy.c"
grep -Fq "pgy_match_variant_compare" "$ROOT_DIR/src/common/match_variant_policy.c"
grep -Fq "bsearch(&name" "$ROOT_DIR/src/common/match_variant_policy.c"
grep -Fq "codegen_match_subject_lookup.c" "$ROOT_DIR/Makefile"
if grep -Fq "pgy_codegen_match_subject_for_case" \
        "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.c" \
        "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.h"; then
    echo "[perf-contract] match-subject owner retained AST case compatibility fallback" >&2
    exit 1
fi
if grep -Fq "ast_find_match_subject_for_case(ast_func_body(func_decl)" \
        "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.c"; then
    echo "[perf-contract] match-subject owner reintroduced AST body subject lookup" >&2
    exit 1
fi
if grep -Fq "mir_instruction_source_payload(" \
        "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.c"; then
    echo "[perf-contract] match-subject owner reopened source payload shape" >&2
    exit 1
fi
for match_subject_consumer in \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"; do
    grep -Fq "pgy_codegen_match_subject_for_branch(" \
        "$match_subject_consumer"
    if grep -Fq "pgy_codegen_match_subject_for_case(func_decl, case_node)" \
        "$match_subject_consumer"; then
        echo "[perf-contract] MIR match condition regressed to AST case subject fallback: $match_subject_consumer" >&2
        exit 1
    fi
    if grep -Fq "ast_find_match_subject_for_case(ast_func_body(func_decl)" \
        "$match_subject_consumer"; then
        echo "[perf-contract] MIR match condition reintroduced direct AST body subject lookup: $match_subject_consumer" >&2
        exit 1
    fi
done
for match_variant_owner in \
    "$ROOT_DIR/src/codegen/transpiler_match_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_pattern_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_payload_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_stmt_match.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_pattern.c"; do
    grep -Fq "pgy_codegen_match_variant_lookup" "$match_variant_owner"
    if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*(name|kind|option_kind|result_kind)[[:space:]]*,[[:space:]]*"(Some|None|Ok|Err)"' \
        "$match_variant_owner"; then
        echo "[perf-contract] match lowering reintroduced direct variant branch in $match_variant_owner" >&2
        exit 1
    fi
done
match_variant_names="$(
    sed -n '/static const PgyMatchVariantSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/common/match_variant_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
match_variant_names_sorted="$(
    printf '%s\n' "$match_variant_names" | sort
)"
if [[ "$match_variant_names" != "$match_variant_names_sorted" ]]; then
    echo "match variant destructor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$match_variant_names_sorted") \
        <(printf '%s\n' "$match_variant_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_registry_type_kind" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "llvm_registry_type_spec_compare" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "bsearch(&type_name" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*type_name[[:space:]]*,[[:space:]]*"(Array|Slice|List|Set|Queue|HashMap|Future|RemoteFuture|Channel|Rc|Weak)"' \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"; then
    echo "[perf-contract] LLVM type registry reintroduced direct type-family branch" >&2
    exit 1
fi
llvm_registry_type_names="$(
    sed -n '/static const LLVMRegistryTypeSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_registry_type_names_sorted="$(
    printf '%s\n' "$llvm_registry_type_names" | sort
)"
if [[ "$llvm_registry_type_names" != "$llvm_registry_type_names_sorted" ]]; then
    echo "LLVM type registry names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_registry_type_names_sorted") \
        <(printf '%s\n' "$llvm_registry_type_names") >&2 || true
    exit 1
fi
grep -Fq "transpiler_mir_resource_op_spec_compare" "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c"
grep -Fq "transpiler_mir_resource_op_lookup" "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c"
grep -Fq "bsearch(op_name" "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*op_name[[:space:]]*,[[:space:]]*"(Claim|Read|Write|Release|Move)"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"; then
    echo "[perf-contract] MIR resource op emission reintroduced direct op branch" >&2
    exit 1
fi
grep -Fq "transpiler_mir_resource_op_lookup(inst->name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*inst->name[[:space:]]*,[[:space:]]*"(Claim|Read|Write|Release|Move)"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"; then
    echo "[perf-contract] C MIR resource-op statement emitter reintroduced direct op branch" >&2
    exit 1
fi
for rel in \
    src/codegen/transpiler_mir_block_schedule_emit.c \
    src/codegen/transpiler_mir_emission_mapping_contract.c \
    src/codegen/transpiler_mir_ssa_utils.c; do
    grep -Fq "transpiler_mir_resource_op_lookup(inst->name)" "$ROOT_DIR/$rel"
    if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*inst->name[[:space:]]*,[[:space:]]*"(Claim|Read|Write|Release|Move)"' \
        "$ROOT_DIR/$rel"; then
        echo "[perf-contract] $rel reintroduced direct MIR slot op branch" >&2
        exit 1
    fi
done
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*inst->name[[:space:]]*,[[:space:]]*"(BorrowRead|BorrowWrite)"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"; then
    echo "[perf-contract] C MIR pin alias seeding reintroduced direct op branch" >&2
    exit 1
fi
grep -Fq "mir_abi_resource_runtime_row_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq "transpiler_slot_runtime_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq '"PinRead"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq '"PinWrite"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq '"Unpin"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
! grep -Fq "transpiler_mir_pin_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
! grep -Fq "pgy_pin_%s_%s" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
! grep -Fq "pgy_secure_pin_%s_%s" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
! grep -Fq "pgy_unpin_%s(&%s);" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
! grep -Fq "pgy_secure_unpin_%s(&%s);" \
    "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "row->call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "transpiler_slot_runtime_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"PinRead"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"PinWrite"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"UnpinCleanup"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq '"Release"' \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "C source slot auto-release requires MIR ABI runtime function row" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "pgy_pin_%s_%s" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "pgy_secure_pin_%s_%s" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "cleanup(pgy_unpin_cleanup_%s)" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "cleanup(pgy_secure_unpin_cleanup_%s)" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "transpiler_block_pin_expected_call_shape" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "pgy_release_%s(&%s);" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
! grep -Fq "pgy_secure_release_%s(&%s, &%s_token);" \
    "$ROOT_DIR/src/codegen/transpiler_block_emit.c"
grep -Fq "transpiler_mir_resource_op_lookup(inst->name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "mir_instruction_source_is_with_slot_release(emit_inst)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "mir_instruction_source_is_with_slot_release" \
    "$ROOT_DIR/src/compiler/mir_source_shape.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*(inst|candidate|emit_inst)->name[[:space:]]*,[[:space:]]*"(Claim|Read|Write|Release|Move|BorrowRead|BorrowWrite)"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"; then
    echo "[perf-contract] C MIR resource hook reintroduced direct slot op branch" >&2
    exit 1
fi
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*inst->name[[:space:]]*,[[:space:]]*"(ProjectRefresh|ProjectPublish)"' \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"; then
    echo "[perf-contract] C MIR resource hook must consume RIR projection op kind, not projection op names" >&2
    exit 1
fi
grep -Fq "inst->rir_op->kind == RIR_OP_PROJECT_REFRESH" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*inst->name[[:space:]]*,[[:space:]]*"(BorrowRead|BorrowWrite)"' \
    "$ROOT_DIR/src/codegen/llvm_mir_resource_view.c"; then
    echo "[perf-contract] LLVM MIR borrow-view aliasing reintroduced direct op branch" >&2
    exit 1
fi
for rel in \
    src/codegen/llvm_mir_emit.c \
    src/codegen/llvm_mir_param_emit.c; do
    grep -Fq "llvm_param_is_implicit_self_local" "$ROOT_DIR/$rel"
    if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*(p|candidate)->name[[:space:]]*,[[:space:]]*"self"' \
        "$ROOT_DIR/$rel"; then
        echo "[perf-contract] $rel must consume the implicit-self predicate" >&2
        exit 1
    fi
done
mir_resource_op_names="$(
    sed -n '/static const TranspilerMIRResourceOpSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
mir_resource_op_names_sorted="$(
    printf '%s\n' "$mir_resource_op_names" | sort
)"
if [[ "$mir_resource_op_names" != "$mir_resource_op_names_sorted" ]]; then
    echo "MIR resource op names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$mir_resource_op_names_sorted") \
        <(printf '%s\n' "$mir_resource_op_names") >&2 || true
    exit 1
fi
grep -Fq "slot_builtin_type_compare" "$ROOT_DIR/src/runtime/slot_type_utils.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/runtime/slot_type_utils.c"
grep -Fq "entry = find_free_entry_locked(manager," \
    "$ROOT_DIR/src/runtime/slot_manager_core_ops.c"
grep -Fq "manager->activeSlots >= manager->tableSize" \
    "$ROOT_DIR/src/runtime/slot_manager_core_ops.c"
grep -A18 -F "RosterAddParty(RosterContext* roster" \
    "$ROOT_DIR/src/runtime/world_roster.c" | \
    grep -Fq "ownedSlotName = world_roster_strdup(slotName)"
grep -A18 -F "WorldAddRoster(WorldContext* world" \
    "$ROOT_DIR/src/runtime/world_roster.c" | \
    grep -Fq "ownedRosterType ="
grep -Fq "world_roster_plan_stats.c" "$ROOT_DIR/Makefile"
if grep -Fq '#include "world_roster_plan_stats.h"' "$RUNTIME_INDEX"; then
    echo "[perf-contract] world roster plan/stats regressed to implementation header include" >&2
    exit 1
fi
grep -Fq 'world name allocation failed' "$ROOT_DIR/src/runtime/world_roster_plan_stats.c"
grep -Fq 'party plan allocation failed' "$ROOT_DIR/src/runtime/world_roster_plan_stats.c"
grep -Fq 'role stats allocation failed' "$ROOT_DIR/src/runtime/world_roster_plan_stats.c"
grep -Fq "transpiler_overlay_host_fields.c" "$ROOT_DIR/Makefile"
grep -Fq "bool current_class_has_field(" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_host_fields.h"
if grep -Fq "static bool current_class_has_field" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_host_fields.h"; then
    echo "[perf-contract] overlay host-field checks regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_mir_reason.c" "$ROOT_DIR/Makefile"
grep -Fq "void transpiler_mir_reasonf(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_reason.h"
if grep -Fq "static void" "$ROOT_DIR/src/codegen/transpiler_mir_reason.h"; then
    echo "[perf-contract] MIR reason formatting regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_collection_runtime_suffix.c" "$ROOT_DIR/Makefile"
grep -Fq "bool collection_runtime_suffix_copy(" \
    "$ROOT_DIR/src/codegen/transpiler_collection_runtime_suffix.h"
if grep -Fq "static bool" \
    "$ROOT_DIR/src/codegen/transpiler_collection_runtime_suffix.h"; then
    echo "[perf-contract] collection runtime suffix regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_stdlib_scalar_builtin.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_call_stdlib_scalar_builtin(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_builtin.h"
if grep -Fq "static char *" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_builtin.h"; then
    echo "[perf-contract] scalar stdlib lowering regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_unary_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "emit_unary(ASTNode *expr, TranspilerCtx *ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_unary_emit.c"
grep -Fq "C backend: unary expression could not lower operand expression" \
    "$ROOT_DIR/src/codegen/transpiler_expr_unary_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" \
    "$ROOT_DIR/src/codegen/transpiler_expr_unary_emit.c"
if grep -Fq "emit_unary(ASTNode *expr" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.h"; then
    echo "[perf-contract] unary expression lowering regressed to dispatch header" >&2
    exit 1
fi
grep -Fq "C backend: Clone requires one value argument" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.c"
! grep -Fq "return pergyra_strdup(\"0\")" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.c"
grep -Fq "transpiler_expr_literal_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_literal_expression(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_literal_emit.h"
! grep -Fq "return pergyra_strdup(\"0\")" \
    "$ROOT_DIR/src/codegen/transpiler_expr_literal_emit.c"
if grep -Fq "static char *" \
    "$ROOT_DIR/src/codegen/transpiler_expr_literal_emit.h"; then
    echo "[perf-contract] literal expression lowering regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_party_instance_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_party_instance_expr(ASTNode *node, TranspilerCtx *ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_party_instance_emit.h"
if grep -Fq "static char *" \
    "$ROOT_DIR/src/codegen/transpiler_expr_party_instance_emit.h"; then
    echo "[perf-contract] party instance expression lowering regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_stdlib_collection_builtin.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_call_stdlib_collection_builtin(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.h"
if grep -Fq "static char *" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.h"; then
    echo "[perf-contract] collection stdlib lowering regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_core_builtins_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.h"
grep -Fq "emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
if grep -Fq "static char *" \
    "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.h"; then
    echo "[perf-contract] core builtin lowering regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_projection_builtin.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_builtin_to_dto(ASTNode *call, TranspilerCtx *ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_projection_builtin.h"
grep -Fq "totobject_unsupported" "$ROOT_DIR/src/codegen/transpiler_expr_projection_builtin.c"
! grep -Fq "/* ToTObject:" "$ROOT_DIR/src/codegen/transpiler_expr_projection_builtin.c"
if grep -Fq "emit_builtin_to_dto(ASTNode *call" \
    "$ROOT_DIR/src/codegen/transpiler_helpers_core_b.h"; then
    echo "[perf-contract] projection conversion lowering regressed to helper shim" >&2
    exit 1
fi
grep -Fq "transpiler_generic_param_query.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_generic_class_naming.c" "$ROOT_DIR/Makefile"
grep -Fq "bool transpiler_func_has_generic_params(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.h"
grep -Fq "bool transpiler_class_has_generic_params(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.h"
grep -Fq "transpiler_class_has_generic_params(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.c"
if grep -Fq "func_has_generic_params(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_helpers_core_b.h"; then
    echo "[perf-contract] generic parameter query regressed to helper shim" >&2
    exit 1
fi
if grep -Fq "class_has_generic_params(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    echo "[perf-contract] generic class parameter query regressed to implementation header" >&2
    exit 1
fi
grep -Fq "bool transpiler_generic_class_method_name(" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_naming.h"
grep -Fq "transpiler_generic_class_method_name(char *out" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_naming.c"
grep -Fq "transpiler_generic_class_format_too_long(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_naming.c"
grep -Fq "char *transpiler_generic_class_specialization_name(" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_naming.h"
grep -Fq "transpiler_generic_class_specialization_name(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_naming.c"
grep -Fq "transpiler_mangled_name.h" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_naming.c"
grep -Fq "transpiler_generic_specialization_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "ensure_generic_specialization(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c"
grep -Fq "#include \"transpiler_mangled_name.h\"" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c"
if grep -Fq "static const char *" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.h"; then
    echo "[perf-contract] generic specialization ensure regressed to implementation header" >&2
    exit 1
fi
if grep -Fq "append_mangled_type_name(CodeBuf" \
    "$ROOT_DIR/src/codegen/transpiler_helpers_core_a.h"; then
    echo "[perf-contract] mangled-name declaration regressed to helper shim" >&2
    exit 1
fi
if awk '
    prev == "static bool" && $0 ~ /^transpiler_generic_class_method_name/ {
        found = 1
    }
    { prev = $0 }
    END { exit found ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    echo "[perf-contract] generic class naming regressed to implementation header" >&2
    exit 1
fi
if awk '
    prev == "static void" && $0 ~ /^transpiler_generic_class_format_too_long/ {
        found = 1
    }
    { prev = $0 }
    END { exit found ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    echo "[perf-contract] generic class naming diagnostics regressed to implementation header" >&2
    exit 1
fi
if grep -Fq "append_mangled_type_name(" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    echo "[perf-contract] generic class specialization key mangling regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_generic_binding_query.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_infer_generic_call_bindings" \
    "$ROOT_DIR/src/codegen/transpiler_generic_binding_query.h"
grep -Fq "TranspilerGenericBindingSnapshot" \
    "$ROOT_DIR/src/codegen/transpiler_generic_binding_query.h"
grep -Fq "transpiler_generic_binding_snapshot(ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_binding_query.c"
grep -Fq "transpiler_generic_binding_restore(ctx, snapshot)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_binding_query.c"
grep -Fq "transpiler_generic_binding_restore(ctx, generic_binding_snapshot)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c"
grep -Fq "transpiler_generic_binding_restore(ctx, snapshot.generic_binding)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "void     codebuf_truncate(CodeBuf *buf, size_t len)" \
    "$ROOT_DIR/src/codegen/transpiler.h"
grep -Fq "codebuf_truncate(CodeBuf *buf, size_t len)" \
    "$ROOT_DIR/src/codegen/transpiler_context.c"
grep -Fq "TranspilerGenericClassSpecSnapshot" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "transpiler_generic_class_spec_rollback(ctx, spec_snapshot)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "codebuf_truncate(ctx->helpers, snapshot.helpers_len)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
if grep -Fq "ctx->generic_class_spec_count--" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; then
    echo "[perf-contract] generic class specialization rollback bypassed transaction owner" >&2
    exit 1
fi
for generic_binding_file in \
    "$ROOT_DIR/src/codegen/transpiler_generic_specialization_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"; do
    if grep -Fq "ctx->generic_binding_count = saved_binding_count" \
        "$generic_binding_file"; then
        echo "[perf-contract] generic binding restore bypassed snapshot owner in $generic_binding_file" >&2
        exit 1
    fi
done
grep -Fq "transpiler_domain_role_ability_names.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_hosted_method_body_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "bool transpiler_role_ability_host_method_name(" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_names.h"
grep -Fq "transpiler_role_ability_host_method_name(char *out" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_names.c"
if grep -Fq "transpiler_role_ability_host_method_name(char *out" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.h"; then
    echo "[perf-contract] role/ability naming regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_enum_method_names.c" "$ROOT_DIR/Makefile"
grep -Fq "bool transpiler_enum_method_emit_name(" \
    "$ROOT_DIR/src/codegen/transpiler_enum_method_names.h"
grep -Fq "transpiler_enum_method_surface_desc(char *out" \
    "$ROOT_DIR/src/codegen/transpiler_enum_method_names.c"
if grep -Fq "transpiler_enum_method_surface_desc(char *out" \
    "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.h"; then
    echo "[perf-contract] enum method naming regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_call_user_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_call_user_function(" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.h"
grep -Fq "emit_call_user_function(ASTNode *call" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "transpiler_call_subject_arg_policy.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_call_arg_needs_subject_address(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_call_subject_arg_policy.c"
grep -Fq "transpiler_call_arg_is_subject_ref(TranspilerCtx *ctx" \
    "$ROOT_DIR/src/codegen/transpiler_call_subject_arg_policy.c"
if grep -Fq "is_pointer_self_host_type_name(ctx, ptn)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"; then
    echo "[perf-contract] subject argument policy regressed to user-call emitter" >&2
    exit 1
fi
if grep -Fq "static char *emit_call_user_function" \
    "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.h"; then
    echo "[perf-contract] user-call lowering regressed to implementation header" >&2
    exit 1
fi
if grep -Fq "infer_generic_call_bindings(TranspilerCtx" \
    "$ROOT_DIR/src/codegen/transpiler_func_forward_helpers.h"; then
    echo "[perf-contract] generic call binding regressed to forward helper" >&2
    exit 1
fi
if grep -Fq "infer_generic_call_bindings(TranspilerCtx" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c"; then
    echo "[perf-contract] generic call binding regressed to collection support" >&2
    exit 1
fi
if grep -Fq "render_type_name_with_bindings(TranspilerCtx" \
    "$ROOT_DIR/src/codegen/transpiler_specialization_registry.h"; then
    echo "[perf-contract] generic binding render regressed to specialization header" >&2
    exit 1
fi
grep -Fq "transpiler_channel_type_query.c" "$ROOT_DIR/Makefile"
grep -Fq "bool channel_inner_type_name_copy(" \
    "$ROOT_DIR/src/codegen/transpiler_channel_type_query.h"
grep -Fq "transpiler_channel_type_query.h" \
    "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "transpiler_expr_infer_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.h"
if grep -Fq "static bool" \
    "$ROOT_DIR/src/codegen/transpiler_type_mapping_helpers.h"; then
    echo "[perf-contract] channel type query regressed to type-mapping helper header" >&2
    exit 1
fi
if grep -Fq "channel_inner_type_name_copy(TranspilerCtx" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c"; then
    echo "[perf-contract] channel type query regressed to collection support local copy" >&2
    exit 1
fi
if grep -Fq "transpiler_collection_infer_expression_type_name" \
    "$CODEGEN_INDEX"; then
    echo "[perf-contract] expression type inference regressed to collection-named API" >&2
    exit 1
fi
grep -Fq "transpiler_domain_provenance_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "void emit_zone_projection_sync_loop_from_mir_refresh_view(" \
    "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.h"
grep -Fq "mir_method = llvm_mir_decl_method_routine(ctx, method_meta)" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"
if grep -Fq "mir_method = llvm_hosted_method_view_routine(" \
    "$ROOT_DIR/src/codegen/llvm_domain_method_emit.c"; then
    echo "[perf-contract] LLVM domain method emission reintroduced hosted method view routine wrapper" >&2
    exit 1
fi
grep -Fq "mir_method = llvm_mir_decl_method_routine(ctx, method_meta)" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
if grep -Fq "mir_method = llvm_hosted_method_view_routine(" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"; then
    echo "[perf-contract] LLVM role method emission reintroduced hosted method view routine wrapper" >&2
    exit 1
fi
grep -Fq "mir_method = transpiler_mir_decl_method_routine(ctx, method_meta)" \
    "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c"
if grep -Fq "mir_method = transpiler_hosted_method_view_routine(" \
    "$ROOT_DIR/src/codegen/transpiler_hosted_method_body_emit.c"; then
    echo "[perf-contract] C hosted method emission reintroduced hosted method view routine wrapper" >&2
    exit 1
fi
if grep -Fq "static void" \
    "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.h"; then
    echo "[perf-contract] domain provenance emission regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_domain_receiver_query.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_projection_sync.c" "$ROOT_DIR/Makefile"
grep -Fq "void emit_zone_action_effect_runtime(" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.h"
if grep -Fq "static " \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.h"; then
    echo "[perf-contract] projection sync emission regressed to implementation header" >&2
    exit 1
fi
grep -Fq "transpiler_resolve_world_zone_subject_receiver" \
    "$ROOT_DIR/src/codegen/transpiler_domain_receiver_query.h"
if grep -Fq "resolve_world_zone_subject_receiver(TranspilerCtx" \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.h"; then
    echo "[perf-contract] world/zone receiver query regressed to projection sync header" >&2
    exit 1
fi
grep -Fq "llvm_runtime_task_memory_decl.c" "$ROOT_DIR/Makefile"
grep -Fq "llvm_declare_runtime_task_memory(ctx)" "$ROOT_DIR/src/codegen/llvm_runtime.c"
grep -Fq "pgy_lane_spawn_dispatch_export" "$ROOT_DIR/src/codegen/llvm_runtime_task_memory_decl.c"
if grep -Eq "pgy_(spawn|async_spawn|spawn_blocking)_export" \
    "$ROOT_DIR/src/codegen/llvm_runtime_task_memory_decl.c"; then
    echo "[perf-contract] LLVM runtime decl reintroduced direct spawn executor export aliases" >&2
    exit 1
fi
grep -Fq "index_keys" "$ROOT_DIR/src/semantic/type_checker.h"
grep -Fq "metadata_index_capacity_is_valid" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_index.c"
grep -Fq "metadata_lookup_entry_index" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_index.c"
grep -Fq "metadata_index_insert" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_index.c"
grep -Fq "pgy_intent_observability_name_is_builtin" "$ROOT_DIR/src/common/intent_observability_abi.c"
grep -Fq "pgy_intent_observability_name_is_builtin" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "mir_instruction_is_intent_semantic_carrier" "$ROOT_DIR/src/compiler/mir_intent_fact.c"
grep -Fq "return mir_instruction_is_intent_semantic_carrier(inst);" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "return mir_instruction_is_intent_semantic_carrier(inst);" "$ROOT_DIR/src/compiler/mir_stmt_population_source.c"
grep -Fq "MIR DCE does not preserve user Intent-prefixed statements" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
! grep -Fq "strncmp(name, \"Intent\", 6)" "$ROOT_DIR/src/parser/ast_analysis.c"
! grep -Fq "strncmp(inst->name, \"Intent\", 6)" "$ROOT_DIR/src/compiler/mir_dce.c"
! grep -Fq "strncmp(inst->name, \"Intent\", 6)" "$ROOT_DIR/src/compiler/mir_stmt_population_source.c"
common_intent_obs_names="$(
    grep -o '"Intent[A-Za-z0-9_]*"' "$ROOT_DIR/src/common/intent_observability_abi.c" \
        | tr -d '"' \
        | grep -vx "Intent" \
        | sort -u
)"
common_intent_obs_names_in_order="$(
    grep -o '"Intent[A-Za-z0-9_]*"' "$ROOT_DIR/src/common/intent_observability_abi.c" \
        | tr -d '"' \
        | grep -vx "Intent"
)"
if [[ "$common_intent_obs_names_in_order" != "$common_intent_obs_names" ]]; then
    echo "common intent observability names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$common_intent_obs_names") \
        <(printf '%s\n' "$common_intent_obs_names_in_order") >&2 || true
    exit 1
fi
mir_intent_carrier_names="$(
    sed -n '/k_mir_intent_semantic_carrier_names\[\]/,/^};/p' \
        "$ROOT_DIR/src/compiler/mir_intent_fact.c" \
        | grep -o '"Intent[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
mir_intent_carrier_names_sorted="$(
    printf '%s\n' "$mir_intent_carrier_names" | sort
)"
if [[ "$mir_intent_carrier_names" != "$mir_intent_carrier_names_sorted" ]]; then
    echo "MIR intent semantic carrier names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$mir_intent_carrier_names_sorted") \
        <(printf '%s\n' "$mir_intent_carrier_names") >&2 || true
    exit 1
fi
parser_call_type_arg_names="$(
    sed -n '/parser_name_accepts_call_type_arguments/,/^}/p' \
        "$ROOT_DIR/src/parser/parser_expr.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
parser_call_type_arg_names_sorted="$(
    printf '%s\n' "$parser_call_type_arg_names" | sort
)"
if [[ "$parser_call_type_arg_names" != "$parser_call_type_arg_names_sorted" ]]; then
    echo "parser call type-argument names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$parser_call_type_arg_names_sorted") \
        <(printf '%s\n' "$parser_call_type_arg_names") >&2 || true
    exit 1
fi
parser_builtin_like_names="$(
    sed -n '/parser_name_is_builtin_like_identifier/,/^}/p' \
        "$ROOT_DIR/src/parser/parser_expr.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
parser_builtin_like_names_sorted="$(
    printf '%s\n' "$parser_builtin_like_names" | sort
)"
if [[ "$parser_builtin_like_names" != "$parser_builtin_like_names_sorted" ]]; then
    echo "parser builtin-like identifier names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$parser_builtin_like_names_sorted") \
        <(printf '%s\n' "$parser_builtin_like_names") >&2 || true
    exit 1
fi
parser_intent_value_type_names="$(
    sed -n '/intent_header_value_type_name/,/^}/p' \
        "$ROOT_DIR/src/parser/parser_intent_bindings.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
parser_intent_value_type_names_sorted="$(
    printf '%s\n' "$parser_intent_value_type_names" | sort
)"
if [[ "$parser_intent_value_type_names" != "$parser_intent_value_type_names_sorted" ]]; then
    echo "parser intent header value type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$parser_intent_value_type_names_sorted") \
        <(printf '%s\n' "$parser_intent_value_type_names") >&2 || true
    exit 1
fi
io_boundary_builtin_names="$(
    sed -n '/kPgyCompilerIOBoundaryBuiltinNames\[\]/,/^};/p' \
        "$ROOT_DIR/src/compiler/io_boundary_builtin.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
io_boundary_builtin_names_sorted="$(
    printf '%s\n' "$io_boundary_builtin_names" | sort
)"
if [[ "$io_boundary_builtin_names" != "$io_boundary_builtin_names_sorted" ]]; then
    echo "compiler IO boundary builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$io_boundary_builtin_names_sorted") \
        <(printf '%s\n' "$io_boundary_builtin_names") >&2 || true
    exit 1
fi
codegen_claim_slot_names="$(
    sed -n '/PgyCodegenClaimSlotSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c" \
        | grep -o '"Claim[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_claim_slot_names_sorted="$(
    printf '%s\n' "$codegen_claim_slot_names" | sort
)"
if [[ "$codegen_claim_slot_names" != "$codegen_claim_slot_names_sorted" ]]; then
    echo "codegen claim-slot builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_claim_slot_names_sorted") \
        <(printf '%s\n' "$codegen_claim_slot_names") >&2 || true
    exit 1
fi
codegen_slot_call_names="$(
    sed -n '/PgyCodegenSlotCallSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_slot_call_names_sorted="$(
    printf '%s\n' "$codegen_slot_call_names" | sort
)"
if [[ "$codegen_slot_call_names" != "$codegen_slot_call_names_sorted" ]]; then
    echo "codegen slot call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_slot_call_names_sorted") \
        <(printf '%s\n' "$codegen_slot_call_names") >&2 || true
    exit 1
fi
codegen_hashmap_key_names="$(
    sed -n '/pgy_hashmap_key_specs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c" \
        | grep -o '{ "[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_hashmap_key_names_sorted="$(
    printf '%s\n' "$codegen_hashmap_key_names" | sort
)"
if [[ "$codegen_hashmap_key_names" != "$codegen_hashmap_key_names_sorted" ]]; then
    echo "codegen HashMap key names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_hashmap_key_names_sorted") \
        <(printf '%s\n' "$codegen_hashmap_key_names") >&2 || true
    exit 1
fi
slot_runtime_fn_names="$(
    sed -n '/TranspilerMIRResourceOpSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c" \
        | grep -o '{ "[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
slot_runtime_fn_names_sorted="$(
    printf '%s\n' "$slot_runtime_fn_names" | sort
)"
if [[ "$slot_runtime_fn_names" != "$slot_runtime_fn_names_sorted" ]]; then
    echo "transpiler slot runtime op names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$slot_runtime_fn_names_sorted") \
        <(printf '%s\n' "$slot_runtime_fn_names") >&2 || true
    exit 1
fi
driver_diag_code_names="$(
    sed -n '/kDriverDiagCodeMaps\[\]/,/^};/p' \
        "$ROOT_DIR/src/compiler/driver_diag.c" \
        | grep -o 'PGY_CODE_[A-Z0-9_]*' \
        | grep -v '^PGY_CODE_$'
)"
driver_diag_code_names_sorted="$(
    printf '%s\n' "$driver_diag_code_names" | sort
)"
if [[ "$driver_diag_code_names" != "$driver_diag_code_names_sorted" ]]; then
    echo "driver diagnostic code map must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$driver_diag_code_names_sorted") \
        <(printf '%s\n' "$driver_diag_code_names") >&2 || true
    exit 1
fi
codegen_intent_obs_names="$common_intent_obs_names"
codegen_intent_obs_names_in_order="$common_intent_obs_names_in_order"
codegen_builtin_names="$(
    sed -n '/static const PgyBuiltinInfo entries\[\]/,/^    };/p' \
        "$ROOT_DIR/src/common/pgy_builtin_type_table.c" \
        | grep -o '{ "[^"]*"' \
        | sed 's/^{ "//'
)"
codegen_builtin_names_sorted="$(
    printf '%s\n' "$codegen_builtin_names" | sort
)"
if [[ "$codegen_builtin_names" != "$codegen_builtin_names_sorted" ]]; then
    echo "codegen builtin type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_builtin_names_sorted") \
        <(printf '%s\n' "$codegen_builtin_names") >&2 || true
    exit 1
fi
if [[ "$codegen_intent_obs_names_in_order" != "$codegen_intent_obs_names" ]]; then
    echo "codegen intent observability names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_intent_obs_names") \
        <(printf '%s\n' "$codegen_intent_obs_names_in_order") >&2 || true
    exit 1
fi
if [[ "$common_intent_obs_names" != "$codegen_intent_obs_names" ]]; then
    echo "intent observability builtin names drifted between common and codegen tables" >&2
    diff -u <(printf '%s\n' "$common_intent_obs_names") \
        <(printf '%s\n' "$codegen_intent_obs_names") >&2 || true
    exit 1
fi
for intent_obs_consumer in \
    "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_intent_observability.c" \
    "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"; do
    grep -Fq "pgy_intent_observability_" "$intent_obs_consumer"
done
resolver_builtin_names="$(
    sed -n '/k_builtin_entries\[\]/,/^};/p' \
        "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c" \
        | grep -o '{"[^"]*"' \
        | sed 's/^{"//'
)"
resolver_builtin_names_sorted="$(
    printf '%s\n' "$resolver_builtin_names" | sort
)"
if [[ "$resolver_builtin_names" != "$resolver_builtin_names_sorted" ]]; then
    echo "semantic builtin resolver names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$resolver_builtin_names_sorted") \
        <(printf '%s\n' "$resolver_builtin_names") >&2 || true
    exit 1
fi
for semantic_dispatch_file in \
    "$ROOT_DIR/src/semantic/type_checker_builtins_channel_state.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_state_tools.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_channel_transport.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_collections.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_body.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_map.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_scalar.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_variant.c" \
    "$ROOT_DIR/src/semantic/slot_analyzer_builtin.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_constructed.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_diagnostics.c"; do
    grep -Fq "bsearch(" "$semantic_dispatch_file"
done
for semantic_dispatch_file in \
    "$ROOT_DIR/src/semantic/type_checker_builtins_channel_state.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_state_tools.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_channel_transport.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_collections.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_body.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_map.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_scalar.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_variant.c" \
    "$ROOT_DIR/src/semantic/slot_analyzer_builtin.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_constructed.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_diagnostics.c"; do
    semantic_dispatch_names="$(
        sed -n '/static const .*\[\]/,/^};/p' "$semantic_dispatch_file" \
            | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
            | grep -o '"[A-Za-z0-9_]*"' \
            | tr -d '"' \
            || true
    )"
    semantic_dispatch_names_sorted="$(
        printf '%s\n' "$semantic_dispatch_names" | sort
    )"
    if [[ "$semantic_dispatch_names" != "$semantic_dispatch_names_sorted" ]]; then
        echo "semantic dispatch names must stay sorted for bsearch: $semantic_dispatch_file" >&2
        diff -u <(printf '%s\n' "$semantic_dispatch_names_sorted") \
            <(printf '%s\n' "$semantic_dispatch_names") >&2 || true
        exit 1
    fi
done
! grep -Eq '"Intent(Active|Current|History|Last|Recent)' \
    "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_intent_observability.c" \
    "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"
grep -Fq "ast_contains_identifier_call" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_decl_methods_contain_identifier_call" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_uses_intent_observability_surface" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_identity.c" "$ROOT_DIR/Makefile"
grep -Fq "uint32_t    stable_id;" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "ast_assign_stable_ids(program)" "$ROOT_DIR/src/parser/parser.c"
grep -Fq "uint32_t         source_stable_id;" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_instruction_source_stable_id" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "ast_node_stable_id(match_case)" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"
grep -Fq "ast_node_stable_id(loop)" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_mir_match_payload_alloca_name(uint32_t case_stable_id" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_pattern.c"
grep -Fq "mir_instruction_source_stable_id(inst)" \
    "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "mir_instruction_source_stable_id(inst)" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "transpiler_mir_match_binding_name(uint32_t case_stable_id" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_pattern_emit.c"
grep -Fq "mir_instruction_source_stable_id(inst)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "ast_node_stable_id(match_case)" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_pattern.c"
! grep -Fq "ast_node_stable_id(case_node)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_pattern_emit.c"
! grep -Fq "ast_node_stable_id(mir_instruction_source_payload(inst))" \
    "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
! grep -Fq "ast_node_stable_id(mir_instruction_source_payload(inst))" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "ast_node_stable_id(mir_instruction_source_payload(inst))" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "llvm_match_payload_alloca_name" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"
grep -Fq "llvm_for_loop_alloca_name" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_mir_match_payload_alloca_name" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_pattern.c"
grep -Fq "llvm_mir_emit_match_case_body_binding" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "transpiler_mir_emit_match_case_body_binding" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "llvm_mir_remap_active_match_bindings" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_case_true_region_contains" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_region.c"
grep -Fq "transpiler_mir_remap_active_match_bindings" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "transpiler_mir_case_true_region_contains" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_region_emit.c"
grep -Fq "llvm_mir_local_type_from_vars(" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
if grep -RIn -F "PGY_DEBUG_BINDING" "$ROOT_DIR/src/codegen"; then
    echo "[perf-contract] temporary binding debug output escaped into codegen" >&2
    exit 1
fi
grep -Fq "transpiler_mir_set_payload_binding_name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"
grep -Fq "transpiler_scratch_strdup(ctx, emitted_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"
grep -Fq "transpiler_mir_set_loop_binding_name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "transpiler_scratch_strdup(ctx, loop_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "transpiler_mir_find_backedge_loop_branch" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_policy.c"
grep -Fq "transpiler_mir_find_backedge_loop_branch(routine, block)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "llvm_mir_emit_for_loop_body_binding" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_find_backedge_range_loop_branch" \
    "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "transpiler_restore_local_binding_counts_local(ctx, saved_slot_count" \
    "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "transpiler_restore_local_binding_counts_local(ctx, saved_slot_count" \
    "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq "LLVMLexicalRegistrySnapshot" \
    "$ROOT_DIR/src/codegen/llvm_internal.h"
grep -Fq "llvm_lexical_registry_snapshot(LLVMGenCtx *ctx)" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_lexical_registry_restore(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
if awk '
    /pgy_lane_spawn_dispatch_export/ && parallel_runtime == 0 { parallel_runtime = NR }
    /pgy_async_detach_export/ && async_runtime == 0 { async_runtime = NR }
    /LLVMAddFunction\(ctx->module, fn_name, wrapper_type\)/ {
        if (parallel_add == 0) {
            parallel_add = NR
        } else if (async_add == 0) {
            async_add = NR
        }
    }
    END {
        ok = parallel_runtime > 0 && parallel_add > 0 \
             && parallel_runtime < parallel_add \
             && async_runtime > 0 && async_add > 0 \
             && async_runtime < async_add
        exit ok ? 1 : 0
    }
' "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
    echo "[perf-contract] LLVM parallel/async wrapper functions are created before runtime preflight" >&2
    exit 1
fi
for rel in \
    "src/codegen/llvm_mir_emit.c" \
    "src/codegen/llvm_stmt.c" \
    "src/codegen/llvm_stmt_loop_match.c" \
    "src/codegen/llvm_stmt_match.c" \
    "src/codegen/llvm_stmt_with.c" \
    "src/codegen/llvm_stmt_select.c" \
    "src/codegen/llvm_stmt_parallel_async.c"; do
    grep -Fq "LLVMLexicalRegistrySnapshot" "$ROOT_DIR/$rel" || {
        echo "[perf-contract] $rel must snapshot LLVM lexical side registries with scope frames" >&2
        exit 1
    }
    grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" "$ROOT_DIR/$rel" || {
        echo "[perf-contract] $rel must restore LLVM lexical side registries after scope pop" >&2
        exit 1
    }
    if grep -Fq "ctx->var_class_count = saved_var_class_count;" "$ROOT_DIR/$rel" ||
       grep -Fq "int saved_var_class_count = ctx->var_class_count;" "$ROOT_DIR/$rel"; then
        echo "[perf-contract] $rel must consume LLVMLexicalRegistrySnapshot instead of manual side-registry counters" >&2
        exit 1
    fi
done
if grep -Fq "payload_ty, option_binding)" "$ROOT_DIR/src/codegen/llvm_stmt_match.c" ||
   grep -Fq "payload_ty, result_binding)" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"; then
    echo "[perf-contract] LLVM match payload alloca regressed to source-name identity" >&2
    exit 1
fi
if grep -Fq "elem_ty, var_name)" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"; then
    echo "[perf-contract] LLVM for-loop binding alloca regressed to source-name identity" >&2
    exit 1
fi
if grep -Fq "payload_ty, binding)" "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"; then
    echo "[perf-contract] LLVM MIR match payload alloca regressed to source-name identity" >&2
    exit 1
fi
if grep -Fq "ctx, ctx->type_i32, variable)" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"; then
    echo "[perf-contract] LLVM MIR range loop alloca regressed to source-name identity" >&2
    exit 1
fi
grep -A3 -F "int32_t %s = %s" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c" | \
    grep -Fq "loop_name,"
if grep -Fq "codebuf_write(buf, \"int32_t %s = %s;\\n\", variable" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"; then
    echo "[perf-contract] C MIR range loop binding regressed to source-name emission" >&2
    exit 1
fi
grep -Fq "pgy_verified_projection_plan_intent_observability" "$ROOT_DIR/src/compiler/verified_projection_plan.c"
grep -Fq "PGY_PROJECTION_TARGET_C" "$ROOT_DIR/src/codegen/transpiler_entry.c"
grep -Fq "PGY_PROJECTION_TARGET_LLVM" "$ROOT_DIR/src/codegen/llvm_api.c"
if [[ "$(grep -Fc "llvm_apply_intent_observability_projection_plan(ctx)" "$ROOT_DIR/src/codegen/llvm_api.c")" -lt 2 ]]; then
    echo "LLVM IR and object codegen paths must both consume the verified projection plan" >&2
    exit 1
fi
! grep -RIn "pgy_mir_program_uses_intent_observability" \
    "$ROOT_DIR/src" --include='*.c' --include='*.h'
grep -Fq "return llvm_result_error_fmt_with_hints(" "$ROOT_DIR/src/codegen/llvm_api.c"
! grep -Eq "ast_|hir_|direct_calls|expr0|expr1|source_ast|mir_routine_inventory" \
    "$ROOT_DIR/src/compiler/verified_projection_plan.c"
grep -Fq "require_slot_token_name" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "token name synthesis is disabled" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
! grep -Fq "lookup_slot_token_name_or_default" "$CODEGEN_INDEX"
! grep -Fq "fallback_token" "$CODEGEN_INDEX"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "transpiler_find_block_binding_from_mir_insts" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "transpiler_pending_binding_from_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "transpiler_pending_binding_from_source_compatibility" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "const MIRRoutine *mir_routine" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "MIR pending-use materialization requires routine source-local facts" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "allow_ast_compat = mir_routine == NULL" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "binding.type_annotation" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "transpiler_mir_routine_source_local_type_name(mir_routine, base)" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "pending value '%s' is missing source-local type metadata" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "mir_routine, block, inst" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "source_node_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "out->initializer = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "transpiler_mir_ssa_local_limit_fail" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
grep -Fq "transpiler_mir_func_ssa_locals_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "PGY_CODE_MIR_SSA_LIMIT" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
grep -Fq "PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
if grep -F "ctx->backend_error = strdup_fmt" "$CODEGEN_INDEX" \
    | grep -E '/transpiler_mir[^/]*\.(c|h):'; then
    echo "C MIR emitters must route backend failures through diagnostic helpers" >&2
    exit 1
fi
if grep -F "ctx->backend_error = strdup_fmt" "$CODEGEN_INDEX"; then
    echo "C backend failures must route through diagnostic helpers" >&2
    exit 1
fi
if grep -E "ctx->backend_error[[:space:]]*=[^=]" "$CODEGEN_INDEX" \
    | grep -v "src/codegen/transpiler_context.c"; then
    echo "C backend must assign backend_error only inside transpiler_context.c" >&2
    exit 1
fi
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "has_source_location" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_map.c"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_map.c"
grep -Fq "mir_block_has_source_location(block)" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "source_ast;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_terminator_condition" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_terminator_value" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "source_terminator_kind" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "source_terminator_has_value" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "MIRStatementInventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "used_non_cfg_body_fallback" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "non_cfg_body_fallback_count" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "has_non_cfg_body_fallback_inventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "non_cfg_body_fallback_total" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "noncfg-fallbacks: total=%zu routines=%zu recorded=%s" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "MIR validator rejects stale program-level non-CFG fallback inventory" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "noncfg=%zu" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "mir_count_non_cfg_body_fallback_inventory" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "mir_count_non_cfg_body_fallback_inventory" "$ROOT_DIR/src/compiler/mir_public_surface.c"
if grep -A1 -E '^[[:space:]]*static[[:space:]]+void[[:space:]]*$' \
    "$ROOT_DIR/src/compiler/mir.c" \
    | grep -Fq "mir_count_non_cfg_body_fallback_inventory"; then
    echo "mir.c must consume fallback inventory counting from mir_public_surface.c" >&2
    exit 1
fi
grep -A1 -E '^[[:space:]]*void[[:space:]]*$' \
    "$ROOT_DIR/src/compiler/mir_public_surface.c" \
    | grep -Fq "mir_count_non_cfg_body_fallback_inventory"
! grep -Fq "mir_compute_non_cfg_fallback_inventory" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "mir_validate_non_cfg_fallback_state" "$ROOT_DIR/src/compiler/mir_program_validate.c"
grep -Fq "used non-CFG body fallback" "$ROOT_DIR/src/compiler/mir_program_validate.c"
grep -Fq "MIR validator rejects CFG-backed non-CFG body fallback state" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "fallback flag without fallback count" "$ROOT_DIR/src/compiler/mir_program_validate.c"
grep -Fq "MIR validator rejects non-CFG fallback flag without count" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "cleanup block %zu is not reachable" "$ROOT_DIR/src/compiler/mir_cfg_contract_cleanup_roots.c"
grep -Fq "MIR validator rejects unreachable cleanup root" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_e.cases.h"
grep -Fq "unreachable block[%zu] has exceptional successor" "$ROOT_DIR/src/compiler/mir_cfg_contract_validate.c"
grep -Fq "MIR validator rejects unreachable exceptional source" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_e.cases.h"
grep -Fq "air_mir_cleanup_root_is_valid" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "air_boundary_requires_mir_pin_cleanup_evidence(boundary)" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
! grep -Fq "boundary->kind == AIR_BOUNDARY_EXECUTION" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "routine->blocks[routine->cleanup_block].is_reachable" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "if (!block->is_reachable || block->is_cleanup)" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "AIR ignores unreachable MIR cleanup root evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR ignores unreachable MIR cleanup source evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "air_has_global_evidence_provider" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "air_has_global_evidence_provider" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_has_global_evidence_provider_subject" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_global_evidence_node_provider_subject" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_global_evidence_node_provider_subject" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
! grep -Fq "air_find_runtime_evidence" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
! grep -Fq "air_has_global_evidence_provider(" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
! grep -Fq "air_find_global_evidence_provider_subject" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "air_boundary_has_evidence_kind_provider" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
! grep -Fq "air_global_evidence_has_provider" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "air_evidence_node_matches_scope(" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_evidence_node_matches_subject(" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_evidence_node_matches_provider(" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "kBoundaryEvidencePolicies" "$ROOT_DIR/src/compiler/air_boundary_evidence_policy.c"
grep -Fq "air_boundary_requires_hir_routine_evidence" "$ROOT_DIR/src/compiler/air_boundary_evidence_policy.c"
grep -Fq "air_boundary_requires_hir_cfg_for_program" "$ROOT_DIR/src/compiler/air_boundary_evidence_policy.c"
grep -Fq "air_boundary_requires_hir_cfg_for_program(air, boundary)" "$ROOT_DIR/src/compiler/air_verify.c"
! grep -Fq "air_boundary_requires_hir_cfg_for_program(const AIRProgram" "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "air_boundary_requires_mir_pin_cleanup_evidence" "$ROOT_DIR/src/compiler/air_boundary_evidence_policy.c"
grep -Fq "air_boundary_has_evidence_kind_subject" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "air_boundary_missing_authority_evidence" "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "strict AIR requires every authorized participant" "$ROOT_DIR/src/compiler/air_verify.c"
grep -Fq "AIR synthesis collects all RIR authority evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR strict evidence requires all authority participants" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "air_collect_runtime_frontier_policy_evidence" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
grep -Fq "air_collect_singleton_runtime_evidence" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
grep -Fq "AIR singleton global evidence has conflicting counts" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
grep -Fq "PGY_FRONTIER_POLICY_SCHEMA" "$ROOT_DIR/src/runtime/pgy_frontier_policy.h"
grep -Fq "AIR rejects invalid runtime frontier policy provider" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR collects singleton global evidence idempotently" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR rejects conflicting singleton global evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "no matching MIR cleanup evidence" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
! grep -Fq "static char *rendered" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
! grep -Fq "static char rendered" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
! grep -Fq "transpiler_mir_copy_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_mir_arena_copy_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_mir_arena_render_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_expr_type_infer.c" "$ROOT_DIR/Makefile"
grep -Fq "transpiler_expr_infer_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.h"
! grep -Fq "static char" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.h"
grep -Fq "transpiler_infer_arena_copy_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "transpiler_infer_arena_format_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pergyra_type_to_c_copy" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "pergyra_ast_type_to_c_copy" "$ROOT_DIR/src/codegen/transpiler_type_render.c"
grep -Fq "pergyra_ast_type_to_c_copy" "$ROOT_DIR/src/codegen/transpiler_type_render.h"
! grep -Fq "const char *pergyra_ast_type_to_c" "$ROOT_DIR/src/codegen/transpiler_type_render.h"
! grep -Fq "pergyra_ast_type_to_c(ASTNode" "$ROOT_DIR/src/codegen/transpiler_func_forward_helpers.h"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, type_ast" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "param_type = ast_let_type(param)" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_type_alias.c"
grep -Fq "slot_inner_type_name_copy" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "slot_inner_type_name_write" "$ROOT_DIR/src/codegen/transpiler_type_name_utils.c"
grep -Fq "slot_inner_type_name_copy" "$ROOT_DIR/src/codegen/codegen_type_mapping.h"
! grep -Fq "const char *slot_inner_type_name" "$ROOT_DIR/src/codegen/codegen_type_mapping.h"
! grep -Fq "const char *slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler.h"
grep -Fq "lookup_slot_type_copy" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "lookup_slot_type_copy" "$ROOT_DIR/src/codegen/transpiler_symbols.h"
! grep -Fq "const char *lookup_slot_type" "$ROOT_DIR/src/codegen/transpiler_symbols.h"
grep -Fq "transpiler_resolve_slot_target_copy" "$ROOT_DIR/src/codegen/transpiler_slot_target.c"
grep -Fq "transpiler_resolve_slot_target_copy" "$ROOT_DIR/src/codegen/transpiler_slot_target.h"
grep -Fq "transpiler_resolve_device_slot_inner_copy_or_error" "$ROOT_DIR/src/codegen/transpiler_slot_target.c"
grep -Fq "transpiler_resolve_device_slot_inner_copy_or_error" "$ROOT_DIR/src/codegen/transpiler_slot_target.h"
! grep -Fq "bool transpiler_resolve_slot_target(TranspilerCtx" "$ROOT_DIR/src/codegen/transpiler_slot_target.h"
! grep -Fq "const char *transpiler_resolve_device_slot_inner_or_error" "$ROOT_DIR/src/codegen/transpiler_slot_target.h"
grep -Fq "transpiler_resolve_slot_target_copy(ctx, slot_arg" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "transpiler_resolve_slot_target(ctx, slot_arg" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "transpiler_resolve_device_slot_inner_or_error(ctx, slot_arg" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "row->call_shape" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "C source slot builtin %s requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_write_%s(%s, %s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_secure_write_%s(%s, %s, &%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_read_%s(%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_secure_read_%s(%s, &%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_release_%s(%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_secure_release_%s(%s, &%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "transpiler_contextual_option_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_option_context.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_option_context.c"
grep -Fq "transpiler_contextual_option_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_helpers_core_a.h"
grep -Fq "transpiler_contextual_option_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_contextual_option_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq "transpiler_contextual_option_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_option_context.c"
grep -Fq "lookup_future_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_future_type_query.h"
! grep -Fq "lookup_future_inner_type(" "$ROOT_DIR/src/codegen/transpiler_future_type_query.h"
grep -Fq "spawn_direct_callee_function_type" "$ROOT_DIR/src/semantic/type_checker_async_channel.c"
grep -Fq "type_function_param_type(callee_type, i)" "$ROOT_DIR/src/semantic/type_checker_async_channel.c"
grep -Fq "type_function_param_mode(callee_type, i)" "$ROOT_DIR/src/semantic/type_checker_async_channel.c"
if grep -Fq "type_check_func_resolve_param_type(param, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_async_channel.c"; then
    echo "[perf-contract] async spawn boundary re-resolved param types from FuncParam AST" >&2
    exit 1
fi
if grep -Fq "domain_resolve_type_ref(param->type, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_async_channel.c"; then
    echo "[perf-contract] async spawn boundary bypassed function signature type resolver" >&2
    exit 1
fi
grep -Fq "type_check_func_resolve_return_type(method, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr_host.c"
grep -Fq "type_check_func_resolve_param_type(param, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr_host.c"
grep -Fq "type_function_return_type(sym->type)" "$ROOT_DIR/src/semantic/type_checker_helpers_late.c"
grep -Fq "type_check_func_resolve_return_type(method, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr_ops.c"
grep -Fq "type_check_func_resolve_param_type(rhs_param, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr_ops.c"
grep -Fq "expr_host_resolve_class_field_type(field.type_ast, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr_host.c"
grep -Fq "semantic_host_resolve_type_ref(type_node, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr_host.c"
grep -Fq "semantic_host_resolve_type_ref(fields[i].type_ast, ctx)" "$ROOT_DIR/src/semantic/type_checker_class_decl.c"
grep -Fq "semantic_host_resolve_type_ref(" "$ROOT_DIR/src/semantic/type_checker_call_constructor.c"
grep -Fq "semantic_host_resolve_type_ref(for_type, ctx)" "$ROOT_DIR/src/semantic/type_checker_role_decl.c"
grep -Fq "type_check_func_resolve_param_type(param, ctx)" "$ROOT_DIR/src/semantic/type_checker_func_action_contract.c"
grep -Fq "type_check_func_resolve_param_type(param, ctx)" "$ROOT_DIR/src/semantic/type_checker_generic_support.c"
if grep -Fq "domain_resolve_type_ref(field->type, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_expr_host.c" \
    || grep -Fq "domain_resolve_type_ref(type_node, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_expr_host.c" \
    || grep -Fq "domain_resolve_type_ref(field->type, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_class_decl.c" \
    || grep -Fq "domain_resolve_type_ref(field_type_node, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_call_constructor.c" \
    || grep -Fq "domain_resolve_type_ref(for_type, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_role_decl.c"; then
    echo "[perf-contract] host-field consumers bypassed host metadata owner" >&2
    exit 1
fi
if grep -E '/[^/]*\.c:.*domain_resolve_type_ref\((param->type, ctx|ast_func_return_type)' \
    "$SEMANTIC_INDEX"; then
    echo "[perf-contract] function signature consumers bypassed type_check_func_types owner" >&2
    exit 1
fi
if grep -Fq "domain_resolve_type_ref(ast_func_return_type(method), ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_expr_call.c"; then
    echo "[perf-contract] member-call return consumers bypassed function signature owner" >&2
    exit 1
fi
if grep -Fq "domain_resolve_type_ref(rhs_param->type, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_expr_ops.c" \
    || grep -Fq "domain_resolve_type_ref(ast_func_return_type(method), ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_expr_ops.c"; then
    echo "[perf-contract] operator overload consumers bypassed function signature owner" >&2
    exit 1
fi
grep -Fq "semantic_event_resolve_type_ref" "$ROOT_DIR/src/semantic/type_checker_event.c"
grep -Fq "type_check_signature_resolve_type_ref(type_ref, ctx)" "$ROOT_DIR/src/semantic/type_checker_event.c"
if grep -Fq "domain_resolve_type_ref(ast_let_type(param), ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_event.c" \
    || grep -Fq "domain_resolve_type_ref(" \
        "$ROOT_DIR/src/semantic/type_checker_event.c"; then
    echo "[perf-contract] event signature validation bypassed event metadata owner" >&2
    exit 1
fi
grep -Fq "semantic_type_resolution_lookup_metadata_type_ref(" \
    "$ROOT_DIR/src/semantic/type_checker_generic_effective_args.c"
if grep -Fq "domain_resolve_type_ref(effective_nodes[i], ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_generic_effective_args.c"; then
    echo "[perf-contract] generic effective args reopened the broad domain resolver" >&2
    exit 1
fi
if grep -Fq "domain_resolve_type_ref(default_type, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_call_generic_where.c" \
    || grep -Fq "domain_resolve_type_ref(default_type, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_helpers_late.c"; then
    echo "[perf-contract] generic default type consumers reopened the broad domain resolver" >&2
    exit 1
fi
grep -Fq "collect_effective_generic_arg_types(" "$ROOT_DIR/src/semantic/type_checker_role_decl.c"
grep -Fq "ability_resolve_type_ref(arg, ctx)" "$ROOT_DIR/src/semantic/type_checker_role_decl.c"
if grep -Fq "domain_resolve_type_ref(effective_args[param_index], ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_role_decl.c" \
    || grep -Fq "domain_resolve_type_ref(arg, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_role_decl.c"; then
    echo "[perf-contract] role generic validation reopened the broad domain resolver" >&2
    exit 1
fi
grep -Fq "flow_resolve_type_ref(slot_type_node, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref)" \
    "$ROOT_DIR/src/semantic/type_checker_world_helpers.c"
if grep -Fq "domain_resolve_type_ref(slot_type_node, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_flow.c" \
    || grep -Fq "return domain_resolve_type_ref(type_ref, ctx)" \
        "$ROOT_DIR/src/semantic/type_checker_world_helpers.c"; then
    echo "[perf-contract] flow/world type-ref owners reopened the broad domain resolver" >&2
    exit 1
fi
grep -Fq "type_check_type_alias_stmt(node, ctx)" "$ROOT_DIR/src/semantic/type_checker.c"
grep -Fq "semantic_type_resolution_lookup_metadata_type_ref(ctx" \
    "$ROOT_DIR/src/semantic/type_checker_type_alias.c"
grep -Fq "type_check_namespace_decl(node, ctx)" "$ROOT_DIR/src/semantic/type_checker.c"
grep -Fq "type_check_unsafe_block(node, ctx)" "$ROOT_DIR/src/semantic/type_checker.c"
grep -Fq "type_check_defer_stmt(node, ctx)" "$ROOT_DIR/src/semantic/type_checker.c"
grep -Fq "type_check_parallel_block_flow(node, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_flow_parallel.c"
! grep -Fq "type_check_parallel_block_flow(node, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker.c"
grep -Fq "type_checker_namespace_decl.c" "$ROOT_DIR/Makefile"
grep -Fq "type_checker_unsafe_block.c" "$ROOT_DIR/Makefile"
grep -Fq "type_check_use_decl(node, ctx)" "$ROOT_DIR/src/semantic/type_checker.c"
! grep -Fq "validate_stdlib_use_decl(node, ctx)" "$ROOT_DIR/src/semantic/type_checker.c"
! grep -Fq "type_checker_stdlib_use_internal.h" "$ROOT_DIR/src/semantic/type_checker.c"
grep -Fq "type_check_use_stmt_flow(node, ctx)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "validate_stdlib_use_decl(node, ctx)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "type_checker_stdlib_use_internal.h" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! test -e "$ROOT_DIR/src/semantic/type_checker_stdlib_use_internal.h"
grep -Fq "static void" "$ROOT_DIR/src/semantic/type_checker_stdlib_use.c"
grep -Fq "type_checker_flow_statement_kinds.c" "$ROOT_DIR/Makefile"
grep -Fq "type_check_with_stmt_flow(node, ctx, loop_flow)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
grep -Fq "type_check_unsafe_block_flow(node, ctx, loop_flow)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
grep -Fq "type_check_defer_stmt_flow(node, ctx)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
grep -Fq "type_check_namespace_flow(node, ctx, loop_flow)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
grep -Fq "type_check_with_stmt_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "type_check_return_stmt_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "type_check_use_stmt_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "type_check_event_stmt_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "type_check_unsafe_block_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "type_check_defer_stmt_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
grep -Fq "type_check_namespace_flow(ASTNode *node" "$ROOT_DIR/src/semantic/type_checker_flow_statement_kinds.c"
! grep -Fq "ast_with_slot_type(node)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "ast_with_body(node)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "ast_unsafe_block_body(node)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "ast_defer_body(node)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "ast_namespace_statement_count(node)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "type_check_return_stmt(node, ctx)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "type_check_use_decl(node, ctx)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "type_check_event_subscription(node, ctx" "$ROOT_DIR/src/semantic/type_checker_flow.c"
! grep -Fq "type_check_event_invoke_stmt(node, ctx)" "$ROOT_DIR/src/semantic/type_checker_flow.c"
if grep -Fq "ast_defer_body(" "$ROOT_DIR/src/semantic/type_checker.c" \
   || grep -Fq "ast_unsafe_block_body(" "$ROOT_DIR/src/semantic/type_checker.c" \
   || grep -Fq "ast_namespace_statement" "$ROOT_DIR/src/semantic/type_checker.c"; then
    echo "[perf-contract] statement dispatcher reopened body traversal" >&2
    exit 1
fi
if grep -Fq "domain_resolve_type_ref(" \
    "$ROOT_DIR/src/semantic/type_checker_type_alias.c" \
    || grep -Fq "domain_resolve_type_ref(" \
        "$ROOT_DIR/src/semantic/type_checker.c"; then
    echo "[perf-contract] type-alias statement validation reopened the broad domain resolver" >&2
    exit 1
fi
grep -Fq "ownership_let_resolve_first_call_type_arg(init, ctx)" \
    "$ROOT_DIR/src/semantic/type_checker_ownership_destructure.c"
grep -Fq "semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(" \
    "$ROOT_DIR/src/semantic/type_checker_ownership_let_helpers.c"
if grep -Fq "domain_resolve_type_ref(" \
    "$ROOT_DIR/src/semantic/type_checker_ownership_destructure.c"; then
    echo "[perf-contract] destructure ClaimSlot generic validation reopened the broad domain resolver" >&2
    exit 1
fi
if grep -Fq "ast_create_type(inner_name)" \
    "$ROOT_DIR/src/semantic/type_checker_ownership_let_helpers.c"; then
    echo "[perf-contract] ClaimSlot generic name validation regressed to synthetic AST construction" >&2
    exit 1
fi
domain_resolver_consumers="$(
    grep -F "domain_resolve_type_ref(" "$SEMANTIC_INDEX" || true
)"
if [ -n "$domain_resolver_consumers" ]; then
    echo "[perf-contract] broad domain resolver API reappeared" >&2
    echo "$domain_resolver_consumers" >&2
    exit 1
fi
if grep -E 'domain_resolve_slot_type\(|domain_resolve_shared_type\(|domain_resolve_named_type_ref\(' \
    "$SEMANTIC_INDEX"; then
    echo "[perf-contract] domain metadata helper regressed to resolver naming" >&2
    exit 1
fi
grep -Fq "domain_lookup_slot_type_metadata" "$ROOT_DIR/src/semantic/type_checker_decls_domain_helpers.c"
grep -Fq "domain_lookup_shared_type_metadata" "$ROOT_DIR/src/semantic/type_checker_decls_domain_helpers.c"
grep -Fq "domain_lookup_named_type_metadata" "$ROOT_DIR/src/semantic/type_checker_decls_domain_helpers.c"
grep -Fq "intent_step_set_where_type_name" "$ROOT_DIR/src/semantic/type_checker_intent_helpers.c"
if grep -E '/type_checker_intent[^/]*\.c:.*ast_intent_step_set_where_type\(' \
    "$SEMANTIC_INDEX" | \
    grep -v "type_checker_intent_helpers.c"; then
    echo "[perf-contract] intent where inference bypassed shared provenance owner" >&2
    exit 1
fi
grep -Fq "channel_inner_type_name_copy" "$ROOT_DIR/src/codegen/transpiler_channel_type_query.h"
! grep -Fq "channel_inner_type_name(TranspilerCtx" "$ROOT_DIR/src/codegen/transpiler_type_mapping_helpers.h"
grep -Fq "select_channel_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "channel_inner_type_name_copy(ctx, channel" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "channel == NULL || channel->type != AST_IDENTIFIER" "$ROOT_DIR/src/codegen/transpiler_select.c"
! grep -Fq "lookup_typed_var(ctx, ast_identifier_name(channel))" "$ROOT_DIR/src/codegen/transpiler_select.c"
! grep -Fq "select_channel_inner_type(ASTNode" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
! grep -Fq "register_slot_var(ctx, p->name, slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "register_slot_var(ctx, p->name, slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "transpiler_return_option_ctor_lookup" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "pgy_codegen_match_variant_lookup(callee_name)" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "PGY_MATCH_VARIANT_SOME" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "PGY_MATCH_VARIANT_NONE_CTOR" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
! grep -Fq 'strcmp(callee_name, "Some")' "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
! grep -Fq 'strcmp(callee_name, "None")' "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
! grep -Fq 'strcmp(callee_name, "Some")' "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq 'strcmp(callee_name, "None")' "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
grep -Fq "transpiler_infer_slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
call_infer_owner="$ROOT_DIR/src/codegen/transpiler_expr_call_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_read(method_name)" "$call_infer_owner"
grep -Fq "pgy_codegen_call_name_is_write(method_name)" "$call_infer_owner"
grep -Fq "pgy_codegen_call_name_is_release(method_name)" "$call_infer_owner"
grep -Fq "pgy_codegen_call_name_is_read(name)" "$call_infer_owner"
grep -Fq "pgy_codegen_call_name_is_write(name)" "$call_infer_owner"
grep -Fq "pgy_codegen_call_name_is_release(name)" "$call_infer_owner"
! grep -Fq 'strcmp(method_name, "Read")' "$call_infer_owner"
! grep -Fq 'strcmp(method_name, "Write")' "$call_infer_owner"
! grep -Fq 'strcmp(method_name, "Release")' "$call_infer_owner"
! grep -Fq 'strcmp(name, "Read")' "$call_infer_owner"
! grep -Fq 'strcmp(name, "Write")' "$call_infer_owner"
! grep -Fq 'strcmp(name, "Release")' "$call_infer_owner"
grep -Fq "kTranspilerInferCallSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
grep -Fq "transpiler_infer_call_lookup" "$call_infer_owner"
grep -Fq "transpiler_infer_call_is_numeric_passthrough" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
grep -Fq "transpiler_infer_call_returns_channel_option" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*name[[:space:]]*,[[:space:]]*"(Min|Max|Abs|Clone|MapGet|MapKeys|ListGet|ViewRead|ViewWrite|Measure|QubitState|DeviceRead|SubmitDeviceRead|TryRecv|RecvTimeout|TrySendStatus|SendTimeoutStatus|Some|None|IsSome|IsNone|UnwrapOption|ToObject|ToTObject)"' \
    "$call_infer_owner"; then
    echo "[perf-contract] C expression type inference reintroduced direct builtin branch" >&2
    exit 1
fi
transpiler_infer_call_names="$(
    sed -n '/kTranspilerInferCallSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_infer_call_names_sorted="$(
    printf '%s\n' "$transpiler_infer_call_names" | sort
)"
if [[ "$transpiler_infer_call_names" != "$transpiler_infer_call_names_sorted" ]]; then
    echo "C expression type inference builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_infer_call_names_sorted") \
        <(printf '%s\n' "$transpiler_infer_call_names") >&2 || true
    exit 1
fi
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -A12 -F "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c" | \
    grep -Fq "channel_inner_type_name_copy"
if grep -A12 -F "case AST_CHANNEL_RECV" \
    "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c" | \
    grep -Fq "lookup_typed_var"; then
    echo "[perf-contract] C channel-recv type inference bypassed channel query owner" >&2
    exit 1
fi
! grep -Fq "return slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "constructed_arg_name_write" "$ROOT_DIR/src/codegen/transpiler_type_name_utils.c"
! grep -Fq "const char *constructed_arg_name_at" "$ROOT_DIR/src/codegen/codegen_type_mapping.h"
! grep -Fq "constructed_arg_name_at(const char" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
! grep -A12 -F "copy_constructed_arg_name_at" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" | grep -Fq "constructed_arg_name_at("
! grep -Fq "const char *pergyra_type_to_c" "$ROOT_DIR/src/codegen/transpiler.h"
! grep -Fq "pergyra_type_to_c(const char" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
! grep -A320 -F "pergyra_type_to_c_copy(const char *name" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" | grep -Fq "pergyra_type_to_c(name)"
grep -Fq "generic_args_to_c_suffix_copy" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "generic_args_to_c_suffix_write" "$ROOT_DIR/src/codegen/transpiler_type_name_utils.c"
grep -Fq "generic_args_to_c_suffix_copy" "$ROOT_DIR/src/codegen/transpiler_type_result_mapping_helpers.c"
! grep -Fq "const char *generic_args_to_c_suffix" "$ROOT_DIR/src/codegen/codegen_type_mapping.h"
! grep -Fq "generic_args_to_c_suffix(const char" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "collection_runtime_suffix_copy" "$ROOT_DIR/src/codegen/transpiler_collection_runtime_suffix.h"
! grep -Fq "collection_runtime_suffix(const char" "$ROOT_DIR/src/codegen/transpiler_collection_runtime_suffix.h"
! grep -Fq "collection_runtime_suffix(" "$CODEGEN_INDEX"
grep -Fq "lookup_enum_variant_qualified_name_copy" "$ROOT_DIR/src/codegen/transpiler_enum.c"
! grep -Fq "lookup_enum_variant_qualified_name(TranspilerCtx" "$ROOT_DIR/src/codegen/transpiler_enum.c"
! grep -Fq "static char qualified" "$ROOT_DIR/src/codegen/transpiler_enum.c"
! grep -Fq "static const char *bindings_buf" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "static ASTNode *binding_types_buf" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "const char *bindings_buf[8]" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "collection_runtime_suffix_copy(inner, suffix_buf" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "collection_runtime_suffix_copy(value, suffix_buf" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq "static char inner_buf[64]" "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
grep -Fq "kTranspilerBoxLetSpecs" "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
grep -Fq "transpiler_box_let_lookup" "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
! grep -Fq 'strcmp(callee_name, "BoxArray")' "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
! grep -Fq 'strcmp(callee_name, "Box")' "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
! grep -Fq 'strcmp(callee_name, "Rc")' "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
transpiler_box_let_names="$(
    sed -n '/static const TranspilerBoxLetSpec kTranspilerBoxLetSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_box_let_names_sorted="$(
    printf '%s\n' "$transpiler_box_let_names" | sort
)"
if [[ "$transpiler_box_let_names" != "$transpiler_box_let_names_sorted" ]]; then
    echo "C backend Box/Rc let builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_box_let_names_sorted") \
        <(printf '%s\n' "$transpiler_box_let_names") >&2 || true
    exit 1
fi
! grep -A70 -F 'strcmp(key, "String") == 0 && value != NULL' "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | grep -Fq "collection_runtime_suffix(value)"
! grep -A70 -F 'strcmp(callee_name, "ListNew")' "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | grep -Fq "collection_runtime_suffix(inner)"
! grep -A70 -F 'strcmp(callee_name, "QueueNew")' "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | grep -Fq "collection_runtime_suffix(inner)"
! grep -A24 -F "strcmp(init->data.call.callee->data.identifier.name, \"SetNew\")" "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | grep -Fq "collection_runtime_suffix(inner)"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, elem_inner" "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner_type_buf" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, init_type" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, init_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
grep -Fq "const MIRInstruction *inst" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.h"
grep -Fq "mir_instruction_destructure_binding_count(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
grep -Fq "transpiler_slot_runtime_fn(" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
! grep -Fq "pgy_claim_%s()" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
! grep -Fq "pgy_claim_secure_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
! grep -Fq "ast_let_destructure" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
grep -Fq "char inner_name_buf[128]" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "slot_inner_type_name_copy(effective_layout->abi_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "slot_inner_type_name_copy(typed_name, inner_name_buf" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner_name" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "char inner_buf[128]" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
grep -Fq "char elem_inner_buf[128]" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
grep -Fq "slot_inner_type_name_copy(resolved_type, inner_buf" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "slot_inner_type_name_copy(arr_type, inner_buf" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "transpiler_slot_runtime_fn(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
! grep -Fq "PgySlot_%s _c = pgy_claim_%s()" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
! grep -Fq "pgy_write_%s(&_c, pgy_read_%s(&%s))" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
channel_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"
grep -Fq "transpiler_channel_require_inner_type(ctx" "$channel_builtin_owner"
grep -Fq "pgy_channel_runtime_payload_has_abi(inner)" "$ROOT_DIR/src/codegen/transpiler_channel_type_query.c"
grep -Fq "PGY_INTENT_ACTIVE_INDEX_MAX" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_active_index_find_slot" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h"
grep -Fq "pgy_intent_active_index_set(handle, i)" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h"
grep -Fq "pgy_intent_find_active_registry_slot(handle)" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h"
grep -Fq "pgy_intent_active_index_find_slot_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c"
grep -Fq "pgy_intent_active_index_set_export(handle, i)" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c"
grep -Fq "pgy_intent_find_active_registry_slot_export(handle)" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c"
grep -Fq "AIR rejects MIR pin cleanup without global cleanup evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR DAG evidence contains unresolved metadata dead-end" "$ROOT_DIR/src/compiler/air_verify_global.c"
grep -Fq "sem.type_resolution_metadata_dead_ends = 2" "$AIR_TEST_INDEX"
! { grep -Fq "type_resolution_metadata_materializer_fallbacks" "$SEMANTIC_INDEX" \
    || grep -Fq "type_resolution_metadata_materializer_fallbacks" "$AIR_TEST_INDEX"; }
grep -Fq "return inst->expr0" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
grep -Fq "ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
! grep -Fq "source_statement_inventory" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
! grep -Fq "source_statement_inventory.items[inst->source_statement_index]" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
grep -Fq "mir_block_source_inventory_count(block)" "$ROOT_DIR/src/compiler/mir_stmt_population.c"
! grep -Fq "while (*stmt_index < block->source_statement_count)" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
! grep -Fq "source_statements;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_statement_count;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_hir_block;" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_block_source_inventory_at(block, s)" "$ROOT_DIR/src/compiler/mir_stmt_population.c"
grep -Fq "mir_block_source_inventory_items(block)" "$ROOT_DIR/src/compiler/mir_stmt_population.c"
grep -Fq "mir_validate_statement_inventory" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "source statement index %zu exceeds inventory count %zu" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "MIR validator rejects invalid statement inventory shape" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b_1.cases.h"
grep -Fq "mir_validate_instruction_surface_usage" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "source payload without surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "missing MIR initializer expression fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "mir_def_source_requires_initializer_fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "mir_instruction_has_surface_payload_or_shape" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "requires_source_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "inst->expr1 = ast_assignment_target(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "requires_source_local_decl_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_channel_receive_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_select_receive_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "is_select_case_body" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "is_select_case_body" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_source_branch_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "DEF is missing source-statement emit fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "channel receive DEF is missing source-statement receive emit fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "select receive DEF is missing select receive emit fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "branch is missing source-branch emit fact" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "source-statement emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
if grep -A8 -F "if (inst->requires_source_statement_emit" \
    "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c" | \
    grep -Fq "mir_instruction_source_payload"; then
    echo "[perf-contract] source-statement emit validation must consume MIR scalar/expression facts, not source payload" >&2
    exit 1
fi
grep -Fq "source-statement receive emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "select receive emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "source-local-decl emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "source-statement LET emit is missing local-decl fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "with-slot Claim resource op is missing MIR ABI type layout fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "with-slot Claim resource op has invalid MIR ABI type layout fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "MIR validator rejects invalid source-statement emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects missing channel receive emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
grep -Fq "MIR validator rejects invalid select receive emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
grep -Fq "MIR validator rejects invalid with-slot claim ABI fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
grep -Fq "MIR validator rejects invalid source-local-decl emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
grep -Fq "source-local-decl-emit" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "select-recv-stmt-emit" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "source-branch emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "MIR match branch uses captured pattern fact without payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
grep -Fq "MIR select dispatch branch uses channel fact without payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "mir_instruction_branch_requires_source_emit" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_has_source_payload" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_branch_payload_matches_shape" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_has_required_source_branch_emit_fact" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_has_required_branch_lowering_fact" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "inst->branch_shape == MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_capture_match_case_facts" "$ROOT_DIR/src/compiler/mir_branch_source_facts.h"
grep -Fq "mir_instruction_match_pattern_count(inst) == 0" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_match_pattern_count(inst)" "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "mir_instruction_match_pattern_count(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"
grep -Fq "MIR match branch requires captured pattern fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_i.cases.h"
grep -Fq "return inst->expr0 != NULL" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "&& inst->expr0 == NULL" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "rejected_predicate_without_subject_fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_h.cases.h"
grep -Fq "source payload without source-location fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "rejected_source_location" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b_1.cases.h"
! grep -Fq "mir_stmt_ast_is_cfg_owned_control(inst->ast)" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_terminator_matches" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_MATCH_CASE)" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_BLOCK)" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_stmt_has_side_effect_hint" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_source_node_type_stmt_has_side_effect_hint" "$ROOT_DIR/src/compiler/mir_source_shape.c"
if grep -A8 -F "mir_instruction_uses_source_statement_emit" \
    "$ROOT_DIR/src/compiler/mir_source_shape.c" | \
    grep -Fq "mir_instruction_source_payload"; then
    echo "[perf-contract] MIR source-statement emit predicate must consume source-location/expression facts, not source payload" >&2
    exit 1
fi
source_ast_eq_count="$(
    grep -F "source_node_type == expected_type" \
        "$ROOT_DIR/src/compiler/mir_source_shape.c" | wc -l
)"
if [ "$source_ast_eq_count" -ne 1 ]; then
    echo "[perf-contract] MIR source-shape AST type equality must stay in mir_instruction_source_matches_ast_type" >&2
    exit 1
fi
! grep -Fq "mir_branch_shape_requires_source_compatibility" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
! grep -Fq "mir_def_source_expression" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "missing MIR value expression fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "missing MIR terminator expression fact" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "mir_instruction_source_terminator_has_value(inst)" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "mir_instruction_source_terminator_matches(" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "mir_instruction_source_terminator_matches(" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "missing MIR body expression fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
! grep -Fq "&& inst->ast != NULL" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "mir_instruction_capture_source_provenance" "$ROOT_DIR/src/compiler/mir_source_shape.c"
! grep -Fq "ast_uses_thread_pool_surface(source_payload)" "$ROOT_DIR/src/compiler/mir_public_surface.c"
! grep -Fq "ast_uses_intent_observability_surface(source_payload)" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "ast_uses_thread_pool_surface(inst->expr0)" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "ast_uses_intent_observability_surface(inst->expr0)" "$ROOT_DIR/src/compiler/mir_public_surface.c"
! grep -Fq "llvm_mir_ast_type_is_cfg_container" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "llvm_mir_stmt_is_cfg_container" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "return llvm_mir_stmt_is_cfg_container(inst->ast)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_is_cfg_container(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_instruction_has_source_payload" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_has_required_branch_lowering_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_branch_requires_source_compatibility" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_source_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_source_local_decl_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_channel_receive_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_uses_select_receive_statement_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_source_resource_defs.c"
! grep -Fq "source_payload" "$ROOT_DIR/src/codegen/llvm_mir_source_resource_defs.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_def_uses_source_statement_compatibility" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
if grep -A28 -F "llvm_mir_def_uses_source_statement_emit" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" | \
    grep -Fq "mir_instruction_source_payload"; then
    echo "[perf-contract] LLVM MIR DEF emit predicates must consume MIR emit facts, not source payload presence" >&2
    exit 1
fi
grep -Fq "mir_instruction_uses_source_statement_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_emit_channel_receive_def(inst, ctx" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "LLVM channel receive DEF requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_emit_mir_destructure_inst" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
grep -Fq "llvm_emit_mir_destructure_inst(inst, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_emit_assignment_parts" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.h"
grep -Fq "llvm_emit_assignment_parts(inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
if awk '
    /case MIR_INST_DESTRUCTURE:/ { in_case=1; next }
    in_case && /case MIR_INST_ASSIGN:/ { in_case=0 }
    in_case && /source_payload/ { bad=1 }
    END { exit bad ? 0 : 1 }
    ' "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"; then
    echo "[perf-contract] LLVM MIR destructure emission must consume MIR destructure facts, not source payload" >&2
    exit 1
fi
if awk '
    /case MIR_INST_ASSIGN:/ { in_case=1; next }
    in_case && /case MIR_INST_STMT:/ { in_case=0 }
    in_case && /source_payload/ { bad=1 }
    END { exit bad ? 0 : 1 }
    ' "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"; then
    echo "[perf-contract] LLVM MIR assignment emission must consume MIR assignment facts, not source payload" >&2
    exit 1
fi
grep -Fq "LLVM MIR STMT source-payload emission is retired" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_reemit_is_redundant(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_call_emit_is_allowed(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_runtime_boundary_emit_is_allowed(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_reemit_is_redundant" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_stmt_call_emit_is_allowed" "$ROOT_DIR/src/compiler/mir_source_shape.c"
if grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_CALL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"; then
    echo "[perf-contract] MIR STMT call emission must consume the MIR source-shape owner, not backend-local AST_CALL checks" >&2
    exit 1
fi
if grep -A44 -F "case MIR_INST_STMT:" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" | \
    grep -Fq "source_payload"; then
    echo "[perf-contract] LLVM MIR STMT emission must consume MIR expr/source-shape facts, not source payload" >&2
    exit 1
fi
if grep -B16 -F "llvm_emit_statement(inst->expr0, ctx)" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" | \
    grep -Eq "AST_(SPAWN_EXPR|AWAIT_EXPR|CHANNEL_SEND|CHANNEL_RECV|EVENT_SUBSCRIBE|EVENT_UNSUBSCRIBE|EVENT_INVOKE|PARALLEL_BLOCK|ASYNC_BLOCK|UNSAFE_BLOCK|TRANSACTION_BLOCK)"; then
    echo "[perf-contract] LLVM MIR STMT runtime-boundary emission must consume the MIR source-shape owner, not a backend-local AST list" >&2
    exit 1
fi
grep -Fq "llvm_emit_statement(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "STMT source-payload emission is retired" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_reemit_is_redundant(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_call_emit_is_allowed(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "mir_instruction_source_stmt_runtime_boundary_emit_is_allowed(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
if grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_BLOCK)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c" \
    || grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_RETURN)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"; then
    echo "[perf-contract] C MIR STMT redundant re-emit policy must consume the MIR source-shape owner, not backend-local AST block/return checks" >&2
    exit 1
fi
if grep -A72 -F "if (inst->kind != MIR_INST_STMT)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c" | \
    grep -Eq "stmt (==|!=)|stmt->|transpiler_mir_find_stmt_for_inst|source_payload"; then
    echo "[perf-contract] C MIR STMT emission must consume MIR expr/source-shape facts, not source payload" >&2
    exit 1
fi
if grep -B16 -F "emit_statement(inst->expr0, ctx)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c" | \
    grep -Eq "AST_(SPAWN_EXPR|AWAIT_EXPR|CHANNEL_SEND|CHANNEL_RECV|EVENT_SUBSCRIBE|EVENT_UNSUBSCRIBE|EVENT_INVOKE|PARALLEL_BLOCK|ASYNC_BLOCK|UNSAFE_BLOCK|TRANSACTION_BLOCK)"; then
    echo "[perf-contract] C MIR STMT runtime-boundary emission must consume the MIR source-shape owner, not a backend-local AST list" >&2
    exit 1
fi
! grep -Fq "llvm_emit_statement(inst->ast" "$CODEGEN_INDEX"
grep -Fq "llvm_emit_option_coalesce" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "coalesce.fallback" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "LLVMBuildPhi(ctx->builder, fields[1]" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
if grep -A95 -F "llvm_emit_option_coalesce" \
    "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c" | \
    grep -Fq "LLVMBuildSelect"; then
    echo "[perf-contract] LLVM coalescing lowering regressed to eager select fallback" >&2
    exit 1
fi
grep -Fq "has two successors without a branch condition terminator" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "LLVMConstInt(LLVMInt1TypeInContext(" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "transpiler_mir_inst_is_cfg_container" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "bool transpiler_mir_inst_is_cfg_container(const MIRInstruction *inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "mir_instruction_source_is_cfg_container(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
if grep -RIn -- "mir_instruction_source_payload" "$ROOT_DIR/src/codegen" >/dev/null; then
    echo "[perf-contract] Backend MIR emission reopened source payload; consume MIR facts or route provenance through source-shape owners" >&2
    grep -RIn -- "mir_instruction_source_payload" "$ROOT_DIR/src/codegen" >&2
    exit 1
fi
! grep -Fq "mir_instruction_source_payload(inst) == stmt" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
! grep -Fq "static bool" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_seed_block_phi_names" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_emit_mir_assignment_expr_stmt" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "transpiler_emit_assignment_expression_parts" "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.h"
grep -Fq "transpiler_mir_def_is_source_assignment_emit" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "source-statement assignment emit is missing target fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "ASSIGN is missing MIR assignment expression facts" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "source-local slot let" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
grep -Fq "source-local channel let" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
grep -Fq "entry->is_view" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_names.c"
! grep -Fq "emit_statement(stmt, ctx)" "$CODEGEN_INDEX"
! grep -Fq "llvm_emit_statement(source_payload, ctx)" "$CODEGEN_INDEX"
grep -Fq "mir_instructions_share_source_statement(resource_inst, stmt_inst)" "$ROOT_DIR/src/codegen/transpiler_mir_stmt_emit.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/transpiler_mir_stmt_emit.c"
! grep -Fq "mir_instruction_source_matches_ast_node" "$SRC_INDEX"
grep -Fq "missing receive emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "missing select receive emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "transpiler_emit_mir_assignment_def_inst" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
! grep -Fq "static TranspilerMIRAssignmentEmitResult" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
if awk '
    /if \(inst->kind == MIR_INST_ASSIGN\)/ { in_block=1; next }
    in_block && /continue;/ { in_block=0 }
    in_block && /stmt ==|stmt->|source_payload|transpiler_mir_find_stmt_for_inst/ { bad=1 }
    END { exit bad ? 0 : 1 }
    ' "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"; then
    echo "[perf-contract] C MIR assignment emission must consume MIR assignment facts, not source payload statements" >&2
    exit 1
fi
grep -Fq "!mir_instruction_uses_source_statement_emit(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "mir_instruction_source_is_defer_stmt(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_is_defer_stmt" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_stmt_runtime_boundary_emit_is_allowed" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "inst->expr1 = ast_let_type(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->expr0 = ast_defer_body(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->arg0 = ast_let_name(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->requires_source_statement_emit = true" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "stale thread-pool surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "stale intent observability surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "inventory surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "inventory_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_inventory_surface_usage_summary" "$ROOT_DIR/src/compiler/mir_surface_usage.h"
grep -Fq "mir_program_has_inventory_surface_usage_facts" "$ROOT_DIR/src/compiler/mir_surface_usage.h"
grep -Fq "mir_program_recorded_inventory_uses_thread_pool_surface" "$ROOT_DIR/src/compiler/mir_surface_usage.h"
grep -Fq "mir_program_recorded_inventory_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir_surface_usage.h"
grep -Fq "summary = mir_inventory_surface_usage_summary(mir)" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "mir_program_has_inventory_surface_usage_facts(mir)" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "mir_program_recorded_inventory_uses_thread_pool_surface(mir)" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "mir_program_recorded_inventory_uses_intent_observability_surface(mir)" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "PGY_PROJECTION_RUNTIME_OBS0" "$ROOT_DIR/src/compiler/verified_projection_plan.h"
grep -Fq "PGY_PROJECTION_RUNTIME_OBS1" "$ROOT_DIR/src/compiler/verified_projection_plan.h"
grep -Fq "mir_program_recorded_inventory_uses_thread_pool_surface(mir)" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "mir_instruction_source_stmt_has_side_effect_hint(inst)" "$ROOT_DIR/src/compiler/mir_dce.c"
! grep -Fq "mir_source_node_stmt_has_side_effect_hint(" "$ROOT_DIR/src/compiler/mir_dce.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "mir_source_node_type_stmt_has_side_effect_hint" "$ROOT_DIR/src/compiler/mir_source_shape.c"
if grep -A14 -F "mir_instruction_source_stmt_residual_emit_is_allowed" \
    "$ROOT_DIR/src/compiler/mir_source_shape.c" | \
    grep -Fq "mir_instruction_source_payload"; then
    echo "[perf-contract] residual STMT emit policy must use MIR source inventory facts, not source payload" >&2
    exit 1
fi
if grep -A8 -F "residual STMT emit is missing source statement inventory fact" \
    "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c" | \
    grep -Fq "mir_instruction_source_payload"; then
    echo "[perf-contract] residual STMT inventory validation must use source-location/order facts, not source payload" >&2
    exit 1
fi
grep -Fq "AST_FAIL_STMT" "$ROOT_DIR/src/compiler/mir_source_shape.c"
! grep -Fq "source_node_type" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "MIR DCE uses statement shape facts without AST payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "mir_stmt_ast_type_is_cfg_owned_control" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "if (!mir_program_has_inventory_surface_usage_facts(mir))" "$ROOT_DIR/src/compiler/verified_projection_plan.c"
! grep -Fq "mir->inventory_uses_" "$CODEGEN_INDEX"
! grep -Fq "mir->has_inventory_surface_usage_facts" "$CODEGEN_INDEX"
! grep -Eq "ast_|hir_|direct_calls|expr0|expr1|source_ast|mir_routine_inventory" \
    "$ROOT_DIR/src/compiler/verified_projection_plan.c"
grep -Fq "mir_instruction_has_source_location(inst)" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "mir_instruction_source_node_type_or(inst, AST_PROGRAM)" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "mir_instruction_source_inline_text(inst)" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "inst->ast->line" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "source_terminator_condition" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "source_terminator_value" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
! grep -Fq "source_statements[0]" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "has_source_statement_index" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "has_surface_usage_facts" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "uses_thread_pool_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "ast_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "MIR records intent observability surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b_1.cases.h"
grep -Fq "intent observability inventory surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b_1.cases.h"
grep -Fq "MIRRoutineInventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_routine_inventory_from_program" "$ROOT_DIR/src/compiler/mir_program_inventory.c"
grep -Fq "mir_routine_inventory_get" "$ROOT_DIR/src/compiler/mir_program_inventory.c"
grep -Fq "mir_program_recorded_inventory_uses_intent_observability_surface(mir)" \
    "$ROOT_DIR/src/compiler/verified_projection_plan.c"
grep -Fq "mir_routine_inventory_from_program(mir, &inventory)" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "branch_shape = mir_branch_shape_from_ast" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_branch_shape_name" "$ROOT_DIR/src/compiler/mir_names.c"
grep -Fq "source-branch-emit" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "inst.requires_source_branch_emit" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_instruction_record_surface_usage(MIRInstruction *inst)" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "mir_instruction_record_surface_usage(&inst);" "$ROOT_DIR/src/compiler/mir_base_helpers.c"
grep -Fq "return append_instruction(block, inst)" "$ROOT_DIR/src/compiler/mir_cleanup.c"
grep -Fq "return append_instruction(block, inst)" "$ROOT_DIR/src/compiler/mir_intent.c"
grep -Fq "inst.expr0 = ast" "$ROOT_DIR/src/compiler/mir_intent.c"
grep -Fq "expression payload fact" "$ROOT_DIR/src/compiler/mir_intent_fact.c"
grep -Fq "rejected_expr_payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b_1.cases.h"
grep -Fq "return inst->expr0" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "exprs[count++] = inst->expr0" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, alias, &var)" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "LLVMConstPointerNull(var.type)" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
! grep -Fq "LLVMVarEntry *var" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "return inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"
grep -Fq "exprs[count++] = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"
grep -Fq "llvm_build_mir_intent_step_sources" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "transpiler_build_mir_intent_step_sources" "$ROOT_DIR/src/codegen/transpiler_intent_emit_metadata_helpers.h"
grep -Fq "missing intent step source mapping" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "missing intent step source mapping" "$ROOT_DIR/src/codegen/transpiler_intent_emit.c"
! grep -Fq "collect_mir_intent_steps" "$CODEGEN_INDEX"
grep -Fq "has_surface_usage_facts" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "allow_fixture_payload_probe" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "block->instruction_count > 0 && block->instructions == NULL" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "allow_legacy_ast_probe" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "pgy_ast_uses_thread_pool(inst->ast" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "routine->hir_routine == NULL" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "pgy_mir_program_uses_thread_pool" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "mir_program_recorded_inventory_uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "pgy_mir_routine_uses_thread_pool" "$ROOT_DIR/src/codegen/thread_pool_usage.h"
grep -Fq "PGY_PROJECTION_TARGET_C" "$ROOT_DIR/src/codegen/transpiler_entry.c"
grep -Fq "pgy_mir_program_uses_thread_pool(ctx->mir)" "$ROOT_DIR/src/codegen/transpiler_inventory_view.c"
grep -Fq "transpiler_active_uses_thread_pool(ctx)" "$ROOT_DIR/src/codegen/transpiler_thread_pool.c"
grep -Fq "mir_program_main_function_name(ctx->mir)" "$ROOT_DIR/src/codegen/transpiler_inventory_view.c"
grep -Fq "transpiler_active_main_function_name(ctx)" "$ROOT_DIR/src/codegen/transpiler.c"
grep -Fq "transpiler_c_executable_emitted_name" "$ROOT_DIR/src/codegen/transpiler.c"
grep -Fq "__pgy_user_main_lowercase" "$ROOT_DIR/src/codegen/transpiler.c"
grep -Fq "PGY_PROJECTION_TARGET_LLVM" "$ROOT_DIR/src/codegen/llvm_api.c"
grep -Fq "pgy_mir_program_uses_thread_pool(ctx->mir)" "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
grep -Fq "mir_program_main_function_name(ctx->mir)" "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
grep -Fq "llvm_active_uses_thread_pool(ctx)" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "llvm_active_main_function_name(ctx)" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "__pgy_user_main_lowercase" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "LLVMValueRef saved_fn = ctx->current_function" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "LLVMTypeRef saved_ret = ctx->current_ret_type" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder)" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "restore_state:" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "ctx->current_function = saved_fn" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "ctx->current_ret_type = saved_ret" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq 'llvm_lookup_function(ctx, "Main")' "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
! grep -Fq 'lookup_or_declare_function(ctx, "Main"' "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
! grep -Fq 'main_user->name = "Main"' "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
! grep -Fq '|| (main_user != NULL)' "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "MIR-only LLVM path missing registered executable function '%s'" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "MIR-only LLVM path missing emitted top-level executable wrapper '__pgy_top_level_exec'" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "MIR-only C path missing registered executable function '%s'" "$ROOT_DIR/src/codegen/transpiler.c"
grep -Fq "MIR-only C path missing synthetic top-level executable function '__pgy_top_level_exec'" "$ROOT_DIR/src/codegen/transpiler.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "mir_instruction_has_required_source_branch_emit_fact(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "mir_instruction_source_branch_payload_matches_shape(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "transpiler_mir_render_select_case_condition(" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "branch_inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "llvm_mir_select_case_channel" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "llvm_mir_emit_select_dispatch_condition(source_payload" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_emit_select_dispatch_condition(" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "inst, routine, mir_block->succ_true, ctx" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
if awk '
    /^llvm_mir_emit_match_case_condition\(const MIRInstruction \*inst,/ { in_func=1 }
    in_func && /^llvm_mir_remap_payload_binding\(LLVMGenCtx \*ctx,/ { in_func=0 }
    in_func && /mir_instruction_source_payload/ { bad=1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"; then
    echo "[perf-contract] LLVM MIR match condition must consume captured pattern/guard facts, not source payload" >&2
    exit 1
fi
if awk '
    /^transpiler_mir_render_match_case_condition\(const MIRInstruction \*inst,/ { in_func=1 }
    in_func && /mir_instruction_source_payload/ { bad=1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"; then
    echo "[perf-contract] C MIR match condition must consume captured pattern/guard facts, not source payload" >&2
    exit 1
fi
grep -Fq "transpiler_mir_render_select_case_condition" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "emit_expression_with_ssa_map(channel, ctx, NULL)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_mir_expr_ssa.c"
grep -Fq "pgy_lane_channel_runtime_name(runtime_fn, sizeof(runtime_fn)," "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "\"ready\", inner" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "MIR select dispatch emits channel readiness in C backend" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "MIR select dispatch materializes bound receive local type" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "MIR select dispatch preserves implicit field channel lvalue" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "pgy_lane_channel_ready_Int(PGY_LANE_PINNED_ZONE, &self.ch)" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "transpiler_select_case_has_receive_binding" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "ASTNode *value = ast_assignment_value(node)" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "value->type == AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "ast_assignment_value(body) != NULL" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "ast_assignment_value(body)->type == AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "_pgy_ssa_v_1 = pgy_lane_channel_recv_val_Int(PGY_LANE_PINNED_ZONE, &ch)" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "\"\\nv = pgy_lane_channel_recv_val_Int(PGY_LANE_PINNED_ZONE, &ch)\"" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "condition = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "? inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "emit_expression_with_ssa_map(inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "emit_expression(inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "branch_shape == MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_has_required_branch_lowering_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_has_required_branch_lowering_fact(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "inst->source_node_type == AST_MATCH_CASE" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "inst->source_node_type == AST_BLOCK" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "branch_shape == MIR_BRANCH_SELECT_DISPATCH)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "llvm_mir_branch_has_required_condition_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast != NULL || inst->expr0 != NULL" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "val = llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "cond = llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "ASTNode *return_expr = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->expr0 != NULL ? inst->expr0 : inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_register_defer(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast->data.defer_stmt" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_is_with_slot_claim(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_local_initializer_expr(inst->expr0)" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
if grep -Fq "ASTNode *value_expr = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"; then
    echo "[perf-contract] LLVM MIR local emit bypassed initializer unwrap fact" >&2
    exit 1
fi
grep -Fq "llvm_mir_local_expected_type_name(routine, inst, NULL)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
if grep -Fq "ast_identifier_name(inst->expr1)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"; then
    echo "[perf-contract] LLVM MIR expected-type resolution reopened AST assignment target names" >&2
    exit 1
fi
grep -Fq "local_type = llvm_mir_local_type_from_source_fact(routine, ctx, name);" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "LLVM let binding '%s' requires concrete Array<T>/Slice<T> element metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "LLVM let binding '%s' Slice() initializer requires concrete element type metadata" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "if (mir_instruction_uses_source_local_decl_emit(inst))" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "source_local_fact = llvm_mir_local_source_fact(" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "alloca_type = llvm_mir_local_type_from_source_fact_entry(" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "ASTNode *type_expr = inst->requires_source_local_decl_emit" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "ASTNode *type_expr = inst->expr1" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "llvm_mir_type_from_ast(ctx, inst->expr1)" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "llvm_mir_type_from_ast(ctx, type_expr)" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "type_expr != NULL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "value_expr != NULL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "source_node_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->expr0 != NULL ? inst->expr0 : inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
grep -Fq "mir_instruction_is_with_slot_claim(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
grep -Fq "transpiler_emit_mir_resource_op_inst" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "static TranspilerMIRInstEmitResult" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "source_node_type == AST_WITH_STMT" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
! grep -Fq "inst->ast == NULL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "return inst->expr1" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "transpiler_emit_mir_resource_hook" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "static bool" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
grep -Fq "mir_instruction_source_is_local_decl" "$ROOT_DIR/src/compiler/mir_source_shape.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
! grep -Fq "inst->ast != NULL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
! grep -Fq "inst->ast->data.call" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
! grep -Fq "inst->ast->data.call" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
! grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_helpers.h"
! grep -Fq "inst->ast->data.call" "$ROOT_DIR/src/codegen/transpiler_helpers.h"
grep -Fq "transpiler_register_defer(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
! grep -Fq "stmt->data.defer_stmt.body" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
! grep -Fq "inst->ast->data.assignment" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
! grep -Fq "inst->ast->data.let_destructure" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
grep -Fq "ASTNode *payload_expr = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
! grep -Fq "transpiler_expr_identifiers_mapped(ctx, inst->ast" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
grep -Fq "branch instruction misses required lowering fact" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "inst->ast->data" "$CODEGEN_INDEX"
! grep -Fq "source_node_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
! grep -Fq "source_node_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent.h"
grep -Fq "mir_instruction_source_is_local_destructure(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
grep -Fq "mir_instruction_source_is_assignment(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
! grep -Fq "inst->ast->type" "$CODEGEN_INDEX"
! grep -Fq "transpiler_emit_mir_preserved_let_stmt" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
! grep -Fq "transpiler_emit_mir_preserved_let_stmt" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.h"
grep -Fq "transpiler_mir_preserved_let_emit.h" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
! grep -Fq "transpiler_mir_fallback_let_emit.h" "$CODEGEN_INDEX"
! grep -Fq "transpiler_emit_mir_fallback_let_stmt" "$CODEGEN_INDEX"
grep -Fq "silent true fallback is disabled" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "Some requires concrete payload type during C emission" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "C match lowering requires a concrete subject type; implicit Int match fallback is disabled" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "transpiler_match_emit_part" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "C match lowering could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "ctx->indent = saved_indent" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "strcmp(subject_type, \"Unknown\") == 0" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "subject_type = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "inner = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "ok_type = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "err_type = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "owned_type_name : \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "C array access requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
grep -Fq "C slice access requires concrete Slice<T> element metadata" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
grep -Fq "transpiler_array_access_emit_operand" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
grep -Fq "C backend: array access could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
grep -Fq "C tuple literal requires concrete tuple layout metadata" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "C sequence literal requires concrete Array/List/Queue<T> element metadata" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "C tuple literal could not lower element %zu" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "tests/cases/backend_compare/sequence_literal_list_queue" "$ROOT_DIR/tests/compare_backends.sh"
grep -Fq "C array literal could not lower element %zu" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "C slot SSA auto-read requires concrete Slot<T> payload metadata" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "row->call_shape" "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
grep -Fq "C slot operation %s requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/transpiler_slot_runtime_row.c"
! grep -Fq "pgy_write_%s(%s, %s)" "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c"
! grep -Fq "pgy_secure_write_%s(%s, %s, &%s)" "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c"
! grep -Fq "pgy_read_%s(%s)" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
! grep -Fq "pgy_secure_read_%s(%s, &%s)" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
! grep -Fq "pgy_read_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
! grep -Fq "pgy_secure_read_%s(&%s, &%s)" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "transpiler_slot_runtime_fn(" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "pgy_write_%s(%s, %s)" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "pgy_secure_write_%s(%s, %s, &%s)" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "pgy_read_%s(%s)" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "pgy_secure_read_%s(%s, &%s)" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "pgy_release_%s(%s)" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "pgy_secure_release_%s(%s, &%s)" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "row->call_shape" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "C let-slot %s requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "pgy_claim_%s()" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "pgy_claim_secure_%s(&%s_token)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "pgy_claim_device_%s()" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "pgy_write_%s(&%s, %s)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "pgy_secure_write_%s(&%s, %s, &%s_token)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "transpiler_slot_runtime_fn(" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "pgy_claim_%s()" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "pgy_claim_secure_%s(&%s_token)" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "pgy_release_%s(&%s);" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "pgy_secure_release_%s(&%s, &%s_token);" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
grep -Fq "transpiler_dispatch_emit_part" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "C backend: %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_operand.c"
grep -Fq "C backend: expression lowering received a null AST node" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "\"array assignment\", \"index\"" "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c"
grep -Fq "\"slot assignment\", \"value\"" "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c"
grep -Fq "\"event invoke\", \"argument\"" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "cannot determine slot payload type for assignment" "$ROOT_DIR/src/codegen/transpiler_expr_assignment_emit.c"
grep -Fq "slot_builtin_emit_operand" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "slot_builtin_emit_slot_operand" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "C backend: %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "DeviceWrite\", \"value\"" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "Release\", \"token\"" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_device_write_%s(&%s, %s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_device_read_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_release_device_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
! grep -Fq "pgy_submit_device_read_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"
grep -Fq "C await expression requires concrete Future<T> result metadata" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "C spawn expression requires a target expression" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "C spawn expression requires concrete Future<T> return metadata" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "requires concrete C type metadata for call" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "pgy_async_spawn(NULL, NULL)" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "/* spawn alloc failed */" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_spawn_channel_emit_expr" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "C backend: %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "\"channel send\", \"value\"" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_require_channel_inner_type" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_channel_expr_is_c_lvalue" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "select_emit_unbound_consume" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "C select lowering could not lower unbound receive channel expression" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "transpiler_event_emit_arg" "$ROOT_DIR/src/codegen/transpiler_event_builtin_emit.c"
grep -Fq "C backend: event '%s' could not lower argument %zu" "$ROOT_DIR/src/codegen/transpiler_event_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_event_builtin_emit.c"
grep -Fq "transpiler_event_op_emit_part" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "C backend: event %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "\"subscribe\", \"event\"" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "\"unsubscribe\", \"handler\"" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "requires a named Channel<T> binding" "$ROOT_DIR/src/codegen/transpiler_channel_type_query.c"
grep -Fq "requires concrete Channel<T> payload metadata" "$ROOT_DIR/src/codegen/transpiler_channel_type_query.c"
grep -Fq "transpiler_channel_require_lvalue" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"
grep -Fq "transpiler_require_channel_inner_type(ctx, expr" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"
! grep -Fq "transpiler_channel_resolve_inner(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"
grep -Fq "transpiler_require_c_addressable_storage" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.h"
grep -Fq "transpiler_expr_is_c_addressable_storage" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c"
grep -Fq "requires addressable %s storage" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_support.c"
grep -Fq "transpiler_require_c_addressable_storage(ctx, list_arg" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.c"
grep -Fq "transpiler_require_c_addressable_storage(ctx, map_arg" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "transpiler_require_c_addressable_storage(ctx, queue_arg" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "transpiler_require_c_addressable_storage(ctx, a0" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c"
grep -Fq "transpiler_require_c_addressable_storage(ctx, arg0" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "transpiler_require_c_addressable_storage(ctx, a0" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_builtin.c"
grep -Fq "BUILTIN_RC_DROP || kind == BUILTIN_RC_GET" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "BUILTIN_BOX_SET || kind == BUILTIN_BOX_DROP" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "BoxSet requires exactly two arguments" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "transpiler_core_builtin_emit_arg" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "C backend: %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
! grep -Fq "return pergyra_strdup(\"false\")" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "RcNew" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "BoxSet" "$ROOT_DIR/src/codegen/transpiler_expr_core_builtins_emit.c"
grep -Fq "AllocatorPool could not lower capacity expression" "$ROOT_DIR/src/codegen/transpiler_allocator_builtin_emit.c"
grep -Fq "AllocatorPool requires exactly one capacity argument" "$ROOT_DIR/src/codegen/transpiler_allocator_builtin_emit.c"
grep -Fq "AllocatorDestroy requires a named Allocator local" "$ROOT_DIR/src/codegen/transpiler_allocator_builtin_emit.c"
grep -Fq "AllocatorDestroy argument '%s' must have type Allocator" "$ROOT_DIR/src/codegen/transpiler_allocator_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_allocator_builtin_emit.c"
grep -Fq "intent_observability_require_arg_count" "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c"
grep -Fq "pgy_intent_observability_abi_row_by_source" "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c"
grep -Fq "row->runtime_name" "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c"
grep -Fq "row->arg_count" "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_intent_observability_builtin_emit.c"
grep -Fq "transpiler_call_arg_can_take_subject_address" "$ROOT_DIR/src/codegen/transpiler_call_subject_arg_policy.c"
grep -Fq "transpiler_user_call_emit_part" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "C backend: user call %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "\"callee\"" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "\"slot argument\"" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "transpiler_member_call_emit_part" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "C backend: member call %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "C method call post-sync wrapper cannot render return type" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "wrapped = pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "\"receiver\"" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "\"slice length\"" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "requires addressable storage" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "requires addressable storage" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "LLVM boundary subject argument requires addressable storage" "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "LLVM intent subject argument" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "requires addressable Rc/Weak storage" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "requires addressable Box storage" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "IntentRecentName missing index fails closed" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "IntentActiveStepName missing step fails closed" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "transpiler_log_emit_arg" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
grep -Fq "C backend: %s could not lower argument %zu" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
grep -Fq "transpiler_log_string_error" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
grep -Fq "C backend: %s could not %s string literal" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
grep -Fq "\"LogRaw\", i" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"/*" "$ROOT_DIR/src/codegen/transpiler_log_builtin_emit.c"
grep -Fq "io_builtin_emit_arg" "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c"
grep -Fq "C backend: %s could not lower %s argument" "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c"
grep -Fq "\"FileOpen\", \"path\"" "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c"
grep -Fq "\"Sleep\", \"milliseconds\"" "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c"
grep -Fq "subject temporary argument rejects pointer-self boundary" "$ROOT_DIR/src/tests/transpile/test_transpile_program_part_a.cases.h"
grep -Fq "subject method temporary argument rejects pointer-self boundary" "$ROOT_DIR/src/tests/transpile/test_transpile_program_part_a.cases.h"
! grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "constructed_single_arg_is_unknown" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "constructed_arg_name_is_unknown" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_join" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "PgyArray_" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "strcmp(inner_buf, \"Unknown\") != 0" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "without concrete Result error type" "$ROOT_DIR/src/codegen/transpiler_match_bindings.c"
grep -Fq "Some(value) without concrete payload type fails closed" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "transpiler_option_type_has_concrete_inner" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_contextual_option_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "IsSome(None()) without concrete Option<T> fails closed" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "transpiler_result_arg_list_has_unknown" "$ROOT_DIR/src/codegen/transpiler_type_result_mapping_helpers.c"
grep -Fq "transpiler_result_type_ident_char" "$ROOT_DIR/src/codegen/transpiler_type_result_mapping_helpers.c"
grep -Fq "Ok(value) with unknown Result payload fails closed" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "Result suffix keeps user type names containing Unknown" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a_1.cases.h"
grep -Fq "kTranspilerResultOptionSpecs" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_result_option_lookup" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_result_option_emit_arg" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "C backend: %s could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
! grep -Fq "return pergyra_strdup(\"false\")" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "emit_call_result_option_builtin(call, callee, ctx, &handled)" "$ROOT_DIR/src/codegen/transpiler_expr_call_spawn_emit.c"
grep -Fq "*handled = true" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "Result/Option builtin '%s' received unsupported argument shape" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "TRANS_RESULT_OPTION_OP_UNWRAP_OR" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
transpiler_result_option_names="$(
    sed -n '/static const TranspilerResultOptionSpec kTranspilerResultOptionSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_result_option_names_sorted="$(
    printf '%s\n' "$transpiler_result_option_names" | sort
)"
if [[ "$transpiler_result_option_names" != "$transpiler_result_option_names_sorted" ]]; then
    echo "C backend Result/Option builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_result_option_names_sorted") \
        <(printf '%s\n' "$transpiler_result_option_names") >&2 || true
    exit 1
fi
grep -Fq "pgy_result_type_arg_has_unknown" "$ROOT_DIR/src/codegen/llvm_type.c"
grep -Fq "cannot materialize Unknown result layout" "$ROOT_DIR/src/codegen/llvm_type.c"
! grep -A56 -F "case PGY_TK_RESULT" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A56 -F "case PGY_TK_RESULT" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "ctx->type_i8ptr"
grep -Fq "ok_ty == NULL || err_ty == NULL" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "LLVM Some(value) requires contextual Option<T>" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "anonymous Option layout fallback is disabled" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "llvm_result_option_error" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "kLLVMResultOptionSpecs" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "llvm_result_option_lookup" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
result_option_builtin_names="$(
    sed -n '/static const LLVMResultOptionSpec kLLVMResultOptionSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
result_option_builtin_names_sorted="$(
    printf '%s\n' "$result_option_builtin_names" | sort
)"
if [[ "$result_option_builtin_names" != "$result_option_builtin_names_sorted" ]]; then
    echo "LLVM Result/Option builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$result_option_builtin_names_sorted") \
        <(printf '%s\n' "$result_option_builtin_names") >&2 || true
    exit 1
fi
! grep -A16 -F "llvm_result_option_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM Ok(value) could not lower payload expression" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "LLVM UnwrapOr(result, default) could not lower operand expression" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "LLVM checked unwrap requires an active function insertion block" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "llvm_result_option_value_struct" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "requires concrete Result<T, E> aggregate operand" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "requires concrete Option<T> aggregate operand" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "LLVM ListNew() requires contextual List<T>" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM SetNew() requires contextual Set<T>" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "implicit i32 fallback is disabled" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "llvm_collection_base_error_out" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
! grep -Fq "(void)recovery;" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM ListNew() could not allocate list temporary" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM SetAdd could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM SetAdd requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM SetHas requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM SetRemove requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "LLVM SetSize requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "kLLVMStmtCollectionCtorSpecs" "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c"
grep -Fq "llvm_stmt_collection_ctor_lookup(callee)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "kLLVMStmtLetCallSpecs" "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c"
grep -Fq "llvm_stmt_let_call_op(init)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "llvm_stmt_register_collection_var" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c"
! grep -Fq 'strcmp(ann_name, "List")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(ann_name, "Set")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(ann_name, "Queue")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(ann_name, "HashMap")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(callee, "ListNew")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(callee, "SetNew")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(callee, "QueueNew")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(callee, "MapNew")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(ast_identifier_name(ast_call_callee(init)), "ToObject")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq 'strcmp(ast_identifier_name(ast_call_callee(init)), "Channel")' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
llvm_stmt_collection_ctor_names="$(
    sed -n '/kLLVMStmtCollectionCtorSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stmt_collection_ctor_names_sorted="$(
    printf '%s\n' "$llvm_stmt_collection_ctor_names" | sort
)"
if [[ "$llvm_stmt_collection_ctor_names" != "$llvm_stmt_collection_ctor_names_sorted" ]]; then
    echo "LLVM let collection ctor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stmt_collection_ctor_names_sorted") \
        <(printf '%s\n' "$llvm_stmt_collection_ctor_names") >&2 || true
    exit 1
fi
llvm_stmt_let_call_names="$(
    sed -n '/static const LLVMStmtLetCallSpec kLLVMStmtLetCallSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stmt_let_call_names_sorted="$(
    printf '%s\n' "$llvm_stmt_let_call_names" | sort
)"
if [[ "$llvm_stmt_let_call_names" != "$llvm_stmt_let_call_names_sorted" ]]; then
    echo "LLVM let special call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stmt_let_call_names_sorted") \
        <(printf '%s\n' "$llvm_stmt_let_call_names") >&2 || true
    exit 1
fi
grep -Fq "*out = NULL;" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "kLLVMCollectionBaseSpecs" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "llvm_collection_base_lookup" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
llvm_collection_base_names="$(
    sed -n '/static const LLVMCollectionBaseSpec kLLVMCollectionBaseSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_collection_base_names_sorted="$(
    printf '%s\n' "$llvm_collection_base_names" | sort
)"
if [[ "$llvm_collection_base_names" != "$llvm_collection_base_names_sorted" ]]; then
    echo "LLVM base collection builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_collection_base_names_sorted") \
        <(printf '%s\n' "$llvm_collection_base_names") >&2 || true
    exit 1
fi
grep -Fq "return NULL;" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq "llvm_constructed_arg_name_at(type_name" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq "const char   *llvm_constructed_arg_name_at" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
! grep -Fq "llvm_constructed_arg_name_at(const char" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c"
grep -Fq "llvm_constructed_arg_name_write" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c"
! grep -A12 -F "llvm_constructed_arg_name_copy" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c" | grep -Fq "llvm_constructed_arg_name_at("
grep -A40 -F "len = (size_t)(p - start);" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c" | grep -Fq "return false;"
grep -Fq "char ok_name_buf[256]" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "char err_name_buf[256]" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "case PGY_TK_LIST:" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "case PGY_TK_SET:" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "case PGY_TK_QUEUE:" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "case PGY_TK_HASHMAP:" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq 'strncmp(type_name, "List<", 5)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq 'strncmp(type_name, "Set<", 4)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq 'strncmp(type_name, "Queue<", 6)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq 'strncmp(type_name, "HashMap<", 8)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
! grep -Fq 'strncmp(ctx->expected_type_name, "List<", 5)' "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
! grep -Fq 'strncmp(ctx->expected_type_name, "Queue<", 6)' "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
! grep -Fq 'strncmp(map_type, "HashMap<", 8)' "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
! grep -Fq 'strncmp(set_type, "Set<", 4)' "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
! grep -Fq 'strncmp(exp, "List<", 5)' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strncmp(exp, "Queue<", 6)' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strncmp(ctx->expected_type_name, "HashMap<", 8)' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strncmp(ctx->expected_type_name, "Set<", 4)' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -A16 -F "llvm_resolve_inner_type(LLVMGenCtx *ctx" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F "PGY_TK_OPTION" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
grep -Fq "if (ctx->has_error || inner == NULL)" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "if (ctx->has_error || list_ty == NULL || elem_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "case LLVM_STMT_COLLECTION_CTOR_SET" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "case LLVM_STMT_COLLECTION_CTOR_QUEUE" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "if (ctx->has_error || map_ty == NULL || value_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -A8 -F "PGY_TK_SLOT" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F "PGY_TK_SECURE_SLOT" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F "PGY_TK_DEVICE_SLOT" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F "PGY_TK_ARRAY" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F "PGY_TK_SLICE" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
grep -Fq "LLVM array literal could not lower Array<T> type" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM tuple literal requires at least 2 elements" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "if (n < 2)" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
! grep -A8 -F "llvm_emit_tuple_literal_expr" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c" | grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVMTypeRef elem_type = NULL" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
! grep -Fq "LLVMTypeRef elem_type = ctx->type_i32" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVMTypeRef elem_type = NULL" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -Fq "LLVMTypeRef elem_type = ctx->type_i32" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "if (elem_type == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "if (ctx->has_error || array_type == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -A12 -F "llvm_type_to_suffix" "$ROOT_DIR/src/codegen/llvm_backend_generic.c" | grep -Fq 'return "Unknown";'
grep -Fq "llvm_array_required_receiver_binding" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "requires registered Array<T> local" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "*out = NULL;" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_array_error_out" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "LLVM ArrayPush could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "LLVM ArraySet could not lower index or value expression" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "LLVM ArrayPop requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.h"
grep -Fq "llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
grep -Fq "bool llvm_collection_required_receiver_var(LLVMGenCtx *ctx" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.h"
grep -Fq "LLVMVarEntry *receiver_out" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.h"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
! grep -Fq "LLVMVarEntry *llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.h"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
grep -Fq "callee_name, \"HashMap\", &map_var, out" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "callee_name, \"collection\", &list_var, out" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "callee_name, \"queue\", &queue_var, out" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "callee_name, \"collection\", &set_var, out" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
! grep -Fq "LLVMVarEntry *map_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
! grep -Fq "LLVMVarEntry *list_var" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
! grep -Fq "LLVMVarEntry *queue_var" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
! grep -Fq "LLVMVarEntry *set_var" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
! grep -Fq "map_var->" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
! grep -Fq "list_var->" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
! grep -Fq "queue_var->" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
! grep -Fq "set_var->" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
! grep -Fq "(void)fallback;" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
! grep -Fq "(void)recovery;" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
! grep -Fq "{ *out = NULL; return true; }" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "LLVM MapSet requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "LLVM MapGet requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "LLVM MapKeys requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "LLVM collection slot source could not lower source expression" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "kListExtendedSpecs" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "llvm_list_extended_lookup" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "LLVM ListPush requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "LLVM ListGet requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "LLVM ListRemove requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "kMapExtendedSpecs" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "llvm_map_extended_lookup" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "llvm_collection_required_receiver_var(ctx, node, set_arg" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "llvm_collection_required_receiver_var(ctx, node, queue_arg" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "requires an identifier receiver" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
grep -Fq "requires registered %s local" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
grep -Fq "MapKeys" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "\"queue\"" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "llvm_queue_error_out" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
! grep -Fq "(void)recovery;" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "kQueueExtendedSpecs" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "llvm_queue_extended_lookup" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "LLVM QueuePush requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "LLVM QueuePop requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "LLVM QueueEmpty requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "llvm_slot_builtin_error_out" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM Write could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM DeviceWrite could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM device slot operation requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
collection_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.c"
grep -Fq "kTranspilerCollectionSpecs" "$collection_builtin_owner"
grep -Fq "transpiler_collection_lookup" "$collection_builtin_owner"
grep -Fq "bsearch(" "$collection_builtin_owner"
grep -Fq "transpiler_collection_emit_arg" "$collection_builtin_owner"
grep -Fq "C backend: collection builtin %s could not lower %s argument" "$collection_builtin_owner"
! grep -Fq "return pergyra_strdup(\"0\")" "$collection_builtin_owner"
grep -Fq "\"ListPush\", \"value\"" "$collection_builtin_owner"
grep -Fq "\"ListSet\", \"index\"" "$collection_builtin_owner"
grep -Fq "\"SetHas\", \"key\"" "$collection_builtin_owner"
transpiler_collection_names="$(
    sed -n '/static const TranspilerCollectionSpec kTranspilerCollectionSpecs\[\]/,/^};/p' \
        "$collection_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_collection_names_sorted="$(
    printf '%s\n' "$transpiler_collection_names" | sort
)"
if [[ "$transpiler_collection_names" != "$transpiler_collection_names_sorted" ]]; then
    echo "C backend collection builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_collection_names_sorted") \
        <(printf '%s\n' "$transpiler_collection_names") >&2 || true
    exit 1
fi
grep -Fq "kTranspilerArraySpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
grep -Fq "transpiler_array_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
grep -Fq "transpiler_stdlib_emit_arg" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "C backend: stdlib builtin %s could not lower %s argument" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "\"ArrayPush\", \"value\"" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -A2 -F '"ArrayMap"' "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c" | \
    grep -Fq '"function"'
grep -A2 -F '"ToString"' "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c" | \
    grep -Fq '"value"'
transpiler_array_names="$(
    sed -n '/static const TranspilerArrayStdlibSpec kTranspilerArraySpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_array_names_sorted="$(
    printf '%s\n' "$transpiler_array_names" | sort
)"
if [[ "$transpiler_array_names" != "$transpiler_array_names_sorted" ]]; then
    echo "C backend Array builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_array_names_sorted") \
        <(printf '%s\n' "$transpiler_array_names") >&2 || true
    exit 1
fi
grep -Fq "kTranspilerStdlibSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
grep -Fq "transpiler_stdlib_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
grep -Fq "TranspilerToStringSpec" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
grep -Fq "transpiler_to_string_kind" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"
if grep -Eq 'strcmp[[:space:]]*\([[:space:]]*arg_type[[:space:]]*,[[:space:]]*"(String|Bool|Float|Double|Long)"' \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"; then
    echo "[perf-contract] C ToString lowering reintroduced direct type branch" >&2
    exit 1
fi
transpiler_to_string_names="$(
    sed -n '/static const TranspilerToStringSpec kTranspilerToStringSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_to_string_names_sorted="$(
    printf '%s\n' "$transpiler_to_string_names" | sort
)"
if [[ "$transpiler_to_string_names" != "$transpiler_to_string_names_sorted" ]]; then
    echo "C backend ToString type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_to_string_names_sorted") \
        <(printf '%s\n' "$transpiler_to_string_names") >&2 || true
    exit 1
fi
transpiler_stdlib_names="$(
    sed -n '/static const TranspilerStdlibSpec kTranspilerStdlibSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_stdlib_names_sorted="$(
    printf '%s\n' "$transpiler_stdlib_names" | sort
)"
if [[ "$transpiler_stdlib_names" != "$transpiler_stdlib_names_sorted" ]]; then
    echo "C backend stdlib builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_stdlib_names_sorted") \
        <(printf '%s\n' "$transpiler_stdlib_names") >&2 || true
    exit 1
fi
grep -Fq "kTranspilerMiscSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c"
grep -Fq "transpiler_misc_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c"
misc_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c"
transpiler_misc_names="$(
    sed -n '/static const TranspilerMiscSpec kTranspilerMiscSpecs\[\]/,/^};/p' \
        "$misc_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_misc_names_sorted="$(
    printf '%s\n' "$transpiler_misc_names" | sort
)"
if [[ "$transpiler_misc_names" != "$transpiler_misc_names_sorted" ]]; then
    echo "C backend misc builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_misc_names_sorted") \
        <(printf '%s\n' "$transpiler_misc_names") >&2 || true
    exit 1
fi
grep -Fq "transpiler_misc_emit_arg" "$misc_builtin_owner"
grep -Fq "C backend: misc builtin %s could not lower %s argument" "$misc_builtin_owner"
! grep -Fq "return pergyra_strdup(\"0\")" "$misc_builtin_owner"
grep -Fq "\"FsmTransition\", \"input\"" "$misc_builtin_owner"
grep -Fq "\"CooldownTick\", \"delta\"" "$misc_builtin_owner"
grep -Fq "\"MapSetStr\", \"value\"" "$misc_builtin_owner"
grep -Fq "kTranspilerMapSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "transpiler_map_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "transpiler_map_emit_arg" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "C backend: map builtin %s could not lower %s argument" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "\"MapSet\", \"value\"" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "\"MapHas\", \"key\"" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "kTranspilerQueueSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "transpiler_queue_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "transpiler_queue_emit_arg" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "C backend: queue builtin %s could not lower %s argument" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "\"QueuePush\", \"value\"" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "\"QueuePop\", \"queue\"" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "LLVM QueuePush could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "LLVM QueuePop could not allocate result temporary" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
! grep -Fq "{ *out = NULL; return true; }" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
! grep -A16 -F "llvm_math_error_out(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
! grep -A16 -F "requires registered runtime function" \
    "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
! grep -A16 -F "could not lower argument expression" \
    "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
! grep -Fq "Slot<Unknown>" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "requires concrete slot type metadata" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
! grep -Fq "static char *rendered_param" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
! grep -Fq "static char rendered_param" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "transpiler_mir_arena_copy_type_name(ctx, owned_param" \
    "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "declarator_ast_type_to_c_copy" "$ROOT_DIR/src/codegen/transpiler_type_declarator.c"
grep -Fq "return pergyra_ast_type_to_c_copy_in_ctx(ctx, type_node, out, out_size)" "$ROOT_DIR/src/codegen/transpiler_type_declarator.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, (ASTNode *)type_node" "$ROOT_DIR/src/codegen/transpiler_mir_signature.c"
grep -Fq "char param_buf[256]" "$ROOT_DIR/src/codegen/transpiler_type_declarator.c"
! grep -Fq "lossy fallback" "$ROOT_DIR/src/codegen/llvm_intent.c"
! grep -Fq 'pergyra_strdup("Unknown")' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "llvm_required_constructed_arg_name_copy" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "return llvm_list_struct_type(ctx, inner_buf)" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "return llvm_hashmap_struct_type(ctx, value_buf)" "$ROOT_DIR/src/codegen/llvm_backend_type_map.c"
grep -Fq "List<HashMap<String, Int>>" "$ROOT_DIR/tests/cases/backend_compare/nested_generic_containers/main.pgy"
grep -Fq "expected_inner_buf" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "expected_inner = expected_inner_buf" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "inner_name_buf" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "llvm_stmt_render_type_annotation_copy" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq "llvm_stmt_render_type_annotation_static" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq "llvm_stmt_render_type_annotation_static" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
! grep -Fq "static char buf[256]" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "pgy_arena_strdup(&ctx->scratch, actual_type)" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "transpiler_scratch_fmt(ctx," "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
grep -Fq "transpiler_scratch_strdup(ctx, field_name)" "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
! grep -Fq "projection_heap_fmt" "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
! grep -Fq "free(source_path)" "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
! grep -Fq "free(source_path)" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
! grep -Fq "pergyra_strdup(field_name)" "$ROOT_DIR/src/codegen/transpiler_projection_emit.c"
grep -Fq "pgy_arena_alloc(&ctx->scratch, len)" "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
grep -Fq "pgy_arena_alloc(&ctx->scratch, len)" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
! grep -Fq "pergyra_strdup(field_name)" "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
! grep -Fq "pergyra_strdup(field_name)" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "pgy_arena_strdup(&ctx->scratch_arena, text)" "$ROOT_DIR/src/semantic/type_checker_projection_path.c"
grep -Fq "pgy_arena_fmt(&ctx->scratch_arena" "$ROOT_DIR/src/semantic/type_checker_projection_path.c"
! grep -Fq "projection_path_strdup_fmt" "$ROOT_DIR/src/semantic/type_checker_projection_path.c"
! grep -Fq "free(source_path)" "$ROOT_DIR/src/semantic/type_checker_domain_projection_fields.c"
! grep -Fq "free(resolved_source_path)" "$ROOT_DIR/src/semantic/type_checker_resolution_graph_zone.c"
grep -Fq "pgy_codegen_call_name_is_slot_operation(callee_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "pgy_codegen_call_name_is_read(callee_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq 'strcmp(callee_name, "Read")' "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq 'strcmp(callee_name, "Write")' "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq 'strcmp(callee_name, "Release")' "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -A60 -F "llvm_infer_spawn_future_inner" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c" | grep -Fq "static char buf"
grep -Fq "ctx->expected_type_name = ctx->current_return_type_name" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "ctx->expected_callable_type = ctx->current_return_callable_type" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "llvm_stmt_render_type_annotation_copy(ctx" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "suffix_buf" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_stmt_contextual_option_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_stmt_contextual_result_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_stmt_infer_scalar_math_return_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "op == TOKEN_COALESCE" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_stmt_infer_nominal_name_from_init(ctx, receiver)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_lookup_enum_variant(ctx, callee)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq '{ "IsOk", "Bool", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq '{ "IsErr", "Bool", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq '{ "IsSome", "Bool", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq '{ "IsNone", "Bool", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq '{ "StringLength", "Int", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq "llvm_stmt_name_in_sorted_table" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "kLLVMCollectionGetSpecs" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "llvm_stmt_collection_get_spec_compare" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq '"SetHas"' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq '"SetSize"' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
! grep -Fq "strcmp(callee, specs[i].name)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
llvm_stmt_collection_get_names="$(
    sed -n '/static const LLVMCollectionGetSpec kLLVMCollectionGetSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stmt_collection_get_names_sorted="$(
    printf '%s\n' "$llvm_stmt_collection_get_names" | sort
)"
if [[ "$llvm_stmt_collection_get_names" != "$llvm_stmt_collection_get_names_sorted" ]]; then
    echo "LLVM stmt collection-get names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stmt_collection_get_names_sorted") \
        <(printf '%s\n' "$llvm_stmt_collection_get_names") >&2 || true
    exit 1
fi
grep -Fq "return pergyra_type_to_llvm(ctx, inner_buf)" "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
! grep -A18 -F "llvm_stmt_unknown_expr_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "left_ty == NULL || right_ty == NULL" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "inner_name = inner_name_buf" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "return_type_owned = pergyra_strdup(return_type)" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_infer_lambda_param_c_type_copy" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_lambda_expected_return_type" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_lambda_expected_param_type" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_restore_local_binding_counts_local(" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
! grep -Fq "ctx->typed_var_count = saved_typed_var_count" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_func_current_return_callable_type" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "transpiler_mir_return_callable_type" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "transpiler_mir_routine_return_type(mir_routine)" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "ASTNode *return_callable_type" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "transpiler_func_current_return_callable_type(ctx);" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
grep -Fq "transpiler_func_current_return_callable_type(ctx);" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "llvm_mir_return_callable_type" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_routine_return_type(routine)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_stmt_lambda_signature_type(ctx, expr)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -B12 -F "llvm_stmt_require_non_void_value(ctx, return_expr" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" | \
    grep -Fq "ctx->expected_callable_type = mir_callable_type;"
grep -Fq "ctx->expected_callable_type = callable_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "ctx->expected_callable_type = let_type" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
grep -Fq "type_node->type == AST_EVENT_HANDLER_TYPE" "$ROOT_DIR/src/compiler/mir_type_helpers.c"
grep -A4 -F "type_node->type == AST_EVENT_HANDLER_TYPE" \
    "$ROOT_DIR/src/compiler/mir_type_helpers.c" | \
    grep -Fq "return NULL;"
! grep -A4 -F "type_node->type == AST_EVENT_HANDLER_TYPE" \
    "$ROOT_DIR/src/compiler/mir_type_helpers.c" | \
    grep -Fq 'pergyra_strdup("Int")'
grep -Fq "mir_render_tuple_type_name(ASTNode *type_node)" \
    "$ROOT_DIR/src/compiler/mir_type_helpers.c"
grep -A4 -F "ast_type_tuple_element_count(type_node) > 0" \
    "$ROOT_DIR/src/compiler/mir_type_helpers.c" | \
    grep -Fq "return mir_render_tuple_type_name(type_node);"
! grep -A4 -F "ast_type_tuple_element_count(type_node) > 0" \
    "$ROOT_DIR/src/compiler/mir_type_helpers.c" | \
    grep -Fq 'pergyra_strdup("Tuple")'
grep -Fq "type_kind = llvm_registry_type_kind(abi_type_name)" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -A95 -F "llvm_register_typed_var_abi_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" | \
    grep -Fq "llvm_register_map_var_binding(ctx, var_name, binding, arg0_name"
grep -A70 -F "llvm_register_typed_var_abi_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" | \
    grep -Fq "llvm_register_array_var_binding(ctx, var_name, binding, elem_type,"
grep -A70 -F "llvm_register_typed_var_abi_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" | \
    grep -Fq "llvm_register_list_var_binding(ctx, var_name, binding, arg0_name)"
grep -Fq "ctx->expected_lambda_type = lambda_expected_type" "$ROOT_DIR/src/semantic/type_checker_ownership_let.c"
grep -Fq "ctx->expected_lambda_type = ctx->current_return" "$ROOT_DIR/src/semantic/type_checker_ownership_return.c"
grep -Fq "type_check_lambda_expression(expr, ctx)" "$ROOT_DIR/src/semantic/type_checker_expr.c"
grep -Fq "Type *expected_lambda_type = ctx->expected_lambda_type" "$ROOT_DIR/src/semantic/type_checker_expr_lambda.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, lambda_return_type" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, param_type_ast" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "char ok_ctype_buf[128]" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "transpiler_copy_c_type_or_user_type_name(ok_type" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "transpiler_copy_c_type_or_user_type_name(err_type" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "transpiler_copy_c_type_or_user_type_name(const char *type_name" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner_type" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "char array_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "TranspilerParallelWrapperState" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_write_capture_address" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_resolve_active_ssa_name(ctx, name)" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_parallel_wrapper_state_enter(" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_parallel_wrapper_state_restore(ctx, &wrapper_state)" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
! grep -Fq "saved_in_pw" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
! grep -Fq "ctx->par_capture_slot_count = saved_slot_count" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
! grep -Fq 'codebuf_write(ctx->out, "&%s", capture_slot_names[i])' "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
! grep -Fq 'codebuf_write(ctx->out, "&%s", capture_typed_names[i])' "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "char inferred_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char annotated_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inferred_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, ann_source_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, array_type_name" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char set_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, ann_type_name" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, ann_type_name" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_try_emit_list_or_queue_new_let" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, ann_type_name" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_let_option_ctor_lookup" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "pgy_codegen_match_variant_lookup(callee_name)" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "PGY_MATCH_VARIANT_SOME" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "PGY_MATCH_VARIANT_NONE_CTOR" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "kTranspilerLetCollectionCtorSpecs" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_let_collection_ctor_lookup" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "Some")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "None")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "ListNew")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "QueueNew")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "MapNew")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
transpiler_let_collection_ctor_names="$(
    sed -n '/kTranspilerLetCollectionCtorSpecs\[\]/,/^        };/p' \
        "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_let_collection_ctor_names_sorted="$(
    printf '%s\n' "$transpiler_let_collection_ctor_names" | sort
)"
if [[ "$transpiler_let_collection_ctor_names" != "$transpiler_let_collection_ctor_names_sorted" ]]; then
    echo "C backend let collection ctor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_let_collection_ctor_names_sorted") \
        <(printf '%s\n' "$transpiler_let_collection_ctor_names") >&2 || true
    exit 1
fi
grep -Fq "result_c_type = result_c_type_buf" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, result_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char ctype_buf[256]" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, tuple_name" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, result_type" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, subject_type" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner" "$ROOT_DIR/src/codegen/transpiler_match_bindings.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, ok_type" "$ROOT_DIR/src/codegen/transpiler_match_bindings.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, err_type" "$ROOT_DIR/src/codegen/transpiler_match_bindings.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, payload_type" "$ROOT_DIR/src/codegen/transpiler_mir_match_payload_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, return_type_name" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "bound_type," "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "inferred_arg_type," "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_scratch_fmt(" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "infer_spawn_return_type_name_scratch" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "infer_spawn_return_type_name_scratch" "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c"
grep -Fq "transpiler_scratch_fmt(ctx, \"Future<%s>\"" "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c"
! grep -Fq "infer_spawn_return_type_name(ctx" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "infer_spawn_return_type_name(ctx" "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c"
! grep -Fq "free(owned_return_type_name)" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq 'strdup_fmt("Future<' "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c"
! grep -Fq 'strdup_fmt("pgy_spawn_wrapper_' "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq 'strdup_fmt("PgySpawnArgs_' "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "free(wrapper_name)" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "free(args_type_name)" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "free(return_type_name)" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
! grep -Fq "free(return_c_type)" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, secure_name" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, slot_name_buf" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "pgy_codegen_call_name_is_move(callee_name)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "pgy_codegen_call_name_is_move(callee)" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "type_name_is_exact_or_generic(type_name, \"Slot\", \"Slot<\")" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "type_name_is_exact_or_generic(type_name, \"SecureSlot\"" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
grep -Fq "inner = llvm_lookup_slot_inner(ctx, source_name)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/transpiler_slot_target.c"
grep -Fq "pgy_codegen_type_name_is_slot(ann_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "pgy_codegen_type_name_is_slot(ann_name)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq 'strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0' "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
! grep -Fq 'strcmp(type_name, "Slot") != 0' "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq 'strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0' "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
! grep -Fq 'strcmp(callee_name, "Move")' "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq 'strcmp(callee, "Move")' "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, type_name" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, ret_name" "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inner" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "pergyra_func_signature_declarator_in_ctx(ctx" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, return_type" "$ROOT_DIR/src/codegen/transpiler_func_forward_metadata.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
grep -Fq "transpiler_slot_runtime_fn(" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
! grep -Fq "pgy_claim_%s()" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
! grep -Fq "pgy_claim_secure_%s(&self.%s)" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_domain_ability_emit.c"
grep -Fq "return_type_name, \"ability method return\"" "$ROOT_DIR/src/codegen/transpiler_domain_ability_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, return_type" "$ROOT_DIR/src/codegen/transpiler_domain_role_include_emit.c"
grep -Fq "transpiler_mir_decl_method_return_type_name(method_meta)" "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "transpiler_mir_decl_method_return_type(method_meta)" "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx," "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "return_type, ret_type_storage" "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx," "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, bt2[b]" "$ROOT_DIR/src/codegen/transpiler_match_bindings.c"
grep -Fq "pergyra_ast_type_to_c_copy_in_ctx(ctx, type_ast, out" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, participant_type" "$ROOT_DIR/src/codegen/transpiler_block_intent_rebind_helpers.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx, elem_names[j]" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
! grep -Fq "static char mapped[128]" "$ROOT_DIR/src/codegen/transpiler_func_forward_helpers.h"
grep -Fq "lookup_future_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "transpiler_type_name_is_any_future" "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "transpiler_type_name_is_remote_future" "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "future_owned_type_to_scratch" "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "transpiler_scratch_strdup(ctx, owned)" "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
! grep -Fq 'pergyra_strdup("Unknown")' "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq 'return out[0] != '\''\0'\'' && strcmp(out, "Unknown") != 0;' "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
! grep -Fq 'return pergyra_str_copy(out, out_size, "Void")' "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "transpiler_type_name_is_any_future" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_is_result" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_is_option" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_is_array_or_slice" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_is_hashmap" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "transpiler_type_name_is_result(result_type)" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_type_name_is_option(subject_type)" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "lookup_future_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "transpiler_require_ast_c_type_copy" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "transpiler_bound_type_name(ctx, eff_type_name)" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "ctx->generic_bindings[i].concrete_type" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "pergyra_type_to_c_copy(resolved_type_name, out" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
! grep -A8 -F "type_name[0] >= 'A'" "$ROOT_DIR/src/codegen/transpiler_type_require.c" | \
    grep -Fq "pergyra_str_copy(out, out_size, type_name)"
! grep -Fq "const char *transpiler_require_ast_c_type" "$ROOT_DIR/src/codegen/transpiler_type_require.h"
! grep -Fq "transpiler_require_ast_c_type(TranspilerCtx" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
! grep -Fq "const char *transpiler_require_type_name_c_type" "$ROOT_DIR/src/codegen/transpiler_type_require.h"
! grep -Fq "transpiler_require_type_name_c_type(TranspilerCtx" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "transpiler_require_type_name_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(" "$ROOT_DIR/src/codegen/transpiler_relation_effect_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(ctx, p->type" "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_extern.c"
grep -Fq "transpiler_require_ast_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_block_intent_rebind_helpers.c"
grep -Fq "transpiler_require_ast_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_intent_prologue_emit.c"
grep -Fq "transpiler_require_ast_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_intent_prologue_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c"
grep -Fq "slot_inner_type_name_copy(array_type, inner_buf" \
    "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "if (result == NULL)" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c"
grep -Fq "if (gp == NULL)" "$ROOT_DIR/src/codegen/llvm_backend_type_render.c"
! grep -A8 -F "LLVM type '%s' is not registered in the LLVM type map" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "llvm_registry_required_arg_name" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "llvm_constructed_arg_name_copy(type_name, arg_index" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "registry requires concrete type metadata" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "if (ctx->has_error || elem_type == NULL)" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "llvm_lookup_or_declare_function" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "LLVMAddFunction(ctx->module, name, decl_type)" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_register_mono(ctx, mangled);" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
if awk '
    /llvm_register_mono\(ctx, mangled\);/ { seen_register = 1 }
    /gp = ast_declaration_generic_params\(generic_ast\);/ && seen_register { bad = 1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"; then
    echo "[perf-contract] LLVM generic monomorphization registered before preflight" >&2
    exit 1
fi
! grep -Fq "llvm_lookup_or_create_function" "$CODEGEN_INDEX"
! grep -Fq "fallback_type" "$CODEGEN_INDEX"
! grep -Fq "fallback_ret_type" "$CODEGEN_INDEX"
grep -Fq "llvm_required_collection_function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "llvm_required_collection_function" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "llvm_required_hashmap_raw_export" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_map_exports.c"
grep -Fq "requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_map_exports.c"
grep -Fq "LLVM MapHas could not lower key expression" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "LLVM MapRemove could not lower key expression" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "LLVM MapKeys could not allocate key array temporary" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
! grep -Fq "llvm_lookup_hashmap_raw_export" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.h"
grep -Fq "llvm_required_collection_function" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_list_" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.h"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_set_" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.h"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_queue_" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "llvm_mir_for_in_required_runtime" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "LLVM MIR for-in lowering requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "LLVM MIR for-in lowering requires identifier iterable metadata" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "LLVM MIR for-in lowering requires Array<T>, Slice<T>, or List<T> iterable metadata" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "size_call = LLVMConstInt(ctx->type_i32, 0, 0)" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "iterable = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_mir_for_in_body_region_contains" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_mir_for_in_block_reaches_avoiding" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -A8 -F "loop_block->succ_false, target_id" \
    "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c" | \
    grep -Fq "loop_block->id"
grep -Fq "llvm_mir_for_in_scope_collection_elem_type" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq 'strncmp(struct_name, "PgyArray_", 9)' "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq 'strncmp(struct_name, "PgySlice_", 9)' "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "idx is in [0, length)" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "LLVMBuildInBoundsGEP2(ctx->builder, elem_ty" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "inst->ast->data.for_loop.iterable" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "start = llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "end = llvm_emit_expression(inst->expr1, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "variable = inst->arg0" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
! grep -Fq "node = inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "llvm_mir_recv_expr_channel(inst->expr0)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "llvm_mir_assignment_recv_channel(inst->ast)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "llvm_mir_declare_assignment_recv_target(inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_claim_inner_type_name(inst" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
grep -Fq "inst->abi_type_name" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
grep -Fq '$(CODEGEN_DIR)/llvm_mir_resource_claim.c' "$ROOT_DIR/Makefile"
! grep -Fq "mir_instruction_source_payload" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
grep -Fq "llvm_mir_emit_borrow_view_alias(inst, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq '$(CODEGEN_DIR)/llvm_mir_resource_view.c' "$ROOT_DIR/Makefile"
! grep -Fq "node->data.with_stmt" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "node->data.with_stmt" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_list_size_raw_export\")" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_list_get_raw_export\")" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "MIR branch condition emission failed" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "MIR return terminator in function" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "MIR cleanup block return in function '%s' could not lower return value" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
! grep -Fq "cond != NULL ? cond : \"false\"" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
! grep -Fq "ret_expr != NULL ? ret_expr : \"0\"" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "MIR for-range start expression could not be emitted" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "MIR for-in condition requires Array<T>, Slice<T>, or List<T> iterable metadata" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "MIR for-in condition could not emit iterable expression" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "MIR for-range end expression could not be emitted" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "MIR for-in body binding could not emit iterable expression" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "C MIR select branch requires source branch emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "collection != NULL ? collection : \"0\"" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "end != NULL ? end : \"0\"" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "start != NULL ? start : \"0\"" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "select_cond != NULL ? select_cond : pergyra_strdup(\"false\")" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "return pergyra_strdup(\"false\")" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "inner = pgy_arena_strdup(&ctx->persistent, inner_name)" "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
grep -Fq "llvm_stmt_for_in_required_runtime" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM statement for-in lowering requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
! grep -Fq "LLVMFuncEntry *size_fn = llvm_lookup_function(ctx, \"pgy_list_size_raw_export\")" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM while lowering could not lower condition expression" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM for-in lowering could not lower iterable expression" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM for-in lowering could not register loop binding" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM for-in lowering lost loop binding metadata" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
! grep -Fq "size_call = LLVMConstInt(ctx->type_i32, 0, 0)" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
! grep -Fq "LLVMFuncEntry *get_fn = llvm_lookup_function(ctx, \"pgy_list_get_raw_export\")" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "kLLVMDomainQuerySpecs" "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"
grep -Fq "llvm_domain_query_lookup" "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"
grep -Fq "domain_query_unsupported" "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c"
! grep -Fq "return pergyra_strdup(\"false\")" "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c"
domain_query_names="$(
    sed -n '/static const LLVMDomainQuerySpec kLLVMDomainQuerySpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
domain_query_names_sorted="$(
    printf '%s\n' "$domain_query_names" | sort
)"
if [[ "$domain_query_names" != "$domain_query_names_sorted" ]]; then
    echo "LLVM domain query names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$domain_query_names_sorted") \
        <(printf '%s\n' "$domain_query_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_required_log_function" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "LLVM log operation requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "LLVM intent observability builtin" "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"
grep -Fq "requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"
grep -Fq "could not lower argument expression" "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"
grep -Fq "llvm_required_runtime_function" "$ROOT_DIR/src/codegen/llvm_runtime_require.c"
grep -Fq "LLVM %s builtin '%s' requires registered runtime function '%s'" "$ROOT_DIR/src/codegen/llvm_runtime_require.c"
grep -Fq "array\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "kArrayBuiltinSpecs" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_array_builtin_lookup" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
array_builtin_names="$(
    sed -n '/static const LLVMArrayBuiltinSpec kArrayBuiltinSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c" |
        sed -n 's/.*{"\([^"]*\)".*/\1/p'
)"
array_builtin_names_sorted="$(printf '%s\n' "$array_builtin_names" | sort)"
if [[ "$array_builtin_names" != "$array_builtin_names_sorted" ]]; then
    echo "LLVM Array builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$array_builtin_names_sorted") \
        <(printf '%s\n' "$array_builtin_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_required_runtime_function(ctx, node" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_emit_required_runtime_call_result" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_stdlib_error_value" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
! grep -A16 -F "llvm_stdlib_error_value(ASTNode *node" \
    "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "could not lower runtime call arguments" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "could not lower string argument" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "could not lower print argument" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "could not lower sleep duration argument" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "stdlib string" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "stdlib file" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"StringContains\")" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"pgy_file_open\")" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_required_checked_math_function" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "llvm_required_scalar_runtime_function" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "checked arithmetic" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "string concatenation" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "string comparison" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "llvm_scalar_expr_error" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
! grep -A16 -F "llvm_scalar_expr_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM try operator could not lower operand expression" "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c"
grep -Fq "LLVM try operator requires Result-like aggregate operand" "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c"
grep -Fq "LLVM try operator cannot coerce Result error payload" "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c"
grep -Fq "PGY_FIX_ALIGN_RESULT_ERROR_TYPE" "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c"
! grep -Fq "err_val = LLVMConstNull(dst_ty)" "$ROOT_DIR/src/codegen/llvm_expr_unary_core.c"
grep -Fq "LLVM binary expression could not lower operand expression" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "llvm_math_error_out" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
grep -Fq "kLLVMMathSpecs" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
grep -Fq "llvm_math_lookup" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
math_builtin_names="$(
    sed -n '/static const LLVMMathSpec kLLVMMathSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
math_builtin_names_sorted="$(
    printf '%s\n' "$math_builtin_names" | sort
)"
if [[ "$math_builtin_names" != "$math_builtin_names_sorted" ]]; then
    echo "LLVM math builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$math_builtin_names_sorted") \
        <(printf '%s\n' "$math_builtin_names") >&2 || true
    exit 1
fi
grep -Fq "LLVM Abs could not lower operand expression" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
grep -Fq "LLVM Min could not lower operand expression" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
grep -Fq "LLVM Max could not lower operand expression" "$ROOT_DIR/src/codegen/llvm_expr_math_calls.c"
grep -Fq "LLVM string coercion requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_string_coerce.c"
grep -Fq "llvm_log_error" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "kLLVMLogSpecs" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "llvm_log_lookup" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
log_builtin_names="$(
    sed -n '/static const LLVMLogSpec kLLVMLogSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
log_builtin_names_sorted="$(
    printf '%s\n' "$log_builtin_names" | sort
)"
if [[ "$log_builtin_names" != "$log_builtin_names_sorted" ]]; then
    echo "LLVM log builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$log_builtin_names_sorted") \
        <(printf '%s\n' "$log_builtin_names") >&2 || true
    exit 1
fi
! grep -A16 -F "llvm_log_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM Log requires at least one argument" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "LLVM Log could not lower its argument" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "LLVM LogBanner could not lower or stringify its argument" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, helper)" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"StringConcat\")" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"pgy_string_equals\")" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "indexed collection access" "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "llvm_expression_error" "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c"
! grep -A16 -F "llvm_expression_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM array access could not lower receiver or index expression" "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "LLVM aggregate array access requires concrete element metadata" "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "strcmp(inner_name, \"Unknown\") == 0" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "if (ctx == NULL || node == NULL || ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM array access receiver is not an array, slice, string, or pointer" "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "llvm_zero_value_for_type" "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c"
grep -Fq "LLVM TaskGroup expression must lower through AIR/RIR/MIR task-group boundary" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "first_value = llvm_emit_expression" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM array literal could not lower element %zu" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM array literal could not allocate array temporary" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM tuple literal could not lower element %zu" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM lambda expression could not lower return type" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM lambda expression could not lower parameter type" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM lambda expression could not lower body expression" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "llvm_stmt_lambda_return_type(ctx, node)" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "llvm_stmt_lambda_param_type(ctx, node, p, (size_t)j)" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "llvm_scope_declare(ctx, param_names[i], NULL, param_types[i])" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "llvm_stmt_lambda_expected_return_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "llvm_stmt_lambda_expected_param_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "llvm_stmt_current_return_callable_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "ctx->expected_callable_type = ctx->current_return_callable_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "llvm_stmt_current_return_callable_type(ctx);" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "ctx->expected_callable_type = type_ann" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "LLVM lambda return type requires an explicit annotation or inferable expression body" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "requires an explicit type annotation" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
! grep -Fq "LLVMTypeRef ret_type = ctx->type_i32" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
! grep -Fq "params[i] = ctx->type_i32" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
! grep -Fq "LLVMTypeRef ret_type = ctx->type_i32" \
    "$ROOT_DIR/src/codegen/llvm_expr.c"
! grep -Fq "lparams[j] = ctx->type_i32" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM context access requires a registered self parameter" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM party instance requires registered class metadata" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM party instance field '%s' is not present in class metadata" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM await expression requires an operand expression" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM event subscribe requires an identifier event target" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "LLVM event invoke could not lower argument expression" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
! grep -A16 -F "llvm_event_expr_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM indexed collection access requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "llvm_emit_inline_array_get" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "LLVMGetInsertBlock(ctx->builder)" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "if (insert_block == NULL)" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "elem_type = pergyra_type_to_llvm(ctx, suffix)" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "inlined = llvm_emit_inline_array_get(ctx, aggregate, elem_type" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "LLVMBuildICmp(ctx->builder, LLVMIntUGE" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "pgy_runtime_panic_out_of_bounds_export" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "LLVMBuildUnreachable(ctx->builder)" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "llvm_emit_inline_array_get(ctx," "$ROOT_DIR/src/codegen/llvm_expr_array_access.c"
grep -Fq "llvm_fn_never_returns" "$ROOT_DIR/src/codegen/llvm_api.c"
grep -Fq "llvm_fn_never_returns" "$ROOT_DIR/src/codegen/llvm_runtime_attrs.c"
grep -Fq "exact_never_return" "$ROOT_DIR/src/codegen/llvm_runtime_attrs.c"
grep -Fq '"pgy_exit"' "$ROOT_DIR/src/codegen/llvm_runtime_attrs.c"
grep -Fq "strcmp(fn_name, exact_never_return[i])" "$ROOT_DIR/src/codegen/llvm_runtime_attrs.c"
grep -Fq "llvm_module_has_runtime_call_use(ctx, \"pgy_exit\")" "$ROOT_DIR/src/codegen/llvm_api.c"
grep -A2 -F "llvm_module_has_runtime_call_use(ctx, \"pgy_exit\")" \
    "$ROOT_DIR/src/codegen/llvm_api.c" | grep -Fq "return;"
! grep -Fq 'strstr(fn_name, "exit")' "$ROOT_DIR/src/codegen/llvm_runtime_attrs.c"
grep -Fq "llvm_array_format_runtime_name" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "\"pgy_array_pop\"" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "pgy_array_pop_##Suffix" "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h"
grep -Fq "LLVM Slice() receiver requires concrete element type metadata" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVM Slice() receiver requires registered Slice<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "pgy_runtime_panic_out_of_bounds_export" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVMIntUGT, start64" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "len_oob = LLVMBuildICmp" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "remaining = LLVMBuildSub" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVMBuildSelect(ctx->builder, len_is_zero" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVM slot method '%s' requires registered slot local" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, slot_name, &slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "llvm_slot_runtime_arg(ctx, &slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "llvm_direct_slot_read(ctx, &slot_var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "LLVMVarEntry *slot_var" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "slot_var->" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "pgy_codegen_call_name_is_slot_operation(method_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "pgy_codegen_call_name_is_write(method_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "pgy_codegen_call_name_is_read(method_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "pgy_codegen_call_name_is_release(method_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq 'strcmp(method_name, "Write")' "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq 'strcmp(method_name, "Read")' "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq 'strcmp(method_name, "Release")' "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "llvm_domain_slice_error" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -A16 -F "llvm_domain_slice_error(ASTNode *node" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM slot Write() could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVM Slice() receiver did not expose array data storage" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "LLVMStructTypeInContext(ctx->context," "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "pgy_channel_runtime_name(init_fn_name, sizeof(init_fn_name)," "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq '"pgy_array_new_",' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "array literal expression" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "channel send expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "channel receive expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "llvm_resolve_channel_target" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "llvm_resolve_channel_target" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "LLVMChannelTarget target" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "llvm_resolve_channel_target_inner" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_resolve_channel_target_inner" "$ROOT_DIR/src/codegen/llvm_channel_target.c"
! grep -Fq "llvm_lookup_channel_inner(ctx, name)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq "llvm_lookup_channel_inner(ctx, name)" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "LLVMChannelTarget" "$ROOT_DIR/src/codegen/llvm_channel_target.c"
grep -Fq "requires registered Channel<T> local storage" "$ROOT_DIR/src/codegen/llvm_channel_target.c"
grep -Fq "llvm_scope_contains(ctx, name)" "$ROOT_DIR/src/codegen/llvm_channel_target.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &local)" "$ROOT_DIR/src/codegen/llvm_channel_target.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_channel_target.c"
grep -Fq "cannot aggregate-construct or default-initialize Channel<T> field" \
    "$ROOT_DIR/src/semantic/type_checker_call_constructor.c"
grep -Fq "default-zeroing that storage would bypass channel runtime initialization" \
    "$ROOT_DIR/src/semantic/type_checker_call_constructor.c"
grep -Fq "class constructor rejects Channel field storage in expression position" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_b_1.cases.h"
grep -Fq "zone constructor rejects default Channel shared field storage" \
    "$ROOT_DIR/src/tests/semantic/test_semantic_effects_part_b_1.cases.h"
grep -Fq "PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID" \
    "$ROOT_DIR/src/semantic/type_checker_call_constructor.c"
grep -Fq "cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"
grep -Fq "cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"
grep -Fq "cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"
! grep -Fq "return pergyra_strdup(\"0\")" \
    "$ROOT_DIR/src/codegen/transpiler_class_constructor_emit.c"
grep -Fq "PGY_FIX_PROVIDE_MOVABLE_HANDLE" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"
grep -Fq "PGY_FIX_PROVIDE_MOVABLE_HANDLE" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"
grep -Fq "PGY_FIX_PROVIDE_MOVABLE_HANDLE" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"
grep -Fq "transpiler_type_name_is_channel" \
    "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_is_channel" \
    "$ROOT_DIR/src/codegen/transpiler_constructor_channel_guard.c"
grep -Fq "pgy_classify_type(expected_type) == PGY_TK_CHANNEL" \
    "$ROOT_DIR/src/codegen/llvm_expr_constructor_channel_guard.c"
grep -Fq 'current-host' "$ROOT_DIR/docs/semantics/06_backend_parity.md"
grep -Fq '`Channel<T>` fields through the same `LLVMChannelTarget`' \
    "$ROOT_DIR/docs/semantics/06_backend_parity.md"
grep -Fq "must not copy" \
    "$ROOT_DIR/docs/semantics/06_backend_parity.md"
grep -Fq "channel storage by value" \
    "$ROOT_DIR/docs/semantics/06_backend_parity.md"
grep -Fq "llvm_channel_expr_error" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
! grep -A16 -F "llvm_channel_expr_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_channel.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i1, 0, 0)"
grep -Fq "LLVM channel send expression could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "LLVM event subscribe requires generated event function" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "LLVM event invoke requires generated event function" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "LLVM event invocation call requires generated event function and storage" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "LLVM event invocation call could not lower argument expression" "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c"
grep -Fq "if (ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "kLLVMCallInlineSpecs" "$ROOT_DIR/src/codegen/llvm_expr_call_inline_policy.c"
grep -Fq "llvm_call_inline_lookup" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_call_inline_policy.c"
call_inline_names="$(
    sed -n '/static const LLVMCallInlineSpec kLLVMCallInlineSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_call_inline_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
call_inline_names_sorted="$(
    printf '%s\n' "$call_inline_names" | sort
)"
if [[ "$call_inline_names" != "$call_inline_names_sorted" ]]; then
    echo "LLVM central inline call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$call_inline_names_sorted") \
        <(printf '%s\n' "$call_inline_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_member_access_error" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
! grep -A16 -F "llvm_member_access_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_member_access.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "requires concrete receiver type metadata" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
grep -Fq "requires registered receiver class metadata" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
grep -Fq "receiver layout is not compatible" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx," "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
grep -Fq "projection_borrow->source_name, &source_var" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
grep -Fq "LLVMValueRef source_base = source_var.alloca" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
! grep -Fq "LLVMVarEntry *source_var" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
! grep -Fq "source_var->" "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_member_access.c"
grep -Fq "llvm_projection_error_recovery" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
! grep -A16 -F "llvm_projection_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM projection source field '%s' is missing from source metadata" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "LLVM projection source field '%s' is ambiguous across vessel paths" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "LLVM subject projection requires target/source class metadata and source storage" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_name, &source_var)" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "source_base = source_var.alloca" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
! grep -Fq "LLVMVarEntry *source_var" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
! grep -Fq "source_var->" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "llvm_domain_projection_value_error" "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
grep -Fq "LLVM domain projection source path is ambiguous" "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
grep -Fq "LLVM domain projection source field path is missing" "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
! grep -Fq "return LLVMConstInt(ctx->type_i32, 0, 0)" "$ROOT_DIR/src/codegen/llvm_domain_projection_value_helpers.c"
grep -Fq "llvm_projection_binding_error" "$ROOT_DIR/src/codegen/llvm_expr_host_spawn_literal_helpers.c"
grep -Fq "LLVM projection binding requires target/source metadata and source storage" "$ROOT_DIR/src/codegen/llvm_expr_host_spawn_literal_helpers.c"
grep -Fq "llvm_call_error_recovery" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -A16 -F "llvm_call_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_errors.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
! grep -A16 -F "llvm_call_arg_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_errors.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM call expression requires a callee" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "could not lower argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_call_errors.c"
grep -Fq "if (ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, arg_name, &arg_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, arg_name, &v)" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "arg_var.alloca" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "v.alloca" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "LLVMVarEntry *arg_var" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "LLVMVarEntry *v" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_constructor_error" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM enum variant constructor could not lower payload argument" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM class constructor could not lower field argument" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM hosted method call argument allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, arg_name, &arg_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
! grep -Fq "llvm_scope_lookup(ctx, arg_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
grep -Fq "LLVM callable variable call could not lower callee expression" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable variable call could not lower callable signature" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable argument could not lower callable signature" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, callee_name, &callee_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
! grep -Fq "llvm_scope_lookup(ctx, callee_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "callable_entry = llvm_lookup_callable_entry(ctx, callee_name)" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "llvm_function_signature_from_callable_entry(ctx, callable_entry)" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable signature parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "LLVM lambda signature parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "if (ctx->has_error || var_type == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "LLVM event-handler type parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c"
grep -Fq "LLVM tuple type field allocation failed" "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c"
grep -Fq "LLVM type rendering requires concrete type metadata" "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c"
! grep -A8 -F "LLVM AST type mapping requires AST_TYPE" \
    "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "LLVM intent forward declaration parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM intent forward declaration could not lower participant type" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM intent forward declaration could not lower ordered binding type" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM intent forward declaration requires binding type metadata; silent i8ptr fallback is not allowed" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "pt = ctx->type_i8ptr;" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "if (ctx->has_error || participant_value_type == NULL)" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, zone_alias, &zone_var)" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, alias, &participant_var)" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
grep -Fq "zone_var.alloca" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
grep -Fq "participant_var.alloca" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
! grep -Fq "LLVMVarEntry *zone_var" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
! grep -Fq "LLVMVarEntry *participant_var" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, zone_alias, &zone_var)" "$ROOT_DIR/src/codegen/llvm_intent_effect.c"
grep -Fq "zone_var.alloca" "$ROOT_DIR/src/codegen/llvm_intent_effect.c"
! grep -Fq "LLVMVarEntry *zone_var" "$ROOT_DIR/src/codegen/llvm_intent_effect.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_intent_effect.c"
! grep -Fq "ast_func_param_count(current_decl)" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "forward_param_count" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM boundary call argument lowering failed" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_boundary_args_error" "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "LLVM boundary call source argument count does not match function signature" "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "LLVM secure boundary slot argument requires paired token binding" "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "LLVM boundary call argument could not be lowered" "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "LLVM call helper could not lower argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_call_args.c"
grep -Fq "llvm_generic_call_required_suffix" "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "requires concrete argument %zu type metadata for specialization" "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "requires argument %zu to bind generic parameter" "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "LLVM generic spawn specialization parameter type allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "LLVM generic specialization '%s' reached backend without an all-path return terminator" "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
! grep -Fq "LLVMBuildRet(ctx->builder, LLVMConstInt(ret, 0, 0))" "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "LLVM spawn expression requires an identifier target" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "LLVM spawn expression requires a target expression" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
! grep -Fq "return LLVMConstNull(ctx->type_task_handle)" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
! grep -A16 -F "llvm_spawn_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A32 -F "llvm_mir_required_type_from_ast(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A12 -F "llvm_mir_type_from_ast(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c" | \
    grep -Fq "return ctx->type_i32"
! grep -Fq "strstr(layout_name" "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
! grep -Fq 'strncmp(runtime_fn, "pgy_claim_' "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
grep -Fq "llvm_mir_pinned_view_type_from_layout" "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
grep -Fq "llvm_constructed_arg_name_copy(layout_name, 0" "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
! grep -Fq 'strcmp(layout_name, "PinnedSlotView<Int>")' "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
! grep -Fq 'strcmp(layout_name, "PinnedSecureSlotView<Int>")' "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
grep -Fq 'strcmp(type_name, "Future") == 0' "$ROOT_DIR/src/codegen/llvm_type.c"
grep -Fq 'strcmp(type_name, "RemoteFuture") == 0' "$ROOT_DIR/src/codegen/llvm_type.c"
grep -Fq "LLVMTypeRef var_type = NULL" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "implicit Int fallback is disabled" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
! grep -Fq "LLVMTypeRef var_type = ctx->type_i32" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
! grep -Fq "llvm_derive_slot_inner_from_current_decl" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq "ast_func_param_count(current_decl)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq "llvm_find_local_let_type_in_block" "$ROOT_DIR/src/codegen/llvm_expr_common.c"
! grep -Fq "llvm_infer_local_let_type_in_block" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_nominal.c"
grep -Fq "llvm_mir_local_elem_type_from_layout" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "LLVMTypeRef elem_type =" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_slice_fact_elem_type_from_receiver(ctx" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_local_require_elem_type" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "LLVMTypeRef elem_type = ctx->type_i32" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "LLVMTypeRef pt = NULL" "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
! grep -Fq "LLVMTypeRef pt = ctx->type_i32" "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "if (ctx->has_error || alloca_type == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_local_type_from_value_fact" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_async_fact_type_from_channel_recv" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_async_fact_future_inner_from_source_local" \
    "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "llvm_mir_try_emit_await_local_def" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "init = inst->expr0" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "type_ann = inst->expr1" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "operand->type == AST_SPAWN_EXPR" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "inner = llvm_infer_spawn_future_inner(ctx, operand)" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "resource_name = operand->type == AST_SPAWN_EXPR ? \"spawn\" : future_name" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "llvm_mir_find_await_resource_op(mir_block, resource_name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
! grep -Fq "mir_instruction_source_payload" \
    "$ROOT_DIR/src/codegen/llvm_mir_await_emit.c"
grep -Fq "mir_routine_source_local_type_name(routine, local_name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c"
grep -Fq "mir_routine_source_local_type_name(routine, future_name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c"
grep -Fq "llvm_constructed_arg_name_copy(type_name, 0, inner" \
    "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c"
if grep -Fq "ast_type_generic_args(" "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c" \
    || grep -Fq "ast_generic_param_constraint(" "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c" \
    || grep -Fq "ast_let_type(" "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c" \
    || grep -Fq "mir_instruction_source_payload(" "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c" \
    || grep -Fq "llvm_infer_spawn_future_inner" "$ROOT_DIR/src/codegen/llvm_mir_async_fact.c"; then
    echo "[perf-contract] LLVM MIR async facts must consume MIR source-local type names, not AST type payloads" >&2
    exit 1
fi
for term in \
    "case AST_BINARY:" \
    "ast_binary_left(node)" \
    "ast_binary_right(node)" \
    "case AST_UNARY:" \
    "ast_unary_operand(node)" \
    "case AST_ARRAY_LITERAL:" \
    "ast_array_literal_element(node, i)" \
    "case AST_MAP_LITERAL:" \
    "ast_map_literal_key(node, i)" \
    "ast_map_literal_value(node, i)"; do
    grep -Fq "$term" "$ROOT_DIR/src/compiler/rir_builder_walk.c"
done
grep -Fq "llvm_mir_slice_fact_type_from_call" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_get_var_entry(vars, var_count, name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_slice_fact.c"
! grep -A24 -F "llvm_decl_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_decl.c" | \
    grep -Fq "return ctx->type_i32"
! grep -Fq "llvm_decl_implicit_self_placeholder_type" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
grep -Fq "LLVM implicit self parameter requires current host metadata" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
! grep -A24 -F "llvm_register_required_ast_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_register.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A24 -F "llvm_domain_event_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "LLVMBasicBlockRef saved_bb" "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "restore_state:" "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "LLVMPositionBuilderAtEnd(ctx->builder, saved_bb)" "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "event parameter type allocation failed" "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "event invoke parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "event invoke call argument allocation failed" "$ROOT_DIR/src/codegen/llvm_domain_event.c"
! grep -A24 -F "llvm_domain_forward_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "LLVM domain method parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "LLVM role method parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_role.c"
grep -Fq "LLVM ability vtable field allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_ability.c"
grep -Fq "LLVM ability method parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward_ability.c"
grep -Fq "LLVM intent forward parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_intent_forward.c"
! grep -A24 -F "llvm_domain_required_ast_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "llvm_domain_required_class_struct_type" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c"
grep -Fq "requires registered class metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c"
grep -Fq "if (ctx->has_error || field_types[j] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM enum field allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM enum payload field allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM enum method parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM payload enum method self type requires registered enum metadata" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
! grep -Fq "LLVMTypeRef self_type = ctx->type_i32" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM class field allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM class method parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM extern parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "LLVM function declaration parameter allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
grep -Fq "LLVM domain struct field allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"
grep -Fq "LLVM world sync previous-active allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c"
grep -Fq "LLVM zone frontier previous-state allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_frontier_state.c"
grep -Fq "LLVM role vtable value allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "LLVM intent completion allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "LLVM intent participant rebind allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "LLVM MIR PHI incoming allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_mir_phi.c"
grep -Fq "LLVM MIR PHI result allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_mir_phi.c"
grep -Fq "restore_builder:" \
    "$ROOT_DIR/src/codegen/llvm_mir_phi.c"
grep -Fq "LLVM select rotation block allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "LLVM scope declaration requires concrete name and type metadata" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "LLVMLexicalRegistrySnapshot lexical_snapshot" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "if (scope_pushed)" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "LLVMBasicBlockRef saved_bb" \
    "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "LLVMLexicalRegistrySnapshot lexical_snapshot" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c"
grep -Fq "LLVMBasicBlockRef saved_bb" \
    "$ROOT_DIR/src/codegen/llvm_domain_world_sync.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_generic.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_lambda_type.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_helpers.c"
grep -Fq "LLVMBasicBlockRef saved_bb" \
    "$ROOT_DIR/src/codegen/llvm_domain_projection_sync_helpers.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync.c"
grep -Fq "LLVMBasicBlockRef saved_bb" \
    "$ROOT_DIR/src/codegen/llvm_domain_zone_sync.c"
grep -Fq "LLVMBasicBlockRef saved_bb" \
    "$ROOT_DIR/src/codegen/llvm_domain_sync_frontier.h"
grep -Fq "LLVMPositionBuilderAtEnd(ctx->builder, saved_bb)" \
    "$ROOT_DIR/src/codegen/llvm_domain_sync_frontier.c"
! grep -R -Fq "LLVMGetLastBasicBlock(saved_fn)" \
    "$ROOT_DIR/src/codegen"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "llvm_lexical_registry_restore(ctx, lexical_snapshot)" \
    "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "if (ctx->has_error || ptypes[k] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
grep -Fq "if (ctx->has_error || ptypes[j] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c"
grep -Fq "if (ctx->has_error || pt == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c"
grep -Fq "if (ctx->has_error || ftypes[idx] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_register.c"
grep -Fq "if (ctx->has_error || pt == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
# The non-Void / all-path return invariant moved out of llvm_decl.c:
# normal-function emission goes through the MIR pipeline, so the only
# LLVM-side site that still emits a function body directly is the
# spawn-generic specialization path (asserted at line 3948 above).
# The save_bb scaffolding for function-emission state likewise moved
# out of llvm_decl.c to llvm_expr.c / llvm_main_wrapper.c /
# llvm_mir_emit.c / llvm_stmt_parallel_async.c. Assert presence at
# one of the new owners so the invariant remains source-gated.
grep -Fq "LLVM return statement could not lower value expression" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM non-Void return statement requires a value expression" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
! grep -Fq "LLVMConstNull(ctx->current_ret_type)" \
    "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
! grep -Fq "LLVMGetLastBasicBlock(saved_fn)" \
    "$ROOT_DIR/src/codegen/llvm_decl.c"
grep -Fq "LLVMBasicBlockRef saved_bb" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
! grep -Fq "LLVMGetLastBasicBlock(saved_fn)" \
    "$ROOT_DIR/src/codegen/llvm_domain_role_emit.c"
grep -Fq "if (ctx->has_error || param_types[i] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "LLVM MIR routine '%s' parameter type allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "LLVM MIR routine '%s' local registry allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "LLVM MIR routine '%s' block registry allocation failed" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "LLVM MIR routine '%s' reached backend without a terminal return value" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
! grep -Fq "LLVMConstNull(ret_type)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "Closure #74: a non-void block with no successors and no" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "LLVMConstNull(ctx->current_ret_type)" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "LLVMPositionBuilderAtEnd(ctx->builder, saved_bb)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
if awk '
    /LLVMPositionBuilderAtEnd\(ctx->builder, saved_bb\)/ {
        if (prev !~ /saved_bb != NULL/)
            bad = 1
    }
    { prev = $0 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
    echo "[perf-contract] LLVM parallel/async wrapper restores saved_bb without null guard" >&2
    exit 1
fi
grep -Fq "frame->entries[j].alloca == NULL" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM parallel capture requires storage-backed binding" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_capture_reject_shared_collection" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM %s capture '%s' cannot share mutable collection" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_lookup_array_var(ctx, name)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_lookup_map_value(ctx, name)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "transpiler_capture_reject_shared_collection" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "cannot share mutable collection" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "TranspilerParallelCallableCapture capture_typed_callables" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "pergyra_func_pointer_declarator_from_type_names_in_ctx" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "transpiler_current_local_callable_capture" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
grep -Fq "mir_routine_source_local_type_fact(routine, name)" \
    "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
if grep -R -n -F "transpiler_find_local_type_ast" \
        "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' >/dev/null; then
    echo "[perf-contract] C backend reintroduced generic local type-AST lookup" >&2
    exit 1
fi
if grep -Fq "transpiler_find_local_type_name(ctx" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"; then
    echo "[perf-contract] C parallel/async emit reintroduced capture type fallback lookup" >&2
    exit 1
fi
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(type_name, false)" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(type_name, true)" \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_type_name(" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "param_type_name, true" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "codegen_worker_boundary_storage_kind_from_constructor_name" \
    "$ROOT_DIR/src/codegen/llvm_expr_spawn_worker_boundary.c"
grep -Fq "LLVM async capture requires storage-backed binding" \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "if (ctx->has_error || pt == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_param_emit.c"
grep -Fq "could not lower argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "loaded-argument allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
if awk '
    /LLVMAddFunction\(ctx->module, wrapper_name/ { seen_add = 1 }
    /argument type allocation failed/ && seen_add { bad = 1 }
    /loaded-argument allocation failed/ && seen_add { bad = 1 }
    END { exit bad ? 0 : 1 }
' "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"; then
    echo "[perf-contract] LLVM spawn wrapper function is created before wrapper preflight" >&2
    exit 1
fi
grep -Fq "llvm_member_call_error_recovery" "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
! grep -A16 -F "llvm_member_call_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_member_call_support.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "could not lower an argument" "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq "could not allocate method name" "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq "requires a self receiver" "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
grep -Fq "is not declared in the backend method registry" "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
grep -Fq "bool has_var = llvm_scope_lookup_snapshot(ctx, var_name, &var)" \
    "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
grep -Fq "var.alloca" "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
grep -Fq 'llvm_scope_lookup_snapshot(ctx, "self", &self_var)' \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"
grep -Fq "self_var.alloca" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"
! grep -Fq "llvm_scope_lookup(ctx," \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_world_effect_sync.c"
grep -Fq 'llvm_scope_lookup_snapshot(ctx, "self", &self_var)' \
    "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"
grep -Fq "self_var.alloca" "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_stmt_zone_action.c"
grep -Fq "llvm_vtable_dispatch_error" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"
grep -Fq "requires a registered receiver variable" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"
grep -Fq "could not lower call argument" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c"
grep -Fq "if (ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
grep -Fq "LLVM checked unwrap requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "LLVM call target '%s' is not declared in the backend function registry" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "[llvm] warning: unknown function" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "if (ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "llvm_rc_error_recovery" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "kLLVMRcSpecs" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "llvm_rc_lookup" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
rc_builtin_names="$(
    sed -n '/static const LLVMRcSpec kLLVMRcSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
rc_builtin_names_sorted="$(
    printf '%s\n' "$rc_builtin_names" | sort
)"
if [[ "$rc_builtin_names" != "$rc_builtin_names_sorted" ]]; then
    echo "LLVM Rc/Weak builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$rc_builtin_names_sorted") \
        <(printf '%s\n' "$rc_builtin_names") >&2 || true
    exit 1
fi
! grep -A16 -F "llvm_rc_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "*out = NULL;" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "LLVM RcNew could not lower payload expression" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "LLVM RcNew expected payload type could not be lowered" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "LLVM RcGet payload type could not be lowered" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "kPgyHostDeclCompatTypes[]" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "pgy_host_decl_compat_name" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "pgy_host_decl_compat_uses_pointer_self" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "pgy_host_decl_compat_types(&host_type_count)" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "host_types[i]" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
! grep -Fq "kLLVMHostDeclTypes" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
! grep -Fq "llvm_host_decl_type_count()" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "mir_find_decl_header_of_type(ctx->mir, decl_type, name)" \
    "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "mir_decl_header_ast_type_or(" \
    "$ROOT_DIR/src/codegen/transpiler_decl_method_view.c"
grep -Fq "AST_PARTY_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "AST_ROLE_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "AST_ROSTER_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "ChannelClose" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c"
grep -Fq "pgy_channel_runtime_name(fname, sizeof(fname)," "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "\"close\", target.inner" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "pgy_lane_channel_runtime_name(fname, sizeof(fname)," "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "llvm_task_channel_error" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "could not lower send value expression" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "has unsupported arity for the LLVM task/channel builtin" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "kTaskRuntimeSpecs" "$ROOT_DIR/src/codegen/llvm_expr_task_calls.c"
grep -Fq "llvm_task_runtime_lookup" "$ROOT_DIR/src/codegen/llvm_expr_task_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_task_calls.c"
task_runtime_names="$(
    sed -n '/static const LLVMTaskRuntimeSpec kTaskRuntimeSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
task_runtime_names_sorted="$(
    printf '%s\n' "$task_runtime_names" | sort
)"
if [[ "$task_runtime_names" != "$task_runtime_names_sorted" ]]; then
    echo "LLVM task runtime names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$task_runtime_names_sorted") \
        <(printf '%s\n' "$task_runtime_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_task_channel_name_compare" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c"
grep -Fq "llvm_channel_query_op_compare" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c"
grep -Fq "llvm_channel_query_runtime_op(callee_name)" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "kTaskChannelOpSpecs" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c"
grep -Fq "llvm_task_channel_op_lookup" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "llvm_stdlib_runtime_call_compare" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_stdlib_string_file_runtime_call_lookup" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
task_channel_names="$(
    sed -n '/static const LLVMTaskChannelNameSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
task_channel_names_sorted="$(
    printf '%s\n' "$task_channel_names" | sort
)"
if [[ "$task_channel_names" != "$task_channel_names_sorted" ]]; then
    echo "LLVM task/channel builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$task_channel_names_sorted") \
        <(printf '%s\n' "$task_channel_names") >&2 || true
    exit 1
fi
task_channel_op_names="$(
    sed -n '/static const LLVMTaskChannelOpSpec kTaskChannelOpSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
task_channel_op_names_sorted="$(
    printf '%s\n' "$task_channel_op_names" | sort
)"
if [[ "$task_channel_op_names" != "$task_channel_op_names_sorted" ]]; then
    echo "LLVM task/channel operation names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$task_channel_op_names_sorted") \
        <(printf '%s\n' "$task_channel_op_names") >&2 || true
    exit 1
fi
channel_query_names="$(
    sed -n '/static const LLVMChannelQueryOpSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
channel_query_names_sorted="$(
    printf '%s\n' "$channel_query_names" | sort
)"
if [[ "$channel_query_names" != "$channel_query_names_sorted" ]]; then
    echo "LLVM channel query op names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$channel_query_names_sorted") \
        <(printf '%s\n' "$channel_query_names") >&2 || true
    exit 1
fi
llvm_stdlib_string_file_names="$(
    sed -n '/static const LLVMStdlibRuntimeCallSpec kLLVMStdlibStringFileRuntimeSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_string_file_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_string_file_names" | sort
)"
if [[ "$llvm_stdlib_string_file_names" != "$llvm_stdlib_string_file_names_sorted" ]]; then
    echo "LLVM stdlib string/file runtime-call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_string_file_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_string_file_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_stdlib_runtime_io_call_lookup" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_stdlib_string_special_lookup" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_stdlib_io_special_lookup" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "kLLVMStdlibStringFileRuntimeSpecs" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "kLLVMStdlibRuntimeIoSpecs" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "kLLVMStdlibStringSpecialSpecs" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "kLLVMStdlibIoSpecialSpecs" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
llvm_stdlib_runtime_io_names="$(
    sed -n '/static const LLVMStdlibRuntimeCallSpec kLLVMStdlibRuntimeIoSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_runtime_io_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_runtime_io_names" | sort
)"
if [[ "$llvm_stdlib_runtime_io_names" != "$llvm_stdlib_runtime_io_names_sorted" ]]; then
    echo "LLVM stdlib runtime/io names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_runtime_io_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_runtime_io_names") >&2 || true
    exit 1
fi
llvm_stdlib_string_special_names="$(
    sed -n '/static const LLVMStdlibStringSpecialSpec kLLVMStdlibStringSpecialSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_string_special_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_string_special_names" | sort
)"
if [[ "$llvm_stdlib_string_special_names" != "$llvm_stdlib_string_special_names_sorted" ]]; then
    echo "LLVM stdlib string special names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_string_special_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_string_special_names") >&2 || true
    exit 1
fi
llvm_stdlib_io_special_names="$(
    sed -n '/static const LLVMStdlibIoSpecialSpec kLLVMStdlibIoSpecialSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_io_special_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_io_special_names" | sort
)"
if [[ "$llvm_stdlib_io_special_names" != "$llvm_stdlib_io_special_names_sorted" ]]; then
    echo "LLVM stdlib io special names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_io_special_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_io_special_names") >&2 || true
    exit 1
fi
grep -Fq "llvm_required_runtime_function(ctx, node" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "\"rc\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "\"weak\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "\"device slot\", callee_name, runtime_fn" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_slot_inner_has_external_runtime_helpers" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
! grep -A12 -F "llvm_direct_slot_read(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
! grep -A12 -F "llvm_direct_secure_slot_read(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "if (ctx->has_error || inner_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "if (ctx->has_error || inner_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_expr_await_task.c"
grep -Fq "llvm_emit_structural_secure_slot_write" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "llvm_emit_structural_secure_slot_read" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_emit_structural_secure_slot_release" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "llvm_require_secure_token_var" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "bool          llvm_lookup_secure_token_var(LLVMGenCtx *ctx" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
grep -Fq "bool          llvm_require_secure_token_var(LLVMGenCtx *ctx" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
! grep -Fq "LLVMVarEntry *llvm_lookup_secure_token_var" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
! grep -Fq "LLVMVarEntry *llvm_require_secure_token_var" "$ROOT_DIR/src/codegen/llvm_internal_api.h"
grep -Fq "llvm_lookup_secure_token_var(ctx, slot_name, out)" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, token_name, out)" "$ROOT_DIR/src/codegen/llvm_registry_resources.c"
! grep -Fq "return llvm_scope_lookup(ctx, token_name)" "$ROOT_DIR/src/codegen/llvm_registry_resources.c"
grep -Fq "llvm_require_secure_token_var(ctx, node, name" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_require_secure_token_var(ctx, node, name" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_lookup_secure_token_var(ctx, source_name," "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_mir_base_name_from_versioned(source_name," "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "source_base_name, &token_var)" "$ROOT_DIR/src/codegen/llvm_expr_boundary_projection_helpers.c"
grep -Fq "llvm_lookup_secure_token_var(ctx, vname, &token_var)" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "llvm_lookup_secure_token_var(ctx, alias, &token_var)" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
! grep -RInF --include='*.c' --include='*.h' "LLVMVarEntry *token_var" "$ROOT_DIR/src/codegen" >/dev/null
! grep -RInF --include='*.c' --include='*.h' "token_var->" "$ROOT_DIR/src/codegen" >/dev/null
grep -Fq "requires paired token binding" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "requires a registered slot local" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "bool llvm_resolve_slot_target(LLVMGenCtx *ctx" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.h"
grep -Fq "llvm_resolve_slot_target(LLVMGenCtx *ctx, ASTNode *slot_arg" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq "LLVMVarEntry *llvm_resolve_slot_target" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.h"
grep -Fq "llvm_scope_lookup_snapshot(ctx, source_name, slot_out)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &entry)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_slot_runtime_arg(ctx, &var)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_direct_slot_read(ctx, &var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_identifier_error" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -A16 -F "llvm_identifier_error(ASTNode *node" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM host field access requires a self receiver" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "LLVM identifier is not declared in the active scope" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_slot_builtin_require_argc" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "kLLVMSlotBuiltinSpecs" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_slot_builtin_lookup" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_resolve_slot_target(ctx, slot_arg, &slot_var" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, slot_name, &slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_slot_runtime_arg(ctx, &slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVMValueRef args[] = { slot_var.alloca" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "LLVMVarEntry *slot_var" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "slot_var->" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
slot_builtin_names="$(
    sed -n '/static const LLVMSlotBuiltinSpec kLLVMSlotBuiltinSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
slot_builtin_names_sorted="$(
    printf '%s\n' "$slot_builtin_names" | sort
)"
if [[ "$slot_builtin_names" != "$slot_builtin_names_sorted" ]]; then
    echo "LLVM Slot/DeviceSlot builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$slot_builtin_names_sorted") \
        <(printf '%s\n' "$slot_builtin_names") >&2 || true
    exit 1
fi
grep -Fq "requires at least %zu argument(s)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "*out = NULL;" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM slot auto-read requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "LLVM slot assignment requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
! grep -Fq 'is_secure ? "pgy_secure_write_%s" : "pgy_write_%s"' "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "indexed array assignment" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "LLVM indexed array assignment requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_stmt_resolve_array_elem_type(ctx, array_node, NULL)" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_assignment_error" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
! grep -A16 -F "llvm_assignment_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
! grep -Fq "requires registered Array<T> local" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "requires concrete Array<T> local metadata" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "requires a writable member lvalue" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "requires a registered local or host field target" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &local_alias)" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &arr_var)" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_scope_lookup_snapshot(ctx, name, &var)" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_emit_structural_secure_slot_write(ctx, &var, val)" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_direct_slot_write(ctx, &var, val)" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
! grep -Fq "llvm_scope_lookup(ctx," "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "\"secure slot\" : \"slot\"" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "MIR_RESOURCE_ABI_DEVICE_SLOT" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "row->call_shape" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq '"SubmitRead"' "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq '"pgy_submit_device_read", inner' "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_slot_format_runtime_name" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_write(ctx, slot_var, val)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_read(ctx, slot_var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_release(ctx, slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM with-slot cleanup requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
! grep -Fq 'is_secure ? "pgy_secure_release_%s" : "pgy_release_%s"' "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "LLVM slot initializer requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
! grep -Fq 'is_secure ? "pgy_secure_write_%s" : "pgy_write_%s"' "$ROOT_DIR/src/codegen/llvm_stmt_let_names.c"
grep -Fq "LLVM auto-release requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/llvm_stmt.c"
! grep -Fq 'is_secure ? "pgy_secure_release_" : "pgy_release_"' "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM secure slot auto-release requires paired token binding" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM secure with-slot cleanup requires paired token binding" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "llvm_select_required_runtime_function" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "LLVMSelectCaseInfo" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "llvm_select_case_info" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "llvm_select_emit_bound_receive_case" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "llvm_select_emit_ready_consume_case" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "\"LLVM select %s requires registered runtime function '%s'\"" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "ctx, info->channel, \"receive\", fn_name" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "ctx, info->channel, \"readiness\", fn_name" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "ctx, info->channel, \"consume\", recv_name" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "LLVMBuildAtomicRMW(ctx->builder" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "LLVMAtomicOrderingMonotonic" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "select_write_case_guard" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "select_emit_unbound_consume" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "select_channel_inner_type" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "static _Atomic unsigned int _sel_rr_" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "atomic_fetch_add_explicit(&_sel_rr_" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "transpiler_channel_query_spec_compare" "$channel_builtin_owner"
grep -Fq "emit_call_stdlib_channel_query_builtin" "$channel_builtin_owner"
scalar_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_builtin.c"
scalar_unary_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_unary.c"
grep -Fq "transpiler_scalar_unary_spec_compare" "$scalar_unary_owner"
grep -Fq "transpiler_scalar_unary_builtin_name(fn" "$scalar_builtin_owner"
transpiler_scalar_unary_names="$(
    sed -n '/static const TranspilerScalarUnarySpec specs\[\]/,/^    };/p' \
        "$scalar_unary_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_scalar_unary_names_sorted="$(
    printf '%s\n' "$transpiler_scalar_unary_names" | sort
)"
if [[ "$transpiler_scalar_unary_names" != "$transpiler_scalar_unary_names_sorted" ]]; then
    echo "C backend scalar unary names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_scalar_unary_names_sorted") \
        <(printf '%s\n' "$transpiler_scalar_unary_names") >&2 || true
    exit 1
fi
grep -Fq "kTranspilerScalarSpecs" "$scalar_builtin_owner"
grep -Fq "transpiler_scalar_lookup" "$scalar_builtin_owner"
grep -Fq "bsearch(" "$scalar_builtin_owner"
grep -Fq "transpiler_scalar_emit_arg" "$scalar_builtin_owner"
grep -Fq "C backend: scalar builtin %s could not lower %s argument" "$scalar_builtin_owner"
! grep -Fq "return pergyra_strdup(\"0\")" "$scalar_builtin_owner"
grep -Fq "fn, \"value\"" "$scalar_builtin_owner"
grep -Fq "fn, \"separator\"" "$scalar_builtin_owner"
grep -Fq "fn, \"seed\"" "$scalar_builtin_owner"
transpiler_scalar_names="$(
    sed -n '/static const TranspilerScalarSpec kTranspilerScalarSpecs\[\]/,/^};/p' \
        "$scalar_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_scalar_names_sorted="$(
    printf '%s\n' "$transpiler_scalar_names" | sort
)"
if [[ "$transpiler_scalar_names" != "$transpiler_scalar_names_sorted" ]]; then
    echo "C backend scalar builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_scalar_names_sorted") \
        <(printf '%s\n' "$transpiler_scalar_names") >&2 || true
    exit 1
fi
transpiler_channel_query_names="$(
    sed -n '/static const TranspilerChannelQuerySpec specs\[\]/,/^    };/p' \
        "$channel_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_channel_query_names_sorted="$(
    printf '%s\n' "$transpiler_channel_query_names" | sort
)"
if [[ "$transpiler_channel_query_names" != "$transpiler_channel_query_names_sorted" ]]; then
    echo "C backend channel query names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_channel_query_names_sorted") \
        <(printf '%s\n' "$transpiler_channel_query_names") >&2 || true
    exit 1
fi
grep -Fq "transpiler_channel_spec_compare" "$channel_builtin_owner"
grep -Fq "transpiler_channel_lookup(fn, argc)" "$channel_builtin_owner"
grep -Fq "bsearch(" "$channel_builtin_owner"
grep -Fq "transpiler_channel_emit_lvalue_arg" "$channel_builtin_owner"
grep -Fq "transpiler_channel_emit_arg" "$channel_builtin_owner"
grep -Fq "C backend: channel builtin %s could not lower channel argument" "$channel_builtin_owner"
grep -Fq "C backend: channel builtin %s could not lower %s argument" "$channel_builtin_owner"
! grep -Fq "return pergyra_strdup(\"0\")" "$channel_builtin_owner"
grep -Fq "\"TrySend\", \"value\"" "$channel_builtin_owner"
grep -Fq "\"RecvTimeout\", \"timeout\"" "$channel_builtin_owner"
transpiler_channel_names="$(
    sed -n '/static const TranspilerChannelSpec specs\[\]/,/^    };/p' \
        "$channel_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_channel_names_sorted="$(
    printf '%s\n' "$transpiler_channel_names" | sort
)"
if [[ "$transpiler_channel_names" != "$transpiler_channel_names_sorted" ]]; then
    echo "C backend channel builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_channel_names_sorted") \
        <(printf '%s\n' "$transpiler_channel_names") >&2 || true
    exit 1
fi
! grep -Fq "ast_create_channel_recv(channel)" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "llvm_mir_required_channel_ready_function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_mir_required_channel_ready_function(channel, ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "sequential fallback is disabled" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "synchronous fallback is disabled" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM spawn expression requires registered runtime functions" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "LLVM await expression requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_await_task.c"
grep -Fq "LLVM await expression could not lower task handle expression" "$ROOT_DIR/src/codegen/llvm_expr_await_task.c"
grep -Fq "llvm_stmt_type_infer_await.c" "$ROOT_DIR/Makefile"
grep -Fq "LLVM await expression type inference requires Future<T> metadata" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c"
grep -Fq "operand '%s' has no registered Future<T> metadata" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c"
grep -A6 -F 'if (strcmp(inner, "Void") == 0)' \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c" | \
    grep -Fq "return ctx->type_void"
! grep -A6 -F 'if (strcmp(inner, "Void") == 0)' \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "llvm_stmt_require_non_void_value" "$ROOT_DIR/src/codegen/llvm_stmt_emit_support.c"
grep -Fq "LLVM void function return must not carry a value expression" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM return statement cannot consume a Void expression value" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM if statement cannot consume a Void expression as condition" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM constructor field '%s' cannot consume a Void expression value" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM constructor field '%s' could not lower initializer expression" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM class constructor could not lower shared-field initializer" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "if (!llvm_emit_class_constructor_shared_defaults" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM MIR for-range could not lower %s expression" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "llvm_mir_loop_bound_error(ctx, \"start\")" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "llvm_mir_loop_bound_error(ctx, \"end\")" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "LLVM MIR branch condition could not be lowered" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "if (cond == NULL && ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "cond = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_match_lower_error" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"
grep -Fq "LLVM match lowering could not lower case pattern" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"
grep -Fq "LLVM match lowering could not lower guard expression" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"
grep -Fq "LLVM MIR match lowering could not lower guard expression" "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "LLVM MIR match lowering could not lower subject expression" "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "LLVM MIR match lowering could not lower case pattern" "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
grep -Fq "LLVM MIR match body binding could not lower subject expression" "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"
! grep -A6 -F "ast_match_case_pattern_count(case_node) > 1" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c" | \
    grep -Fq "continue;"
! grep -A3 -F "guard = llvm_emit_expression(ast_match_case_guard(case_node), ctx)" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c" | \
    grep -Fq "guard = LLVMConstInt"
grep -Fq "LLVM if statement could not lower condition expression" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM while statement cannot consume a Void expression as condition" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM for-in statement cannot consume a Void expression as iterable" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM for-range start cannot consume a Void expression value" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM for-range end cannot consume a Void expression value" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
! grep -Fq "cond = LLVMConstInt(ctx->type_i1, 0, 0)" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM destructuring let could not lower initializer expression" "$ROOT_DIR/src/codegen/llvm_stmt_destructure.c"
grep -Fq "LLVM match lowering could not lower subject expression" "$ROOT_DIR/src/codegen/llvm_stmt_match.c"
grep -Fq "LLVM intent predicate could not lower condition expression" "$ROOT_DIR/src/codegen/llvm_intent_emit_support.c"
grep -A6 -F "llvm_required_task_function(ctx, node, callee_name" \
    "$ROOT_DIR/src/codegen/llvm_expr_task_calls.c" | \
    grep -Fq "if (fn == NULL)"
grep -Fq "LLVM for-range lowering could not lower %s expression" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_for_range_bound_error(ast_for_range_start(node), ctx, \"start\")" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "llvm_for_range_bound_error(ast_for_range_end(node), ctx, \"end\")" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM MIR DEF cannot consume a Void expression value" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "LLVM MIR branch cannot consume a Void expression as condition" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "LLVM MIR return cannot consume a Void expression value" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "LLVM MIR return could not lower value expression" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "LLVM MIR non-Void return requires a value expression" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
if grep -B4 -F "LLVM MIR non-Void return requires a value expression" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" | \
    grep -Fq "llvm_set_error_at_with_hints"; then
    echo "[perf-contract] LLVM MIR non-Void return topology diagnostic must not reopen source payload anchors" >&2
    exit 1
fi
grep -Fq "llvm_stmt_infer_expr_type(ctx, arg)" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM enum variant constructor '%s' cannot consume a Void expression as payload %zu" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM call helper cannot pass a Void expression as argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_call_args.c"
grep -Fq "LLVMTypeRef arg_type = llvm_stmt_infer_expr_type(ctx, arg_nodes[i])" "$ROOT_DIR/src/codegen/llvm_expr_call_args.c"
grep -Fq "LLVM call '%s' cannot consume a Void expression as argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM intent call '%s' cannot consume a Void expression as argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM hosted method call '%s' cannot consume a Void expression as argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
grep -Fq "llvm_void_expression_placeholder" "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c"
for void_call_owner in \
    "$ROOT_DIR/src/codegen/llvm_expr_call_args.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_methods_vtable_dispatch.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_await_task.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
    "$ROOT_DIR/src/codegen/llvm_member_call_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_event_calls.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c" \
    "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"; do
    if grep -Fq "return LLVMConstInt(ctx->type_i32, 0, 0)" "$void_call_owner" \
        || grep -Fq "result = LLVMConstInt(ctx->type_i32, 0, 0)" "$void_call_owner"; then
        echo "[perf-contract] LLVM void expression placeholder bypassed owner in $void_call_owner" >&2
        exit 1
    fi
done
grep -Fq "C backend: call '%s' cannot consume a Void expression as argument %zu" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "C backend: hosted method '%s.%s' cannot consume a Void expression as argument %zu" "$ROOT_DIR/src/codegen/transpiler_expr_call_user_emit.c"
grep -Fq "C constructor field '%s' cannot consume a Void expression value" "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
grep -Fq "C constructor field '%s' could not lower initializer expression" "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
! grep -Fq 'arg != NULL ? arg : "0"' "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
! grep -Fq 'init_expr != NULL ? init_expr : "0"' "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c"
grep -Fq "C enum variant constructor '%s' cannot consume a Void expression as payload %zu" "$ROOT_DIR/src/codegen/transpiler_enum_constructor_emit.c"
grep -Fq "C enum variant constructor '%s' could not lower payload %zu" "$ROOT_DIR/src/codegen/transpiler_enum_constructor_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_enum_constructor_emit.c"
grep -Fq "C party instance '%s' could not lower field '%s' expression" "$ROOT_DIR/src/codegen/transpiler_expr_party_instance_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_expr_party_instance_emit.c"
grep -Fq "transpiler_let_emit_initializer" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "C let binding '%s' could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "transpiler_let_collection_emit_arg" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "C collection let binding '%s' could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_box_let_emit_arg" "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
grep -Fq "C Box/Rc let binding '%s' could not lower %s expression" "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c"
grep -Fq "C destructuring let could not lower initializer expression" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
grep -Fq "C lambda expression could not lower body expression" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
! grep -Fq "return pergyra_strdup(\"0\")" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "C statement lowering could not lower expression statement" "$ROOT_DIR/src/codegen/transpiler_statement_dispatch.c"
! grep -A18 -F "llvm_stmt_await_unknown_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_await.c" | \
    grep -Fq "return ctx->type_i32"
! grep -Fq "poison i32 until Future<T>" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_stmt_lookup_visible_function" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_stmt_host_method_return_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
! grep -Fq "llvm_stmt_find_with_slot_inner_in_body" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_find_host_method_metadata_in_context" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_mir_decl_method_return_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_current_field_class_name(ctx, receiver_name)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
grep -Fq "llvm_current_zone_slot_type_name(ctx" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c"
if grep -Fq "llvm_hosted_domain_slot_view_from_decl(ctx" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"; then
    echo "[perf-contract] LLVM stmt type inference reopened domain slot-view metadata directly" >&2
    exit 1
fi
grep -Fq "llvm_current_zone_slot_type_name(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_lookup.c"
grep -Fq "tests/cases/backend_compare/zone_host_method_abi_combo" "$ROOT_DIR/tests/compare_backends.sh"
grep -Fq "channel receive '%s' has no registered Channel<T> metadata" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A34 -F "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "ctx->expected_type_name"
grep -A34 -F "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "pgy_classify_type(ctx->expected_type_name) != PGY_TK_CHANNEL"
grep -Fq '{ "Input", "String", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq '{ "Concat", "String", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
grep -Fq '{ "StringConcat", "String", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/common/pgy_builtin_type_table.c"
! grep -Fq 'strcmp(callee, "Input")' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strcmp(callee, "Concat")' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strcmp(callee, "StringConcat")' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A24 -F "case AST_NUMBER" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "ast_number_is_long(expr)"
grep -A24 -F "case AST_NUMBER" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "return ctx->type_f64"
grep -A6 -F "if (init->type == AST_NUMBER)" "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | \
    grep -Fq "infer_expression_type_name(ctx, init)"
grep -A6 -F "if (init->type == AST_NUMBER)" "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | \
    grep -Fq "transpiler_require_type_name_c_type_copy(ctx, inferred_type"
grep -Fq "transpiler_promote_numeric_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -A28 -F "if (op == TOKEN_PLUS)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c" | \
    grep -Fq "transpiler_promote_numeric_type_name(left_type, right_type)"
grep -Fq "llvm_stmt_promote_numeric_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A22 -F "if (op == TOKEN_PLUS)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty)"
grep -A12 -F "case AST_ARRAY_ACCESS" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "llvm_stmt_resolve_array_elem_type(ctx, array, NULL)"
if grep -A12 -F "case AST_ARRAY_ACCESS" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "llvm_lookup_array_var"; then
    echo "[perf-contract] LLVM array access type inference bypassed array elem owner" >&2
    exit 1
fi
grep -Fq "llvm_stmt_lookup_declared_call_return_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -A14 -F "llvm_stmt_lookup_visible_function(ctx, callee)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c" | \
    grep -Fq "llvm_stmt_lookup_declared_call_return_type(ctx, callee)"
grep -A12 -F "Domain helper result types are owned by typed inference" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_call.c" | \
    grep -Fq "ctx->expected_type_name"
grep -A48 -F "case AST_BINARY" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "unsupported binary operator has no inferred LLVM type"
grep -Fq "llvm_stmt_expected_array_elem_type" "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -A10 -F "llvm_stmt_resolve_array_elem_type" "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c" | \
    grep -Fq "llvm_stmt_expected_array_elem_type(ctx)"
grep -Fq "llvm_stmt_array_elem_type_from_scope_entry" \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq 'strncmp(struct_name, "PgyArray_", 9)' \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -Fq 'strncmp(struct_name, "PgySlice_", 9)' \
    "$ROOT_DIR/src/codegen/llvm_stmt_array_type_infer.c"
grep -A8 -F "llvm_register_future_var_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL || inner_type == NULL"
grep -A14 -F "llvm_register_future_var(ctx, name, future_inner" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c" | \
    grep -Fq "free(future_inner)"
grep -A10 -F "llvm_register_view_var(ctx, name, source_name, inner" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c" | \
    grep -Fq "free(inner)"
grep -A10 -F "llvm_register_slot_var(ctx, name, inner, is_secure)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c" | \
    grep -Fq "free(inner)"
grep -A10 -F "llvm_register_slot_var(ctx, name, inner, is_secure)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_slots.c" | \
    grep -Fq "free(inner)"
grep -A10 -F "llvm_register_device_slot_var(ctx, name, inner)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_slots.c" | \
    grep -Fq "free(inner)"
! grep -Fq "generic_args->params[0]->name" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_slots.c"
grep -Fq "llvm_keep_rendered_persistent" \
    "$ROOT_DIR/src/codegen/llvm_backend_type_render.c"
grep -Fq "llvm_keep_rendered_persistent(ctx" \
    "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
! grep -Fq "generic_args->params[0]->name" \
    "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
! grep -Fq "llvm_keep_rendered_persistent(ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq "generic_args->params[0]->name" \
    "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_keep_rendered_persistent(ctx" \
    "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
! grep -Fq "generic_args->params[0]->name" \
    "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
if command -v rg >/dev/null 2>&1 && rg --version >/dev/null 2>&1; then
    if rg -Fq "generic_args->params[0]->name" \
        "$ROOT_DIR/src/codegen" "$ROOT_DIR/src/compiler" \
        "$ROOT_DIR/src/semantic" "$ROOT_DIR/src/parser"; then
        echo "[perf-contract] generic type lowering regressed to direct params[0]->name field reads" >&2
        exit 1
    fi
fi
grep -Fq "render_type_name_in_ctx(ctx, constraint)" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "constraint->data.type.name" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "transpiler_generic_param_effective_arg_name" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.h"
grep -Fq "transpiler_generic_param_effective_arg_name(GenericParam *formal" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.c"
grep -Fq "render_type_name_in_ctx(ctx, arg_constraint)" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.c"
! grep -Fq "transpiler_generic_class_effective_arg_name" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
! grep -Fq "garg->constraint->data.type.name" \
    "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -A10 -F "llvm_register_channel_var(ctx, name, channel_inner)" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c" | \
    grep -Fq "free(channel_inner)"
grep -Fq "owned_inner_name = llvm_stmt_render_type_arg" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -A10 -F "llvm_register_array_var(ctx, name, elem_type" \
    "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c" | \
    grep -Fq "free(owned_inner_name)"
grep -A10 -F "llvm_register_array_var_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_arrays.c" | \
    grep -Fq "ctx == NULL"
grep -A40 -F "llvm_register_array_var_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_arrays.c" | \
    grep -Fq "ctx->array_vars[ctx->array_var_count].binding = binding"
grep -A14 -F "llvm_lookup_array_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_arrays.c" | \
    grep -Fq "ctx->array_vars[i].binding == entry.alloca"
! grep -Fq "void llvm_register_array_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -A8 -F "llvm_lookup_future_inner(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL"
grep -A8 -F "llvm_register_channel_var_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL || inner_type == NULL"
grep -A8 -F "llvm_lookup_channel_inner(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL"
grep -A14 -F "llvm_register_device_slot_var_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "owned_var_name = llvm_registry_keep_string(ctx, var_name)"
grep -A14 -F "llvm_register_device_slot_var_binding(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "owned_inner_type = llvm_registry_keep_string(ctx, inner_type)"
grep -A16 -F "llvm_lookup_secure_token_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "(size_t)written >= sizeof(token_name)"
grep -Fq "LLVM thread-pool entry requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "LLVM event initialization requires generated event function" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "ast_contains_identifier_ref" "$ROOT_DIR/src/parser/ast_analysis.h"
grep -Fq "ast_contains_free_identifier_ref" "$ROOT_DIR/src/parser/ast_analysis.h"
grep -Fq "ast_contains_identifier_ref" "$ROOT_DIR/src/parser/ast_identifier_ref_analysis.c"
grep -Fq "ast_contains_free_identifier_ref" "$ROOT_DIR/src/parser/ast_identifier_ref_analysis.c"
grep -Fq "ast_block_contains_free_identifier_ref" "$ROOT_DIR/src/parser/ast_identifier_ref_analysis.c"
grep -Fq "ast_match_case_binds_name" "$ROOT_DIR/src/parser/ast_identifier_ref_analysis.c"
grep -Fq "ast_array_patterns_bind_name" "$ROOT_DIR/src/parser/ast_identifier_ref_analysis.c"
grep -Fq "ast_lambda_params_bind_name" "$ROOT_DIR/src/parser/ast_identifier_ref_analysis.c"
grep -Fq "llvm_capture_entry_is_required" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "ast_contains_free_identifier_ref(body, frame->entries[index].name)" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_scope_frame_entry_is_current(ctx, frame, index)" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "entry == &frame->entries[index]" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "ast_contains_free_identifier_ref(node, name)" "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
grep -Fq "ctx->slot_vars[i].name" "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
grep -Fq "ctx->typed_vars[i].name" "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"
if grep -Fq "capture_count += (size_t)frame->count" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"; then
    echo "[perf-contract] LLVM parallel/async capture regressed to whole-scope capture" >&2
    exit 1
fi
if grep -Fq "switch (node->type)" "$ROOT_DIR/src/codegen/transpiler_parallel_capture.c"; then
    echo "[perf-contract] C parallel/async capture regressed to local AST switch walking" >&2
    exit 1
fi
grep -Fq "LLVM MIR select readiness requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "if (ctx->has_error || value_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "if (ctx->has_error || elem_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "if (ctx->has_error || elem_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "if (ctx->has_error || val_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "mir_abi_resource_runtime_row_by_kind(" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "row->call_shape" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq '"PinReadInit"' "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq '"PinWriteInit"' "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq '"Unpin"' "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
! grep -Fq "mir_abi_resource_runtime_fn_by_kind(" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "LLVM MIR secure pin requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "LLVM MIR secure pin requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "mir_block_has_pin_guard_amortization_region(block)" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "llvm_mir_emit_plain_pin_inline_enter" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "llvm_mir_emit_plain_pin_inline_exit" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "LLVM MIR pin cleanup requires MIR ABI runtime function row" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "LLVM MIR pin cleanup requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
! grep -Fq "pgy_secure_pin_%s_init_%s" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
! grep -Fq "pgy_secure_unpin_%s" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "\"secure slot\", method_name, runtime_fn" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "\"slot\", method_name, runtime_fn" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "llvm_direct_secure_slot_write(ctx, slot_var, val)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "return llvm_direct_secure_slot_read(ctx, slot_var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "llvm_direct_secure_slot_release(ctx, slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "define pgy_link" "$ROOT_DIR/Makefile"
grep -Fq '$(notdir $@).rsp' "$ROOT_DIR/Makefile"
grep -Fq '@$(BUILD_DIR)/$(notdir $@).rsp' "$ROOT_DIR/Makefile"
grep -Fq 'filter 3.%,$(MAKE_VERSION)' "$ROOT_DIR/Makefile"
grep -Fq "MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/compiler/mir_cfg_contract_validate.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/test_mir.c"
grep -Fq "transpiler_mir_block_has_source_order_metadata" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.h"
grep -Fq "transpiler_mir_inst_should_precede" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.c"
! grep -Fq "static bool" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.h"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.c"
! grep -Fq "block->source_statement_count" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "semantic_loaded_modules_append" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "capacity;" "$ROOT_DIR/src/semantic/type_checker_flow_resources.c"
grep -Fq "next_capacity = snap.capacity == 0 ? 8 : snap.capacity * 2" \
    "$ROOT_DIR/src/semantic/type_checker_flow_resources.c"
grep -Fq "var_capacity" "$ROOT_DIR/src/semantic/type_system.h"
grep -Fq "type_capacity" "$ROOT_DIR/src/semantic/type_system.h"
grep -Fq "node_capacity" "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "edge_capacity" "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "intent_capacity" "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "owned_name_capacity" "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "participant_capacity" "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "who_capacity" "$ROOT_DIR/src/compiler/dir.h"
grep -Fq "item_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "decl_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "routine_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "callee_routine_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "signature_type_ref_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "direct_call_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "predecessor_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "dominance_frontier_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "phi_candidate_capacity" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "scope_capacity" "$ROOT_DIR/src/compiler/rir.h"
grep -Fq "fact_capacity" "$ROOT_DIR/src/compiler/rir.h"
grep -Fq "op_capacity" "$ROOT_DIR/src/compiler/rir.h"
grep -Fq "state_summary_capacity" "$ROOT_DIR/src/compiler/rir.h"
grep -Fq "instruction_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "block_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "routine_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "predecessor_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "decl_header_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "value_summary_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "use_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "renamed_local_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "ssa_entry_value_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "ssa_exit_value_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "live_in_name_capacity" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "live_out_name_capacity" "$ROOT_DIR/src/compiler/mir.h"
if grep -Fq "hir_find_routine_index_by_name" "$ROOT_DIR/src/compiler/hir.c"; then
    echo "[perf-contract] HIR call graph regressed to routine-name linear lookup" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(ASTNode *)" "$ROOT_DIR/src/compiler/hir.c"; then
    echo "[perf-contract] HIR top-level AST append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(HIRTopLevelItem)" "$ROOT_DIR/src/compiler/hir.c"; then
    echo "[perf-contract] HIR item append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(size_t)" "$ROOT_DIR/src/compiler/hir.c"; then
    echo "[perf-contract] HIR callee append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(HIRDecl)" "$ROOT_DIR/src/compiler/hir_routines.c"; then
    echo "[perf-contract] HIR decl append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(hir->routine_count + 1) * sizeof(HIRRoutine)" \
    "$ROOT_DIR/src/compiler/hir_routines.c"; then
    echo "[perf-contract] HIR routine append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(const char *)" "$ROOT_DIR/src/compiler/hir_analysis.c"; then
    echo "[perf-contract] HIR signature/call collection regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(size_t)" "$ROOT_DIR/src/compiler/hir_cfg.c"; then
    echo "[perf-contract] HIR CFG index fact append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(const char *)" "$ROOT_DIR/src/compiler/hir_cfg.c"; then
    echo "[perf-contract] HIR CFG name fact append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(ASTNode *)" "$ROOT_DIR/src/compiler/hir_lower_intent_cfg.c"; then
    echo "[perf-contract] HIR intent CFG statement append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(HIRBasicBlock)" "$ROOT_DIR/src/compiler/hir_lower_intent_cfg.c"; then
    echo "[perf-contract] HIR intent CFG block append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(ASTNode *)" "$ROOT_DIR/src/compiler/hir_lower_cfg_blocks.c"; then
    echo "[perf-contract] HIR CFG statement append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(HIRBasicBlock)" "$ROOT_DIR/src/compiler/hir_lower_cfg_blocks.c"; then
    echo "[perf-contract] HIR CFG block append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "rir->scope_count + 1" "$ROOT_DIR/src/compiler/rir_facts.c"; then
    echo "[perf-contract] RIR scope append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "scope->fact_count + 1" "$ROOT_DIR/src/compiler/rir_facts.c"; then
    echo "[perf-contract] RIR fact append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "scope->op_count + 1" "$ROOT_DIR/src/compiler/rir_facts.c"; then
    echo "[perf-contract] RIR op append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "scope->state_summary_count + 1" "$ROOT_DIR/src/compiler/rir_facts.c"; then
    echo "[perf-contract] RIR state summary append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "block->instruction_count + 1" "$ROOT_DIR/src/compiler/mir_base_helpers.h"; then
    echo "[perf-contract] MIR base instruction append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "routine->block_count + 1" "$ROOT_DIR/src/compiler/mir_base_helpers.h"; then
    echo "[perf-contract] MIR base block append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "mir->routine_count + 1" "$ROOT_DIR/src/compiler/mir_base_helpers.h"; then
    echo "[perf-contract] MIR base routine append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(const char *)" "$ROOT_DIR/src/compiler/mir_base_helpers.h"; then
    echo "[perf-contract] MIR base name-list append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "append_name_unique(names, count, name)" "$ROOT_DIR/src/compiler/mir_liveness_dce.h"; then
    echo "[perf-contract] MIR liveness name-set append lost capacity tracking" >&2
    exit 1
fi
if grep -Fq "block->instruction_count + 1" "$ROOT_DIR/src/compiler/mir_cleanup.c"; then
    echo "[perf-contract] MIR cleanup instruction append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "routine->block_count + 1" "$ROOT_DIR/src/compiler/mir_cleanup.c"; then
    echo "[perf-contract] MIR cleanup block append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(size_t)" "$ROOT_DIR/src/compiler/mir_cleanup.c"; then
    echo "[perf-contract] MIR cleanup predecessor append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "block->instruction_count + 1" "$ROOT_DIR/src/compiler/mir_intent.c"; then
    echo "[perf-contract] MIR intent instruction append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "mir->decl_header_count + 1" "$ROOT_DIR/src/compiler/mir_decl_headers.h"; then
    echo "[perf-contract] MIR declaration header append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "routine->value_summary_count + 1" "$ROOT_DIR/src/compiler/mir_liveness_dce.h"; then
    echo "[perf-contract] MIR value summary append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(DIRNode)" "$ROOT_DIR/src/compiler/dir.c"; then
    echo "[perf-contract] DIR node append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(DIREdge)" "$ROOT_DIR/src/compiler/dir.c"; then
    echo "[perf-contract] DIR edge append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "dir->owned_name_count + 1" "$ROOT_DIR/src/compiler/dir.c"; then
    echo "[perf-contract] DIR owned-name append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(DIRIntent" "$ROOT_DIR/src/compiler/dir_collect.c"; then
    echo "[perf-contract] DIR intent append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(*count + 1) * sizeof(const char *)" "$ROOT_DIR/src/compiler/dir_collect.c"; then
    echo "[perf-contract] DIR intent name append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "program->data.program.count + 1" "$ROOT_DIR/src/semantic/semantic.c"; then
    echo "[perf-contract] semantic stdlib preload regressed to program count+1 append" >&2
    exit 1
fi
if grep -Fq "loaded_count + 1" "$ROOT_DIR/src/semantic/semantic.c"; then
    echo "[perf-contract] semantic stdlib preload regressed to loaded-module count+1 append" >&2
    exit 1
fi
if grep -Fq "snap.count + 1" "$ROOT_DIR/src/semantic/type_checker_flow_resources.c"; then
    echo "[perf-contract] semantic resource snapshot append regressed to count+1 allocation" >&2
    exit 1
fi
if grep -Fq "n + 1" "$ROOT_DIR/src/semantic/type_env.c"; then
    echo "[perf-contract] semantic type environment append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "(old_count + 1) * sizeof(ASTNode *)" "$ROOT_DIR/src/parser/parser_expr.c"; then
    echo "[perf-contract] parser call-argument prepend regressed to count+1 realloc" >&2
    exit 1
fi
grep -Fq "name_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "param_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "field_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "method_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "statement_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "case_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "pattern_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "method_capacity" "$ROOT_DIR/src/parser/ast.h"
grep -Fq "param_capacity" "$ROOT_DIR/src/parser/ast.h"
if grep -Fq "let_destructure.name_count + 1" "$ROOT_DIR/src/parser/parser.c"; then
    echo "[perf-contract] parser destructuring names regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "next_count = *count + 1" "$ROOT_DIR/src/parser/parser_async.c"; then
    echo "[perf-contract] parser async node append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "async_func_decl.param_count + 1" "$ROOT_DIR/src/parser/parser_async.c"; then
    echo "[perf-contract] parser async params regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "func_decl.param_count + 1" "$ROOT_DIR/src/parser/parser_decl.c"; then
    echo "[perf-contract] parser function params regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "class_decl.field_count + 1" "$ROOT_DIR/src/parser/parser_decl.c"; then
    echo "[perf-contract] parser nominal fields regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "class_decl.method_count + 1" "$ROOT_DIR/src/parser/parser_decl.c"; then
    echo "[perf-contract] parser nominal methods regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "func_decl.required_ability_count + 1" \
    "$ROOT_DIR/src/parser/parser_decl_function_clause.c"; then
    echo "[perf-contract] parser function requires clause regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "func_decl.authorized_by_count + 1" \
    "$ROOT_DIR/src/parser/parser_decl_function_clause.c"; then
    echo "[perf-contract] parser function authorized-by clause regressed to count+1 realloc" >&2
    exit 1
fi
grep -Fq "who_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "involve_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "value_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "binding_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "step_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "default_who_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "on_expr_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "compensate_expr_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "required_ability_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "authorized_by_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "slot_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "refresh_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "authority_capacity" "$ROOT_DIR/src/parser/ast_domain_data.h"
grep -Fq "size_t         capacity;" "$ROOT_DIR/src/parser/ast_types.h"
grep -Fq "tag_capacity" "$ROOT_DIR/src/parser/ast_types.h"
grep -Fq "bound_capacity" "$ROOT_DIR/src/parser/ast_types.h"
grep -Fq "param_capacity" "$ROOT_DIR/src/parser/ast.h"
if grep -Fq "(*count + 1) * sizeof(char *)" \
    "$ROOT_DIR/src/semantic/type_checker_intent_action_contract.c"; then
    echo "[perf-contract] intent action contract name append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "required_ability_count + 1" \
    "$ROOT_DIR/src/semantic/type_checker_intent_action_contract.c"; then
    echo "[perf-contract] intent action contract ability append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "next_count = *count + 1" "$ROOT_DIR/src/parser/parser_intent.c"; then
    echo "[perf-contract] parser intent declaration append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "required_ability_count + 1" "$ROOT_DIR/src/parser/parser_intent_step.c"; then
    echo "[perf-contract] parser intent step requires append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "next_count = *count + 1" "$ROOT_DIR/src/parser/parser_domain_world.c"; then
    echo "[perf-contract] parser world compose input append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "next_count = *count + 1" "$ROOT_DIR/src/parser/parser_domain_zone.c"; then
    echo "[perf-contract] parser zone group-name append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "next_count = *count + 1" "$ROOT_DIR/src/parser/parser_domain_projection.c"; then
    echo "[perf-contract] parser projection append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "pattern_count + 1" "$ROOT_DIR/src/parser/parser_stmt.c"; then
    echo "[perf-contract] parser match pattern append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "case_count + 1" "$ROOT_DIR/src/parser/parser_stmt.c"; then
    echo "[perf-contract] parser match case append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "method_count + 1" "$ROOT_DIR/src/parser/parser_enum.c"; then
    echo "[perf-contract] parser enum method append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "parser_append_expr_node(" "$ROOT_DIR/src/parser/parser_expr.c"; then
    echo "[perf-contract] parser expression append bypassed capacity helper" >&2
    exit 1
fi
if grep -Fq "next_count = *slot_count + 1" "$ROOT_DIR/src/parser/parser_domain.c"; then
    echo "[perf-contract] parser domain slot append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "next_count = *count + 1" "$ROOT_DIR/src/parser/parser_domain.c"; then
    echo "[perf-contract] parser domain child append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "tag_count + 1" "$ROOT_DIR/src/parser/parser_doc.c"; then
    echo "[perf-contract] parser structured-comment tags regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "params->count + 1" "$ROOT_DIR/src/parser/parser_type.c"; then
    echo "[perf-contract] parser generic parameter append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "constraint->bound_count + 1" "$ROOT_DIR/src/parser/parser_type.c"; then
    echo "[perf-contract] parser type-bound append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "where->count + 1" "$ROOT_DIR/src/parser/parser_type.c"; then
    echo "[perf-contract] parser where-clause append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "parser_append_type_node(" "$ROOT_DIR/src/parser/parser_type.c"; then
    echo "[perf-contract] parser type node append bypassed capacity helper" >&2
    exit 1
fi
if grep -Fq "ctx->type_resolution_metadata.keys[i] == (void *)type_node" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata.c"; then
    echo "[perf-contract] DAG metadata lookup regressed to linear scan in metadata owner" >&2
    exit 1
fi
if grep -Fq "sizeof(AIREvidenceNode) * (air->evidence_count + 1)" \
    "$ROOT_DIR/src/compiler/air.c"; then
    echo "[perf-contract] AIR evidence append regressed to count+1 realloc" >&2
    exit 1
fi
if grep -Fq "sizeof(AIRDrift) * (air->drift_count + 1)" \
    "$ROOT_DIR/src/compiler/air_verify.c"; then
    echo "[perf-contract] AIR drift append regressed to count+1 realloc" >&2
    exit 1
fi
grep -Fq "AIR boundary evidence duplicate has invalid counts" \
    "$ROOT_DIR/src/compiler/air_evidence_node.c"
if grep -Fq "Two-pass: count then fill" "$ROOT_DIR/src/semantic/slot_analyzer.c"; then
    echo "[perf-contract] slot analyzer live-slot collection regressed to two-pass" >&2
    exit 1
fi
if grep -Fq 'participant = pgy_runtime_strdup("")' \
    "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_events_inline.h"; then
    echo "[perf-contract] inline intent step begin reintroduced empty participant allocation" >&2
    exit 1
fi
if grep -Fq 'participant = pgy_runtime_strdup_export("")' \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.h"; then
    echo "[perf-contract] exported intent step begin reintroduced empty participant allocation" >&2
    exit 1
fi
if grep -Fq "pgy_mir_resource_op_export" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"; then
    echo "[perf-contract] MIR trace exports regressed into the intent trace owner" >&2
    exit 1
fi
grep -Fq "char ret_type_storage[128]" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_include_emit.c" || {
    echo "[perf-contract] included-role wrapper stopped freezing return C type" >&2
    exit 1
}
grep -Fq "char ret_type_storage[128]" \
    "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c" || {
    echo "[perf-contract] role operator alias stopped freezing return C type" >&2
    exit 1
}
grep -Fq "transpiler_infer_slot_inner_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c" || {
    echo "[perf-contract] expression type inference stopped owning slot inner strings" >&2
    exit 1
}
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" \
    "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c" || {
    echo "[perf-contract] expression type inference bypassed slot inner copy seam" >&2
    exit 1
}
slot_inner_escape_matches="$(
    grep -F "slot_inner_type_name(" "$CODEGEN_INDEX" \
    | grep -v "transpiler_type_mapping" \
    | grep -v "transpiler.h" \
    | grep -v "transpiler_infer_slot_inner_type_name" || true
)"
if [ -n "$slot_inner_escape_matches" ]; then
    echo "[perf-contract] C backend direct slot_inner_type_name call escaped copy seam" >&2
    exit 1
fi
if grep -Fq "slot inner not yet registered" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"; then
    echo "[perf-contract] LLVM slot type inference must not invent i32 for missing slot metadata" >&2
    exit 1
fi
if grep -A16 -F "llvm_stmt_lookup_slot_or_view_inner(" \
    "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" \
    | grep -Fq "return ctx->type_i32"; then
    echo "[perf-contract] LLVM slot Read must fail closed when slot/view metadata is absent" >&2
    exit 1
fi
grep -Fq "llvm_type_subst_restore_owned" \
    "$ROOT_DIR/src/codegen/llvm_backend_generic.c" || {
    echo "[perf-contract] LLVM generic type-substitution restore owner disappeared" >&2
    exit 1
}
for rel in \
    "src/codegen/llvm_backend_type_map.c" \
    "src/codegen/llvm_member_call_specialize.c"; do
    if grep -Fq "ctx->type_subst_count = saved_subst" "$ROOT_DIR/$rel"; then
        echo "[perf-contract] $rel bypassed owned type-substitution restore" >&2
        exit 1
    fi
    grep -Fq "llvm_type_subst_restore_owned(ctx, saved_subst)" \
        "$ROOT_DIR/$rel" || {
        echo "[perf-contract] $rel must restore owned type substitutions through the generic owner" >&2
        exit 1
    }
done

echo "[perf-contract] perf summary contract is smoke-gated"
