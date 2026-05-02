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
pgy_mir_instruction_uses_intent_observability(const MIRInstruction *inst)
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

    /*
     * MIR_STMT still carries generic AST-backed statements for calls whose
     * callee has not been materialized as an instruction fact yet. Keep this
     * fallback until statement call facts are part of MIR lowering.
     */
    return pgy_ast_uses_intent_observability(inst->ast)
        || pgy_ast_uses_intent_observability(inst->expr0)
        || pgy_ast_uses_intent_observability(inst->expr1);
}

static bool
pgy_mir_block_uses_intent_observability(const MIRBasicBlock *block)
{
    if (block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        if (pgy_mir_instruction_uses_intent_observability(&block->instructions[i]))
            return true;
    }

    return false;
}

static bool
pgy_mir_routine_uses_intent_observability(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        if (pgy_mir_block_uses_intent_observability(&routine->blocks[i]))
            return true;
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
