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
                                              bool allow_legacy_payload_probe)
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

    if (inst->has_surface_usage_facts)
        return inst->uses_intent_observability_surface;

    if (!allow_legacy_payload_probe)
        return false;

    /*
     * Direct statement calls are carried in MIR_STMT.arg0, direct initializer
     * calls are carried in MIR_INST_DEF.arg1, and lowered declaration
     * inventory is carried by MIRProgram inventory surface facts. Keep only
     * payload probing for hand-built legacy MIR fixtures without HIR
     * provenance; source statement AST scanning is no longer a codegen usage
     * discovery path.
     */
    return pgy_ast_uses_intent_observability(inst->expr0)
        || pgy_ast_uses_intent_observability(inst->expr1);
}

static bool
pgy_mir_block_uses_intent_observability(const MIRBasicBlock *block,
                                        bool allow_legacy_payload_probe)
{
    if (block == NULL)
        return false;

    if (block->instruction_count > 0 && block->instructions == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        if (pgy_mir_instruction_uses_intent_observability(
                &block->instructions[i], allow_legacy_payload_probe)) {
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
    if (mir == NULL)
        return false;

    /*
     * Lowered MIR records inventory surface usage during mir_lower(); the MIR
     * validator rejects missing or stale facts. Codegen must consume that fact
     * instead of rediscovering declaration usage from AST inventory arrays.
     */
    if (!mir->has_inventory_surface_usage_facts)
        return false;

    return mir->inventory_uses_intent_observability_surface;
}

bool
pgy_mir_program_uses_intent_observability(const MIRProgram *mir)
{
    MIRRoutineInventory inventory;

    if (mir == NULL)
        return false;

    mir_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (pgy_mir_routine_uses_intent_observability(routine))
            return true;
    }

    return pgy_mir_inventory_uses_intent_observability(mir);
}
