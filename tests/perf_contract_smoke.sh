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
grep -Fq "pgy_intent_append_line_len_export" "$ROOT_DIR/src/runtime/pgy_runtime_lib_set_intent_trace_exports.c"
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
grep -Fq "air_ensure_owned_name_capacity" "$ROOT_DIR/src/compiler/air_names.c"
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
if [[ "$(grep -Fc "ctx->uses_intent_observability = pgy_mir_program_uses_intent_observability(mir)" "$ROOT_DIR/src/codegen/llvm_api.c")" -lt 2 ]]; then
    echo "LLVM IR and object codegen paths must both preserve intent observability runtime selection" >&2
    exit 1
fi
grep -Fq "return llvm_result_error_with_hints(msg, ctx->error_code" "$ROOT_DIR/src/codegen/llvm_api.c"
grep -Fq "allow_legacy_payload_probe" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "allow_legacy_ast_probe" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
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
! grep -Fq "pgy_ast_uses_intent_observability(inst->ast" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "require_slot_token_name" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
grep -Fq "token name synthesis is disabled" "$ROOT_DIR/src/codegen/transpiler_symbols.c"
! grep -R -Fq "lookup_slot_token_name_or_default" "$ROOT_DIR/src/codegen"
! grep -R -Fq "fallback_token" "$ROOT_DIR/src/codegen"
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "transpiler_find_block_binding_from_mir_insts" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "transpiler_pending_binding_from_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
! grep -Fq "transpiler_pending_binding_from_source_compatibility" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "!inst->requires_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "!inst->requires_source_local_decl_emit" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "out->initializer = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "out->type_annotation = inst->expr1" "$ROOT_DIR/src/codegen/transpiler_mir_pending_uses.h"
grep -Fq "transpiler_mir_ssa_local_limit_fail" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.h"
grep -Fq "PGY_CODE_MIR_SSA_LIMIT" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.h"
grep -Fq "PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED" "$ROOT_DIR/src/codegen/transpiler_mir_func_ssa_locals_emit.h"
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
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "has_source_location" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_map.c"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/codegen/transpiler_mir_ssa_map.c"
grep -Fq "block->has_source_location" "$ROOT_DIR/src/compiler/mir_public_surface.h"
! grep -Fq "source_ast;" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_terminator_condition" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "source_terminator_value" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "source_terminator_kind" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "source_terminator_has_value" "$ROOT_DIR/src/compiler/mir.h"
! grep -Fq "block->source_ast" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "MIRStatementInventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "used_non_cfg_body_fallback" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "non_cfg_body_fallback_count" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "noncfg=%zu" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "mir_validate_non_cfg_fallback_state" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "used non-CFG body fallback" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "MIR validator rejects CFG-backed non-CFG body fallback state" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "return inst->expr0" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
grep -Fq "ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
! grep -Fq "source_statement_inventory" "$ROOT_DIR/src/compiler/mir_ssa_rename.c"
grep -Fq "source_statement_inventory" "$ROOT_DIR/src/compiler/mir_stmt_population.c"
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
grep -Fq "source payload without surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "missing MIR initializer expression fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "mir_def_source_requires_initializer_fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "mir_instruction_has_surface_payload_or_shape" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "requires_source_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_source_local_decl_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_channel_receive_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_select_receive_statement_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "is_select_case_body" "$ROOT_DIR/src/compiler/hir.h"
grep -Fq "is_select_case_body" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "requires_source_branch_emit" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "DEF is missing source-statement emit fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "channel receive DEF is missing source-statement receive emit fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "select receive DEF is missing select receive emit fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "branch is missing source-branch emit fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "source-statement emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "source-statement receive emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "select receive emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "source-local-decl emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "source-statement LET emit is missing local-decl fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "with-slot Claim resource op is missing MIR ABI type layout fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "with-slot Claim resource op has invalid MIR ABI type layout fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "MIR validator rejects invalid source-statement emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects missing channel receive emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects invalid select receive emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects invalid with-slot claim ABI fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "MIR validator rejects invalid source-local-decl emit fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "source-local-decl-emit" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "select-recv-stmt-emit" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "source-branch emit fact is invalid" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "MIR validator rejects source-compatible branch without payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "mir_branch_shape_requires_source_emit" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
! grep -Fq "mir_branch_shape_requires_source_compatibility" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
! grep -Fq "mir_def_source_expression" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "missing MIR value expression fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "missing MIR terminator expression fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "source_terminator_has_value" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "missing MIR body expression fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
! grep -Fq "&& inst->ast != NULL" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "llvm_mir_ast_type_is_cfg_container" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_branch_requires_source_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_branch_requires_source_compatibility" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_source_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_source_local_decl_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_channel_receive_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_def_uses_select_receive_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "llvm_mir_def_uses_source_statement_compatibility" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "inst->requires_source_statement_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_emit_channel_receive_def(inst, ctx" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "LLVM channel receive DEF requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_emit_statement(inst->ast, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "transpiler_mir_def_uses_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_def_uses_source_local_decl_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_def_uses_channel_receive_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_def_uses_select_receive_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "inst->requires_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"
grep -Fq "transpiler_mir_def_uses_source_statement_emit(" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
grep -Fq "missing receive emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
grep -Fq "missing select receive emit fact" "$ROOT_DIR/src/codegen/transpiler_mir_assignment_emit.h"
grep -Fq "!inst->requires_source_statement_emit" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.h"
if grep -B2 -F "source_ast_type == AST_DEFER_STMT" \
    "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c" | grep -Fq "inst->ast != NULL"; then
    echo "LLVM defer STMT emission must consume MIR defer facts, not AST presence" >&2
    exit 1
fi
grep -Fq "inst->expr1 = stmt->data.let_decl.type" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->expr0 = stmt->data.defer_stmt.body" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->arg0 = stmt->data.let_decl.name" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "inst->requires_source_statement_emit = true" "$ROOT_DIR/src/compiler/mir_call_fact.c"
grep -Fq "stale thread-pool surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "stale intent observability surface usage fact" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "inventory surface usage facts" "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq "inventory_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_stmt_shape_has_side_effect" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "source_ast_type" "$ROOT_DIR/src/compiler/mir_dce.c"
grep -Fq "MIR DCE uses statement shape facts without AST payload" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_c.cases.h"
grep -Fq "mir_stmt_ast_type_is_cfg_owned_control" "$ROOT_DIR/src/compiler/mir_cfg_contract_validate.c"
grep -Fq "if (!mir->has_inventory_surface_usage_facts)" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "pgy_ast_array_uses_intent_observability" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "mir->types" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
! grep -Fq "mir->intents" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
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
grep -Fq "ast_uses_intent_observability_surface" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "MIR records intent observability surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "intent observability inventory surface usage fact" "$ROOT_DIR/src/tests/mir/test_mir_lowering_part_b.cases.h"
grep -Fq "MIRRoutineInventory" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "mir_routine_inventory_from_program" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "mir_routine_inventory_get" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "mir_routine_inventory_from_program(mir, &inventory)" "$ROOT_DIR/src/codegen/intent_observability_usage.c"
grep -Fq "mir_routine_inventory_from_program(mir, &inventory)" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/compiler/mir.h"
grep -Fq "branch_shape = mir_branch_shape_from_ast" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_branch_shape_name" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "source-branch-emit" "$ROOT_DIR/src/compiler/mir_public_surface.h"
grep -Fq "inst.requires_source_branch_emit" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_instruction_record_surface_usage(&inst);" "$ROOT_DIR/src/compiler/mir.c"
grep -Fq "mir_instruction_record_surface_usage(&inst)" "$ROOT_DIR/src/compiler/mir_base_helpers.c"
grep -Fq "mir_instruction_record_surface_usage(&inst)" "$ROOT_DIR/src/compiler/mir_cleanup.c"
grep -Fq "mir_instruction_record_surface_usage(&inst)" "$ROOT_DIR/src/compiler/mir_intent.c"
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
grep -Fq "missing intent step source mapping" "$ROOT_DIR/src/codegen/transpiler_intent_emit.h"
! grep -R -Fq "collect_mir_intent_steps" "$ROOT_DIR/src/codegen"
grep -Fq "has_surface_usage_facts" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "allow_legacy_payload_probe" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "allow_legacy_ast_probe" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "allow_ast_fallback" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
! grep -Fq "pgy_ast_uses_thread_pool(inst->ast" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "routine->hir_routine == NULL" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "pgy_mir_program_uses_thread_pool" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "inventory_uses_thread_pool_surface" "$ROOT_DIR/src/codegen/thread_pool_usage.c"
grep -Fq "pgy_mir_program_uses_thread_pool(ctx->mir)" "$ROOT_DIR/src/codegen/transpiler_thread_pool.c"
grep -Fq "pgy_mir_program_uses_thread_pool(ctx->mir)" "$ROOT_DIR/src/codegen/llvm_pipeline.c"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "branch_shape == MIR_BRANCH_FOR_RANGE" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "transpiler_mir_render_select_case_condition" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "pgy_channel_ready_%s(&%s)" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "MIR select dispatch emits channel readiness in C backend" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "MIR select dispatch materializes bound receive local type" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "transpiler_select_case_has_receive_binding" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "node->data.assignment.value->type == AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_binding.c"
grep -Fq "body->data.assignment.value->type == AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.h"
grep -Fq "case AST_CHANNEL_RECV" "$ROOT_DIR/src/codegen/transpiler_mir_local_type_lookup.h"
grep -Fq "_pgy_ssa_v_1 = pgy_channel_recv_val_Int(&ch)" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "strstr(output, \"\\nv = pgy_channel_recv_val_Int(&ch)\") == NULL" "$ROOT_DIR/src/tests/transpile/test_transpile_mir_part_b.cases.h"
grep -Fq "condition = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
! grep -Fq "? inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.h"
grep -Fq "emit_expression_with_ssa_map(inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_terminator_emit.h"
grep -Fq "emit_expression(inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_func_emit.h"
grep -Fq "branch_shape == MIR_BRANCH_FOR_IN" "$ROOT_DIR/src/codegen/llvm_mir_for_in_control.c"
grep -Fq "branch_shape == MIR_BRANCH_MATCH_CASE" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "inst->requires_source_branch_emit" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "inst->requires_source_branch_emit" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.h"
! grep -Fq "branch_shape == MIR_BRANCH_SELECT_DISPATCH)" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.h"
grep -Fq "llvm_mir_branch_has_required_condition_fact(inst)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast != NULL || inst->expr0 != NULL" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "? llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "cond = llvm_emit_expression(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "ASTNode *return_expr = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->expr0 != NULL ? inst->expr0 : inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "llvm_register_defer(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast->data.defer_stmt" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "source_ast_type == AST_WITH_STMT" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_block_emit.c"
grep -Fq "ASTNode *value_expr = inst->expr0" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "ASTNode *type_expr = inst->expr1" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "type_expr != NULL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "value_expr != NULL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->expr0 != NULL ? inst->expr0 : inst->ast" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/llvm_mir_local_emit.c"
grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
grep -Fq "source_ast_type == AST_WITH_STMT" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
grep -Fq "&& !(inst->has_source_location" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "inst->ast == NULL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
grep -Fq "source_ast_type == AST_LET_DECL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "inst->ast != NULL" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "inst->ast->type" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "inst->ast->data.call" "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_emit.h"
! grep -Fq "inst->ast->data.call" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "inst->expr0" "$ROOT_DIR/src/codegen/transpiler_helpers.h"
! grep -Fq "inst->ast->data.call" "$ROOT_DIR/src/codegen/transpiler_helpers.h"
grep -Fq "transpiler_register_defer(inst->expr0, ctx)" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.h"
! grep -Fq "stmt->data.defer_stmt.body" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.h"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/transpiler_mir_resource_hook_emit.h"
! grep -Fq "inst->ast->data.let_decl" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.h"
! grep -Fq "inst->ast->data.assignment" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.h"
! grep -Fq "inst->ast->data.let_destructure" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.h"
grep -Fq "ASTNode *payload_expr = inst->expr0" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.h"
! grep -Fq "transpiler_expr_identifiers_mapped(ctx, inst->ast" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.h"
grep -Fq "branch instruction misses condition expression fact" "$ROOT_DIR/src/codegen/transpiler_mir_emission_contract.h"
! grep -R -Fq "inst->ast->data" "$ROOT_DIR/src/codegen"
! grep -Fq "source_ast_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/llvm_intent_flow.c"
! grep -Fq "source_ast_type != AST_INTENT_STEP" "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent.h"
grep -Fq "source_ast_type == AST_LET_DESTRUCTURE" "$ROOT_DIR/src/codegen/transpiler_mir_emission_mapping_contract.h"
! grep -R -Fq "inst->ast->type" "$ROOT_DIR/src/codegen"
grep -Fq "transpiler_emit_mir_preserved_let_stmt" "$ROOT_DIR/src/codegen/transpiler_mir_preserved_let_emit.h"
grep -Fq "transpiler_mir_preserved_let_emit.h" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.h"
! grep -R -Fq "transpiler_mir_fallback_let_emit.h" "$ROOT_DIR/src/codegen"
! grep -R -Fq "transpiler_emit_mir_fallback_let_stmt" "$ROOT_DIR/src/codegen"
grep -Fq "silent true fallback is disabled" "$ROOT_DIR/src/codegen/llvm_intent.c"
! grep -Fq "lossy fallback" "$ROOT_DIR/src/codegen/llvm_intent.c"
grep -Fq "llvm_lookup_or_declare_function" "$ROOT_DIR/src/codegen/llvm_registry.c"
grep -Fq "LLVMAddFunction(ctx->module, name, decl_type)" "$ROOT_DIR/src/codegen/llvm_registry.c"
! grep -R -Fq "llvm_lookup_or_create_function" "$ROOT_DIR/src/codegen"
! grep -R -Fq "fallback_type" "$ROOT_DIR/src/codegen"
! grep -R -Fq "fallback_ret_type" "$ROOT_DIR/src/codegen"
grep -Fq "llvm_required_collection_function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "llvm_required_collection_function" "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c"
grep -Fq "llvm_required_hashmap_raw_export" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
grep -Fq "requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_call_collections_extended.c"
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
grep -Fq "llvm_required_log_function" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "LLVM log operation requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c"
grep -Fq "LLVM intent observability builtin" "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"
grep -Fq "requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_intent_observability_calls.c"
grep -Fq "llvm_required_runtime_function" "$ROOT_DIR/src/codegen/llvm_runtime_require.c"
grep -Fq "LLVM %s builtin '%s' requires registered runtime function '%s'" "$ROOT_DIR/src/codegen/llvm_runtime_require.c"
grep -Fq "array\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "llvm_required_runtime_function(ctx, node" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_emit_required_runtime_call_result" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "stdlib string" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "stdlib file" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"StringContains\")" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"pgy_file_open\")" "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c"
grep -Fq "llvm_required_checked_math_function" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "llvm_required_scalar_runtime_function" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "checked arithmetic" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "string concatenation" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "string comparison" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "LLVM string coercion requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_string_coerce.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, helper)" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"StringConcat\")" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, \"pgy_string_equals\")" "$ROOT_DIR/src/codegen/llvm_expr_scalar_core.c"
grep -Fq "indexed collection access" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM indexed collection access requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_helpers.c"
grep -Fq "pgy_array_pop_%s" "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c"
grep -Fq "pgy_array_pop_##Suffix" "$ROOT_DIR/src/runtime/pgy_runtime_lib_array_map_exports.h"
grep -Fq "pgy_channel_init_%s" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "pgy_array_new_%s" "$ROOT_DIR/src/codegen/llvm_stmt_let_collections.c"
grep -Fq "array literal expression" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "channel send expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "channel receive expression" "$ROOT_DIR/src/codegen/llvm_expr_channel.c"
grep -Fq "LLVM event subscribe requires generated event function" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM event invoke requires generated event function" "$ROOT_DIR/src/codegen/llvm_expr.c"
grep -Fq "LLVM checked unwrap requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"
grep -Fq "LLVM call target '%s' is not declared in the backend function registry" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
! grep -Fq "[llvm] warning: unknown function" "$ROOT_DIR/src/codegen/llvm_expr_call_dispatch.c"
grep -Fq "static const ASTNodeType kLLVMHostDeclTypes[]" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "llvm_host_decl_type_count()" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "kLLVMHostDeclTypes[i]" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "AST_PARTY_DECL" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "AST_ROLE_DECL" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "AST_ROSTER_DECL" "$ROOT_DIR/src/codegen/llvm_inventory_decl_lookup.c"
grep -Fq "ChannelClose" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "pgy_channel_close_%s" "$ROOT_DIR/src/codegen/llvm_expr_task_channel_calls.c"
grep -Fq "llvm_required_runtime_function(ctx, node" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "\"rc\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "\"weak\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c"
grep -Fq "\"device slot\", callee_name, fn_name" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "llvm_slot_inner_has_external_runtime_helpers" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "llvm_emit_structural_secure_slot_write" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "llvm_emit_structural_secure_slot_read" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "llvm_emit_structural_secure_slot_release" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "llvm_require_secure_token_var" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "requires paired token binding" "$ROOT_DIR/src/codegen/llvm_expr_slot_runtime_utils.c"
grep -Fq "LLVM slot auto-read requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_identifier_slot_helpers.c"
grep -Fq "LLVM slot assignment requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "indexed array assignment" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "LLVM indexed array assignment requires concrete Array<T> element metadata" "$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
grep -Fq "\"secure slot\" : \"slot\"" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_write(ctx, slot_var, val)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_read(ctx, slot_var, inner)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
! grep -Fq "llvm_direct_secure_slot_release(ctx, slot_var)" "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c"
grep -Fq "LLVM with-slot cleanup requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "LLVM slot initializer requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt_let_with.c"
grep -Fq "LLVM auto-release requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM secure slot auto-release requires paired token binding" "$ROOT_DIR/src/codegen/llvm_stmt.c"
grep -Fq "LLVM secure with-slot cleanup requires paired token binding" "$ROOT_DIR/src/codegen/llvm_stmt_with.c"
grep -Fq "llvm_select_required_runtime_function" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVMSelectCaseInfo" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_select_case_info" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_select_emit_bound_receive_case" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "llvm_select_emit_ready_consume_case" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "\"LLVM select %s requires registered runtime function '%s'\"" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "ctx, info->channel, \"receive\", fn_name" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "ctx, info->channel, \"readiness\", fn_name" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "ctx, info->channel, \"consume\", recv_name" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "select_write_case_guard" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "select_emit_unbound_consume" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "select_channel_inner_type" "$ROOT_DIR/src/codegen/transpiler_select.c"
! grep -Fq "ast_create_channel_recv(channel)" "$ROOT_DIR/src/codegen/transpiler_select.c"
grep -Fq "llvm_mir_required_channel_ready_function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "llvm_mir_required_channel_ready_function(channel, ctx, fn_name)" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
grep -Fq "sequential fallback is disabled" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "synchronous fallback is disabled" "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c"
grep -Fq "LLVM spawn expression requires registered runtime functions" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "LLVM await expression requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_expr_spawn_call_helpers.c"
grep -Fq "LLVM thread-pool entry requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_pipeline.c"
grep -Fq "LLVM event initialization requires generated event function" "$ROOT_DIR/src/codegen/llvm_pipeline.c"
grep -Fq "LLVM MIR select readiness requires registered runtime function" "$ROOT_DIR/src/codegen/llvm_mir_cfg_control.c"
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
! grep -Fq "block->source_statements" "$ROOT_DIR/src/codegen/transpiler_mir_block_schedule_emit.h"
! grep -Fq "block->source_statement_count" "$ROOT_DIR/src/codegen/transpiler_mir_block_emit.h"
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
