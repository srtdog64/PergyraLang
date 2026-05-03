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
grep -Fq "P10" "$ROOT_DIR/TODO.md"
grep -Fq "trace_len" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "pgy_intent_active_count" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_inline.h"
grep -Fq "count = pgy_intent_active_count" "$ROOT_DIR/src/runtime/pgy_runtime_intent_active_exports.h"
grep -Fq "pgy_intent_append_line_len" "$ROOT_DIR/src/runtime/pgy_runtime_intent_trace_events_inline.h"
grep -Fq "pgy_intent_append_line_len_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.h"
grep -Fq "symbol_ptr_array_contains" "$ROOT_DIR/src/semantic/slot_analyzer.c"
grep -Fq "bsearch(&needle" "$ROOT_DIR/src/semantic/slot_analyzer.c"
grep -Fq "air_collect_mir_pin_block_evidence" "$ROOT_DIR/src/compiler/air_evidence.c"
grep -Fq "air_has_mir_pin_cleanup_evidence" "$ROOT_DIR/src/compiler/air_evidence.c"
grep -Fq "realloc(q->data, nc * sizeof" "$ROOT_DIR/src/runtime/pgy_runtime_queue_inline.h"
grep -Fq "HIRRoutineNameIndex" "$ROOT_DIR/src/compiler/hir.c"
grep -Fq "hir_build_routine_name_index" "$ROOT_DIR/src/compiler/hir.c"
grep -Fq "hir_lookup_routine_index_by_name" "$ROOT_DIR/src/compiler/hir.c"
grep -Fq "evidence_capacity" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "drift_capacity" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "owned_name_capacity" "$ROOT_DIR/src/compiler/air.h"
grep -Fq "air_ensure_owned_name_capacity" "$ROOT_DIR/src/compiler/air.c"
grep -Fq "AIR boundary node %zu has %s summary without evidence node" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "AIR boundary evidence node %zu has no matching boundary summary flag" "$ROOT_DIR/src/compiler/air_validate_evidence.c"
grep -Fq "HIR CFG evidence summary without evidence node" "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "no matching boundary summary flag" "$ROOT_DIR/src/tests/air/test_air_core_part_a.cases.h"
grep -Fq "AIR strict evidence rejects stale legacy summary flags" "$ROOT_DIR/src/test_air.c"
grep -Fq "index_keys" "$ROOT_DIR/src/semantic/type_checker.h"
grep -Fq "metadata_lookup_entry_index" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata.c"
grep -Fq "metadata_index_insert" "$ROOT_DIR/src/semantic/type_checker_resolution_metadata.c"
grep -Fq "strncmp(name, \"Intent\", 6)" "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c"
grep -Fq "ast_contains_identifier_call" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_decl_methods_contain_identifier_call" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "ast_uses_intent_observability_surface" "$ROOT_DIR/src/parser/ast_analysis.c"
grep -Fq "pgy_mir_instruction_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "inst->has_surface_usage_facts" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "uses_intent_observability_surface" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "pgy_name_array_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "routine->hir_routine->direct_calls" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "pgy_mir_block_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "routine->hir_routine == NULL" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "routine->ast" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "block->source_terminator_condition" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "block->source_terminator_value" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "transpiler_find_block_let_decl_from_mir_insts" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "has_source_location" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_map.h"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_map.h"
grep -Fq "block->has_source_location" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "source_ast;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_terminator_condition" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_terminator_value" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "source_terminator_kind" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "MIRStatementInventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "stmt = inst->ast" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.h"
! grep -Fq "source_statement_inventory" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.h"
grep -Fq "source_statement_inventory" "$ROOT_DIR/src/compiler/mir_stmt_population.h"
! grep -Fq "while (*stmt_index < block->source_statement_count)" "$ROOT_DIR/src/compiler/mir_ssa_use_edges.h"
! grep -Fq "source_statements;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_statement_count;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_hir_block;" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_block_source_inventory_at(block, s)" "$ROOT_DIR/src/compiler/mir_stmt_population.h"
grep -Fq "mir_block_source_inventory_items(block)" "$ROOT_DIR/src/compiler/mir_stmt_population.h"
grep -Fq "mir_validate_statement_inventory" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "source statement index %zu exceeds inventory count %zu" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "MIR validator rejects invalid statement inventory shape" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "mir_validate_instruction_surface_usage" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "source payload without surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "stale thread-pool surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "stale intent observability surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "inventory surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_validate.h"
grep -Fq "inventory_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "inst->has_source_location" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "inst->source_ast_type" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "inst->ast->line" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "source_terminator_condition" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "source_terminator_value" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "source_statements[0]" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "has_source_statement_index" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "has_surface_usage_facts" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "uses_thread_pool_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "ast_uses_intent_observability_surface(inst->ast)" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "MIR records intent observability surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "intent observability inventory surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "branch_shape = mir_branch_shape_from_ast" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_instruction_record_surface_usage(&inst);" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_instruction_record_surface_usage(&inst)" "$ROOT_DIR/src/compiler/mir_base_helpers.h"
grep -Fq "mir_instruction_record_surface_usage(&inst)" "$ROOT_DIR/src/compiler/mir_cleanup.c"
grep -Fq "mir_instruction_record_surface_usage(&inst)" "$ROOT_DIR/src/compiler/mir_intent.c"
grep -Fq "has_surface_usage_facts" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "routine->hir_routine == NULL" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "branch_shape == MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.h"
grep -Fq "source_ast_type == AST_WITH_STMT" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.h"
grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.h"
grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.h"
grep -Fq "source_ast_type == AST_CALL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
grep -Fq "source_ast_type != AST_WITH_STMT" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
grep -Fq "source_ast_type == AST_CALL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
grep -Fq "source_ast_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
grep -Fq "source_ast_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent.h"
grep -Fq "source_ast_type == AST_LET_DESTRUCTURE" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.h"
grep -Fq "source_ast_type == AST_CALL" "$ROOT_DIR/src/codegen/transpiler_helpers.h"
! grep -R -Fq "inst->ast->type" "$ROOT_DIR/src/codegen"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/compiler/mir_cfg_contract_validate.h"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/test_mir.c"
grep -Fq "transpiler_mir_block_has_source_order_metadata" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.h"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.h"
! grep -Fq "block->source_statement_count" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.h"
grep -Fq "semantic_loaded_modules_append" "$ROOT_DIR/src/semantic/semantic.c"
grep -Fq "capacity;" "$ROOT_DIR/src/semantic/type_checker_flow_resources.h"
grep -Fq "next_capacity = snap.capacity == 0 ? 8 : snap.capacity * 2" \
    "$ROOT_DIR/src/semantic/type_checker_flow_resources.h"
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
if grep -Fq "snap.count + 1" "$ROOT_DIR/src/semantic/type_checker_flow_resources.h"; then
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

echo "[perf-contract] perf summary contract is smoke-gated"
