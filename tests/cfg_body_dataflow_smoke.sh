#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"
DOCS_CHECK_DONE=0

require_literal() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || {
        echo "cfg body dataflow fallback check missing term in $rel: $term" >&2
        exit 1
    }
}

run_literal_doc_contract_smoke() {
    local required_files=(
        "docs/103_cfg_body_dataflow_need.md"
        "docs/semantics/08_slot_capability_calculus.md"
        "docs/100_beta_readiness_checklist.md"
        "TODO.md"
        "Makefile"
        "src/semantic/type_checker_flow.c"
        "src/semantic/type_checker_flow_resources.h"
        "src/semantic/type_checker_flow_parallel.h"
        "src/semantic/type_checker_lambda_capture.c"
        "src/compiler/mir_cleanup.c"
        "src/compiler/mir_call_fact.h"
        "src/compiler/mir_call_fact.c"
        "src/compiler/mir_non_cfg_stmt_population.h"
        "src/compiler/mir_non_cfg_stmt_population.c"
        "src/compiler/mir_cleanup_fact_names.h"
        "src/compiler/mir_cfg_contract_cleanup_fact.h"
        "src/compiler/mir_cfg_contract_cleanup_fact.c"
        "src/compiler/mir_cfg_contract_pin.h"
        "src/compiler/mir_cfg_contract_pin.c"
        "src/compiler/mir_cfg_contract_edges.h"
        "src/compiler/mir_cfg_contract_edges.c"
        "src/compiler/mir_cfg_contract_validate.h"
        "src/compiler/mir_cfg_contract_validate.c"
        "src/compiler/mir_cfg_contract_control.c"
        "src/compiler/mir_ssa_rename.c"
        "src/test_mir.c"
        "src/semantic/type_checker_ownership_let.c"
        "src/codegen/llvm_mir_emit.c"
        "src/codegen/llvm_mir_contract.c"
    )

    for rel in "${required_files[@]}"; do
        [[ -f "$ROOT_DIR/$rel" ]] || {
            echo "missing required cfg/body dataflow contract input: $rel" >&2
            exit 1
        }
    done

    require_literal "docs/103_cfg_body_dataflow_need.md" "HIR has function CFG v0"
    require_literal "docs/103_cfg_body_dataflow_need.md" "All-path return"
    require_literal "docs/103_cfg_body_dataflow_need.md" "Definite assignment"
    require_literal "docs/103_cfg_body_dataflow_need.md" "Move/use-after-move"
    require_literal "docs/103_cfg_body_dataflow_need.md" "Drop/cleanup"
    require_literal "src/compiler/mir_cleanup_fact_names.h" "MIR_CLEANUP_FACT_EDGE"
    require_literal "src/compiler/mir_cleanup_fact_names.h" "cleanup-edge"
    require_literal "src/compiler/mir_cleanup_fact_names.h" "MIR_CLEANUP_FACT_PIN_UNPIN_EDGE"
    require_literal "src/compiler/mir_cfg_contract_cleanup_fact.c" "slot_anchor"
    require_literal "src/compiler/mir_cfg_contract_cleanup_fact.c" "arg0"
    require_literal "src/compiler/mir_cfg_contract_pin.c" "MIR_CLEANUP_FACT_PIN_UNPIN_EDGE"
    require_literal "src/compiler/mir_cfg_contract_pin.c" "mir_block_find_pin_cleanup_edge_fact"
    require_literal "src/compiler/mir_cfg_contract_pin.h" "mir_block_pin_cleanup_missing_reason"
    require_literal "src/compiler/mir_cfg_contract_pin.c" "pin cleanup fact does not match source slot, view, and access mode"
    require_literal "src/compiler/mir_cfg_contract_edges.c" "mir_validate_edge_predecessor_link"
    require_literal "src/compiler/mir_cfg_contract_edges.c" "mir_validate_successor_index"
    require_literal "src/compiler/mir_cfg_contract_cleanup_root_membership.c" "mir_cleanup_block_is_registered_root"
    require_literal "src/compiler/mir_cfg_contract_validate.c" "not registered as a cleanup root"
    require_literal "src/compiler/mir_cfg_contract_cleanup_roots.c" "cleanup block %zu is not reachable"
    require_literal "src/compiler/mir_cfg_contract_cleanup_fact.c" "instruction_count > 0 && block->instructions == NULL"
    require_literal "src/compiler/mir_cfg_contract_pin.c" "instruction_count > 0 && block->instructions == NULL"
    require_literal "src/compiler/mir_dce.c" "instruction_count > 0 && block->instructions == NULL"
    require_literal "src/compiler/mir_fact_validate.c" "instruction count without instruction inventory during"
    require_literal "src/compiler/mir_liveness_dce.c" "instruction_count > 0 && block->instructions == NULL"
    require_literal "src/compiler/mir_liveness_summary.c" "instruction_count > 0 && block->instructions == NULL"
    require_literal "src/compiler/mir_validation.c" "instruction count without instruction inventory during use validation"
    require_literal "src/compiler/mir_lifecycle.c" "invalid: instruction count without instruction inventory"
    require_literal "src/compiler/mir_lifecycle.c" "without routine inventory"
    require_literal "src/compiler/mir_lifecycle.c" "without block inventory"
    require_literal "src/compiler/mir_lifecycle.c" "without value-summary inventory"
    require_literal "src/compiler/mir_lifecycle.c" "invalid: routine count without routine inventory"
    require_literal "src/compiler/mir_lifecycle.c" "invalid: block count without block inventory"
    require_literal "src/compiler/mir_lifecycle.c" "invalid: value-summary count without value-summary inventory"
    require_literal "src/compiler/mir_cfg_contract_validate.c" "unreachable block[%zu] has exceptional successor"
    require_literal "src/compiler/mir_cfg_contract_validate.c" "instruction count without instruction inventory"
    require_literal "src/compiler/mir_lifecycle.c" "instructions != NULL"
    require_literal "src/codegen/transpiler_mir_emission_contract.h" "instruction count without instruction inventory"
    require_literal "src/codegen/llvm_mir_block_emit.c" "instruction count without instruction inventory"
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_set_mir_topology_invalid(ctx"
    require_literal "src/codegen/llvm_mir_contract.c" "instruction count without instruction inventory"
    require_literal "src/compiler/mir_cfg_contract_validate.c" "rollback successor"
    require_literal "src/compiler/mir_cfg_contract_validate.c" "invalidation successor"
    require_literal "src/compiler/mir_cleanup.c" "#include \"mir_base_helpers.h\""
    require_literal "src/compiler/mir_cleanup.c" "mir_cleanup_next_capacity"
    require_literal "src/compiler/mir_intent.c" "#include \"mir_base_helpers.h\""
    require_literal "src/compiler/mir_intent.c" "return append_instruction(block, inst)"
    require_literal "src/compiler/mir_liveness_dce.c" "mir_liveness_next_capacity"
    require_literal "src/compiler/mir_liveness_summary.c" "mir_value_summary_next_capacity"
    require_literal "src/compiler/hir_cfg.c" "hir_cfg_next_capacity"
    require_literal "src/compiler/hir_cfg.c" "next_capacity > SIZE_MAX / elem_size"
    require_literal "src/compiler/hir_lower_cfg_blocks.c" "hir_lower_cfg_next_capacity"
    require_literal "src/compiler/hir_lower_cfg_blocks.c" "next_capacity > SIZE_MAX / elem_size"
    require_literal "src/compiler/hir_lower_intent_cfg.c" "#include \"hir_lower_cfg_internal.h\""
    require_literal "src/compiler/hir_analysis.c" "hir_analysis_next_capacity"
    require_literal "src/compiler/hir_analysis.c" "next_capacity > SIZE_MAX / elem_size"
    require_literal "src/compiler/hir_routines.c" "hir_routines_next_capacity"
    require_literal "src/compiler/hir_routines.c" "next_capacity > SIZE_MAX / elem_size"
    require_literal "src/compiler/mir_ssa_use_edges.c" "mir_def_instruction_source_expr"
    require_literal "src/compiler/mir_ssa_use_edges.c" "return inst->expr0"
    require_literal "src/compiler/mir_ssa_use_edges.c" "ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1"
    require_literal "src/compiler/mir_stmt_population.c" "#include \"mir_call_fact.h\""
    require_literal "src/compiler/mir_stmt_population.c" "mir_set_inst_source_statement_index(&new_insts[*new_count - 1]"
    require_literal "src/compiler/mir_non_cfg_stmt_population.c" "routine->hir_routine != NULL && routine->hir_routine->has_cfg"
    require_literal "src/compiler/mir_call_fact.h" "mir_attach_statement_call_fact"
    require_literal "src/compiler/mir_call_fact.c" "inst->arg0 = stmt->data.call.callee->data.identifier.name"
    require_literal "src/compiler/mir_call_fact.h" "mir_attach_def_initializer_call_fact"
    require_literal "src/compiler/mir_call_fact.c" "inst->arg1 = expr->data.call.callee->data.identifier.name"
    require_literal "src/compiler/mir_call_fact.c" "inst->requires_source_statement_emit = true"
    require_literal "src/compiler/mir_call_fact.c" "inst->requires_source_local_decl_emit = true"
    require_literal "src/compiler/mir_call_fact.c" "inst->requires_channel_receive_statement_emit = true"
    require_literal "src/compiler/mir.h" "requires_source_local_decl_emit"
    require_literal "src/compiler/mir.h" "requires_select_receive_statement_emit"
    require_literal "src/compiler/hir.h" "is_select_case_body"
    require_literal "src/compiler/mir.h" "is_select_case_body"
    require_literal "src/compiler/mir_fact_validate.c" "DEF is missing source-statement emit fact"
    require_literal "src/compiler/mir_fact_validate.c" "channel receive DEF is missing source-statement receive emit fact"
    require_literal "src/compiler/mir_fact_validate.c" "select receive DEF is missing select receive emit fact"
    require_literal "src/compiler/mir_fact_terminator_validate.c" "branch is missing source-branch emit fact"
    require_literal "src/compiler/mir_fact_validate.c" "source-statement emit fact is invalid"
    require_literal "src/compiler/mir_fact_validate.c" "source-statement receive emit fact is invalid"
    require_literal "src/compiler/mir_fact_validate.c" "select receive emit fact is invalid"
    require_literal "src/compiler/mir_fact_validate.c" "source-local-decl emit fact is invalid"
    require_literal "src/compiler/mir_fact_validate.c" "source-statement LET emit is missing local-decl fact"
    require_literal "src/compiler/mir_fact_validate.c" "STMT fallback is missing source statement inventory fact"
    require_literal "src/compiler/mir_fact_validate.c" "STMT fallback is outside allowed residual statement policy"
    require_literal "src/codegen/llvm_mir_block_emit.c" "mir_instruction_source_stmt_fallback_is_allowed(inst)"
    require_literal "src/codegen/transpiler_mir_block_emit.h" "mir_instruction_source_stmt_fallback_is_allowed(inst)"
    require_literal "src/compiler/mir_fact_validate.c" "with-slot Claim resource op is missing MIR ABI type layout fact"
    require_literal "src/compiler/mir_fact_validate.c" "with-slot Claim resource op has invalid MIR ABI type layout fact"
    require_literal "src/compiler/mir_fact_validate.c" "mir_instruction_source_is_local_decl(inst)"
    require_literal "src/compiler/mir_fact_validate.c" "mir_instruction_source_is_assignment(inst)"
    require_literal "src/compiler/mir_fact_validate.c" "mir_instruction_source_is_defer_stmt(inst)"
    require_literal "src/compiler/mir_fact_validate.c" "mir_instruction_source_is_with_slot_claim(inst)"
    require_literal "src/compiler/mir_intent_fact.c" "mir_instruction_source_is_intent_step(inst)"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects invalid source-statement emit fact"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects missing channel receive emit fact"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects invalid select receive emit fact"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects invalid with-slot claim ABI fact"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects invalid source-local-decl emit fact"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects residual STMT without source inventory fact"
    require_literal "src/compiler/mir_lifecycle.c" "source-local-decl-emit"
    require_literal "src/compiler/mir_lifecycle.c" "select-recv-stmt-emit"
    require_literal "src/compiler/mir_fact_terminator_validate.c" "source-branch emit fact is invalid"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_branch_requires_source_emit"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_has_source_payload"
    require_literal "src/compiler/mir_source_shape.c" "mir_block_has_hir_source_mapping"
    require_literal "src/compiler/mir_source_shape.c" "mir_block_has_source_location"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_source_is_intent_step"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_source_is_cfg_owned_control"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_source_stmt_has_side_effect_hint"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_source_stmt_fallback_is_allowed"
    require_literal "src/compiler/mir_source_shape.c" "k_pure_query_builtins"
    require_literal "src/compiler/mir_source_shape.c" "bsearch(&callee"
    require_literal "src/compiler/mir_cfg_contract_validate.c" "mir_instruction_source_is_cfg_owned_control(inst)"
    require_literal "src/compiler/mir_dce.c" "mir_instruction_source_stmt_has_side_effect_hint(inst)"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_source_branch_payload_matches_shape"
    require_literal "src/tests/mir/test_mir_lowering_part_c.cases.h" "MIR validator rejects source-compatible branch without payload"
    require_literal "src/compiler/mir_lifecycle.c" "source-branch-emit"
    require_literal "src/codegen/llvm_mir_block_emit.c" "mir_instruction_branch_requires_source_emit(inst)"
    require_literal "src/codegen/transpiler_mir_emission_contract.h" "mir_instruction_branch_requires_source_emit(inst)"
    require_literal "src/compiler/air_evidence.c" "mir_block_has_hir_source_mapping(block)"
    require_literal "src/codegen/transpiler_mir_ssa_map.c" "mir_block_source_hir_id(block)"
    require_literal "src/codegen/transpiler_mir_ssa_map.c" "mir_block_source_line(block)"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_uses_source_statement_emit"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_uses_source_local_decl_emit"
    require_literal "src/codegen/llvm_mir_block_emit.c" "mir_instruction_uses_source_statement_emit(inst)"
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_mir_def_uses_source_local_decl_emit"
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_mir_def_uses_channel_receive_statement_emit"
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_mir_def_uses_select_receive_statement_emit"
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_mir_emit_channel_receive_def(inst, ctx"
    require_literal "src/codegen/llvm_mir_cfg_control.c" "llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx)"
    require_literal "src/codegen/llvm_mir_cfg_control.c" "LLVM channel receive DEF requires registered runtime function"
    require_literal "src/codegen/llvm_mir_resource_claim.c" "llvm_mir_claim_inner_type_name(inst"
    require_literal "src/compiler/mir_source_shape.c" "mir_instruction_source_is_with_slot_claim"
    require_literal "src/codegen/llvm_mir_block_emit.c" "mir_instruction_is_with_slot_claim(inst)"
    require_literal "src/codegen/transpiler_mir_resource_op_emit.h" "mir_instruction_is_with_slot_claim(inst)"
    require_literal "src/codegen/llvm_mir_resource_claim.c" "inst->type_layout->abi_type_name"
    require_literal "Makefile" '$(CODEGEN_DIR)/llvm_mir_resource_claim.c'
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_mir_emit_borrow_view_alias(inst, ctx)"
    require_literal "Makefile" '$(CODEGEN_DIR)/llvm_mir_resource_view.c'
    require_literal "src/codegen/llvm_mir_block_emit.c" "llvm_emit_statement(source_payload, ctx)"
    require_literal "src/codegen/llvm_mir_block_emit.c" "LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0)"
    require_literal "tests/llvm_smoke.sh" "select_fairness"
    require_literal "tests/llvm_smoke.sh" "case v = <-a:"
    require_literal "tests/llvm_smoke.sh" "case v = <-b:"
    require_literal "src/codegen/transpiler_mir_pending_uses.h" "!mir_instruction_uses_source_local_decl_emit(inst)"
    require_literal "src/codegen/transpiler_mir_block_emit_helpers.h" "transpiler_mir_def_uses_source_statement_emit"
    require_literal "src/codegen/transpiler_mir_block_emit_helpers.h" "transpiler_mir_def_uses_source_local_decl_emit"
    require_literal "src/codegen/transpiler_mir_block_emit_helpers.h" "transpiler_mir_def_uses_channel_receive_statement_emit"
    require_literal "src/codegen/transpiler_mir_block_emit_helpers.h" "transpiler_mir_def_uses_select_receive_statement_emit"
    require_literal "src/codegen/transpiler_mir_block_emit_helpers.h" "transpiler_mir_find_stmt_for_inst(const MIRInstruction *inst)"
    require_literal "src/codegen/transpiler_mir_block_emit_helpers.h" "return mir_instruction_source_payload(inst)"
    require_literal "src/compiler/mir_stmt_population.c" "mir_instruction_source_matches_ast_node(inst, stmt)"
    require_literal "src/codegen/transpiler_mir_assignment_emit.h" "transpiler_mir_def_uses_source_statement_emit("
    require_literal "src/codegen/transpiler_mir_assignment_emit.h" "missing receive emit fact"
    require_literal "src/codegen/transpiler_mir_assignment_emit.h" "missing select receive emit fact"
    if grep -Fq "transpiler_find_let_decl_by_name(func_decl, inst->arg0)" \
        "$ROOT_DIR/src/codegen/transpiler_mir_block_emit_helpers.h"; then
        echo "C MIR block emission helper reintroduced function-body let lookup fallback" >&2
        exit 1
    fi
    if grep -R "transpiler_find_let_decl_by_name" "$ROOT_DIR/src/codegen" >/dev/null; then
        echo "C MIR emission reintroduced name-based function-body let lookup" >&2
        exit 1
    fi
    if grep -RIn -- 'inst->ast\|resource_inst->ast' "$ROOT_DIR/src/codegen" >/dev/null; then
        echo "Backend MIR emission reopened raw instruction AST payload; use mir_instruction_source_payload(...)" >&2
        grep -RIn -- 'inst->ast\|resource_inst->ast' "$ROOT_DIR/src/codegen" >&2
        exit 1
    fi
    local raw_payload_hits
    raw_payload_hits="$(grep -RIn -- 'inst->ast\|resource_inst->ast' "$ROOT_DIR/src/compiler" \
        | grep -v 'src/compiler/mir.c:' \
        | grep -v 'src/compiler/mir_source_shape.c:' \
        | grep -v 'src/compiler/mir_non_cfg_stmt_population.c:' \
        || true)"
    if [ -n "$raw_payload_hits" ]; then
        echo "Compiler MIR consumers reopened raw instruction AST payload; use mir_instruction_source_payload(...)" >&2
        printf '%s\n' "$raw_payload_hits" >&2
        exit 1
    fi
    require_literal "src/semantic/type_checker_flow.c" "type_check_while_loop_flow(node, ctx)"
    require_literal "src/semantic/type_checker_flow.c" "type_check_for_loop_flow(node, ctx)"
    require_literal "src/semantic/type_checker_flow.c" "type_check_loop_control_flow(node, ctx, loop_flow, true)"
    require_literal "src/semantic/type_checker_flow_loop_control.c" "flow_validate_loop_control"
    require_literal "src/semantic/type_checker_flow_loop_control.c" "loop_flow_record(loop_flow, is_break"
    require_literal "src/semantic/type_checker_lambda_capture.c" "PGY_CODE_SEM_BORROW_ESCAPE"
    require_literal "src/semantic/type_checker_lambda_capture.c" "capture_state_has_local"
    require_literal "src/semantic/type_checker_lambda_capture.c" "beta lambdas lower to standalone callable bodies without a closure environment"
    require_literal "src/semantic/type_checker_flow_loops.c" "type_check_while_loop_flow(ASTNode *node, SemanticContext *ctx)"
    require_literal "src/semantic/type_checker_flow_loops.c" "type_check_for_loop_flow(ASTNode *node, SemanticContext *ctx)"
    require_literal "src/semantic/type_checker_flow_loops.c" "condition_static_false"
    require_literal "src/semantic/type_checker_flow.c" "restore_resource_states(&base)"
    require_literal "src/semantic/type_checker_flow_loops.c" "restore_resource_states(&merged)"
    require_literal "src/semantic/type_checker_flow_parallel.h" "restore_resource_states(&base)"
    require_literal "src/tests/semantic/test_semantic_misc_a_part_a.cases.h" "CFG body flow accepts while-true all-path return"
    require_literal "src/tests/semantic/test_semantic_misc_a_part_a.cases.h" "CFG body flow accepts static single-iteration for all-path return"
    require_literal "src/tests/semantic/test_semantic_misc_a_part_a.cases.h" "CFG body flow keeps zero-iteration for as fallthrough"
    require_literal "src/tests/semantic/test_semantic_misc_a_part_a.cases.h" "CFG static false while does not merge unreachable resource state"
    require_literal "src/codegen/transpiler_mir_emission_contract.h" "mir_block_has_expected_cleanup_edge_fact(routine, i)"
    require_literal "src/codegen/transpiler_mir_emission_contract.h" "mir_block_has_pin_cleanup_edge(block)"
    require_literal "src/compiler/mir_fact_validate.c" "mir_validate_routine_emission_facts"
    require_literal "src/codegen/transpiler_mir_emission_contract.h" "mir_validate_routine_emission_facts(routine"
    require_literal "src/codegen/llvm_mir_contract.c" "llvm_mir_validate_cleanup_contract"
    require_literal "src/codegen/llvm_mir_contract.c" "mir_validate_emission_topology(routine"
    require_literal "src/codegen/llvm_mir_contract.c" "mir_validate_routine_emission_facts(routine"
    require_literal "src/codegen/llvm_mir_contract.c" "mir_block_has_expected_cleanup_edge_fact(routine, i)"
    require_literal "src/codegen/llvm_mir_contract.c" "mir_block_has_pin_cleanup_edge(block)"
    require_literal "src/compiler/air_evidence.c" "mir_block_has_expected_cleanup_edge_fact(routine, i)"
    require_literal "src/compiler/air_evidence.c" "mir_block_find_pin_cleanup_edge_fact(block)"
    require_literal "src/tests/mir/test_mir_lowering_part_e.cases.h" "pin-unpin-cleanup-edge"
    require_literal "src/tests/mir/test_mir_lowering_part_e.cases.h" "MIR validator rejects unreachable cleanup root"
    require_literal "src/tests/mir/test_mir_lowering_part_e.cases.h" "MIR validator rejects unreachable exceptional source"
    require_literal "src/semantic/type_checker_ownership_let.c" "function-body lets must be initialized at the binding site"
    require_literal "Makefile" "cfg-body-dataflow-test-smoke"

    echo "cfg body dataflow docs: ok (literal fallback)"
}

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        run_literal_doc_contract_smoke
        DOCS_CHECK_DONE=1
    fi
fi

if [[ "$DOCS_CHECK_DONE" -eq 0 ]]; then
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
doc_path = root / "docs" / "103_cfg_body_dataflow_need.md"
slot_proof_path = root / "docs" / "semantics" / "08_slot_capability_calculus.md"
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
todo_path = root / "TODO.md"
makefile_path = root / "Makefile"
board_path = root / "docs" / "70_beta_closure_master_board.md"
report_path = root / "docs" / "98_beta_closure_readiness_report.md"
flow_path = root / "src" / "semantic" / "type_checker_flow.c"
flow_effects_path = root / "src" / "semantic" / "type_checker_flow_effects.c"
flow_match_path = root / "src" / "semantic" / "type_checker_flow_match.c"
flow_resources_header_path = root / "src" / "semantic" / "type_checker_flow_resources.h"
flow_resources_path = root / "src" / "semantic" / "type_checker_flow_resources.c"
flow_loop_control_path = root / "src" / "semantic" / "type_checker_flow_loop_control.c"
flow_loops_header_path = root / "src" / "semantic" / "type_checker_flow_loops.h"
flow_loops_path = root / "src" / "semantic" / "type_checker_flow_loops.c"
flow_parallel_path = root / "src" / "semantic" / "type_checker_flow_parallel.h"
mir_cleanup_path = root / "src" / "compiler" / "mir_cleanup.c"
mir_intent_path = root / "src" / "compiler" / "mir_intent.c"
mir_cleanup_fact_names_path = root / "src" / "compiler" / "mir_cleanup_fact_names.h"
mir_call_fact_path = root / "src" / "compiler" / "mir_call_fact.h"
mir_call_fact_impl_path = root / "src" / "compiler" / "mir_call_fact.c"
mir_cfg_contract_cleanup_fact_path = root / "src" / "compiler" / "mir_cfg_contract_cleanup_fact.h"
mir_cfg_contract_cleanup_fact_impl_path = root / "src" / "compiler" / "mir_cfg_contract_cleanup_fact.c"
mir_cfg_contract_pin_path = root / "src" / "compiler" / "mir_cfg_contract_pin.h"
mir_cfg_contract_pin_impl_path = root / "src" / "compiler" / "mir_cfg_contract_pin.c"
mir_cfg_contract_edges_path = root / "src" / "compiler" / "mir_cfg_contract_edges.h"
mir_cfg_contract_edges_impl_path = root / "src" / "compiler" / "mir_cfg_contract_edges.c"
mir_cfg_contract_control_path = root / "src" / "compiler" / "mir_cfg_contract_control.h"
mir_cfg_contract_control_impl_path = root / "src" / "compiler" / "mir_cfg_contract_control.c"
mir_cfg_contract_validate_path = root / "src" / "compiler" / "mir_cfg_contract_validate.h"
mir_cfg_contract_validate_impl_path = root / "src" / "compiler" / "mir_cfg_contract_validate.c"
mir_path = root / "src" / "compiler" / "mir.c"
mir_ssa_rename_path = root / "src" / "compiler" / "mir_ssa_rename.h"
mir_ssa_rename_impl_path = root / "src" / "compiler" / "mir_ssa_rename.c"
mir_ssa_use_edges_path = root / "src" / "compiler" / "mir_ssa_use_edges.c"
mir_liveness_dce_header_path = root / "src" / "compiler" / "mir_liveness_dce.h"
mir_liveness_dce_path = root / "src" / "compiler" / "mir_liveness_dce.c"
mir_liveness_summary_path = root / "src" / "compiler" / "mir_liveness_summary.c"
mir_dce_header_path = root / "src" / "compiler" / "mir_dce.h"
mir_dce_path = root / "src" / "compiler" / "mir_dce.c"
mir_stmt_population_header_path = root / "src" / "compiler" / "mir_stmt_population.h"
mir_stmt_population_path = root / "src" / "compiler" / "mir_stmt_population.c"
mir_stmt_source_path = root / "src" / "compiler" / "mir_stmt_source.c"
mir_source_shape_path = root / "src" / "compiler" / "mir_source_shape.c"
mir_non_cfg_stmt_population_path = root / "src" / "compiler" / "mir_non_cfg_stmt_population.h"
mir_non_cfg_stmt_population_impl_path = root / "src" / "compiler" / "mir_non_cfg_stmt_population.c"
hir_lower_cfg_path = root / "src" / "compiler" / "hir_lower_cfg.c"
hir_lower_cfg_blocks_path = root / "src" / "compiler" / "hir_lower_cfg_blocks.c"
hir_lower_intent_cfg_path = root / "src" / "compiler" / "hir_lower_intent_cfg.c"
hir_analysis_path = root / "src" / "compiler" / "hir_analysis.c"
hir_routines_path = root / "src" / "compiler" / "hir_routines.c"
mir_c_control_emit_path = root / "src" / "codegen" / "transpiler_mir_cfg_control_emit.h"
mir_llvm_emit_path = root / "src" / "codegen" / "llvm_mir_emit.c"
mir_llvm_contract_path = root / "src" / "codegen" / "llvm_mir_contract.c"
mir_llvm_control_emit_path = root / "src" / "codegen" / "llvm_mir_cfg_control.c"
mir_llvm_loop_control_path = root / "src" / "codegen" / "llvm_mir_loop_control.c"
mir_llvm_block_emit_path = root / "src" / "codegen" / "llvm_mir_block_emit.h"
mir_llvm_for_in_control_path = root / "src" / "codegen" / "llvm_mir_for_in_control.c"
mir_llvm_internal_api_path = root / "src" / "codegen" / "llvm_internal_api.h"
mir_tests_path = root / "src" / "test_mir.c"
mir_test_case_paths = [
    root / "src" / "tests" / "mir" / "test_mir_lowering_part_a.cases.h",
    root / "src" / "tests" / "mir" / "test_mir_lowering_part_b.cases.h",
    root / "src" / "tests" / "mir" / "test_mir_lowering_part_c.cases.h",
    root / "src" / "tests" / "mir" / "test_mir_lowering_part_d.cases.h",
    root / "src" / "tests" / "mir" / "test_mir_lowering_part_e.cases.h",
]
async_channel_path = root / "src" / "semantic" / "type_checker_async_channel.h"
helpers_effects_path = root / "src" / "semantic" / "type_checker_helpers_effects.c"
builtins_query_channel_path = root / "src" / "semantic" / "type_checker_builtins_query_channel.c"
builtins_cancel_path = root / "src" / "semantic" / "type_checker_builtins_cancel.c"
type_system_path = root / "src" / "semantic" / "type_system.h"
type_system_impl_path = root / "src" / "semantic" / "type_system.c"
expr_path = root / "src" / "semantic" / "type_checker_expr.c"
program_path = root / "src" / "semantic" / "type_checker_func_decl.c"
diag_path = root / "src" / "semantic" / "diag_codes.h"
diag_doc_path = root / "docs" / "72_diagnostic_codes.md"
parser_path = root / "src" / "parser" / "parser.c"
let_path = root / "src" / "semantic" / "type_checker_ownership_let.c"
semantic_tests_part_a_path = root / "src" / "tests" / "semantic" / "test_semantic_misc_a_part_a.cases.h"
semantic_tests_part_a2_path = root / "src" / "tests" / "semantic" / "test_semantic_misc_a_part_a2.cases.h"
semantic_tests_part_b_path = root / "src" / "tests" / "semantic" / "test_semantic_misc_a_part_b.cases.h"
semantic_async_tests_part_a_path = root / "src" / "tests" / "semantic" / "test_semantic_async_part_a.cases.h"
semantic_async_tests_part_b_path = root / "src" / "tests" / "semantic" / "test_semantic_async_part_b.cases.h"
semantic_effect_tests_part_a_path = root / "src" / "tests" / "semantic" / "test_semantic_effects_part_a.cases.h"
semantic_effect_tests_part_b_path = root / "src" / "tests" / "semantic" / "test_semantic_effects_part_b.cases.h"
semantic_parallel_context_tests_path = root / "src" / "tests" / "semantic" / "test_semantic_parallel_context.cases.h"

for path in (
    doc_path,
    slot_proof_path,
    checklist_path,
    todo_path,
    makefile_path,
    board_path,
    report_path,
    flow_path,
    flow_effects_path,
    flow_match_path,
    flow_resources_header_path,
    flow_resources_path,
    flow_loop_control_path,
    flow_loops_header_path,
    flow_loops_path,
    flow_parallel_path,
    mir_cleanup_path,
    mir_intent_path,
    mir_cleanup_fact_names_path,
    mir_call_fact_path,
    mir_call_fact_impl_path,
    mir_cfg_contract_cleanup_fact_path,
    mir_cfg_contract_cleanup_fact_impl_path,
    mir_cfg_contract_pin_path,
    mir_cfg_contract_pin_impl_path,
    mir_cfg_contract_edges_path,
    mir_cfg_contract_edges_impl_path,
    mir_cfg_contract_control_path,
    mir_cfg_contract_control_impl_path,
    mir_cfg_contract_validate_path,
    mir_cfg_contract_validate_impl_path,
    mir_path,
    mir_ssa_rename_path,
    mir_ssa_rename_impl_path,
    mir_ssa_use_edges_path,
    mir_liveness_dce_header_path,
    mir_liveness_dce_path,
    mir_liveness_summary_path,
    mir_dce_header_path,
    mir_dce_path,
    mir_stmt_population_header_path,
    mir_stmt_population_path,
    mir_stmt_source_path,
    mir_source_shape_path,
    mir_non_cfg_stmt_population_path,
    mir_non_cfg_stmt_population_impl_path,
    hir_lower_cfg_path,
    hir_lower_cfg_blocks_path,
    hir_lower_intent_cfg_path,
    hir_analysis_path,
    hir_routines_path,
    mir_c_control_emit_path,
    mir_llvm_emit_path,
    mir_llvm_contract_path,
    mir_llvm_control_emit_path,
    mir_llvm_loop_control_path,
    mir_llvm_block_emit_path,
    mir_llvm_for_in_control_path,
    mir_llvm_internal_api_path,
    mir_tests_path,
    *mir_test_case_paths,
    async_channel_path,
    helpers_effects_path,
    builtins_query_channel_path,
    builtins_cancel_path,
    type_system_path,
    type_system_impl_path,
    expr_path,
    program_path,
    diag_path,
    diag_doc_path,
    parser_path,
    let_path,
    semantic_tests_part_a_path,
    semantic_tests_part_a2_path,
    semantic_tests_part_b_path,
    semantic_async_tests_part_a_path,
    semantic_async_tests_part_b_path,
    semantic_effect_tests_part_a_path,
    semantic_effect_tests_part_b_path,
    semantic_parallel_context_tests_path,
):
    if not path.exists():
        raise SystemExit(f"missing required cfg/body dataflow document: {path.relative_to(root)}")

doc = doc_path.read_text(encoding="utf-8")
slot_proof = slot_proof_path.read_text(encoding="utf-8")
checklist = checklist_path.read_text(encoding="utf-8")
todo = todo_path.read_text(encoding="utf-8")
makefile = makefile_path.read_text(encoding="utf-8")
board = board_path.read_text(encoding="utf-8")
report = report_path.read_text(encoding="utf-8")
flow = (
    flow_path.read_text(encoding="utf-8")
    + "\n"
    + flow_effects_path.read_text(encoding="utf-8")
    + "\n"
    + flow_match_path.read_text(encoding="utf-8")
    + "\n"
    + flow_resources_path.read_text(encoding="utf-8")
    + "\n"
    + flow_loop_control_path.read_text(encoding="utf-8")
    + "\n"
    + flow_loops_path.read_text(encoding="utf-8")
    + "\n"
    + flow_parallel_path.read_text(encoding="utf-8")
    + "\n"
    + async_channel_path.read_text(encoding="utf-8")
    + "\n"
    + helpers_effects_path.read_text(encoding="utf-8")
    + "\n"
    + builtins_query_channel_path.read_text(encoding="utf-8")
    + "\n"
    + builtins_cancel_path.read_text(encoding="utf-8")
    + "\n"
    + type_system_path.read_text(encoding="utf-8")
    + "\n"
    + type_system_impl_path.read_text(encoding="utf-8")
    + "\n"
    + expr_path.read_text(encoding="utf-8")
)
mir_cleanup = mir_cleanup_path.read_text(encoding="utf-8")
mir_intent_text = mir_intent_path.read_text(encoding="utf-8")
mir_cleanup_fact_names = mir_cleanup_fact_names_path.read_text(encoding="utf-8")
mir_call_fact = (
    mir_call_fact_path.read_text(encoding="utf-8")
    + "\n"
    + mir_call_fact_impl_path.read_text(encoding="utf-8")
)
mir_cfg_contract_cleanup_fact = (
    mir_cfg_contract_cleanup_fact_path.read_text(encoding="utf-8")
    + "\n"
    + mir_cfg_contract_cleanup_fact_impl_path.read_text(encoding="utf-8")
)
mir_cfg_contract_pin = (
    mir_cfg_contract_pin_path.read_text(encoding="utf-8")
    + "\n"
    + mir_cfg_contract_pin_impl_path.read_text(encoding="utf-8")
)
mir_cfg_contract_edges = (
    mir_cfg_contract_edges_path.read_text(encoding="utf-8")
    + "\n"
    + mir_cfg_contract_edges_impl_path.read_text(encoding="utf-8")
)
mir_cfg_contract_control = (
    mir_cfg_contract_control_path.read_text(encoding="utf-8")
    + "\n"
    + mir_cfg_contract_control_impl_path.read_text(encoding="utf-8")
)
mir_cfg_contract_validate = (
    mir_cfg_contract_validate_path.read_text(encoding="utf-8")
    + "\n"
    + mir_cfg_contract_validate_impl_path.read_text(encoding="utf-8")
)
mir_cfg_contract_validator = (
    mir_cleanup_fact_names
    + "\n"
    + mir_cfg_contract_cleanup_fact
    + "\n"
    + mir_cfg_contract_pin
    + "\n"
    + mir_cfg_contract_edges
    + "\n"
    + mir_cfg_contract_validate
)
mir = mir_path.read_text(encoding="utf-8")
mir_ssa_rename = mir_ssa_rename_path.read_text(encoding="utf-8")
mir_ssa_rename_impl = mir_ssa_rename_impl_path.read_text(encoding="utf-8")
mir_ssa_use_edges = mir_ssa_use_edges_path.read_text(encoding="utf-8")
mir_liveness_dce = mir_liveness_dce_path.read_text(encoding="utf-8")
mir_liveness_summary = mir_liveness_summary_path.read_text(encoding="utf-8")
mir_dce = mir_dce_path.read_text(encoding="utf-8")
mir_stmt_population = mir_stmt_population_path.read_text(encoding="utf-8")
mir_stmt_source = mir_stmt_source_path.read_text(encoding="utf-8")
mir_source_shape = mir_source_shape_path.read_text(encoding="utf-8")
mir_non_cfg_stmt_population = (
    mir_non_cfg_stmt_population_path.read_text(encoding="utf-8")
    + "\n"
    + mir_non_cfg_stmt_population_impl_path.read_text(encoding="utf-8")
)
mir_codegen_control = (
    mir_c_control_emit_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_emit_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_contract_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_control_emit_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_loop_control_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_block_emit_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_for_in_control_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_internal_api_path.read_text(encoding="utf-8")
)
mir_llvm_block_emit = mir_llvm_block_emit_path.read_text(encoding="utf-8")
mir_tests = "\n".join(
    [mir_tests_path.read_text(encoding="utf-8")]
    + [path.read_text(encoding="utf-8") for path in mir_test_case_paths]
)
program = program_path.read_text(encoding="utf-8")
diag = diag_path.read_text(encoding="utf-8")
diag_doc = diag_doc_path.read_text(encoding="utf-8")
parser = parser_path.read_text(encoding="utf-8")
let_checker = let_path.read_text(encoding="utf-8")
semantic_tests = (
    semantic_tests_part_a_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_tests_part_a2_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_tests_part_b_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_async_tests_part_a_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_async_tests_part_b_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_effect_tests_part_a_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_effect_tests_part_b_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_parallel_context_tests_path.read_text(encoding="utf-8")
)

for term in [
    '#include "mir_base_helpers.h"',
    "mir_cleanup_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
]:
    if term not in mir_cleanup:
        raise SystemExit(f"MIR cleanup capacity/base-helper contract missing {term}")
for term in [
    '#include "mir_base_helpers.h"',
    "return append_instruction(block, inst)",
]:
    if term not in mir_intent_text:
        raise SystemExit(f"MIR intent append helper contract missing {term}")
for term in [
    "mir_liveness_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
]:
    if term not in mir_liveness_dce:
        raise SystemExit(f"MIR liveness DCE capacity guard missing {term}")
for term in [
    "mir_value_summary_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
]:
    if term not in mir_liveness_summary:
        raise SystemExit(f"MIR value-summary capacity guard missing {term}")

required_doc_terms = [
    "HIR has function CFG v0",
    "RIR carries flow-block summaries",
    "MIR has routine/block/instruction/cleanup blocks",
    "MIR cleanup consumes RIR flow/fact/semantic summaries",
    "All-path return",
    "Definite assignment",
    "Move/use-after-move",
    "Borrow lifetime",
    "Drop/cleanup",
    "Zone/effect transition",
    "Parallel/channel boundary",
    "Interprocedural summaries",
    "Diagnostics Contract",
    "Implementation Skeleton",
    "Completion Criteria",
    "PGY_SEM_UNINIT_LOCAL",
    "Slot Borrow-Safety Bridge Facts",
    "NoEscape(view, region)",
    "NoSuspend(view, region)",
    "WriteExclusive(slot, region)",
    "DropOnce(owner, all_cfg_exits)",
    "ReleaseAfterUnpin(slot, all_cfg_exits)",
    "NoUnsupportedTokenTransport(token, boundary)",
    "PGY_SEM_PIN_ESCAPE",
    "PGY_SEM_PIN_AWAIT_BOUNDARY",
    "PGY_SEM_PIN_PARALLEL_CONFLICT",
]

missing = [term for term in required_doc_terms if term not in doc]
if missing:
    raise SystemExit("cfg body dataflow doc missing terms: " + ", ".join(missing))

if "docs/103_cfg_body_dataflow_need.md" not in checklist:
    raise SystemExit("beta readiness checklist must reference CFG body dataflow source doc")
if "docs/103_cfg_body_dataflow_need.md" not in todo:
    raise SystemExit("TODO must reference CFG body dataflow source doc")
if "Function CFG / body dataflow" not in board:
    raise SystemExit("master board must track Function CFG / body dataflow")
if "Function CFG And Body Dataflow Source Of Truth" not in report:
    raise SystemExit("readiness report must track CFG/body source-of-truth blocker")
if "make cfg-body-dataflow-test-smoke" not in checklist:
    raise SystemExit("checklist must include cfg-body-dataflow-test-smoke")
if "Bridge Obligation: Borrow-Checker-Equivalent Safety" not in slot_proof:
    raise SystemExit("slot proof pack must name the borrow-checker-equivalent bridge")
for term in [
    "NoEscape(view, region)",
    "NoSuspend(view, region)",
    "WriteExclusive(slot, region)",
    "DropOnce(owner, all_cfg_exits)",
    "ReleaseAfterUnpin(slot, all_cfg_exits)",
    "NoUnsupportedTokenTransport(token, boundary)",
]:
    if term not in slot_proof:
        raise SystemExit(f"slot proof pack missing CFG bridge fact {term}")

required_mir_cleanup_terms = [
    "mir_rir_scope_requires_rollback",
    "mir_rir_scope_requires_invalidation",
    "RIR_FLOW_INVALIDATION",
    "RIR_FLOW_WORLD_HANDOFF",
    "RIR_FLOW_PROJECTION_INVALIDATION",
    "rir_scope->flow_blocks",
    "RIR_STATE_HANDOFF_PENDING",
    "RIR_STATE_AUTHORITY_LOST",
    "RIR_RESOURCE_WORLD_HANDLE",
]
missing_mir_cleanup = [term for term in required_mir_cleanup_terms if term not in mir_cleanup]
if missing_mir_cleanup:
    raise SystemExit(
        "MIR cleanup must consume RIR flow/fact summaries: "
        + ", ".join(missing_mir_cleanup)
    )
for forbidden in [
    "mir_intent_ast_needs_invalidation",
    "data.intent_step.using_expr",
    "data.intent_step.transfer_from_alias",
    "data.intent_step.transfer_to_alias",
]:
    if forbidden in mir_cleanup:
        raise SystemExit(
            "MIR cleanup reintroduced AST-carried intent invalidation fallback: "
            + forbidden
        )

required_mir_cleanup_validator_terms = [
    "mir_validate_edge_predecessor_link",
    "mir_validate_successor_index",
    "mir_block_has_pin_cleanup_edge",
    "mir_block_pin_cleanup_missing_reason",
    "mir_instruction_source_is_cfg_owned_control",
    "pin-unpin-cleanup-edge",
    "slot_anchor",
    "arg0",
    "incomplete loop-init fact",
    "incomplete loop-branch fact",
    "pin-region block[%zu] missing pin-unpin cleanup fact",
    "pin cleanup fact does not match source slot, view, and access mode",
    "rollback block missing cleanup-edge MIR fact",
    "invalidation block missing cleanup-edge MIR fact",
    "rollback successor",
    "invalidation successor",
    "unreachable block[%zu] has exceptional successor",
    "cleanup block[%zu] must not have normal CFG successors",
    "cleanup block[%zu] must not be a pin region",
    "mir_cleanup_block_is_registered_root",
    "not registered as a cleanup root",
    "CFG-owned control statement as fallback STMT",
]
missing_mir_cleanup_validator = [
    term for term in required_mir_cleanup_validator_terms
    if term not in mir_cfg_contract_validator
]
if missing_mir_cleanup_validator:
    raise SystemExit(
        "MIR cleanup validator must reject pin regions without unpin facts: "
        + ", ".join(missing_mir_cleanup_validator)
    )

if "AST_PARALLEL_BLOCK" in mir_cfg_contract_control:
    raise SystemExit(
        "parallel blocks are not CFG-owned until HIR/MIR has real parallel CFG lowering"
    )
if "AST_PARALLEL_BLOCK" not in mir_source_shape:
    raise SystemExit(
        "MIR source-shape owner must preserve parallel blocks as side-effecting statements"
    )

required_mir_codegen_control_terms = [
    "transpiler_mir_emit_for_loop_init_inst",
    "transpiler_mir_render_for_loop_condition_inst",
    "transpiler_mir_emit_for_in_body_binding",
    "_pgy_idx_%s",
    "transpiler_mir_find_loop_branch_inst",
	    "llvm_mir_emit_for_loop_init(const MIRInstruction *inst",
	    "llvm_mir_emit_for_loop_condition(const MIRInstruction *inst",
	    "llvm_mir_emit_for_in_body_binding",
	    "AST_IF_STMT",
	    "AST_WHILE_LOOP",
	    "AST_RETURN",
	    "AST_BREAK",
	    "AST_CONTINUE",
	    "__pgy_idx_%s",
	    "pgy_list_size_raw_export",
	    "pgy_list_get_raw_export",
    "llvm_mir_find_loop_branch_inst",
]
missing_mir_codegen_control = [
    term for term in required_mir_codegen_control_terms
    if term not in mir_codegen_control
]
if missing_mir_codegen_control:
    raise SystemExit(
        "MIR C/LLVM control emitters must consume MIR loop facts: "
        + ", ".join(missing_mir_codegen_control)
    )
for forbidden in [
    "llvm_mir_emit_for_loop_init_from_ast",
    "target->source_ast",
]:
    if forbidden in mir_codegen_control:
        raise SystemExit(
            "MIR C/LLVM control emitter reintroduced AST fallback: "
            + forbidden
        )

mir_owner_limits = {
    mir_path: 600,
    mir_ssa_rename_path: 600,
    mir_ssa_rename_impl_path: 600,
    mir_ssa_use_edges_path: 600,
    mir_liveness_dce_header_path: 600,
    mir_liveness_dce_path: 600,
    mir_liveness_summary_path: 600,
    mir_dce_header_path: 600,
    mir_dce_path: 600,
    mir_call_fact_path: 600,
    mir_call_fact_impl_path: 600,
    mir_stmt_population_header_path: 600,
    mir_stmt_population_path: 600,
    mir_stmt_source_path: 600,
    mir_non_cfg_stmt_population_path: 600,
    mir_non_cfg_stmt_population_impl_path: 600,
}
for path, limit in mir_owner_limits.items():
    loc = len(path.read_text(encoding="utf-8").splitlines())
    if loc > limit:
        raise SystemExit(
            f"MIR CFG/body owner {path.relative_to(root)} is {loc} LOC; "
            f"split-review limit is {limit}"
        )

required_mir_owner_terms = {
    "src/compiler/mir.c": [
        "#include \"mir_ssa_rename.h\"",
        "#include \"mir_liveness_dce.h\"",
        "#include \"mir_stmt_population.h\"",
        "mir_build_blocks_from_hir",
        "mir_populate_instructions",
    ],
    "src/compiler/mir_ssa_rename.h": [
        "mir_apply_ssa_rename",
        "mir_populate_use_edges",
    ],
    "src/compiler/mir_ssa_rename.c": [
        "mir_collect_ssa_names",
    ],
    "src/compiler/mir_ssa_use_edges.c": [
        "mir_append_versioned_use",
        "mir_append_block_versioned_name",
        "mir_parse_versioned_name_owned",
        "mir_def_instruction_source_expr",
        "return inst->expr0",
        "ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1",
        "mir_populate_use_edges",
    ],
    "src/compiler/mir_liveness_dce.c": [
        "mir_compute_liveness",
    ],
    "src/compiler/mir_liveness_summary.c": [
        "mir_build_value_summaries",
        "inst->kind != MIR_INST_DEF",
        "write_name = mir_liveness_summary_slot_anchor(inst)",
    ],
    "src/compiler/mir_dce.c": [
        "mir_run_dce_on_routine",
        "mir_remove_instruction",
        "mir_reset_routine_analysis",
        "mir_instruction_source_stmt_has_side_effect_hint(inst)",
        "mir_source_ast_stmt_has_side_effect_hint",
    ],
    "src/compiler/mir_call_fact.h": [
        "PERGYRA_MIR_CALL_FACT_H",
        "mir_attach_statement_call_fact",
        "mir_attach_def_initializer_call_fact",
    ],
    "src/compiler/mir_call_fact.c": [
        "#include \"mir_call_fact.h\"",
        "inst->arg0 = stmt->data.call.callee->data.identifier.name",
        "inst->arg1 = expr->data.call.callee->data.identifier.name",
    ],
    "src/compiler/mir_stmt_population.c": [
        "mir_populate_stmt_instructions",
        "MIR_INST_LOOP_INIT",
        "mir_stmt_is_for_loop_init_payload",
        "mir_block == NULL",
        "mir_stmt_population_is_semantic_carrier(&old_insts[r])",
        "Intent metadata is MIR semantic inventory",
        "#include \"mir_cfg_contract_control.h\"",
        "mir_stmt_ast_is_cfg_owned_control(stmt)",
        "#include \"mir_call_fact.h\"",
        "mir_set_inst_source_statement_index(&new_insts[*new_count - 1]",
    ],
    "src/compiler/mir_stmt_source.c": [
        "mir_stmt_def_name",
        "mir_let_decl_requires_stmt_preservation",
        "mir_stmt_is_def_source",
    ],
    "src/compiler/mir_non_cfg_stmt_population.h": [
        "mir_append_non_cfg_body_statements",
    ],
    "src/compiler/mir_non_cfg_stmt_population.c": [
        "routine->hir_routine != NULL && routine->hir_routine->has_cfg",
        "used_non_cfg_body_fallback",
        "non_cfg_body_fallback_count",
        "mir_attach_statement_call_fact",
        "mir_attach_def_initializer_call_fact",
    ],
    "src/compiler/mir_public_surface.h": [
        "mir_validate_non_cfg_fallback_state",
        "used non-CFG body fallback",
        "non_cfg_body_fallback_count",
        "fallback flag without fallback count",
    ],
    "src/compiler/mir_cfg_contract_control.h": [
        "PERGYRA_MIR_CFG_CONTRACT_CONTROL_H",
        "mir_stmt_ast_is_cfg_owned_control",
        "AST_WITH_STMT",
        "AST_UNSAFE_BLOCK",
        "AST_FOR_LOOP",
        "AST_SELECT_STMT",
        "AST_MATCH_STMT",
        "AST_BREAK",
        "AST_CONTINUE",
    ],
}
mir_owner_text = {
    "src/compiler/mir.c": mir,
    "src/compiler/mir_ssa_rename.h": mir_ssa_rename,
    "src/compiler/mir_ssa_rename.c": mir_ssa_rename_impl,
    "src/compiler/mir_ssa_use_edges.c": mir_ssa_use_edges,
    "src/compiler/mir_liveness_dce.c": mir_liveness_dce,
    "src/compiler/mir_liveness_summary.c": mir_liveness_summary,
    "src/compiler/mir_dce.c": mir_dce,
    "src/compiler/mir_call_fact.h": mir_call_fact,
    "src/compiler/mir_call_fact.c": mir_call_fact,
    "src/compiler/mir_stmt_population.c": mir_stmt_population,
    "src/compiler/mir_stmt_source.c": mir_stmt_source,
    "src/compiler/mir_non_cfg_stmt_population.h": mir_non_cfg_stmt_population,
    "src/compiler/mir_non_cfg_stmt_population.c": mir_non_cfg_stmt_population,
    "src/compiler/mir_public_surface.h": (root / "src" / "compiler" / "mir_public_surface.h").read_text(encoding="utf-8"),
    "src/compiler/mir_cfg_contract_control.h": mir_cfg_contract_control,
}
for owner, terms in required_mir_owner_terms.items():
    text = mir_owner_text[owner]
    missing_terms = [term for term in terms if term not in text]
    if missing_terms:
        raise SystemExit(
            "MIR CFG/body owner split lost required terms in "
            + owner
            + ": "
            + ", ".join(missing_terms)
        )

required_flow_terms = [
    "FLOW_FALLTHROUGH",
    "FLOW_RETURN",
    "type_check_if_stmt_flow",
    "type_check_match_stmt_flow",
    "semantic_check_body_flow",
    "flow_record_statement_result",
    "flow_has_fallthrough",
    "flow_terminating_flags",
    "match_stmt_has_total_case_coverage",
    "flow_record_unreachable_statement",
    "loop_flow_record",
    "resource_snapshots_equal",
    "type_check_while_loop",
    "type_check_for_loop",
    "merge_resource_snapshots_or",
    "condition_static_false",
    "restore_resource_states(&base)",
    "type_check_defer_body_flow",
    "flow_reject_dynamic_defer_control",
    "PGY_CODE_SEM_DEFER_DYNAMIC_CONTROL",
    "type_check_parallel_block_flow",
    "flow_snapshot_tracks_symbol",
    "semantic_classify_ownership_type",
    "used_states",
    "PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT",
    "semantic_validate_spawn_ref_boundary",
    "semantic_reject_anonymous_async_spawn",
    "cannot cross spawn boundary",
    "Anonymous async spawn bodies are beta-out-of-scope",
    "type_check_channel_close_builtin",
    "type_check_cancel_rejects_payload",
    "body_summary_mask",
    "type_function_body_summary",
    "semantic_record_callee_body_summary",
    "semantic_record_callable_decl_summary",
    "lambda_body_summary",
    "BODY_SUMMARY_SPAWNS_TASK",
    "BODY_SUMMARY_SENDS_CHANNEL",
    "BODY_SUMMARY_MAY_ESCAPE_REF",
    "ChannelClose does not support",
    "Cancel does not support",
    "cannot yield Token values yet",
    "PGY_CODE_SEM_BORROW_ESCAPE",
    "SlotState",
    "ResourceConsumeSnapshot before_defer",
    "type_check_defer_body_flow(node->data.defer_stmt.body, ctx)",
    "type_check_block_flow(body, ctx, NULL)",
    "restore_resource_states(&before_defer)",
]
missing_flow = [term for term in required_flow_terms if term not in flow]
if missing_flow:
    raise SystemExit(
        "semantic CFG body flow is missing implementation terms: "
        + ", ".join(missing_flow)
    )
if "if (a->used_states[i] != b->used_states[i])" not in flow:
    raise SystemExit(
        "semantic CFG loop fixed-point equality must compare resource used_states"
    )

hir_routines = program_path.parent.parent / "compiler" / "hir_routines.c"
hir_routines_text = hir_routines.read_text(encoding="utf-8")
for term in [
    "hir_validate_cfg_shape",
    "hir_validate_cfg_predecessors",
    "HIR_BLOCK_FALLTHROUGH",
    "hir_cfg_successor_in_range",
    "hir_cfg_block_targets",
    "hir_cfg_predecessors_contain",
    "return_block_count",
    "normal_exit_block_count",
]:
    if term not in hir_routines_text:
        hir_header = root / "src" / "compiler" / "hir.h"
        hir_cfg = root / "src" / "compiler" / "hir_cfg.c"
        hir_cfg_phi = root / "src" / "compiler" / "hir_cfg_phi.c"
        hir_routine_cfg = root / "src" / "compiler" / "hir_routine_cfg.c"
        hir_public = root / "src" / "compiler" / "hir_public.c"
        joined = (
            hir_header.read_text(encoding="utf-8")
            + "\n"
            + hir_cfg.read_text(encoding="utf-8")
            + "\n"
            + hir_cfg_phi.read_text(encoding="utf-8")
            + "\n"
            + hir_routine_cfg.read_text(encoding="utf-8")
            + "\n"
            + hir_public.read_text(encoding="utf-8")
        )
        if term not in joined:
            raise SystemExit(f"HIR CFG validation/summary gate missing {term}")

hir_cfg_path = root / "src" / "compiler" / "hir_cfg.c"
hir_cfg_phi_path = root / "src" / "compiler" / "hir_cfg_phi.c"
hir_cfg_internal_path = root / "src" / "compiler" / "hir_cfg_internal.h"
for path in (
    hir_cfg_path,
    hir_cfg_phi_path,
    hir_cfg_internal_path,
    hir_lower_cfg_path,
    hir_lower_cfg_blocks_path,
    hir_lower_intent_cfg_path,
    hir_analysis_path,
    hir_routines_path,
):
    if not path.exists():
        raise SystemExit(f"missing HIR CFG owner file: {path.relative_to(root)}")

hir_cfg_text = hir_cfg_path.read_text(encoding="utf-8")
hir_cfg_phi_text = hir_cfg_phi_path.read_text(encoding="utf-8")
hir_lower_cfg_text = hir_lower_cfg_path.read_text(encoding="utf-8")
hir_lower_cfg_blocks_text = hir_lower_cfg_blocks_path.read_text(encoding="utf-8")
hir_lower_intent_cfg_text = hir_lower_intent_cfg_path.read_text(encoding="utf-8")
hir_analysis_text = hir_analysis_path.read_text(encoding="utf-8")
hir_routines_text = hir_routines_path.read_text(encoding="utf-8")
for term in [
    "hir_cfg_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
    "hir_compute_cfg_dominance",
    "hir_compute_cfg_dominance_frontier",
    "hir_compute_cfg_dom_tree",
    "hir_compute_cfg_loops",
    "hir_finalize_cfg_summary",
]:
    if term not in hir_cfg_text:
        raise SystemExit(f"HIR CFG structural owner missing {term}")
for term in [
    "hir_collect_cfg_local_defs",
    "hir_compute_cfg_phi_candidates",
    "hir_materialize_phi_nodes",
    "hir_routine_collect_ssa_names",
]:
    if term not in hir_cfg_phi_text:
        raise SystemExit(f"HIR CFG phi owner missing {term}")
for term in [
    "hir_lower_func_body_cfg",
    "hir_lower_intent_cfg",
]:
    if term not in (hir_lower_cfg_text + "\n" + hir_lower_intent_cfg_text):
        raise SystemExit(f"HIR CFG lowerer split missing {term}")
for term in [
    "hir_lower_cfg_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
]:
    if term not in hir_lower_cfg_blocks_text:
        raise SystemExit(f"HIR CFG lowerer capacity guard missing {term}")
if "intent_cfg_new_block" in hir_lower_intent_cfg_text or "intent_cfg_append_stmt" in hir_lower_intent_cfg_text:
    raise SystemExit("HIR intent CFG lowerer reintroduced duplicate block/statement append helpers")
for term in [
    "hir_analysis_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
]:
    if term not in hir_analysis_text:
        raise SystemExit(f"HIR analysis capacity guard missing {term}")
for term in [
    "hir_routines_next_capacity",
    "next_capacity > SIZE_MAX / elem_size",
]:
    if term not in hir_routines_text:
        raise SystemExit(f"HIR routine capacity guard missing {term}")
for term in [
    "intent_cfg_append_step_statements",
    "step->data.intent_step.using_expr",
    "step->data.intent_step.compensate_exprs",
]:
    if term not in hir_lower_intent_cfg_text:
        raise SystemExit(f"HIR intent CFG owner missing {term}")
for term in [
    "$(COMPILER_DIR)/hir_lower_intent_cfg.c",
    "$(BUILD_DIR)/compiler/hir_lower_intent_cfg.o",
]:
    if term not in makefile:
        raise SystemExit(f"Makefile must wire HIR intent CFG owner: {term}")

for term in [
    "semantic_check_body_flow",
    "PGY_CODE_SEM_MISSING_RETURN",
    "PGY_CAUSE_CFG_MISSING_RETURN",
]:
    if term not in program:
        raise SystemExit(f"function declaration checker is not wired to {term}")

for term in [
    "PGY_CODE_SEM_MISSING_RETURN",
    "PGY_CAUSE_CFG_MISSING_RETURN",
    "PGY_FIX_ADD_RETURN_ON_ALL_PATHS",
    "PGY_CODE_SEM_UNREACHABLE_CODE",
    "PGY_CAUSE_CFG_UNREACHABLE_STATEMENT",
    "PGY_FIX_REMOVE_OR_MOVE_BEFORE_TERMINATOR",
]:
    if term not in diag:
        raise SystemExit(f"diagnostic registry is missing {term}")

for term in ["PGY_SEM_MISSING_RETURN", "PGY_SEM_UNREACHABLE_CODE"]:
    if term not in diag_doc:
        raise SystemExit(f"diagnostic docs must document {term}")

for term in [
    "CFG body flow accepts while-true all-path return",
    "CFG body flow accepts static single-iteration for all-path return",
    "CFG body flow keeps zero-iteration for as fallthrough",
    "CFG body flow warns on unreachable statement after return",
    "CFG body flow warns after all if branches terminate",
    "CFG body flow warns after exhaustive match terminates",
    "CFG body flow warns after loop break terminates path",
    "CFG body flow warns after loop continue terminates path",
    "CFG loop move join consumes QubitSlot on break path",
    "CFG loop move join rejects consumed QubitSlot on continue backedge",
    "CFG static false while does not merge unreachable resource state",
    "CFG defer return does not make following statement unreachable",
    "CFG defer return does not satisfy non-Void all-path return",
    "CFG defer QubitSlot release does not consume current path",
    "CFG defer loop break does not consume current path resource state",
    "CFG dynamic branch defer is explicitly rejected",
    "CFG static match defer remains accepted",
    "CFG dynamic match defer is explicitly rejected",
    "CFG dynamic loop defer is explicitly rejected",
    "defer statement fallback restores QubitSlot resource state",
    "CFG slot release in terminating branch does not poison fallthrough path",
    "CFG slot release in fallthrough branch poisons joined path",
    "CFG own subject move in terminating branch does not poison fallthrough path",
    "CFG own subject move in fallthrough branch poisons joined path",
    "CFG parallel task return does not terminate outer path",
    "CFG parallel task move consumes resource after join",
    "CFG parallel task own subject move consumes boundary after join",
    "CFG parallel tasks reject double resource consume",
    "CFG parallel tasks reject double own subject consume",
    "CFG parallel tasks reject ref and own subject boundary conflict",
    "CFG parallel tasks allow shared ref subject boundary reads",
    "CFG spawn rejects borrowed subject boundary crossing",
    "CFG spawn allows copy ref boundary crossing",
    "CFG spawn rejects authority Token boundary crossing",
    "CFG spawn rejects anonymous async body until capture lifetime is closed",
    "CFG parallel channel send consumes resource after join",
    "CFG parallel channel sends reject double resource consume",
    "channel send with active ReadView uses pin boundary diagnostic",
    "channel send rejects authority Token payload",
    "TryRecv rejects movable resource channel payloads",
    "TryRecv with active ReadView uses pin boundary diagnostic",
    "RecvTimeout rejects movable resource channel payloads",
    "TryRecv rejects anchored slot-handle channel payloads",
    "RecvTimeout rejects boundary-value channel payloads",
    "TryRecv rejects authority Token channel payloads",
    "SendTimeout rejects movable resource channel payloads",
    "TrySendStatus rejects authority Token channel payloads",
    "SendTimeoutStatus rejects authority Token channel payloads",
    "Cancel rejects movable resource Future payloads",
    "Cancel with active ReadView uses pin boundary diagnostic",
    "Cancel rejects anchored slot-handle Future payloads",
    "Cancel rejects boundary-value Future payloads",
    "Cancel rejects authority Token Future payloads",
    "ChannelClose(Channel<Int>) returns Void",
    "ChannelClose with active ReadView uses pin boundary diagnostic",
    "ChannelClose rejects movable resource channel payloads",
    "ChannelClose rejects authority Token channel payloads",
    "TrySend with active ReadView uses pin boundary diagnostic",
    "function body summary records param boundary modes",
    "function call propagates callee body summary",
    "direct function call records callable declaration boundary summary",
    "method call records callable declaration body summary",
    "lambda body summary stays on lambda type",
    "lambda body summary does not leak to enclosing function",
    "lambda local capture is rejected until closure environments exist",
    "lambda block local shadow is not treated as capture",
    "lambda call propagates lambda body summary",
    "ReadView return escape uses pin escape diagnostic",
    "await with active ReadView uses pin await diagnostic",
    "spawn with active ReadView uses pin boundary diagnostic",
    "async block with active ReadView uses pin boundary diagnostic",
    "parallel with active ReadView uses pin conflict diagnostic",
    "ViewRead inside parallel task is rejected by pin conflict diagnostic",
    "ViewRead rejects QubitSlot with pin qubit diagnostic",
    "WriteView requires exclusive slot view access",
    "ReadView after WriteView is rejected by exclusive view gate",
    "multiple ReadView bindings are accepted",
]:
    if term not in semantic_tests:
        raise SystemExit(f"semantic regression must cover {term}")

for term in [
    "MIR lowers for-loop init as loop-init instead of fallback statement",
    "MIR lowers for-in init as loop-init instead of fallback statement",
    "MIR validator rejects intent instruction metadata drift",
    "MIR validator rejects CFG-owned control fallback statements",
    "MIR validator rejects terminal CFG-owned control fallback statements",
    "MIR validator rejects non-CFG fallback flag without count",
    "MIR validator rejects residual STMT without source inventory fact",
    "MIR validator rejects missing routine inventory",
    "MIR validator rejects missing block inventory",
    "MIR validator rejects missing value-summary inventory",
    "MIR validator rejects missing rollback and invalidation cleanup facts",
    "MIR validator rejects pin-region without cleanup root",
    "MIR validator rejects orphan cleanup-marked block",
    "MIR keeps pin cleanup fact across early return",
    "MIR keeps pin cleanup fact across branch returns",
    "MIR keeps pin cleanup fact across loop break and continue",
    "MIR carries direct statement call facts",
    "routine_has_stmt_call_fact_named",
    "MIR carries direct initializer call facts",
    "routine_has_def_call_fact_named",
    "source_terminator_kind == HIR_BLOCK_RETURN",
    "source_terminator_kind != HIR_BLOCK_GOTO",
    "routine_has_complete_loop_init_for",
    "routine_has_complete_loop_branch_for",
]:
    if term not in mir_tests:
        raise SystemExit(f"MIR regression must cover {term}")

if 'parser_consume(parser, TOKEN_ASSIGN, "Expected \'=\' in let declaration")' not in parser:
    raise SystemExit("parser must keep local let declarations initialized")

if "source_statement_inventory.items[inst->source_statement_index]" in mir_ssa_use_edges:
    raise SystemExit(
        "MIR DEF use-edge collection must consume instruction-carried AST, "
        "not reopen block source_statement_inventory"
    )

if "inst->has_source_location" in mir_dce:
    raise SystemExit(
        "MIR DCE must consume mir_instruction_has_source_location(...) "
        "instead of reopening raw source-location fields"
    )

if "inst->source_ast_type" in mir_llvm_block_emit:
    raise SystemExit(
        "LLVM MIR block emission must consume MIR source-shape accessors "
        "instead of reopening raw source_ast_type"
    )

if "inst->has_source_location" in mir_llvm_block_emit:
    raise SystemExit(
        "LLVM MIR block emission must consume MIR source-shape accessors "
        "instead of reopening raw source-location fields"
    )

mir_stmt_population_impl = (
    root / "src/compiler/mir_stmt_population.c"
).read_text(encoding="utf-8")
if "inst->source_line" in mir_stmt_population_impl:
    raise SystemExit(
        "MIR statement population must consume source-location match helper "
        "instead of reopening raw source line fields"
    )

if "inst->has_source_location" in mir_stmt_population_impl:
    raise SystemExit(
        "MIR statement population must consume source-location match helper "
        "instead of reopening raw source-location fields"
    )

mir_lifecycle_impl = (
    root / "src/compiler/mir_lifecycle.c"
).read_text(encoding="utf-8")
if "inst->source_ast_type" in mir_lifecycle_impl:
    raise SystemExit(
        "MIR dump/lifecycle must consume source-shape accessors instead of "
        "reopening raw instruction source_ast_type"
    )

if "inst->source_line" in mir_lifecycle_impl:
    raise SystemExit(
        "MIR dump/lifecycle must consume source-shape accessors instead of "
        "reopening raw instruction source_line"
    )

if "block->source_line" in mir_lifecycle_impl:
    raise SystemExit(
        "MIR dump/lifecycle must consume block source accessors instead of "
        "reopening raw block source_line"
    )

raw_source_fields = (
    "->source_ast_type",
    "->has_source_location",
    "->source_line",
    "->source_column",
)
raw_source_allowed = {
    pathlib.Path("src/compiler/mir.c"),
    pathlib.Path("src/compiler/mir_source_shape.c"),
}
raw_source_leaks = []
for rel_root in ("src/compiler", "src/codegen", "src/semantic"):
    for path in (root / rel_root).rglob("*.[ch]"):
        rel = path.relative_to(root)
        if rel in raw_source_allowed:
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        for field in raw_source_fields:
            if re.search(re.escape(field) + r"\b", text):
                raw_source_leaks.append(f"{rel}:{field}")
                break
if raw_source_leaks:
    raise SystemExit(
        "MIR source/location raw fields escaped source-shape owners:\n"
        + "\n".join(raw_source_leaks)
    )

if re.search(
    r"has_source_location\s*&&\s*inst->source_ast_type\s*==\s*AST_WITH_STMT",
    mir_llvm_block_emit,
):
    raise SystemExit(
        "LLVM with-slot claim setup must not require source-location metadata"
    )

if re.search(
    r"for\s*\([^)]*mir_block->instruction_count[^)]*\)\s*\{[^{}]*"
    r"MIR_INST_RESOURCE_OP[^{}]*AST_WITH_STMT",
    mir_llvm_block_emit,
    flags=re.S,
):
    raise SystemExit(
        "LLVM with-slot claim setup must not reintroduce a block-entry prepass"
    )

if re.search(
    r"case\s+MIR_INST_STMT\s*:[\s\S]*?source_ast_type\s*==\s*AST_WITH_STMT",
    mir_llvm_block_emit,
):
    raise SystemExit(
        "LLVM with-slot claim setup must consume resource-op Claim facts, not residual statements"
    )

for term in [
    "PGY_CODE_SEM_UNINIT_LOCAL",
    "PGY_CAUSE_UNINIT_LOCAL",
    "PGY_FIX_INITIALIZE_AT_BINDING",
    "function-body lets must be initialized at the binding site",
]:
    if term not in let_checker:
        raise SystemExit(f"semantic let checker is missing uninit-local guard {term}")

print("cfg body dataflow docs: ok")
PY
fi

if grep -RIn "inst->arg0 != NULL ? inst->arg0 : inst->name" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent.h" \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c"; then
    echo "MIR intent step names must consume validator-owned arg0, not fallback to inst->name" >&2
    exit 1
fi
if grep -RIn "inst->arg0" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c" \
    "$ROOT_DIR/src/codegen/llvm_intent_mir_meta.c"; then
    echo "MIR intent metadata readers must consume payloads through mir_instruction_intent_payload" >&2
    exit 1
fi
require_literal "src/compiler/mir_intent_fact.c" "mir_instruction_intent_step_matches"
require_literal "src/compiler/mir_intent_fact.c" "mir_instruction_intent_phase_matches"
require_literal "src/compiler/mir_intent_fact.c" "mir_instruction_intent_payload"
require_literal "src/compiler/mir_intent_fact.c" "mir_instruction_intent_step_name"
require_literal "src/compiler/mir_intent_fact.c" "mir_validate_intent_instruction_fact"
require_literal "src/compiler/mir_intent_fact.c" "inst == NULL || inst->kind != MIR_INST_STMT"
require_literal "src/codegen/transpiler_mir_inventory_intent_collect.c" "mir_instruction_intent_step_name(inst)"
require_literal "src/codegen/llvm_intent_flow.c" "mir_instruction_intent_step_name(inst)"
require_literal "src/codegen/transpiler_mir_inventory_intent_collect.c" "mir_instruction_intent_phase_matches(inst, phase_name)"
require_literal "src/codegen/llvm_intent_flow.c" "mir_instruction_intent_phase_matches(inst, phase_name)"
require_literal "src/codegen/transpiler_mir_inventory_intent_collect.c" "mir_instruction_intent_payload(inst)"
require_literal "src/codegen/llvm_intent_mir_meta.c" "mir_instruction_intent_payload(inst)"
require_literal "src/codegen/transpiler_mir_intent_query.c" "mir_instruction_intent_payload(inst)"
require_literal "src/codegen/transpiler_mir_intent_query.c" "mir_instruction_intent_step_matches(inst, step_name)"
if grep -RIn "inst->arg1 == NULL || strcmp(inst->arg1, step_name)" \
    "$ROOT_DIR/src/codegen/transpiler_mir_intent_query.c"; then
    echo "C intent helper must use MIR intent step matching API" >&2
    exit 1
fi
if grep -n "source_ast_type != AST_INTENT_STEP" \
    "$ROOT_DIR/src/codegen/llvm_intent_flow.c"; then
    echo "LLVM intent step collection must consume MIR intent-step facts, not source_ast_type filters" >&2
    exit 1
fi
if grep -n "source_ast_type != AST_INTENT_STEP" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"; then
    echo "C intent step collection must consume MIR intent-step facts, not source_ast_type filters" >&2
    exit 1
fi

DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
PGY_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
    PGY_EXPLICIT=1
elif [[ -x "$DEFAULT_PGY" ]]; then
    PGY="$DEFAULT_PGY"
elif [[ -x "$TMP_PGY" ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi

EXAMPLE="${1:-$ROOT_DIR/examples/logistics_intent_probe/main.pgy}"
WORK_BASE="$ROOT_DIR/.tmp/cfg-body-dataflow"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

to_native_path_for_pgy() {
    local path="$1"
    if [[ "$PGY" != *.exe ]]; then
        printf '%s\n' "$path"
        return 0
    fi
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$path"
        return 0
    fi
    if [[ "$path" =~ ^/mnt/([A-Za-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local rest="${BASH_REMATCH[2]//\//\\}"
        printf '%s:\\%s\n' "${drive^^}" "$rest"
        return 0
    fi
    if [[ "$path" =~ ^/([A-Za-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local rest="${BASH_REMATCH[2]//\//\\}"
        printf '%s:\\%s\n' "${drive^^}" "$rest"
        return 0
    fi
    printf '%s\n' "$path"
}

if [[ ! -x "$PGY" ]]; then
    echo "cfg-body-dataflow smoke: SKIP executable probe; source/doc contract already checked"
    exit 0
fi

if [[ ! -f "$EXAMPLE" ]]; then
    echo "missing example source: $EXAMPLE" >&2
    exit 1
fi

if ! "$PGY" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "cfg-body-dataflow smoke: SKIP default compiler executable probe; source/doc contract already checked"
        exit 0
    fi
    echo "cfg body dataflow compiler binary is not runnable: $PGY" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

EXAMPLE_FOR_PGY="$(to_native_path_for_pgy "$EXAMPLE")"

HIR_CFG_OUT="$WORK_DIR/hir_cfg.txt"
HIR_DOM_OUT="$WORK_DIR/hir_dom.txt"
RIR_OUT="$WORK_DIR/rir.txt"
MIR_OUT="$WORK_DIR/mir.txt"
PARALLEL_SELECT="$WORK_DIR/parallel_select.pgy"
PARALLEL_SELECT_AST="$WORK_DIR/parallel_select_ast.txt"
PARALLEL_SELECT_MIR="$WORK_DIR/parallel_select_mir.txt"
WITH_SLOT_ORDER="$WORK_DIR/with_slot_order.pgy"
WITH_SLOT_ORDER_C="$WORK_DIR/with_slot_order.c"

"$PGY" "$EXAMPLE_FOR_PGY" --hir-cfg > "$HIR_CFG_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --hir-dom > "$HIR_DOM_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --rir > "$RIR_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --mir > "$MIR_OUT"

cat > "$PARALLEL_SELECT" <<'EOF'
func Main() -> Void {
    let ch: Channel<Int> = Channel(4);
    parallel {
        ch <- 7;
    }
    select {
        case v = <-ch:
            Log(v);
        default:
            Log(0);
    }
}
EOF
PARALLEL_SELECT_FOR_PGY="$(to_native_path_for_pgy "$PARALLEL_SELECT")"
"$PGY" "$PARALLEL_SELECT_FOR_PGY" --ast > "$PARALLEL_SELECT_AST"
"$PGY" "$PARALLEL_SELECT_FOR_PGY" --mir > "$PARALLEL_SELECT_MIR"

cat > "$WITH_SLOT_ORDER" <<'EOF'
func Cost() -> Int {
    return 4;
}

func Main() -> Void {
    with slot<Int> as s {
        Write(s, Cost());
        Print(ToString(Read(s)));
    }
}
EOF
WITH_SLOT_ORDER_FOR_PGY="$(to_native_path_for_pgy "$WITH_SLOT_ORDER")"
"$PGY" "$WITH_SLOT_ORDER_FOR_PGY" --emit-c -o "$WITH_SLOT_ORDER_C" > "$WORK_DIR/with_slot_order.out"

grep -Fq "HIR cfg view" "$HIR_CFG_OUT"
grep -Fq "function MergeRouteScore" "$HIR_CFG_OUT"
grep -Fq "blocks=6" "$HIR_CFG_OUT"
grep -Fq "blocks-with-phi=2" "$HIR_CFG_OUT"
grep -Fq "succ=TF" "$HIR_CFG_OUT"

grep -Fq "HIR dom view" "$HIR_DOM_OUT"
grep -Fq "idom=" "$HIR_DOM_OUT"
grep -Fq "df=" "$HIR_DOM_OUT"
grep -Fq "loop=" "$HIR_DOM_OUT"
grep -Fq "rpo=" "$HIR_DOM_OUT"

grep -Fq "flow-block[" "$RIR_OUT"
grep -Fq "join=yes" "$RIR_OUT"
grep -Fq "semantics=authority|world-handoff|invalidation|authority-loss" "$RIR_OUT"
grep -Fq "kind=ProjectionTObject state=Published" "$RIR_OUT"

grep -Fq "routine[02] function MergeRouteScore blocks=6" "$MIR_OUT"
grep -Fq "phi=2" "$MIR_OUT"
grep -Fq "value[00] score.1 slot=score" "$MIR_OUT"
grep -Fq "cleanup-block=yes rollback-block=yes invalidation-block=yes" "$MIR_OUT"
grep -Fq "cleanup-edge" "$MIR_OUT"
grep -Fq "DetachInvalidation" "$MIR_OUT"

grep -Fq "Parallel:" "$PARALLEL_SELECT_AST"
grep -Fq "ChannelSend: ch <- 7" "$PARALLEL_SELECT_AST"
grep -Fq "inst[01] stmt" "$PARALLEL_SELECT_MIR"
grep -Fq "ast-type=9" "$PARALLEL_SELECT_MIR"

claim_line="$(grep -n "PgySlot_Int s = pgy_claim_Int();" "$WITH_SLOT_ORDER_C" | head -1 | cut -d: -f1)"
read_line="$(grep -n "pgy_read_Int(&s)" "$WITH_SLOT_ORDER_C" | head -1 | cut -d: -f1)"
if [[ -z "$claim_line" || -z "$read_line" || "$claim_line" -ge "$read_line" ]]; then
    echo "with-slot MIR resource ops must preserve source order before residual Read statements" >&2
    exit 1
fi

echo "cfg-body-dataflow smoke: PASS $EXAMPLE"
