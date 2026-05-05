#include "mir.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../common/arena.h"
#include "../runtime/pgy_abi_spec.h"
#include "../parser/ast_analysis.h"

void
mir_instruction_record_surface_usage(MIRInstruction *inst)
{
    if (inst == NULL)
        return;
    if (inst->ast != NULL) {
        inst->has_source_location = true;
        inst->source_line = inst->ast->line;
        inst->source_column = inst->ast->column;
        inst->source_ast_type = inst->ast->type;
    } else {
        inst->has_source_location = false;
        inst->source_line = 0;
        inst->source_column = 0;
        inst->source_ast_type = 0;
    }
    inst->has_surface_usage_facts = true;
    inst->uses_thread_pool_surface =
        ast_uses_thread_pool_surface(inst->ast)
        || ast_uses_thread_pool_surface(inst->expr0)
        || ast_uses_thread_pool_surface(inst->expr1);
    inst->uses_intent_observability_surface =
        ast_uses_intent_observability_surface(inst->ast)
        || ast_uses_intent_observability_surface(inst->expr0)
        || ast_uses_intent_observability_surface(inst->expr1);
}

static MIRBranchShape
mir_branch_shape_from_ast(const ASTNode *node)
{
    if (node == NULL)
        return MIR_BRANCH_EXPR;
    if (node->type == AST_FOR_LOOP)
        return node->data.for_loop.iterable != NULL
            ? MIR_BRANCH_FOR_IN
            : MIR_BRANCH_FOR_RANGE;
    if (node->type == AST_MATCH_CASE)
        return MIR_BRANCH_MATCH_CASE;
    if (node->type == AST_BLOCK)
        return MIR_BRANCH_SELECT_DISPATCH;
    return MIR_BRANCH_EXPR;
}

#include "mir_base_helpers.h"
#include "mir_cleanup.h"
#include "mir_intent.h"
#include "mir_surface_usage.h"
#include "mir_type_helpers.h"
#include "mir_validation.h"

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->source_phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_PHI;
        inst.name = block->source_phi_nodes[i].name;
        inst.slot_anchor = block->source_phi_nodes[i].name;
        inst.arg0 = "phi";
        if (!append_instruction(block, inst))
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
    inst.id = routine->instruction_count++;
    inst.kind = (terminator_kind == HIR_BLOCK_BRANCH)
                    ? MIR_INST_BRANCH
                    : MIR_INST_RETURN;
    inst.name = (terminator_kind == HIR_BLOCK_BRANCH) ? "branch" : "return";
    inst.source_terminator_kind = terminator_kind;
    inst.has_source_terminator_kind = true;
    inst.ast = (terminator_kind == HIR_BLOCK_BRANCH)
                   ? terminator_condition
                   : terminator_value;
    if (inst.kind == MIR_INST_BRANCH)
        inst.branch_shape = mir_branch_shape_from_ast(inst.ast);
    if (inst.kind == MIR_INST_BRANCH
        && inst.ast != NULL
        && inst.ast->type == AST_FOR_LOOP) {
        inst.arg0 = inst.ast->data.for_loop.variable;
        if (inst.ast->data.for_loop.iterable != NULL) {
            inst.expr0 = inst.ast->data.for_loop.iterable;
            inst.expr1 = inst.ast->data.for_loop.iterable;
        } else {
            inst.expr0 = inst.ast->data.for_loop.range_start;
            inst.expr1 = inst.ast->data.for_loop.range_end;
        }
    }
    mir_instruction_record_surface_usage(&inst);
    return append_instruction(block, inst);
}

static bool
mir_copy_ast_nodes(ASTNode ***dst, size_t *dst_count, ASTNode **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(ASTNode *));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(ASTNode *));
    *dst_count = src_count;
    return true;
}

static bool
mir_copy_names(const char ***dst, size_t *dst_count, const char **src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(const char *));
    if (*dst == NULL)
        return false;
    memcpy((void *)*dst, src, src_count * sizeof(const char *));
    *dst_count = src_count;
    return true;
}

static bool
mir_copy_phi_nodes(MIRSourcePhiNode **dst, size_t *dst_count,
                   const HIRPhiNode *src, size_t src_count)
{
    if (dst == NULL || dst_count == NULL)
        return false;
    *dst = NULL;
    *dst_count = 0;
    if (src == NULL || src_count == 0)
        return true;
    *dst = calloc(src_count, sizeof(MIRSourcePhiNode));
    if (*dst == NULL)
        return false;
    *dst_count = src_count;
    for (size_t i = 0; i < src_count; i++) {
        (*dst)[i].name = src[i].name;
        if (!copy_indices(&(*dst)[i].incoming_predecessors,
                          &(*dst)[i].incoming_predecessor_count,
                          src[i].incoming_predecessors,
                          src[i].incoming_predecessor_count)) {
            for (size_t j = 0; j < i; j++)
                free((*dst)[j].incoming_predecessors);
            free(*dst);
            *dst = NULL;
            *dst_count = 0;
            return false;
        }
    }
    return true;
}

static void
mir_block_record_source_location(MIRBasicBlock *block, const ASTNode *source_ast)
{
    if (block == NULL)
        return;
    block->has_source_location = source_ast != NULL;
    block->source_line = source_ast != NULL ? source_ast->line : 0;
    block->source_column = source_ast != NULL ? source_ast->column : 0;
}

static bool
mir_add_resource_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op)
{
    MIRInstruction inst;
    char *claim_type_name = NULL;
    const char *abi_type_name = NULL;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_RESOURCE_OP;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    /* ABI type layout — lookup from type table */
    if (op->kind == RIR_OP_CLAIM)
        claim_type_name = mir_claim_abi_type_name_from_ast(op->ast);
    abi_type_name = claim_type_name != NULL
        ? claim_type_name
        : (op->arg0 != NULL ? op->arg0 : op->subject);
    inst.type_layout = mir_abi_lookup(abi_type_name);
    free(claim_type_name);
    mir_instruction_record_surface_usage(&inst);
    return append_instruction(block, inst);
}

#include "mir_ssa_rename.h"

#include "mir_liveness_dce.h"
#include "mir_dce.h"

#include "mir_fact_validate.h"
#include "mir_stmt_population.h"
#include "mir_non_cfg_stmt_population.h"

static bool
mir_populate_instructions(MIRRoutine *routine)
{
    const RIRScope *rir_scope;
    MIRBasicBlock *entry;
    MIRBasicBlock *rollback;
    MIRBasicBlock *invalidation;
    bool appended_intent_steps = false;

    if (routine == NULL || routine->block_count == 0)
        return true;

    rir_scope = routine->rir_scope;
    entry = &routine->blocks[routine->entry_block];
    rollback = routine->has_rollback_block ? &routine->blocks[routine->rollback_block] : NULL;
    invalidation = routine->has_invalidation_block ? &routine->blocks[routine->invalidation_block] : NULL;

    if (routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL) {
        if (!mir_append_intent_step_instructions(routine, entry))
            return false;
        appended_intent_steps = true;
    }

    if (rir_scope == NULL)
        return true;

    for (size_t i = 0; i < rir_scope->op_count; i++) {
        const RIROp *op = &rir_scope->ops[i];
        switch (op->kind) {
            case RIR_OP_ABORT_INTENT:
            case RIR_OP_COMPENSATE_INTENT_STEP:
                if (rollback != NULL) {
                    if (!mir_add_cleanup_instruction(routine, rollback, op))
                        return false;
                    break;
                }
                /* fallthrough */
            default:
                if (!mir_add_resource_instruction(routine, entry, op))
                    return false;
                break;
        }
    }

    if (invalidation != NULL) {
        for (size_t i = 0; i < rir_scope->fact_count; i++) {
            const RIRFact *fact = &rir_scope->facts[i];
            MIRInstruction inst;
            if (fact->kind != RIR_FACT_PROJECTION
                && fact->resource_kind != RIR_RESOURCE_EFFECT_INSTANCE
                && fact->resource_kind != RIR_RESOURCE_RELATION_INSTANCE
                && fact->resource_kind != RIR_RESOURCE_ZONE_HANDLE) {
                continue;
            }
            memset(&inst, 0, sizeof(inst));
            inst.id = routine->instruction_count++;
            inst.kind = MIR_INST_CLEANUP_EDGE;
            inst.name = "DetachInvalidation";
            inst.slot_anchor = fact->slot_anchor != NULL ? fact->slot_anchor : fact->name;
            inst.arg0 = fact->name;
            inst.arg1 = rir_resource_kind_name(fact->resource_kind);
            inst.ast = fact->ast;
            if (!append_instruction(invalidation, inst))
                return false;
            routine->cleanup_instruction_count++;
        }
        if (!mir_append_intent_invalidation_markers(routine, invalidation))
            return false;
    }

    if (!appended_intent_steps && routine->kind == MIR_SCOPE_INTENT
        && routine->hir_routine != NULL) {
        if (!mir_append_intent_step_instructions(routine, entry))
            return false;
    } else if (routine->hir_routine != NULL
               && !routine->hir_routine->has_cfg) {
        if (!mir_append_non_cfg_body_statements(routine, entry))
            return false;
    }

    return true;
}

static bool
mir_build_blocks_from_hir(MIRRoutine *routine, const HIRRoutine *hir_routine)
{
    if (routine == NULL)
        return false;

    if (hir_routine == NULL || !hir_routine->has_cfg || hir_routine->cfg.block_count == 0) {
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        mir_block_record_source_location(&block, NULL);
        routine->entry_block = 0;
        return append_block(routine, block);
    }

    routine->entry_block = hir_routine->cfg.entry_block;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        const ASTNode *source_ast = NULL;
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = i;
        block.is_entry = (i == hir_routine->cfg.entry_block);
        block.is_reachable = src->is_reachable;
        block.is_pin_region = src->is_pin_region;
        block.pin_view_is_write = src->pin_view_is_write;
        block.pin_source_name = src->pin_source_name;
        block.pin_view_name = src->pin_view_name;
        block.pin_block_ast = src->pin_block_ast;
        block.source_hir_block_id = src->id;
        if (src->statement_count > 0)
            source_ast = src->statements[0];
        else if (src->terminator_condition != NULL)
            source_ast = src->terminator_condition;
        else if (src->terminator_value != NULL)
            source_ast = src->terminator_value;
        mir_block_record_source_location(&block, source_ast);
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
        if (!append_block(routine, block))
            return false;
    }

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

    return true;
}

#include "mir_decl_headers.h"
#include "mir_lower_public_api.h"
#include "mir_cfg_contract_validate.h"
#include "mir_public_surface.h"
#include "mir_abi_layout.h"
