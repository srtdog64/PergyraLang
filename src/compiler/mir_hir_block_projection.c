#include "mir_hir_block_projection.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "mir_base_helpers.h"
#include "mir_branch_source_facts.h"
#include "mir_source_inventory_build.h"
#include "mir_timing.h"

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->source_phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.kind = MIR_INST_PHI;
        inst.name = block->source_phi_nodes[i].name;
        inst.slot_anchor = block->source_phi_nodes[i].name;
        inst.arg0 = "phi";
        if (!mir_commit_instruction(routine, block, &inst))
            return false;
    }
    return true;
}

static bool
mir_add_terminator_instruction(MIRRoutine *routine,
                               MIRBasicBlock *block,
                               HIRBlockTerminatorKind terminator_kind,
                               ASTNode *terminator_condition,
                               ASTNode *terminator_value)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL)
        return false;
    if (terminator_kind != HIR_BLOCK_BRANCH
        && terminator_kind != HIR_BLOCK_RETURN)
        return true;
    memset(&inst, 0, sizeof(inst));
    inst.kind = (terminator_kind == HIR_BLOCK_BRANCH)
                    ? MIR_INST_BRANCH
                    : MIR_INST_RETURN;
    inst.name = (terminator_kind == HIR_BLOCK_BRANCH) ? "branch" : "return";
    inst.source_terminator_kind = terminator_kind;
    inst.has_source_terminator_kind = true;
    inst.source_terminator_has_value = terminator_value != NULL;
    inst.ast = (terminator_kind == HIR_BLOCK_BRANCH)
                   ? terminator_condition
                   : terminator_value;
    mir_instruction_capture_source_provenance(&inst, inst.ast);
    if (inst.kind == MIR_INST_BRANCH) {
        inst.branch_shape = mir_branch_shape_from_ast(inst.ast);
        inst.requires_source_branch_emit =
            inst.branch_shape == MIR_BRANCH_MATCH_CASE
            || inst.branch_shape == MIR_BRANCH_SELECT_DISPATCH;
    }
    if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_EXPR)
        inst.expr0 = terminator_condition;
    else if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_MATCH_CASE)
        mir_capture_match_case_facts(&inst, terminator_condition,
                                     terminator_value);
    else if (inst.kind == MIR_INST_BRANCH
        && inst.branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        inst.expr0 = mir_select_case_channel(terminator_condition);
    else if (inst.kind == MIR_INST_RETURN)
        inst.expr0 = terminator_value;
    if (inst.kind == MIR_INST_BRANCH
        && inst.ast != NULL
        && inst.ast->type == AST_FOR_LOOP) {
        inst.arg0 = ast_for_variable(inst.ast);
        if (ast_for_iterable(inst.ast) != NULL) {
            inst.expr0 = ast_for_iterable(inst.ast);
            inst.expr1 = ast_for_iterable(inst.ast);
        } else {
            inst.expr0 = ast_for_range_start(inst.ast);
            inst.expr1 = ast_for_range_end(inst.ast);
        }
    }
    return mir_commit_instruction(routine, block, &inst);
}

bool
mir_build_blocks_from_hir(MIRRoutine *routine, const HIRRoutine *hir_routine)
{
    if (routine == NULL)
        return false;

    if (hir_routine == NULL || !hir_routine->has_cfg
        || hir_routine->cfg.block_count == 0) {
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        mir_block_record_source_location(&block, NULL);
        if (!mir_seed_non_cfg_block_source_inventory(&block, routine->ast)) {
            free(block.source_statement_inventory.items);
            return false;
        }
        routine->entry_block = 0;
        if (!append_block(routine, block)) {
            free(block.source_statement_inventory.items);
            return false;
        }
        return true;
    }

    routine->entry_block = hir_routine->cfg.entry_block;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        const ASTNode *source_node = NULL;
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = i;
        block.is_entry = (i == hir_routine->cfg.entry_block);
        block.is_reachable = src->is_reachable;
        block.is_pin_region = src->is_pin_region;
        block.is_select_case_body = src->is_select_case_body;
        block.pin_view_is_write = src->pin_view_is_write;
        block.pin_source_name = src->pin_source_name;
        block.pin_view_name = src->pin_view_name;
        block.pin_block_ast = src->pin_block_ast;
        block.source_hir_block_id = src->id;
        if (src->statement_count > 0)
            source_node = src->statements[0];
        else if (src->terminator_condition != NULL)
            source_node = src->terminator_condition;
        else if (src->terminator_value != NULL)
            source_node = src->terminator_value;
        double t_bb = mir_timing_now();
        mir_block_record_source_location(&block, source_node);
        mir_timing_add(MIR_TIMING_BB_SOURCE_LOC, mir_timing_now() - t_bb);
        t_bb = mir_timing_now();
        block.succ_true = src->succ_true;
        block.succ_false = src->succ_false;
        block.has_succ_true = src->has_succ_true;
        block.has_succ_false = src->has_succ_false;
        if (!copy_indices(&block.predecessors,
                          &block.predecessor_count,
                          src->predecessors,
                          src->predecessor_count)) {
            free(block.predecessors);
            return false;
        }
        block.predecessor_capacity = block.predecessor_count;
        if (!mir_copy_ast_nodes(&block.source_statement_inventory.items,
                                &block.source_statement_inventory.count,
                                src->statements,
                                src->statement_count)
            || !mir_copy_names(&block.source_local_defs,
                               &block.source_local_def_count,
                               src->local_defs,
                               src->local_def_count)
            || !copy_indices(&block.source_dom_tree_children,
                             &block.source_dom_tree_child_count,
                             src->dom_tree_children,
                             src->dom_tree_child_count)
            || !mir_copy_phi_nodes(&block.source_phi_nodes,
                                   &block.source_phi_node_count,
                                   src->phi_nodes,
                                   src->phi_node_count)) {
            free(block.predecessors);
            free(block.source_statement_inventory.items);
            free((void *)block.source_local_defs);
            free(block.source_dom_tree_children);
            if (block.source_phi_nodes != NULL) {
                for (size_t j = 0; j < block.source_phi_node_count; j++)
                    free(block.source_phi_nodes[j].incoming_predecessors);
            }
            free(block.source_phi_nodes);
            return false;
        }
        mir_timing_add(MIR_TIMING_BB_COPIES, mir_timing_now() - t_bb);
        t_bb = mir_timing_now();
        if (!append_block(routine, block))
            return false;
        mir_timing_add(MIR_TIMING_BB_APPEND, mir_timing_now() - t_bb);
    }

    {
        double t_pt = mir_timing_now();
        for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
            const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
            if (!mir_add_phi_placeholders(routine, &routine->blocks[i]))
                return false;
            if (!mir_add_terminator_instruction(routine,
                                                &routine->blocks[i],
                                                src->terminator_kind,
                                                src->terminator_condition,
                                                src->terminator_value))
                return false;
        }
        mir_timing_add(MIR_TIMING_BB_PHI_TERM, mir_timing_now() - t_pt);
    }

    return true;
}
