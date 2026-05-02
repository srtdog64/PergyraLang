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
pgy_mir_instruction_uses_thread_pool(const MIRInstruction *inst)
{
    if (inst == NULL)
        return false;

    return pgy_ast_uses_thread_pool(inst->ast)
        || pgy_ast_uses_thread_pool(inst->expr0)
        || pgy_ast_uses_thread_pool(inst->expr1);
}

bool
pgy_mir_routine_uses_thread_pool(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        for (size_t j = 0; j < block->instruction_count; j++) {
            if (pgy_mir_instruction_uses_thread_pool(&block->instructions[j]))
                return true;
        }
    }

    return false;
}
