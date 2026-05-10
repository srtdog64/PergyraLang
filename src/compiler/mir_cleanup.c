#include "mir_cleanup.h"

#include "mir_base_helpers.h"
#include "mir_cleanup_fact_names.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
mir_cleanup_next_capacity(size_t *capacity, size_t initial, size_t elem_size)
{
    if (capacity == NULL || initial == 0 || elem_size == 0)
        return false;

    size_t current = *capacity;
    size_t next_capacity = initial;
    if (current != 0) {
        if (current > SIZE_MAX / 2)
            return false;
        next_capacity = current * 2;
    }
    if (next_capacity > SIZE_MAX / elem_size)
        return false;

    *capacity = next_capacity;
    return true;
}

static bool
mir_cleanup_append_instruction(MIRBasicBlock *block, MIRInstruction inst)
{
    return append_instruction(block, inst);
}

static bool
mir_cleanup_append_block(MIRRoutine *routine, MIRBasicBlock block)
{
    return append_block(routine, block);
}

static bool
mir_cleanup_append_index_unique(size_t **items, size_t *count, size_t *capacity, size_t value)
{
    if (items == NULL || count == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*items)[i] == value)
            return true;
    }
    if (*count == *capacity) {
        size_t next_capacity = *capacity;
        if (!mir_cleanup_next_capacity(&next_capacity, 4, sizeof(size_t)))
            return false;
        size_t *grown = realloc(*items, next_capacity * sizeof(size_t));
        if (grown == NULL)
            return false;
        *items = grown;
        *capacity = next_capacity;
    }
    (*items)[*count] = value;
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
mir_rir_scope_requires_rollback(const RIRScope *rir_scope)
{
    if (rir_scope == NULL)
        return false;
    for (size_t i = 0; i < rir_scope->op_count; i++) {
        if (rir_scope->ops[i].kind == RIR_OP_ABORT_INTENT
            || rir_scope->ops[i].kind == RIR_OP_COMPENSATE_INTENT_STEP) {
            return true;
        }
    }
    for (size_t i = 0; i < rir_scope->fact_count; i++) {
        const RIRFact *fact = &rir_scope->facts[i];
        if (fact->kind == RIR_FACT_INTENT_POLICY
            && fact->name != NULL
            && strcmp(fact->name, "rollback") == 0) {
            return true;
        }
    }
    return false;
}

static bool
mir_rir_scope_requires_invalidation(const RIRScope *rir_scope)
{
    if (rir_scope == NULL)
        return false;

    if ((rir_scope->conservative_semantics
         & (RIR_FLOW_INVALIDATION | RIR_FLOW_WORLD_HANDOFF | RIR_FLOW_PROJECTION_INVALIDATION)) != 0U) {
        return true;
    }
    for (size_t i = 0; i < rir_scope->flow_block_count; i++) {
        const RIRFlowBlock *flow = &rir_scope->flow_blocks[i];
        unsigned int semantics = flow->entry_semantics | flow->exit_semantics;
        if ((semantics
             & (RIR_FLOW_INVALIDATION | RIR_FLOW_WORLD_HANDOFF | RIR_FLOW_PROJECTION_INVALIDATION)) != 0U) {
            return true;
        }
        for (size_t j = 0; j < flow->fact_count; j++) {
            const RIRFlowFact *fact = &flow->facts[j];
            if (fact->entry_conflict || fact->has_merge_conflict)
                return true;
            if (fact->entry_state == RIR_STATE_DIRTY
                || fact->entry_state == RIR_STATE_STALE
                || fact->entry_state == RIR_STATE_DETACHED
                || fact->entry_state == RIR_STATE_HANDOFF_PENDING
                || fact->entry_state == RIR_STATE_HANDED_OFF
                || fact->entry_state == RIR_STATE_AUTHORITY_LOST
                || fact->exit_state == RIR_STATE_DIRTY
                || fact->exit_state == RIR_STATE_STALE
                || fact->exit_state == RIR_STATE_DETACHED
                || fact->exit_state == RIR_STATE_HANDOFF_PENDING
                || fact->exit_state == RIR_STATE_HANDED_OFF
                || fact->exit_state == RIR_STATE_AUTHORITY_LOST) {
                return true;
            }
        }
    }
    for (size_t i = 0; i < rir_scope->fact_count; i++) {
        const RIRFact *fact = &rir_scope->facts[i];
        if (fact->kind == RIR_FACT_PROJECTION
            || fact->resource_kind == RIR_RESOURCE_EFFECT_INSTANCE
            || fact->resource_kind == RIR_RESOURCE_RELATION_INSTANCE
            || fact->resource_kind == RIR_RESOURCE_ZONE_HANDLE
            || fact->resource_kind == RIR_RESOURCE_WORLD_HANDLE) {
            return true;
        }
    }
    return false;
}

static bool
mir_routine_has_pin_regions(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (routine->blocks[i].is_pin_region)
            return true;
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

    if (routine == NULL)
        return true;

    needs_rollback = mir_rir_scope_requires_rollback(rir_scope);
    needs_invalidation = mir_rir_scope_requires_invalidation(rir_scope);
    if (needs_rollback || needs_invalidation)
        needs_cleanup = true;
    if (mir_routine_has_pin_regions(routine))
        needs_cleanup = true;

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

static bool
mir_materialize_pin_cleanup_edges(MIRRoutine *routine, MIRBasicBlock *block)
{
    MIRInstruction inst;
    if (routine == NULL || block == NULL || !block->is_pin_region)
        return true;
    if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0')
        return false;
    if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0')
        return false;

    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = MIR_CLEANUP_FACT_PIN_UNPIN_EDGE;
    inst.slot_anchor = block->pin_source_name;
    inst.arg0 = block->pin_view_name;
    inst.arg1 = block->pin_view_is_write
        ? MIR_PIN_CLEANUP_ACCESS_WRITE
        : MIR_PIN_CLEANUP_ACCESS_READ;
    inst.ast = block->pin_block_ast;
    routine->cleanup_instruction_count++;
    return mir_cleanup_append_instruction(block, inst);
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
                                    .name = MIR_CLEANUP_FACT_EDGE,
                                    .slot_anchor = MIR_CLEANUP_FACT_ANCHOR,
                                    .arg0 = MIR_CLEANUP_FACT_ANCHOR,
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!mir_cleanup_append_index_unique(&routine->blocks[routine->cleanup_block].predecessors,
                                             &routine->blocks[routine->cleanup_block].predecessor_count,
                                             &routine->blocks[routine->cleanup_block].predecessor_capacity,
                                             i)) {
            return false;
        }
        if (!mir_materialize_pin_cleanup_edges(routine, block))
            return false;
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
                                             &rollback->predecessor_capacity,
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
                                    .name = MIR_CLEANUP_FACT_EDGE_FROM_ROLLBACK,
                                    .slot_anchor = MIR_CLEANUP_FACT_ANCHOR,
                                    .arg0 = MIR_CLEANUP_FACT_ANCHOR,
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!mir_cleanup_append_index_unique(&routine->blocks[rollback_cleanup_target].predecessors,
                                             &routine->blocks[rollback_cleanup_target].predecessor_count,
                                             &routine->blocks[rollback_cleanup_target].predecessor_capacity,
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
                                             &invalidation->predecessor_capacity,
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
                                    .name = MIR_CLEANUP_FACT_EDGE_FROM_INVALIDATION,
                                    .slot_anchor = MIR_CLEANUP_FACT_ANCHOR,
                                    .arg0 = MIR_CLEANUP_FACT_ANCHOR,
                                    .arg1 = NULL,
                                    .ast = NULL,
                                })) {
            return false;
        }
        if (!mir_cleanup_append_index_unique(&cleanup->predecessors,
                                             &cleanup->predecessor_count,
                                             &cleanup->predecessor_capacity,
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
                                             &invalidation->predecessor_capacity,
                                             rollback->id)) {
            return false;
        }
    }
    return true;
}
