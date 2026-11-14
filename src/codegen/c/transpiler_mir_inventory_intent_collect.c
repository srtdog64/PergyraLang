/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR intent metadata collectors.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_inventory_view.h"

static bool
transpiler_collect_next_capacity(size_t capacity,
                                 size_t elem_size,
                                 size_t *new_capacity)
{
    size_t next;

    if (new_capacity == NULL || elem_size == 0)
        return false;
    if (capacity == 0) {
        next = 4;
    } else {
        if (capacity > SIZE_MAX / 2)
            return false;
        next = capacity * 2;
    }
    if (next > SIZE_MAX / elem_size)
        return false;
    *new_capacity = next;
    return true;
}

const MIRRoutine *
transpiler_find_mir_function(const TranspilerCtx *ctx,
                             const ASTNode *func_decl)
{
    TranspilerMIRRoutineInventory inventory;
    const char *target;

    if (ctx == NULL || func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || ast_declaration_name(func_decl) == NULL) {
        return NULL;
    }

    target = ast_declaration_name(func_decl);
    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        const char *routine_name = transpiler_mir_routine_name(routine);

        if (routine == NULL
            || transpiler_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION
            || routine_name == NULL) {
            continue;
        }
        if (strcmp(routine_name, target) == 0)
            return routine;
    }
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        const char *routine_name = transpiler_mir_routine_name(routine);
        size_t name_len;

        if (routine == NULL
            || transpiler_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION
            || routine_name == NULL) {
            continue;
        }

        name_len = strlen(target);
        if (strncmp(routine_name, target, name_len) == 0
            && (routine_name[name_len] == '_'
                || routine_name[name_len] == '\0')) {
            return routine;
        }
    }

    return NULL;
}

const MIRRoutine *
transpiler_find_mir_intent(const TranspilerCtx *ctx,
                           const ASTNode *intent_decl)
{
    TranspilerMIRRoutineInventory inventory;

    if (ctx == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL
        || ast_intent_decl_name(intent_decl) == NULL) {
        return NULL;
    }

    transpiler_active_routine_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine =
            transpiler_routine_inventory_get(&inventory, i);
        const char *routine_name = transpiler_mir_routine_name(routine);
        if (routine == NULL)
            continue;
        if (transpiler_mir_routine_kind(routine) != MIR_SCOPE_INTENT
            || routine_name == NULL
            || strcmp(routine_name,
                      ast_intent_decl_name(intent_decl)) != 0) {
            continue;
        }
        return routine;
    }

    return NULL;
}

const char *
transpiler_find_mir_intent_meta_arg(const MIRRoutine *routine,
                                    const char *step_name,
                                    const char *inst_name)
{
    if (routine == NULL || inst_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, inst_name))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            return payload;
        }
    }
    return NULL;
}

size_t
transpiler_collect_mir_intent_step_names(const MIRRoutine *routine,
                                         const char ***names_out)
{
    const char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (names_out != NULL)
        *names_out = NULL;
    if (routine == NULL || names_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown;

            if (mir_instruction_intent_step_name(inst) == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                grown = realloc((void *)names,
                                new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)names);
                    return 0;
                }
                names = grown;
                capacity = new_capacity;
            }
            names[count++] = mir_instruction_intent_step_name(inst);
        }
    }

    *names_out = names;
    return count;
}

ASTNode *
transpiler_find_mir_intent_check_expr(const MIRRoutine *routine,
                                      const char *step_name,
                                      const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->expr0 == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentCheck"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            return inst->expr0;
        }
    }
    return NULL;
}

size_t
transpiler_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                         const char *step_name,
                                         const char *phase_name,
                                         ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            ASTNode **grown;

            if (inst->expr0 == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentEval"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity;
                if (!transpiler_collect_next_capacity(capacity,
                                                      sizeof(ASTNode *),
                                                      &new_capacity)) {
                    free(exprs);
                    return 0;
                }
                grown = realloc(exprs, new_capacity * sizeof(ASTNode *));
                if (grown == NULL) {
                    free(exprs);
                    return 0;
                }
                exprs = grown;
                capacity = new_capacity;
            }
            exprs[count++] = inst->expr0;
        }
    }

    *exprs_out = exprs;
    return count;
}

ASTNode *
transpiler_find_mir_intent_eval_expr(const MIRRoutine *routine,
                                     const char *step_name,
                                     const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = transpiler_collect_mir_intent_eval_exprs(
        routine, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    free(exprs);
    return result;
}
