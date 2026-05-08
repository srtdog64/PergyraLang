/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared C/LLVM runtime thread-pool dependency analysis.
 */

#include "thread_pool_usage.h"

#include "../parser/ast_analysis.h"

#include <stddef.h>

static bool
pgy_ast_uses_thread_pool(const ASTNode *node)
{
    return ast_uses_thread_pool_surface(node);
}

static bool
pgy_mir_instruction_uses_thread_pool(const MIRInstruction *inst,
                                     bool allow_legacy_payload_probe)
{
    if (inst == NULL)
        return false;

    if (inst->has_surface_usage_facts)
        return inst->uses_thread_pool_surface;

    if (!allow_legacy_payload_probe)
        return false;

    /*
     * Normal lowered MIR has HIR provenance and validated surface facts. Keep
     * only expression-payload scanning for hand-built legacy MIR without HIR
     * provenance; source statement AST scanning is not a codegen dependency
     * discovery path.
     */
    return pgy_ast_uses_thread_pool(inst->expr0)
        || pgy_ast_uses_thread_pool(inst->expr1);
}

bool
pgy_mir_routine_uses_thread_pool(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        if (block->instruction_count > 0 && block->instructions == NULL)
            return false;

        for (size_t j = 0; j < block->instruction_count; j++) {
            if (pgy_mir_instruction_uses_thread_pool(
                    &block->instructions[j], routine->hir_routine == NULL)) {
                return true;
            }
        }
    }

    return false;
}

bool
pgy_mir_program_uses_thread_pool(const MIRProgram *mir)
{
    MIRRoutineInventory inventory;

    if (mir == NULL)
        return false;

    if (mir->has_inventory_surface_usage_facts
        && mir->inventory_uses_thread_pool_surface) {
        return true;
    }

    mir_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (pgy_mir_routine_uses_thread_pool(routine))
            return true;
    }

    return false;
}
