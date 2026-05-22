#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMPDIR="${TMPDIR:-/tmp}"
WORK_DIR="$(mktemp -d "$TMPDIR/pgy_perf_contract.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

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

grep -Fq "test-abi-perf" "$ROOT_DIR/docs/100_beta_readiness_checklist.md"
grep -Fq "perf-summary" "$ROOT_DIR/docs/100_beta_readiness_checklist.md"
grep -Fq "perf-c-baseline-test-smoke" "$ROOT_DIR/docs/100_beta_readiness_checklist.md"
grep -Fq "pgy_over_c_ratio" "$ROOT_DIR/docs/100_beta_readiness_checklist.md"
grep -Fq "perf-c-baseline-test-smoke" "$ROOT_DIR/TODO.md"
grep -Fq "pgy_over_c_ratio" "$ROOT_DIR/TODO.md"
grep -Fq "perf-c-baseline-test-smoke" "$ROOT_DIR/Makefile"
grep -Fq "tests/perf_c_baseline_smoke.sh" "$ROOT_DIR/Makefile"
grep -Fq "Invoke-CheckedNative" "$ROOT_DIR/tests/perf_c_baseline_smoke.ps1"
grep -Fq "ArgList" "$ROOT_DIR/tests/perf_c_baseline_smoke.ps1"
grep -Fq "c_baseline_arith_loop.pgy" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "c_baseline_arith_loop.c" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "constant nonzero modulo regressed to checked helper" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "constant nonzero division regressed to checked helper" "$ROOT_DIR/tests/perf_c_baseline_smoke.sh"
grep -Fq "codegen_scalar_arithmetic_policy.c" "$ROOT_DIR/Makefile"
grep -Fq "pgy_codegen_ast_number_is_nonzero_i32_literal" "$ROOT_DIR/src/codegen/codegen_scalar_arithmetic_policy.c"
grep -Fq "pgy_codegen_ast_number_is_nonzero_i32_literal" "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c"
grep -Fq "pgy_codegen_ast_number_is_nonzero_i32_literal" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
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
grep -Fq "return g_schedulerByTag[tag]" "$ROOT_DIR/src/runtime/party_runtime_scheduler.c"
grep -Fq "party_runtime_stats.c" "$ROOT_DIR/Makefile"
grep -Fq "indexHashes" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "indexHealthy" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "fiber_stats_lookup(roleId)" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "fiber_stats_index_insert(stats->roleId" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
grep -Fq "return fiber_stats_lookup_linear(roleId);" "$ROOT_DIR/src/runtime/party_runtime_stats.c"
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
grep -Fq "const char  *last_lookup_name;" "$ROOT_DIR/src/codegen/llvm_internal.h"
grep -Fq "LLVMVarEntry *last_lookup;" "$ROOT_DIR/src/codegen/llvm_internal.h"
grep -Fq "frame->last_lookup_name" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "frame->last_lookup = &frame->entries" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "llvm_intent_current_handle_or_zero" "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
! grep -Fq "llvm_scope_lookup(ctx, \"__intent_handle\")->alloca" \
    "$ROOT_DIR/src/codegen/llvm_intent_zone_bind.c"
grep -Fq "ScaffoldKindSpec" "$ROOT_DIR/src/compiler/driver_scaffold.c"
grep -Fq "scaffold_find_kind" "$ROOT_DIR/src/compiler/driver_scaffold.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/compiler/driver_scaffold.c"
grep -Fq "TranspilerTypeNameMap" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_lookup_type_name_map" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_forward_type_name_is_allowed" "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
grep -Fq "transpiler_forward_allowed_type_compare" "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
grep -Fq "bsearch(&name" "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"
for direct_forward_type in \
    Int Float Bool String Char Byte Void Qubit Result Option Slot SecureSlot \
    DeviceSlot RemoteFuture Array Slice Channel Box Rc Weak Future; do
    if grep -Fq "strcmp(name, \"$direct_forward_type\")" \
        "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c"; then
        echo "[perf-contract] function forward policy reintroduced direct type branch: $direct_forward_type" >&2
        exit 1
    fi
done
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
grep -Fq "air_has_mir_pin_cleanup_evidence" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "type_resolution_dag_generic_contract_evidence_count" "$ROOT_DIR/src/semantic/semantic.h"
grep -Fq "type_resolution_dag_ability_consumer_evidence_count" "$ROOT_DIR/src/semantic/semantic.h"
grep -Fq "type_resolution_dag_ability_consumer_evidence_count" "$ROOT_DIR/src/semantic/type_checker.h"
grep -Fq "ctx->type_resolution_dag_generic_contract_evidence_count" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "ctx->type_resolution_dag_ability_consumer_evidence_count" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "semantic_record_dag_generic_contract_evidence" "$ROOT_DIR/src/semantic/type_checker_resolution_stage_signature.c"
grep -Fq "[type-res-stats] dag-evidence:" "$ROOT_DIR/src/semantic/type_checker_program_stats.c"
grep -Fq "dag_generic_contract_evidence" "$ROOT_DIR/tests/type_resolution_dag_smoke.sh"
grep -Fq "dag_ability_consumer_evidence" "$ROOT_DIR/tests/type_resolution_dag_smoke.sh"
grep -Fq "semantic_result_dag_generic_contract_evidence_count(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_dag_ability_consumer_evidence_count(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_type_resolution_metadata_entries(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
grep -Fq "semantic_result_type_resolution_metadata_dead_ends(sem)" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
! grep -Fq "sem->type_resolution_" "$ROOT_DIR/src/compiler/air_evidence_dag.c"
if grep -A1 -F "result->type_resolution_dag_generic_contract_evidence_count =" \
    "$ROOT_DIR/src/semantic/semantic.c" | \
    grep -Fq "ctx->type_resolution_stage_compat_generic_contract_count"; then
    echo "[perf-contract] DAG generic evidence result regressed to compat counter" >&2
    exit 1
fi
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
grep -Fq "HIRRoutineNameIndex" "$ROOT_DIR/src/compiler/hir_callgraph.c"
grep -Fq "hir_build_routine_name_index" "$ROOT_DIR/src/compiler/hir_callgraph.c"
grep -Fq "hir_lookup_routine_index_by_name" "$ROOT_DIR/src/compiler/hir_callgraph.c"
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
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_hir.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_evidence_runtime.c"
grep -Fq "air_evidence_node_count(air)" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "air_evidence_node_at(air, i)" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
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
grep -Fq "air_evidence_kind_is_known(evidence->kind)" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
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
    "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
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
grep -Fq "bsearch(effective_name" "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c"
grep -Fq "codegen_match_variant_policy.c" "$ROOT_DIR/Makefile"
grep -Fq "pgy_codegen_match_variant_lookup" "$ROOT_DIR/src/codegen/codegen_match_variant_policy.c"
grep -Fq "pgy_codegen_match_variant_compare" "$ROOT_DIR/src/codegen/codegen_match_variant_policy.c"
grep -Fq "bsearch(&name" "$ROOT_DIR/src/codegen/codegen_match_variant_policy.c"
for match_variant_owner in \
    "$ROOT_DIR/src/codegen/transpiler_match_emit.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c" \
    "$ROOT_DIR/src/codegen/llvm_stmt_match.c" \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"; do
    grep -Fq "pgy_codegen_match_variant_lookup" "$match_variant_owner"
    for direct_match_variant in Some None Ok Err; do
        if grep -Fq "strcmp(name, \"$direct_match_variant\")" \
            "$match_variant_owner"; then
            echo "[perf-contract] match lowering reintroduced direct destructor-name branch: $direct_match_variant in $match_variant_owner" >&2
            exit 1
        fi
        if grep -Fq "strcmp(kind, \"$direct_match_variant\")" \
            "$match_variant_owner"; then
            echo "[perf-contract] match lowering reintroduced direct destructor-kind branch: $direct_match_variant in $match_variant_owner" >&2
            exit 1
        fi
        if grep -Fq "strcmp(option_kind, \"$direct_match_variant\")" \
            "$match_variant_owner"; then
            echo "[perf-contract] match lowering reintroduced direct option-kind branch: $direct_match_variant in $match_variant_owner" >&2
            exit 1
        fi
        if grep -Fq "strcmp(result_kind, \"$direct_match_variant\")" \
            "$match_variant_owner"; then
            echo "[perf-contract] match lowering reintroduced direct result-kind branch: $direct_match_variant in $match_variant_owner" >&2
            exit 1
        fi
    done
done
match_variant_names="$(
    sed -n '/static const PgyCodegenMatchVariantSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/codegen_match_variant_policy.c" \
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
for direct_llvm_registry_type in \
    Array Slice List Set Queue HashMap Future RemoteFuture Channel Rc Weak; do
    if grep -Fq "strcmp(type_name, \"$direct_llvm_registry_type\")" \
        "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"; then
        echo "[perf-contract] LLVM type registry reintroduced direct type-family branch: $direct_llvm_registry_type" >&2
        exit 1
    fi
done
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
grep -Fq "slot_runtime_fn_spec_compare" "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c"
grep -Fq "transpiler_mir_resource_op_lookup" "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c"
grep -Fq "bsearch(op_name" "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c"
for direct_mir_resource_op in Claim Read Write Release Move; do
    if grep -Fq "strcmp(op_name, \"$direct_mir_resource_op\")" \
        "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"; then
        echo "[perf-contract] MIR resource op emission reintroduced direct op branch: $direct_mir_resource_op" >&2
        exit 1
    fi
done
for direct_mir_view_op in BorrowRead BorrowWrite; do
    if grep -Fq "strcmp(inst->name, \"$direct_mir_view_op\")" \
        "$ROOT_DIR/src/codegen/transpiler_mir_pin_emit.c"; then
        echo "[perf-contract] C MIR pin alias seeding reintroduced direct op branch: $direct_mir_view_op" >&2
        exit 1
    fi
    if grep -Fq "strcmp(inst->name, \"$direct_mir_view_op\")" \
        "$ROOT_DIR/src/codegen/llvm_mir_resource_view.c"; then
        echo "[perf-contract] LLVM MIR borrow-view aliasing reintroduced direct op branch: $direct_mir_view_op" >&2
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
grep -B10 -A2 -F "entry = find_free_entry_locked(manager);" \
    "$ROOT_DIR/src/runtime/slot_manager_core_ops.c" | \
    grep -Fq "manager->activeSlots >= manager->tableSize"
grep -A18 -F "RosterAddParty(RosterContext* roster" \
    "$ROOT_DIR/src/runtime/world_roster.c" | \
    grep -Fq "ownedSlotName = world_roster_strdup(slotName)"
grep -A18 -F "WorldAddRoster(WorldContext* world" \
    "$ROOT_DIR/src/runtime/world_roster.c" | \
    grep -Fq "ownedRosterType ="
grep -Fq "world_roster_plan_stats.c" "$ROOT_DIR/Makefile"
if grep -R -Fq '#include "world_roster_plan_stats.h"' "$ROOT_DIR/src/runtime"; then
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
if grep -Fq "emit_unary(ASTNode *expr" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.h"; then
    echo "[perf-contract] unary expression lowering regressed to dispatch header" >&2
    exit 1
fi
grep -Fq "transpiler_expr_literal_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "char *emit_literal_expression(ASTNode *node)" \
    "$ROOT_DIR/src/codegen/transpiler_expr_literal_emit.h"
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
grep -Fq "transpiler_generic_class_specialization_name(ASTNode *class_decl" \
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
if grep -R -Fq "transpiler_collection_infer_expression_type_name" \
    "$ROOT_DIR/src/codegen"; then
    echo "[perf-contract] expression type inference regressed to collection-named API" >&2
    exit 1
fi
grep -Fq "transpiler_domain_provenance_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "void emit_domain_projection_sync_loop(" \
    "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.h"
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
grep -Fq "pgy_spawn_blocking_export" "$ROOT_DIR/src/codegen/llvm_runtime_task_memory_decl.c"
grep -Fq "index_keys" "$ROOT_DIR/src/semantic/type_checker.h"
grep -Fq "metadata_index_capacity_is_valid" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_index.c"
grep -Fq "metadata_lookup_entry_index" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_index.c"
grep -Fq "metadata_index_insert" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_index.c"
grep -Fq "pgy_intent_observability_name_is_builtin" "$ROOT_DIR/src/common/intent_observability_names.c"
grep -Fq "pgy_intent_observability_name_is_builtin" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "mir_instruction_is_intent_semantic_carrier" "$ROOT_DIR/src/compiler/mir_intent_fact.c"
grep -Fq "return mir_instruction_is_intent_semantic_carrier(inst);" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "return mir_instruction_is_intent_semantic_carrier(inst);" "$ROOT_DIR/src/compiler/mir_stmt_population_source.c"
grep -Fq "MIR DCE does not preserve user Intent-prefixed statements" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
! grep -Fq "strncmp(name, \"Intent\", 6)" "$ROOT_DIR/src/parser/ast_analysis.c"
! grep -Fq "strncmp(inst->name, \"Intent\", 6)" "$ROOT_DIR/src/compiler/mir_dce.c"
! grep -Fq "strncmp(inst->name, \"Intent\", 6)" "$ROOT_DIR/src/compiler/mir_stmt_population_source.c"
common_intent_obs_names="$(
    grep -o '"Intent[A-Za-z0-9_]*"' "$ROOT_DIR/src/common/intent_observability_names.c" \
        | tr -d '"' \
        | grep -vx "Intent" \
        | sort -u
)"
common_intent_obs_names_in_order="$(
    grep -o '"Intent[A-Za-z0-9_]*"' "$ROOT_DIR/src/common/intent_observability_names.c" \
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
    sed -n '/SlotRuntimeFnSpec specs\[\]/,/^    };/p' \
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
codegen_intent_obs_names="$(
    grep 'PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY' \
        "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c" \
        | grep -o '"Intent[A-Za-z0-9_]*"' \
        | tr -d '"' \
        | sort -u
)"
codegen_intent_obs_names_in_order="$(
    grep 'PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY' \
        "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c" \
        | grep -o '"Intent[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_builtin_names="$(
    sed -n '/static const PgyBuiltinInfo entries\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c" \
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
semantic_intent_obs_names="$(
    grep 'BUILTIN_INTENT_' "$ROOT_DIR/src/semantic/type_checker_builtins_intent_observability.c" \
        | grep -o '"Intent[A-Za-z0-9_]*"' \
        | tr -d '"' \
        | sort -u
)"
resolver_intent_obs_names="$(
    grep 'BUILTIN_INTENT_' "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c" \
        | grep -o '"Intent[A-Za-z0-9_]*"' \
        | tr -d '"' \
        | sort -u
)"
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
llvm_intent_obs_names="$(
    grep -o '"Intent[A-Za-z0-9_]*"' \
        "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" \
        | tr -d '"' \
        | sort -u
)"
llvm_intent_obs_names_in_order="$(
    grep -o '"Intent[A-Za-z0-9_]*"' \
        "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c" \
        | tr -d '"'
)"
if [[ "$llvm_intent_obs_names_in_order" != "$llvm_intent_obs_names" ]]; then
    echo "LLVM intent observability names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_intent_obs_names") \
        <(printf '%s\n' "$llvm_intent_obs_names_in_order") >&2 || true
    exit 1
fi
for pair in \
    "semantic:$semantic_intent_obs_names" \
    "resolver:$resolver_intent_obs_names" \
    "llvm:$llvm_intent_obs_names"; do
    table_name="${pair%%:*}"
    table_names="${pair#*:}"
    if [[ "$common_intent_obs_names" != "$table_names" ]]; then
        echo "intent observability builtin names drifted between common and ${table_name} tables" >&2
        diff -u <(printf '%s\n' "$common_intent_obs_names") \
            <(printf '%s\n' "$table_names") >&2 || true
        exit 1
    fi
done
grep -Fq "ast_contains_identifier_call" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_decl_methods_contain_identifier_call" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_uses_intent_observability_surface" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "pgy_mir_instruction_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
if [[ "$(grep -Fc "llvm_active_uses_intent_observability(ctx)" "$ROOT_DIR/src/codegen/llvm_api.c")" -lt 2 ]]; then
    echo "LLVM IR and object codegen paths must both preserve intent observability runtime selection" >&2
    exit 1
fi
grep -Fq "transpiler_active_uses_intent_observability(ctx)" "$ROOT_DIR/src/codegen/transpiler_entry.c"
grep -Fq "pgy_mir_program_uses_intent_observability(ctx->mir)" "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
grep -Fq "pgy_mir_program_uses_intent_observability(ctx->mir)" "$ROOT_DIR/src/codegen/transpiler_inventory_view.c"
! grep -Fq "pgy_mir_program_uses_intent_observability(mir)" "$ROOT_DIR/src/codegen/llvm_api.c"
! grep -Fq "pgy_mir_program_uses_intent_observability(mir)" "$ROOT_DIR/src/codegen/transpiler_entry.c"
grep -Fq "return llvm_result_error_fmt_with_hints(" "$ROOT_DIR/src/codegen/llvm_api.c"
grep -Fq "allow_fixture_payload_probe" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "allow_legacy_ast_probe" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "inst->has_surface_usage_facts" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "uses_intent_observability_surface" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "pgy_name_array_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "routine->hir_routine->direct_calls" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "pgy_mir_block_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "block->instruction_count > 0 && block->instructions == NULL" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "routine->hir_routine == NULL" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "routine->ast" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "block->source_terminator_condition" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "block->source_terminator_value" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "pgy_ast_uses_intent_observability(inst->ast" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "require_slot_token_name" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "token name synthesis is disabled" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
! grep -R -Fq "lookup_slot_token_name_or_default" "$ROOT_DIR/src/codegen"
! grep -R -Fq "fallback_token" "$ROOT_DIR/src/codegen"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "transpiler_find_block_binding_from_mir_insts" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "transpiler_pending_binding_from_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "transpiler_pending_binding_from_source_compatibility" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "!mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
! grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "out->initializer = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "out->type_annotation = inst->expr1" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.c"
grep -Fq "transpiler_mir_ssa_local_limit_fail" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
grep -Fq "transpiler_mir_func_ssa_locals_emit.c" "$ROOT_DIR/Makefile"
grep -Fq "PGY_CODE_MIR_SSA_LIMIT" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
grep -Fq "PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
if grep -RIn --include 'transpiler_mir*.h' --include 'transpiler_mir*.c' \
    "ctx->backend_error = strdup_fmt" "$ROOT_DIR/src/codegen"; then
    echo "C MIR emitters must route backend failures through diagnostic helpers" >&2
    exit 1
fi
if grep -RIn "ctx->backend_error = strdup_fmt" "$ROOT_DIR/src/codegen"; then
    echo "C backend failures must route through diagnostic helpers" >&2
    exit 1
fi
if grep -RInE "ctx->backend_error[[:space:]]*=[^=]" "$ROOT_DIR/src/codegen" \
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
grep -Fq "routine->blocks[routine->cleanup_block].is_reachable" "$ROOT_DIR/src/compiler/air_evidence_mir_facts.c"
grep -Fq "if (!block->is_reachable || block->is_cleanup)" "$ROOT_DIR/src/compiler/air_evidence_mir_pin.c"
grep -Fq "AIR ignores unreachable MIR cleanup root evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR ignores unreachable MIR cleanup source evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "air_global_evidence_has_provider" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "air_boundary_has_evidence_kind_provider" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"
grep -Fq "kBoundaryEvidencePolicies" "$ROOT_DIR/src/compiler/air_boundary_evidence_policy.c"
grep -Fq "air_boundary_requires_hir_routine_evidence" "$ROOT_DIR/src/compiler/air_boundary_evidence_policy.c"
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
grep -Fq "pergyra_ast_type_to_c_copy(type_ast" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "param_type = ast_let_type(param)" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(param_type" "$ROOT_DIR/src/codegen/transpiler_event_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_type_alias_target_type(node)" "$ROOT_DIR/src/codegen/transpiler_type_alias.c"
grep -Fq "slot_inner_type_name_copy" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "slot_inner_type_name_write" "$ROOT_DIR/src/codegen/transpiler_type_name_utils.c"
grep -Fq "slot_inner_type_name_copy" "$ROOT_DIR/src/codegen/transpiler_type_mapping.h"
! grep -Fq "const char *slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_type_mapping.h"
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
grep -Fq "transpiler_contextual_option_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_option_context.c"
grep -Fq "transpiler_contextual_option_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_helpers_core_a.h"
grep -Fq "transpiler_contextual_option_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_contextual_option_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq "transpiler_contextual_option_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_option_context.c"
grep -Fq "lookup_future_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_future_type_query.h"
! grep -Fq "lookup_future_inner_type(" "$ROOT_DIR/src/codegen/transpiler_future_type_query.h"
grep -Fq "channel_inner_type_name_copy" "$ROOT_DIR/src/codegen/transpiler_channel_type_query.h"
! grep -Fq "channel_inner_type_name(TranspilerCtx" "$ROOT_DIR/src/codegen/transpiler_type_mapping_helpers.h"
grep -Fq "select_channel_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_select.c"
! grep -Fq "select_channel_inner_type(ASTNode" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_func_forward_emit.c"
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
grep -Fq "slot_inner_type_name_copy(type_name, inner_buf" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
! grep -Fq "register_slot_var(ctx, p->name, slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq "register_slot_var(ctx, p->name, slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "kTranspilerReturnOptionCtorSpecs" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "transpiler_return_option_ctor_lookup" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c"
! grep -Fq 'strcmp(callee_name, "Some")' "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
! grep -Fq 'strcmp(callee_name, "None")' "$ROOT_DIR/src/codegen/transpiler_func_class_flow_emit.c"
transpiler_return_option_ctor_names="$(
    sed -n '/kTranspilerReturnOptionCtorSpecs\[\]/,/^        };/p' \
        "$ROOT_DIR/src/codegen/transpiler_func_flow_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_return_option_ctor_names_sorted="$(
    printf '%s\n' "$transpiler_return_option_ctor_names" | sort
)"
if [[ "$transpiler_return_option_ctor_names" != "$transpiler_return_option_ctor_names_sorted" ]]; then
    echo "C backend return Option ctor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_return_option_ctor_names_sorted") \
        <(printf '%s\n' "$transpiler_return_option_ctor_names") >&2 || true
    exit 1
fi
grep -Fq "transpiler_infer_slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_read(method_name)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_write(method_name)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_release(method_name)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_read(name)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_write(name)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "pgy_codegen_call_name_is_release(name)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq 'strcmp(method_name, "Read")' "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq 'strcmp(method_name, "Write")' "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq 'strcmp(method_name, "Release")' "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq 'strcmp(name, "Read")' "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq 'strcmp(name, "Write")' "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
! grep -Fq 'strcmp(name, "Release")' "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "kTranspilerInferCallSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
grep -Fq "transpiler_infer_call_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "transpiler_infer_call_is_numeric_passthrough" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
grep -Fq "transpiler_infer_call_returns_channel_option" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"
for direct_infer_name in \
    Min Max Abs Clone MapGet MapKeys ListGet ViewRead ViewWrite Measure \
    QubitState DeviceRead SubmitDeviceRead TryRecv RecvTimeout \
    TrySendStatus SendTimeoutStatus Some None IsSome IsNone \
    UnwrapOption ToObject ToTObject; do
    if grep -Fq "strcmp(name, \"$direct_infer_name\")" \
        "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"; then
        echo "[perf-contract] C expression type inference reintroduced direct builtin branch: $direct_infer_name" >&2
        exit 1
    fi
done
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
! grep -Fq "return slot_inner_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "constructed_arg_name_write" "$ROOT_DIR/src/codegen/transpiler_type_name_utils.c"
! grep -Fq "const char *constructed_arg_name_at" "$ROOT_DIR/src/codegen/transpiler_type_mapping.h"
! grep -Fq "constructed_arg_name_at(const char" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
! grep -A12 -F "copy_constructed_arg_name_at" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" | grep -Fq "constructed_arg_name_at("
! grep -Fq "const char *pergyra_type_to_c" "$ROOT_DIR/src/codegen/transpiler.h"
! grep -Fq "pergyra_type_to_c(const char" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
! grep -A320 -F "pergyra_type_to_c_copy(const char *name" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c" | grep -Fq "pergyra_type_to_c(name)"
grep -Fq "generic_args_to_c_suffix_copy" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "generic_args_to_c_suffix_write" "$ROOT_DIR/src/codegen/transpiler_type_name_utils.c"
grep -Fq "generic_args_to_c_suffix_copy" "$ROOT_DIR/src/codegen/transpiler_type_result_mapping_helpers.c"
! grep -Fq "const char *generic_args_to_c_suffix" "$ROOT_DIR/src/codegen/transpiler_type_mapping.h"
! grep -Fq "generic_args_to_c_suffix(const char" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "collection_runtime_suffix_copy" "$ROOT_DIR/src/codegen/transpiler_collection_runtime_suffix.h"
! grep -Fq "collection_runtime_suffix(const char" "$ROOT_DIR/src/codegen/transpiler_collection_runtime_suffix.h"
! grep -R "collection_runtime_suffix(" "$ROOT_DIR/src/codegen" \
    --include='*.c' --include='*.h' >/dev/null
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
grep -Fq "pergyra_type_to_c_copy(elem_inner" "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c"
grep -Fq "pergyra_type_to_c_copy(inner_type_buf" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "pergyra_type_to_c_copy(init_type" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
grep -Fq "pergyra_type_to_c_copy(init_type_name" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
grep -Fq "char inner_name_buf[128]" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "slot_inner_type_name_copy(effective_layout->abi_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "slot_inner_type_name_copy(typed_name, inner_name_buf" \
    "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "pergyra_type_to_c_copy(inner_name" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c"
grep -Fq "char inner_buf[128]" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
grep -Fq "char elem_inner_buf[128]" "$ROOT_DIR/src/codegen/transpiler_mir_destructure_emit.c"
grep -Fq "slot_inner_type_name_copy(resolved_type, inner_buf" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "slot_inner_type_name_copy(arr_type, inner_buf" \
    "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "pergyra_type_to_c_copy(inner" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
channel_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"
grep -Fq "pergyra_type_to_c_copy(inner, c_inner_buf" "$channel_builtin_owner"
grep -Fq "PGY_INTENT_ACTIVE_INDEX_MAX" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_active_index_find_slot" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h"
grep -Fq "pgy_intent_active_index_set(handle, i)" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h"
grep -Fq "pgy_intent_find_active_registry_slot(handle)" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_index_inline.h"
grep -Fq "pgy_intent_active_index_find_slot_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c"
grep -Fq "pgy_intent_active_index_set_export(handle, i)" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c"
grep -Fq "pgy_intent_find_active_registry_slot_export(handle)" "$ROOT_DIR/src/runtime/pgy_runtime_lib_intent_active_index_exports.c"
grep -Fq "AIR rejects MIR pin cleanup without global cleanup evidence" "$ROOT_DIR/src/test_air.c"
grep -Fq "AIR DAG evidence contains unresolved metadata dead-end" "$ROOT_DIR/src/compiler/air_verify_global.c"
grep -R -Fq "sem.type_resolution_metadata_dead_ends = 2" "$ROOT_DIR/src/tests/air"
! grep -R -Fq "type_resolution_metadata_materializer_fallbacks" "$ROOT_DIR/src/semantic" "$ROOT_DIR/src/tests/air"
grep -Fq "return inst->expr0" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
grep -Fq "ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
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
grep -Fq "MIR validator rejects invalid statement inventory shape" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "mir_validate_instruction_surface_usage" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "source payload without surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "missing MIR initializer expression fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "mir_def_source_requires_initializer_fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "mir_instruction_has_surface_payload_or_shape" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "requires_source_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
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
grep -Fq "source-statement receive emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "select receive emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "source-local-decl emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "source-statement LET emit is missing local-decl fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "with-slot Claim resource op is missing MIR ABI type layout fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "with-slot Claim resource op has invalid MIR ABI type layout fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "MIR validator rejects invalid source-statement emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects missing channel receive emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects invalid select receive emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects invalid with-slot claim ABI fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects invalid source-local-decl emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "source-local-decl-emit" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "select-recv-stmt-emit" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "source-branch emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"
grep -Fq "MIR validator rejects source-compatible branch without payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "mir_instruction_branch_requires_source_emit" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_has_source_payload" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_branch_payload_matches_shape" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_terminator_matches" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_MATCH_CASE)" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_matches_ast_type(inst, AST_BLOCK)" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_stmt_has_side_effect_hint(inst)" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "mir_instruction_source_payload(inst) != NULL" "$ROOT_DIR/src/compiler/mir_source_shape.c"
source_ast_eq_count="$(
    grep -F "source_ast_type == expected_type" \
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
grep -Fq "llvm_mir_ast_type_is_cfg_container" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "return llvm_mir_stmt_is_cfg_container(inst->ast)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_is_cfg_container(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_payload(inst) != NULL" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_branch_requires_source_compatibility" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_source_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_source_local_decl_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_channel_receive_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_select_receive_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_def_uses_source_statement_compatibility" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_uses_source_statement_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_emit_channel_receive_def(inst, ctx" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "LLVM channel receive DEF requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_emit_statement(source_payload, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -R -Fq "llvm_emit_statement(inst->ast" "$ROOT_DIR/src/codegen"
grep -Fq "llvm_emit_option_coalesce" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "coalesce.fallback" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "LLVMBuildPhi(ctx->builder, fields[1]" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
if grep -A95 -F "llvm_emit_option_coalesce" \
    "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c" | \
    grep -Fq "LLVMBuildSelect"; then
    echo "[perf-contract] LLVM ?? lowering regressed to eager select fallback" >&2
    exit 1
fi
grep -Fq "LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "transpiler_mir_def_uses_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "transpiler_mir_inst_is_cfg_container" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "mir_instruction_source_payload(inst) == stmt" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "mir_instruction_source_is_cfg_container(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "transpiler_mir_def_uses_source_local_decl_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "transpiler_mir_def_uses_channel_receive_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "transpiler_mir_def_uses_select_receive_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "mir_instruction_uses_source_statement_emit(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
! grep -Fq "static bool" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_seed_block_phi_names" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_def_uses_source_statement_emit(" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "mir_instruction_source_matches_ast_type(inst, expected_type)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.c"
grep -Fq "mir_instruction_source_payload(resource_inst) != stmt" "$ROOT_DIR/src/codegen/transpiler_mir_stmt_emit.c"
! grep -R -Fq "mir_instruction_source_matches_ast_node" "$ROOT_DIR/src"
grep -Fq "missing receive emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "missing select receive emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.c"
grep -Fq "transpiler_emit_mir_assignment_def_inst" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
! grep -Fq "static TranspilerMIRAssignmentEmitResult" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
grep -Fq "!mir_instruction_uses_source_statement_emit(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
grep -Fq "mir_instruction_source_is_defer_stmt(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_source_is_defer_stmt" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "inst->expr1 = ast_let_type(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->expr0 = ast_defer_body(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->arg0 = ast_let_name(stmt)" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->requires_source_statement_emit = true" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "stale thread-pool surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "stale intent observability surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_surface_validate.c"
grep -Fq "inventory surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "inventory_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_inventory_surface_usage_summary" "$ROOT_DIR/src/compiler/mir_surface_usage.h"
grep -Fq "summary = mir_inventory_surface_usage_summary(mir)" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "return inventory_uses_surface" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "return mir->inventory_uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "mir_source_ast_stmt_has_side_effect_hint(" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "mir_instruction_source_payload(inst)" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "mir_source_ast_type_stmt_has_side_effect_hint" "$ROOT_DIR/src/compiler/mir_source_shape.c"
! grep -Fq "source_ast_type" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "MIR DCE uses statement shape facts without AST payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "mir_stmt_ast_type_is_cfg_owned_control" "$ROOT_DIR/src/compiler/mir_source_shape.c"
grep -Fq "if (!mir->has_inventory_surface_usage_facts)" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "pgy_ast_array_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "mir->types" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "mir->intents" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "mir_instruction_has_source_location(inst)" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
grep -Fq "mir_instruction_source_ast_type_or(inst, -1)" "$ROOT_DIR/src/compiler/mir_lifecycle.c"
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
grep -Fq "MIR records intent observability surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "intent observability inventory surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "MIRRoutineInventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_routine_inventory_from_program" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "mir_routine_inventory_get" "$ROOT_DIR/src/compiler/mir_public_surface.c"
grep -Fq "mir_routine_inventory_from_program(mir, &inventory)" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
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
grep -Fq "rejected_expr_payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "return inst->expr0" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "exprs[count++] = inst->expr0" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "return inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"
grep -Fq "exprs[count++] = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"
grep -Fq "llvm_build_mir_intent_step_sources" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "transpiler_build_mir_intent_step_sources" "$ROOT_DIR/src/codegen/transpiler_intent_emit_metadata_helpers.h"
grep -Fq "missing intent step source mapping" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "missing intent step source mapping" "$ROOT_DIR/src/codegen/transpiler_intent_emit.c"
! grep -R -Fq "collect_mir_intent_steps" "$ROOT_DIR/src/codegen"
grep -Fq "has_surface_usage_facts" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "allow_fixture_payload_probe" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "block->instruction_count > 0 && block->instructions == NULL" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "allow_legacy_ast_probe" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "pgy_ast_uses_thread_pool(inst->ast" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "routine->hir_routine == NULL" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "pgy_mir_program_uses_thread_pool" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "inventory_uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "pgy_mir_routine_uses_thread_pool" "$ROOT_DIR/src/codegen/thread_pool_usage.h"
grep -Fq "pgy_mir_program_uses_intent_observability(ctx->mir)" "$ROOT_DIR/src/codegen/transpiler_inventory_view.c"
grep -Fq "transpiler_active_uses_intent_observability(ctx)" "$ROOT_DIR/src/codegen/transpiler_entry.c"
grep -Fq "pgy_mir_program_uses_thread_pool(ctx->mir)" "$ROOT_DIR/src/codegen/transpiler_inventory_view.c"
grep -Fq "transpiler_active_uses_thread_pool(ctx)" "$ROOT_DIR/src/codegen/transpiler_thread_pool.c"
grep -Fq "pgy_mir_program_uses_intent_observability(ctx->mir)" "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
grep -Fq "llvm_active_uses_intent_observability(ctx)" "$ROOT_DIR/src/codegen/llvm_api.c"
grep -Fq "pgy_mir_program_uses_thread_pool(ctx->mir)" "$ROOT_DIR/src/codegen/llvm_inventory_internal.c"
grep -Fq "llvm_active_uses_thread_pool(ctx)" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "mir_instruction_source_branch_payload_matches_shape(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "transpiler_mir_render_select_case_condition" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "pgy_channel_ready_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "MIR select dispatch emits channel readiness in C backend" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "MIR select dispatch materializes bound receive local type" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "transpiler_select_case_has_receive_binding" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "ASTNode *value = ast_assignment_value(node)" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "value->type == AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "ast_assignment_value(body) != NULL" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "ast_assignment_value(body)->type == AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.c"
grep -Fq "_pgy_ssa_v_1 = pgy_channel_recv_val_Int(&ch)" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "strstr(output, \"\\nv = pgy_channel_recv_val_Int(&ch)\") == NULL" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "condition = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
! grep -Fq "? inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c"
grep -Fq "emit_expression_with_ssa_map(inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.c"
grep -Fq "emit_expression(inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "branch_shape == MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_has_required_branch_condition_fact(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "inst->source_ast_type == AST_MATCH_CASE" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "inst->source_ast_type == AST_BLOCK" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "branch_shape == MIR_BRANCH_SELECT_DISPATCH)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -Fq "llvm_mir_branch_has_required_condition_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast != NULL || inst->expr0 != NULL" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "? llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "cond = llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "ASTNode *return_expr = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->expr0 != NULL ? inst->expr0 : inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_register_defer(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast->data.defer_stmt" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_is_with_slot_claim(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "mir_instruction_uses_source_local_decl_emit(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "ASTNode *value_expr = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "ASTNode *type_expr = inst->expr1" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "type_expr != NULL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "value_expr != NULL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->expr0 != NULL ? inst->expr0 : inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
grep -Fq "mir_instruction_is_with_slot_claim(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
grep -Fq "transpiler_emit_mir_resource_op_inst" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "static TranspilerMIRInstEmitResult" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "source_ast_type == AST_WITH_STMT" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
! grep -Fq "inst->ast == NULL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.c"
grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "mir_instruction_source_is_local_decl(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.c"
grep -Fq "transpiler_emit_mir_resource_hook" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "static bool" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
grep -Fq "mir_instruction_source_is_local_decl" "$ROOT_DIR/src/compiler/mir_source_shape.c"
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
grep -Fq "branch instruction misses required condition fact" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.c"
! grep -R -Fq "inst->ast->data" "$ROOT_DIR/src/codegen"
! grep -Fq "source_ast_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
! grep -Fq "source_ast_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent.h"
grep -Fq "mir_instruction_source_is_local_destructure(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
grep -Fq "mir_instruction_source_is_assignment(inst)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.c"
! grep -R -Fq "inst->ast->type" "$ROOT_DIR/src/codegen"
grep -Fq "transpiler_emit_mir_preserved_let_stmt" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
grep -Fq "transpiler_mir_preserved_let_emit.h" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.c"
! grep -R -Fq "transpiler_mir_fallback_let_emit.h" "$ROOT_DIR/src/codegen"
! grep -R -Fq "transpiler_emit_mir_fallback_let_stmt" "$ROOT_DIR/src/codegen"
grep -Fq "silent true fallback is disabled" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "Some requires concrete payload type during C emission" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "C match lowering requires a concrete subject type; implicit Int match fallback is disabled" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "strcmp(subject_type, \"Unknown\") == 0" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "subject_type = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "inner = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "ok_type = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "err_type = \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
! grep -Fq "owned_type_name : \"Unknown\"" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "C array access requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
grep -Fq "C slice access requires concrete Slice<T> element metadata" "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c"
grep -Fq "C tuple literal requires concrete tuple layout metadata" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "C array literal requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "C slot SSA auto-read requires concrete Slot<T> payload metadata" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "cannot determine slot payload type for assignment" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "C await expression requires concrete Future<T> result metadata" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "C spawn expression requires a target expression" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "C spawn expression requires concrete Future<T> return metadata" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "requires concrete C type metadata for call" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "channel send requires concrete Channel<T> payload metadata" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "channel receive requires concrete Channel<T> payload metadata" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "constructed_single_arg_is_unknown" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "constructed_arg_name_is_unknown" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "transpiler_type_name_join" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "PgyArray_" "$ROOT_DIR/src/codegen/transpiler_type_mapping.c"
grep -Fq "strcmp(inner_buf, \"Unknown\") != 0" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin.c"
grep -Fq "without concrete Result error type" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "Some(value) without concrete payload type emits diagnostic recovery" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a.cases.h"
grep -Fq "transpiler_option_type_has_concrete_inner" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_contextual_option_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -Fq "IsSome(None()) without concrete Option<T> emits diagnostic recovery" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a.cases.h"
grep -Fq "transpiler_result_arg_list_has_unknown" "$ROOT_DIR/src/codegen/transpiler_type_result_mapping_helpers.c"
grep -Fq "transpiler_result_type_ident_char" "$ROOT_DIR/src/codegen/transpiler_type_result_mapping_helpers.c"
grep -Fq "Ok(value) with unknown Result payload emits diagnostic recovery" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a.cases.h"
grep -Fq "Result suffix keeps user type names containing Unknown" "$ROOT_DIR/src/tests/transpile/test_transpile_core_part_a.cases.h"
grep -Fq "kTranspilerResultOptionSpecs" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "transpiler_result_option_lookup" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c"
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
! grep -A8 -F 'strncmp(type_name, "List<", 5)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F 'strncmp(type_name, "Set<", 4)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F 'strncmp(type_name, "Queue<", 6)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
! grep -A8 -F 'strncmp(type_name, "HashMap<", 8)' "$ROOT_DIR/src/codegen/llvm_backend_type_map.c" | grep -Fq "return ctx->type_i32"
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
grep -Fq "if (ctx->has_error || array_type == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
! grep -A12 -F "llvm_type_to_suffix" "$ROOT_DIR/src/codegen/llvm_backend_generic.c" | grep -Fq 'return "Unknown";'
grep -Fq "llvm_array_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "requires registered Array<T> local" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "*out = NULL;" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.h"
grep -Fq "llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "llvm_collection_required_receiver_var" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
! grep -Fq "(void)fallback;" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
! grep -Fq "(void)recovery;" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_require.c"
grep -Fq "{ *out = NULL; return true; }" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "kListExtendedSpecs" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "llvm_list_extended_lookup" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_call_list_extended.c"
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
collection_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.c"
grep -Fq "kTranspilerCollectionSpecs" "$collection_builtin_owner"
grep -Fq "transpiler_collection_lookup" "$collection_builtin_owner"
grep -Fq "bsearch(" "$collection_builtin_owner"
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
for direct_to_string_type in String Bool Float Double Long; do
    if grep -Fq "strcmp(arg_type, \"$direct_to_string_type\")" \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c"; then
        echo "[perf-contract] C ToString lowering reintroduced direct type branch: $direct_to_string_type" >&2
        exit 1
    fi
done
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
transpiler_misc_names="$(
    sed -n '/static const TranspilerMiscSpec kTranspilerMiscSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c" \
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
grep -Fq "kTranspilerMapSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "transpiler_map_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_map_builtin.c"
grep -Fq "kTranspilerQueueSpecs" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "transpiler_queue_lookup" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_queue_builtin.c"
grep -Fq "LLVM QueuePush could not lower value expression" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "LLVM QueuePop could not allocate result temporary" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
grep -Fq "{ *out = NULL; return true; }" "$ROOT_DIR/src/codegen/llvm_expr_call_queue_extended.c"
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
grep -Fq "return pergyra_ast_type_to_c_copy(type_node, out, out_size)" "$ROOT_DIR/src/codegen/transpiler_type_declarator.c"
grep -Fq "pergyra_ast_type_to_c_copy((ASTNode *)type_node" "$ROOT_DIR/src/codegen/transpiler_mir_signature.c"
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
grep -Fq "pgy_codegen_call_name_is_slot_operation(callee_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "pgy_codegen_call_name_is_read(callee_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq 'strcmp(callee_name, "Read")' "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq 'strcmp(callee_name, "Write")' "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -Fq 'strcmp(callee_name, "Release")' "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
! grep -A60 -F "llvm_infer_spawn_future_inner" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c" | grep -Fq "static char buf"
grep -Fq "ctx->expected_type_name = llvm_stmt_render_type_annotation_copy" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "llvm_stmt_render_type_annotation_copy(ctx" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "suffix_buf" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_stmt_name_in_sorted_table" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "kLLVMCollectionGetSpecs" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "llvm_stmt_collection_get_spec_compare" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
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
grep -Fq "return pergyra_type_to_llvm(ctx, inner_buf)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "inner_name = inner_name_buf" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "return_type_owned = pergyra_strdup(return_type)" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "pergyra_type_to_c_copy(inferred_return_type" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "transpiler_infer_lambda_param_c_type_copy" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(lambda_return_type" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(param_type_ast" "$ROOT_DIR/src/codegen/transpiler_lambda_emit.c"
grep -Fq "char ok_ctype_buf[128]" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "pergyra_type_to_c_copy(ok_type" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "pergyra_type_to_c_copy(err_type" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "pergyra_type_to_c_copy(inner_type, ctype_buf" "$ROOT_DIR/src/codegen/transpiler_specialization_registry.c"
grep -Fq "char array_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char inferred_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char annotated_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "pergyra_type_to_c_copy(inferred_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "pergyra_type_to_c_copy(ann_source_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "pergyra_type_to_c_copy(array_type_name" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char set_c_type_buf[256]" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "pergyra_type_to_c_copy(ann_type_name" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "pergyra_type_to_c_copy(ann_type_name, map_c_type_buf" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_try_emit_list_or_queue_new_let" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "pergyra_type_to_c_copy(ann_type_name, c_type_buf" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "kTranspilerLetOptionCtorSpecs" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_let_option_ctor_lookup" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "kTranspilerLetCollectionCtorSpecs" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "transpiler_let_collection_ctor_lookup" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "Some")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "None")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "ListNew")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "QueueNew")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
! grep -Fq 'strcmp(callee_name, "MapNew")' "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c"
transpiler_let_option_ctor_names="$(
    sed -n '/static const TranspilerLetOptionCtorSpec kTranspilerLetOptionCtorSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_let_option_ctor_names_sorted="$(
    printf '%s\n' "$transpiler_let_option_ctor_names" | sort
)"
if [[ "$transpiler_let_option_ctor_names" != "$transpiler_let_option_ctor_names_sorted" ]]; then
    echo "C backend let Option ctor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_let_option_ctor_names_sorted") \
        <(printf '%s\n' "$transpiler_let_option_ctor_names") >&2 || true
    exit 1
fi
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
grep -Fq "pergyra_type_to_c_copy(result_type" "$ROOT_DIR/src/codegen/transpiler_let_emit.c"
grep -Fq "char ctype_buf[256]" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "pergyra_type_to_c_copy(tuple_name" "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c"
grep -Fq "pergyra_type_to_c_copy(result_type" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.c"
grep -Fq "pergyra_type_to_c_copy(subject_type" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "pergyra_type_to_c_copy(inner, inner_c_type_buf" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "pergyra_type_to_c_copy(ok_type" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "pergyra_type_to_c_copy(err_type" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "pergyra_type_to_c_copy(payload_type" "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"
grep -Fq "pergyra_type_to_c_copy(inner, inner_c_type_buf" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "pergyra_type_to_c_copy(return_type_name" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "pergyra_type_to_c_copy(bound_type" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "pergyra_type_to_c_copy(inferred_arg_type" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "pergyra_type_to_c_copy(secure_name" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "pergyra_type_to_c_copy(slot_name_buf" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "pgy_codegen_call_name_is_move(callee_name)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "pgy_codegen_call_name_is_move(callee)" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "type_name_is_exact_or_generic(type_name, \"Slot\", \"Slot<\")" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "type_name_is_exact_or_generic(type_name, \"SecureSlot\"" "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
grep -Fq "pgy_codegen_type_name_is_secure_slot(type_name)" "$ROOT_DIR/src/codegen/transpiler_slot_target.c"
grep -Fq "pgy_codegen_type_name_is_slot(ann_name)" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "pgy_codegen_type_name_is_slot(ann_name)" "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq 'strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0' "$ROOT_DIR/src/codegen/llvm_boundary_slot_param.c"
! grep -Fq 'strcmp(type_name, "Slot") != 0' "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
! grep -Fq 'strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0' "$ROOT_DIR/src/codegen/llvm_mir_type_helpers.c"
! grep -Fq 'strcmp(callee_name, "Move")' "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq 'strcmp(callee, "Move")' "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "pergyra_type_to_c_copy(type_name, c_type_buf" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.c"
grep -Fq "pergyra_type_to_c_copy(ret_name" "$ROOT_DIR/src/codegen/transpiler_domain_role_ability_emit.c"
grep -Fq "pergyra_type_to_c_copy(ret_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_call_member_emit.c"
grep -Fq "pergyra_type_to_c_copy(inner, inner_c_type_buf" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "pergyra_type_to_c_copy(owner_role_subject_name" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(p->type, pt_buf" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(return_type" "$ROOT_DIR/src/codegen/transpiler_func_forward_metadata.c"
grep -Fq "pergyra_ast_type_to_c_copy(" "$ROOT_DIR/src/codegen/transpiler_spawn_channel_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_class_decl_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_enum_decl_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_func_return_type(func)" "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_domain_role_methods_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(ast_func_return_type(method)" "$ROOT_DIR/src/codegen/transpiler_generic_class_specialization_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(bt2[b]" "$ROOT_DIR/src/codegen/transpiler_match_emit.c"
grep -Fq "pergyra_ast_type_to_c_copy(type_ast, out" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "pergyra_type_to_c_copy(participant_type" "$ROOT_DIR/src/codegen/transpiler_block_intent_rebind_helpers.c"
grep -Fq "pergyra_type_to_c_copy(elem_names[j]" "$ROOT_DIR/src/codegen/transpiler_destructure_emit.c"
! grep -Fq "static char mapped[128]" "$ROOT_DIR/src/codegen/transpiler_func_forward_helpers.h"
grep -Fq "lookup_future_inner_type_copy" "$ROOT_DIR/src/codegen/transpiler_future_type_query.c"
grep -Fq "lookup_future_inner_type_copy(ctx" "$ROOT_DIR/src/codegen/transpiler_expr_dispatch_emit.c"
grep -Fq "transpiler_require_type_name_c_type_copy" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "transpiler_require_ast_c_type_copy" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
grep -Fq "pergyra_type_to_c_copy(type_name, out" "$ROOT_DIR/src/codegen/transpiler_type_require.c"
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
grep -Fq "llvm_registry_render_required_type_name" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "registry requires concrete type metadata" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "if (ctx->has_error || elem_type == NULL)" "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c"
grep -Fq "llvm_lookup_or_declare_function" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "LLVMAddFunction(ctx->module, name, decl_type)" "$ROOT_DIR/src/codegen/llvm_registry.c"
! grep -R -Fq "llvm_lookup_or_create_function" "$ROOT_DIR/src/codegen"
! grep -R -Fq "fallback_type" "$ROOT_DIR/src/codegen"
! grep -R -Fq "fallback_ret_type" "$ROOT_DIR/src/codegen"
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
grep -Fq "iterable = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "inst->ast->data.for_loop.iterable" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "start = llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "end = llvm_emit_expression(inst->expr1, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "variable = inst->arg0" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
! grep -Fq "node = inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_loop_control.c"
grep -Fq "llvm_mir_recv_expr_channel(inst->expr0)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "llvm_mir_assignment_recv_channel(inst->ast)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
! grep -Fq "llvm_mir_declare_assignment_recv_target(inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_claim_inner_type_name(inst" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
grep -Fq "inst->type_layout->abi_type_name" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
grep -Fq '$(CODEGEN_DIR)/llvm_mir_resource_claim.c' "$ROOT_DIR/Makefile"
grep -Fq "llvm_mir_emit_borrow_view_alias(inst, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq '$(CODEGEN_DIR)/llvm_mir_resource_view.c' "$ROOT_DIR/Makefile"
! grep -Fq "node->data.with_stmt" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "node->data.with_stmt" "$ROOT_DIR/src/codegen/llvm_mir_resource_claim.c"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_list_size_raw_export\")" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
! grep -Fq "llvm_lookup_function(ctx, \"pgy_list_get_raw_export\")" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "llvm_stmt_for_in_required_runtime" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "LLVM statement for-in lowering requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
! grep -Fq "LLVMFuncEntry *size_fn = llvm_lookup_function(ctx, \"pgy_list_size_raw_export\")" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
! grep -Fq "LLVMFuncEntry *get_fn = llvm_lookup_function(ctx, \"pgy_list_get_raw_export\")" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "kLLVMDomainQuerySpecs" "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"
grep -Fq "llvm_domain_query_lookup" "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"
grep -Fq "bsearch(" "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c"
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
grep -Fq "indexed collection access" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "llvm_expression_error" "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c"
! grep -A16 -F "llvm_expression_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM array access could not lower receiver or index expression" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM aggregate array access requires concrete element metadata" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "strcmp(inner_name, \"Unknown\") == 0" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "if (ctx == NULL || node == NULL || ctx->has_error)" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM array access receiver is not an array, slice, string, or pointer" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "llvm_zero_value_for_type" "$ROOT_DIR/src/codegen/llvm_expr_emit_support.c"
grep -Fq "LLVM TaskGroup expression must lower through AIR/RIR/MIR task-group boundary" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "first_value = llvm_emit_expression" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM array literal could not lower element %zu" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM array literal could not allocate array temporary" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM tuple literal could not lower element %zu" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "LLVM lambda expression could not lower return type" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM lambda expression could not lower parameter type" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM lambda expression could not lower body expression" "$ROOT_DIR/src/codegen/llvm_expr.c"
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
grep -Fq "llvm_array_format_runtime_name" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "\"pgy_array_pop\"" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "pgy_array_pop_##Suffix" "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h"
grep -Fq "LLVM Slice() receiver requires concrete element type metadata" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVM Slice() receiver requires registered Slice<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "LLVM slot method '%s' requires registered slot local" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
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
grep -Fq '"pgy_channel_init_",' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq '"pgy_array_new_",' "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "array literal expression" "$ROOT_DIR/src/codegen/llvm_expr_aggregate.c"
grep -Fq "channel send expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "channel receive expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "llvm_channel_required_binding" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "requires registered Channel<T> local" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
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
grep -Fq "llvm_projection_error_recovery" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
! grep -A16 -F "llvm_projection_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "LLVM projection source field '%s' is missing from source metadata" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "LLVM projection source field '%s' is ambiguous across vessel paths" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
grep -Fq "LLVM subject projection requires target/source class metadata and source storage" "$ROOT_DIR/src/codegen/llvm_expr_projection_path_helpers.c"
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
grep -Fq "llvm_constructor_error" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM enum variant constructor could not lower payload argument" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM class constructor could not lower field argument" "$ROOT_DIR/src/codegen/llvm_expr_constructor_calls.c"
grep -Fq "LLVM hosted method call argument allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_call_hosted.c"
grep -Fq "LLVM callable variable call could not lower callee expression" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable variable call could not lower callable signature" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable argument could not lower callable signature" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM event-handler callable could not lower return type" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM event-handler callable could not lower parameter type" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable parameter declaration type could not be lowered" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
grep -Fq "LLVM callable signature parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "LLVM lambda signature parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_stmt_let_helpers.c"
grep -Fq "if (ctx->has_error || var_type == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "LLVM event-handler type parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c"
grep -Fq "LLVM tuple type field allocation failed" "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c"
grep -Fq "LLVM type rendering requires concrete type metadata" "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c"
! grep -A8 -F "LLVM AST type mapping requires AST_TYPE" \
    "$ROOT_DIR/src/codegen/llvm_backend_ast_type.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "LLVM intent forward declaration parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM intent forward declaration could not lower participant type" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "LLVM intent forward declaration could not lower value type" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "pt = ctx->type_i8ptr;" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "if (ctx->has_error || participant_value_type == NULL)" "$ROOT_DIR/src/codegen/llvm_intent_zone.c"
grep -Fq "LLVM event-handler callable parameter allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_call_variable.c"
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
grep -Fq "if (ctx->has_error || alloca_type == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_local_type_from_value_fact" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "llvm_mir_get_var_entry(vars, var_count, name)" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -A24 -F "llvm_decl_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_decl.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A24 -F "llvm_register_required_ast_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_register.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A24 -F "llvm_domain_event_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_event.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A24 -F "llvm_domain_forward_required_param_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_forward.c" | \
    grep -Fq "return ctx->type_i32"
! grep -A24 -F "llvm_domain_required_ast_type(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c" | \
    grep -Fq "return ctx->type_i32"
grep -Fq "llvm_domain_required_class_struct_type" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c"
grep -Fq "requires registered class metadata" \
    "$ROOT_DIR/src/codegen/llvm_domain_struct_fields.c"
grep -Fq "if (ctx->has_error || field_types[j] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_register.c"
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
grep -Fq "if (ctx->has_error || param_types[i] == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_emit.c"
grep -Fq "if (ctx->has_error || pt == NULL)" \
    "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "could not lower argument %zu" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "loaded-argument allocation failed" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "llvm_member_call_error_recovery" "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
! grep -A16 -F "llvm_member_call_error_recovery(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_member_call_support.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "could not lower an argument" "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq "could not allocate method name" "$ROOT_DIR/src/codegen/llvm_member_call_support.c"
grep -Fq "requires a self receiver" "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
grep -Fq "is not declared in the backend method registry" "$ROOT_DIR/src/codegen/llvm_member_call_emit.c"
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
grep -Fq "return decl_header->ast_type == decl_type" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "return decl_header->ast_type == decl_type" "$ROOT_DIR/src/codegen/transpiler_decl_lookup.c"
grep -Fq "AST_PARTY_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "AST_ROLE_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "AST_ROSTER_DECL" "$ROOT_DIR/src/codegen/host_decl_compat.c"
grep -Fq "ChannelClose" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c"
grep -Fq "llvm_task_channel_format_runtime_name" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "\"pgy_channel_close\"" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
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
grep -Fq "\"device slot\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
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
grep -Fq "requires paired token binding" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "requires a registered slot local" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
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
grep -Fq "LLVM slot assignment requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "indexed array assignment" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "LLVM indexed array assignment requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "llvm_assignment_error" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
! grep -A16 -F "llvm_assignment_error(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c" | \
    grep -Fq "LLVMConstInt(ctx->type_i32, 0, 0)"
grep -Fq "requires registered Array<T> local" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "requires a writable member lvalue" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "requires a registered local or host field target" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "\"secure slot\" : \"slot\"" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_write(ctx, slot_var, val)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_read(ctx, slot_var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_release(ctx, slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM with-slot cleanup requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "LLVM slot initializer requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt_let_resources.c"
grep -Fq "LLVM auto-release requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt.c"
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
grep -Fq "select_write_case_guard" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "select_emit_unbound_consume" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "select_channel_inner_type" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "transpiler_channel_query_spec_compare" "$channel_builtin_owner"
grep -Fq "emit_call_stdlib_channel_query_builtin" "$channel_builtin_owner"
scalar_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_builtin.c"
grep -Fq "transpiler_scalar_unary_spec_compare" "$scalar_builtin_owner"
grep -Fq "transpiler_scalar_unary_builtin_name(fn)" "$scalar_builtin_owner"
transpiler_scalar_unary_names="$(
    sed -n '/static const TranspilerScalarUnarySpec specs\[\]/,/^    };/p' \
        "$scalar_builtin_owner" \
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
! grep -Fq "poison i32 until Future<T>" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "llvm_stmt_lookup_visible_function" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -Fq "channel receive '%s' has no registered Channel<T> metadata" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A34 -F "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "ctx->expected_type_name"
grep -A34 -F "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq 'strncmp(ctx->expected_type_name, "Channel<", 8) != 0'
grep -Fq '{ "Input", "String", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c"
grep -Fq '{ "Concat", "String", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c"
grep -Fq '{ "StringConcat", "String", PGY_BUILTIN_FLAG_NONE }' "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c"
! grep -Fq 'strcmp(callee, "Input")' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strcmp(callee, "Concat")' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
! grep -Fq 'strcmp(callee, "StringConcat")' "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A14 -F "case AST_NUMBER" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "ast_number_is_long(expr)"
grep -A14 -F "case AST_NUMBER" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "return ctx->type_f64"
grep -A6 -F "if (init->type == AST_NUMBER)" "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | \
    grep -Fq "infer_expression_type_name(ctx, init)"
grep -A6 -F "if (init->type == AST_NUMBER)" "$ROOT_DIR/src/codegen/transpiler_let_emit.c" | \
    grep -Fq "pergyra_type_to_c_copy(inferred_type"
grep -Fq "transpiler_promote_numeric_type_name" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c"
grep -A28 -F "if (op == TOKEN_PLUS)" "$ROOT_DIR/src/codegen/transpiler_expr_type_infer.c" | \
    grep -Fq "transpiler_promote_numeric_type_name(left_type, right_type)"
grep -Fq "llvm_stmt_promote_numeric_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A22 -F "if (op == TOKEN_PLUS)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty)"
grep -Fq "llvm_stmt_lookup_declared_call_return_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c"
grep -A14 -F "llvm_stmt_lookup_visible_function(ctx, callee)" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "llvm_stmt_lookup_declared_call_return_type(ctx, callee)"
grep -A12 -F "Domain helper calls can be emitted" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "ctx->expected_type_name"
grep -A28 -F "case AST_BINARY" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "unsupported binary operator has no inferred LLVM type"
grep -Fq "llvm_stmt_expected_array_elem_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c"
grep -A10 -F "llvm_stmt_resolve_array_elem_type" "$ROOT_DIR/src/codegen/llvm_stmt_type_infer.c" | \
    grep -Fq "llvm_stmt_expected_array_elem_type(ctx)"
grep -A8 -F "llvm_register_future_var(LLVMGenCtx *ctx" \
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
grep -Fq "llvm_keep_rendered_persistent(ctx" \
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
grep -Fq "render_type_name(constraint)" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
! grep -Fq "constraint->data.type.name" \
    "$ROOT_DIR/src/codegen/transpiler_let_slot_emit.c"
grep -Fq "transpiler_generic_param_effective_arg_name" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.h"
grep -Fq "transpiler_generic_param_effective_arg_name(GenericParam *formal" \
    "$ROOT_DIR/src/codegen/transpiler_generic_param_query.c"
grep -Fq "render_type_name(arg_constraint)" \
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
grep -A8 -F "llvm_lookup_future_inner(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL"
grep -A8 -F "llvm_register_channel_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL || inner_type == NULL"
grep -A8 -F "llvm_lookup_channel_inner(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "ctx == NULL || var_name == NULL"
grep -A14 -F "llvm_register_device_slot_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "owned_var_name = llvm_registry_keep_string(ctx, var_name)"
grep -A14 -F "llvm_register_device_slot_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "owned_inner_type = llvm_registry_keep_string(ctx, inner_type)"
grep -A10 -F "llvm_lookup_secure_token_var(LLVMGenCtx *ctx" \
    "$ROOT_DIR/src/codegen/llvm_registry_resources.c" | \
    grep -Fq "(size_t)written >= sizeof(token_name)"
grep -Fq "LLVM thread-pool entry requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "LLVM event initialization requires generated event function" "$ROOT_DIR/src/codegen/llvm_main_wrapper.c"
grep -Fq "LLVM MIR select readiness requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "if (ctx->has_error || value_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "if (ctx->has_error || elem_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "if (ctx->has_error || elem_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_loop_match.c"
grep -Fq "if (ctx->has_error || val_ty == NULL)" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"
grep -Fq "LLVM MIR secure pin requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "LLVM MIR pin requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "LLVM MIR pin cleanup requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_pin_region.c"
grep -Fq "\"secure slot\", method_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "\"slot\", method_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "llvm_direct_secure_slot_write(ctx, slot_var, val)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "return llvm_direct_secure_slot_read(ctx, slot_var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
! grep -Fq "llvm_direct_secure_slot_release(ctx, slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_call_methods_domain_slice.c"
grep -Fq "define pgy_link" "$ROOT_DIR/Makefile"
grep -Fq '$(notdir $@).rsp' "$ROOT_DIR/Makefile"
grep -Fq '@"$(BUILD_DIR)/$(notdir $@).rsp"' "$ROOT_DIR/Makefile"
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
if grep -Fq "hir->routine_count + 1" "$ROOT_DIR/src/compiler/hir_routines.c"; then
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
    "$ROOT_DIR/src/codegen/transpiler_domain_nominal_emit.c" || {
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
if grep -R "return slot_inner_type_name(" "$ROOT_DIR/src/codegen" >/dev/null; then
    echo "[perf-contract] C backend reintroduced static slot inner return" >&2
    exit 1
fi
if grep -R "= slot_inner_type_name(" "$ROOT_DIR/src/codegen" >/dev/null; then
    echo "[perf-contract] C backend reintroduced static slot inner assignment" >&2
    exit 1
fi
if grep -R "slot_inner_type_name(.*)" "$ROOT_DIR/src/codegen" \
    | grep -v "transpiler_type_mapping" \
    | grep -v "transpiler.h" \
    | grep -v "transpiler_infer_slot_inner_type_name" >/dev/null; then
    echo "[perf-contract] C backend direct slot_inner_type_name call escaped copy seam" >&2
    exit 1
fi

echo "[perf-contract] perf summary contract is smoke-gated"
