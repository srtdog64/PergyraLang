#include "mir_cleanup.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
mir_cleanup_append_instruction(MIRBasicBlock *block, MIRInstruction inst)
{
    MIRInstruction *grown;
    if (block == NULL)
        return false;
    grown = realloc(block->instructions, (block->instruction_count + 1) * sizeof(MIRInstruction));
    if (grown == NULL)
        return false;
    grown[block->instruction_count] = inst;
    block->instructions = grown;
    block->instruction_count++;
    return true;
}

static bool
mir_cleanup_append_block(MIRRoutine *routine, MIRBasicBlock block)
{
    MIRBasicBlock *grown;
    if (routine == NULL)
        return false;
    grown = realloc(routine->blocks, (routine->block_count + 1) * sizeof(MIRBasicBlock));
    if (grown == NULL)
        return false;
    grown[routine->block_count] = block;
    routine->blocks = grown;
    routine->block_count++;
    return true;
}

static bool
mir_cleanup_append_index_unique(size_t **items, size_t *count, size_t value)
{
    size_t *grown;
    if (items == NULL || count == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }
    grown = realloc(*items, (*count + 1) * sizeof(size_t));
    if (grown == NULL)
        return false;
    grown[*count] = value;
    *items = grown;
    (*count)++;
    return true;
}

bool
mir_add_cleanup_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op)
{
    MIRInstruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = rir_op_kind_name(op->kind);
    inst.slot_anchor = op->slot_anchor;
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    routine->cleanup_instruction_count++;
    return mir_cleanup_append_instruction(block, inst);
}

static bool
mir_add_rollback_invalidation(MIRRoutine *routine, MIRBasicBlock *cleanup, const RIRScope *rir_scope)
{
    if (routine == NULL || cleanup == NULL || rir_scope == NULL)
        return false;
    for (size_t i = 0; i < rir_scope->fact_count; i++) {
        const RIRFact *fact = &rir_scope->facts[i];
        MIRInstruction inst;
        if (fact->kind != RIR_FACT_INTENT_POLICY || fact->name == NULL || strcmp(fact->name, "rollback") != 0)
            continue;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_CLEANUP_EDGE;
        inst.name = "RollbackPolicy";
        inst.slot_anchor = fact->slot_anchor;
        inst.arg0 = fact->arg0;
        if (!mir_cleanup_append_instruction(cleanup, inst))
            return false;
        routine->cleanup_instruction_count++;
    }
    return true;
}

static bool
mir_intent_ast_needs_invalidation(const HIRRoutine *hir_routine)
{
    ASTNode *intent;
    if (hir_routine == NULL || hir_routine->ast == NULL || hir_routine->ast->type != AST_INTENT_DECL)
        return false;
    intent = hir_routine->ast;
    for (size_t i = 0; i < intent->data.intent_decl.step_count; i++) {
        ASTNode *step = intent->data.intent_decl.steps[i];
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step->data.intent_step.using_expr != NULL
            || step->data.intent_step.transfer_from_alias != NULL
            || step->data.intent_step.transfer_to_alias != NULL) {
            return true;
        }
    }
    return false;
}

bool
mir_append_cleanup_block(MIRRoutine *routine, const RIRScope *rir_scope)
{
    bool needs_cleanup = false;
    bool needs_rollback = false;
    bool needs_invalidation = false;
    MIRBasicBlock block;

    if (routine == NULL || rir_scope == NULL)
        return true;

    for (size_t i = 0; i < rir_scope->op_count; i++) {
        if (rir_scope->ops[i].kind == RIR_OP_ABORT_INTENT
            || rir_scope->ops[i].kind == RIR_OP_COMPENSATE_INTENT_STEP) {
            needs_cleanup = true;
            needs_rollback = true;
            break;
        }
    }

    for (size_t i = 0; i < rir_scope->fact_count; i++) {
        const RIRFact *fact = &rir_scope->facts[i];
        if (fact->kind == RIR_FACT_INTENT_POLICY
            && fact->name != NULL
            && strcmp(fact->name, "rollback") == 0) {
            needs_cleanup = true;
            needs_rollback = true;
        }
        if (fact->kind == RIR_FACT_PROJECTION
            || fact->resource_kind == RIR_RESOURCE_EFFECT_INSTANCE
            || fact->resource_kind == RIR_RESOURCE_RELATION_INSTANCE
            || fact->resource_kind == RIR_RESOURCE_ZONE_HANDLE) {
            needs_cleanup = true;
            needs_invalidation = true;
        }
    }
    if (mir_intent_ast_needs_invalidation(routine->hir_routine)) {
        needs_cleanup = true;
        needs_invalidation = true;
    }

    if (!needs_cleanup)
        return true;

    memset(&block, 0, sizeof(block));
    block.id = routine->block_count;
    block.is_cleanup = true;
    block.is_reachable = true;
    block.source_hir_block_id = SIZE_MAX;
    routine->cleanup_block = block.id;
    routine->has_cleanup_block = true;
    if (!mir_cleanup_append_block(routine, block))
        return false;

    if (needs_rollback) {
        memset(&block, 0, sizeof(block));
        block.id = routine->block_count;
        block.is_cleanup = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        routine->rollback_block = block.id;
        routine->has_rollback_block = true;
        if (!mir_cleanup_append_block(routine, block))
            return false;
        if (!mir_add_rollback_invalidation(routine, &routine->blocks[routine->rollback_block], rir_scope))
            return false;
    }

    if (needs_invalidation) {
        memset(&block, 0, sizeof(block));
        block.id = routine->block_count;
        block.is_cleanup = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        routine->invalidation_block = block.id;
        routine->has_invalidation_block = true;
        if (!mir_cleanup_append_block(routine, block))
            return false;
    }

    return true;
}

bool
mir_materialize_cleanup_edges(MIRRoutine *routine)
{
    if (routine == NULL || !routine->has_cleanup_block)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        block->cleanup_succ = routine->cleanup_block;
        block->has_cleanup_succ = true;
        routine->cleanup_edge_count++;
        if (!mir_cleanup_append_instruction(block,
                                (MIRInstruction){
                                    .id = routine->instruction_count++,
                                    .kind = MIR_INST_CLEANUP_EDGE,
                                    .name = "cleanup-edge",
                                    .slot_anchor = "cleanup",
                                    .arg0 = "cleanup",
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!mir_cleanup_append_index_unique(&routine->blocks[routine->cleanup_block].predecessors,
                                             &routine->blocks[routine->cleanup_block].predecessor_count,
                                             i)) {
            return false;
        }
    }

    if (routine->has_rollback_block) {
        MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        size_t rollback_cleanup_target = routine->has_invalidation_block
            ? routine->invalidation_block
            : routine->cleanup_block;
        cleanup->rollback_succ = routine->rollback_block;
        cleanup->has_rollback_succ = true;
        if (!mir_cleanup_append_index_unique(&rollback->predecessors,
                                             &rollback->predecessor_count,
                                             cleanup->id)) {
            return false;
        }
        rollback->cleanup_succ = rollback_cleanup_target;
        rollback->has_cleanup_succ = true;
        routine->cleanup_edge_count++;
        if (!mir_cleanup_append_instruction(rollback,
                                (MIRInstruction){
                                    .id = routine->instruction_count++,
                                    .kind = MIR_INST_CLEANUP_EDGE,
                                    .name = "cleanup-edge-from-rollback",
                                    .slot_anchor = "cleanup",
                                    .arg0 = "cleanup",
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!mir_cleanup_append_index_unique(&routine->blocks[rollback_cleanup_target].predecessors,
                                             &routine->blocks[rollback_cleanup_target].predecessor_count,
                                             routine->rollback_block)) {
            return false;
        }
    }

    if (routine->has_invalidation_block) {
        MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
        cleanup->invalidation_succ = routine->invalidation_block;
        cleanup->has_invalidation_succ = true;
        if (!mir_cleanup_append_index_unique(&invalidation->predecessors,
                                             &invalidation->predecessor_count,
                                             cleanup->id)) {
            return false;
        }
        invalidation->cleanup_succ = routine->cleanup_block;
        invalidation->has_cleanup_succ = true;
        routine->cleanup_edge_count++;
        if (!mir_cleanup_append_instruction(invalidation,
                                (MIRInstruction){
                                    .id = routine->instruction_count++,
                                    .kind = MIR_INST_CLEANUP_EDGE,
                                    .name = "cleanup-edge-from-invalidation",
                                    .slot_anchor = "cleanup",
                                    .arg0 = "cleanup",
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!mir_cleanup_append_index_unique(&cleanup->predecessors,
                                             &cleanup->predecessor_count,
                                             routine->invalidation_block)) {
            return false;
        }
    }

    if (routine->has_rollback_block && routine->has_invalidation_block) {
        MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
        rollback->invalidation_succ = invalidation->id;
        rollback->has_invalidation_succ = true;
        if (!mir_cleanup_append_index_unique(&invalidation->predecessors,
                                             &invalidation->predecessor_count,
                                             rollback->id)) {
            return false;
        }
    }
    return true;
}
