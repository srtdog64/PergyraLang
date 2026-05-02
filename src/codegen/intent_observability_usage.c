/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "intent_observability_usage.h"
#include "transpiler_builtin_type_table.h"

#include "../compiler/mir.h"
#include "../parser/ast_analysis.h"

static bool
pgy_identifier_is_intent_observability(const char *name, void *userdata)
{
    (void)userdata;
    return pgy_builtin_is_intent_observability(name);
}

static bool
pgy_ast_uses_intent_observability(const ASTNode *node)
{
    return ast_contains_identifier_call(
        node, pgy_identifier_is_intent_observability, NULL);
}

static bool
pgy_ast_array_uses_intent_observability(ASTNode *const *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pgy_ast_uses_intent_observability(nodes[i]))
            return true;
    }
    return false;
}

static bool
pgy_mir_symbol_uses_intent_observability(const char *name)
{
    return pgy_builtin_is_intent_observability(name);
}

static bool
pgy_name_array_uses_intent_observability(const char *const *names, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pgy_mir_symbol_uses_intent_observability(names[i]))
            return true;
    }
    return false;
}

static bool
pgy_mir_instruction_uses_intent_observability(const MIRInstruction *inst,
                                              bool allow_ast_fallback)
{
    if (inst == NULL)
        return false;

    if (pgy_mir_symbol_uses_intent_observability(inst->name)
        || pgy_mir_symbol_uses_intent_observability(inst->arg0)
        || pgy_mir_symbol_uses_intent_observability(inst->arg1)
        || pgy_mir_symbol_uses_intent_observability(inst->slot_anchor)
        || pgy_mir_symbol_uses_intent_observability(inst->result_name)) {
        return true;
    }

    if (!allow_ast_fallback)
        return false;

    /*
     * Direct statement calls are carried in MIR_STMT.arg0, and direct
     * initializer calls are carried in MIR_INST_DEF.arg1. Keep AST-backed
     * scanning only for nested expression calls and declaration inventory that
     * has not yet been lowered into a dedicated MIR fact.
     */
    return pgy_ast_uses_intent_observability(inst->ast)
        || pgy_ast_uses_intent_observability(inst->expr0)
        || pgy_ast_uses_intent_observability(inst->expr1);
}

static bool
pgy_mir_block_uses_intent_observability(const MIRBasicBlock *block,
                                        bool allow_ast_fallback)
{
    if (block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        if (pgy_mir_instruction_uses_intent_observability(
                &block->instructions[i], allow_ast_fallback)) {
            return true;
        }
    }

    return false;
}

static bool
pgy_mir_routine_uses_intent_observability(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    if (routine->hir_routine != NULL
        && pgy_name_array_uses_intent_observability(
            routine->hir_routine->direct_calls,
            routine->hir_routine->direct_call_count)) {
        return true;
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        if (pgy_mir_block_uses_intent_observability(
                &routine->blocks[i], routine->hir_routine == NULL)) {
            return true;
        }
    }

    return false;
}

static bool
pgy_mir_inventory_uses_intent_observability(const MIRProgram *mir)
{
    return pgy_ast_array_uses_intent_observability(mir->types, mir->type_count)
        || pgy_ast_array_uses_intent_observability(mir->abilities, mir->ability_count)
        || pgy_ast_array_uses_intent_observability(mir->roles, mir->role_count)
        || pgy_ast_array_uses_intent_observability(mir->parties, mir->party_count)
        || pgy_ast_array_uses_intent_observability(mir->rosters, mir->roster_count)
        || pgy_ast_array_uses_intent_observability(mir->worlds, mir->world_count)
        || pgy_ast_array_uses_intent_observability(mir->relations, mir->relation_count)
        || pgy_ast_array_uses_intent_observability(mir->effects, mir->effect_count)
        || pgy_ast_array_uses_intent_observability(mir->zones, mir->zone_count)
        || pgy_ast_array_uses_intent_observability(mir->events, mir->event_count)
        || pgy_ast_array_uses_intent_observability(mir->intents, mir->intent_count)
        || pgy_ast_array_uses_intent_observability(mir->functions, mir->function_count)
        || pgy_ast_array_uses_intent_observability(mir->externs, mir->extern_count);
}

bool
pgy_mir_program_uses_intent_observability(const MIRProgram *mir)
{
    if (mir == NULL)
        return false;

    for (size_t i = 0; i < mir->routine_count; i++) {
        if (pgy_mir_routine_uses_intent_observability(&mir->routines[i]))
            return true;
    }

    return pgy_mir_inventory_uses_intent_observability(mir);
}
