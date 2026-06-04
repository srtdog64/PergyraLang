/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR intent alias/participant metadata collectors.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "transpiler_mir_inventory_intent_collect.h"

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

size_t
transpiler_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                          const char *step_name,
                                          const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown;

            if (!mir_instruction_is_intent_stmt(inst, "IntentWho"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity;
                if (!transpiler_collect_next_capacity(capacity,
                                                      sizeof(const char *),
                                                      &new_capacity)) {
                    free((void *)aliases);
                    return 0;
                }
                grown = realloc((void *)aliases,
                                new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

size_t
transpiler_collect_mir_intent_authorized_aliases(
    const MIRRoutine *routine,
    const char *step_name,
    const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown;

            if (!mir_instruction_is_intent_stmt(inst, "IntentAuthorizedBy"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity;
                if (!transpiler_collect_next_capacity(capacity,
                                                      sizeof(const char *),
                                                      &new_capacity)) {
                    free((void *)aliases);
                    return 0;
                }
                grown = realloc((void *)aliases,
                                new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

size_t
transpiler_collect_mir_intent_bindings(const MIRRoutine *routine,
                                       const char ***kinds_out,
                                       const char ***aliases_out,
                                       const char ***types_out)
{
    const char **kinds = NULL;
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (kinds_out != NULL)
        *kinds_out = NULL;
    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (types_out != NULL)
        *types_out = NULL;
    if (routine == NULL || kinds_out == NULL
        || aliases_out == NULL || types_out == NULL) {
        return 0;
    }

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char **grown_kinds;
            const char **grown_aliases;
            const char **grown_types;

            if (!mir_instruction_is_intent_stmt(inst, "IntentBinding"))
                continue;
            if (inst->slot_anchor == NULL || inst->arg0 == NULL
                || inst->arg1 == NULL) {
                continue;
            }

            if (count >= capacity) {
                size_t new_capacity;
                if (!transpiler_collect_next_capacity(capacity,
                                                      sizeof(const char *),
                                                      &new_capacity)) {
                    free((void *)kinds);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                grown_kinds = malloc(new_capacity * sizeof(const char *));
                grown_aliases = malloc(new_capacity * sizeof(const char *));
                grown_types = malloc(new_capacity * sizeof(const char *));
                if (grown_kinds == NULL || grown_aliases == NULL
                    || grown_types == NULL) {
                    free((void *)grown_kinds);
                    free((void *)grown_aliases);
                    free((void *)grown_types);
                    free((void *)kinds);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                if (count > 0) {
                    memcpy((void *)grown_kinds, (const void *)kinds,
                           count * sizeof(const char *));
                    memcpy((void *)grown_aliases, (const void *)aliases,
                           count * sizeof(const char *));
                    memcpy((void *)grown_types, (const void *)types,
                           count * sizeof(const char *));
                }
                free((void *)kinds);
                free((void *)aliases);
                free((void *)types);
                kinds = grown_kinds;
                aliases = grown_aliases;
                types = grown_types;
                capacity = new_capacity;
            }
            kinds[count] = inst->slot_anchor;
            aliases[count] = inst->arg0;
            types[count] = inst->arg1;
            count++;
        }
    }

    *kinds_out = kinds;
    *aliases_out = aliases;
    *types_out = types;
    return count;
}

size_t
transpiler_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                               const char *step_name,
                                               const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown;

            if (!mir_instruction_is_intent_stmt(inst, "IntentDispatch"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity;
                if (!transpiler_collect_next_capacity(capacity,
                                                      sizeof(const char *),
                                                      &new_capacity)) {
                    free((void *)aliases);
                    return 0;
                }
                grown = realloc((void *)aliases,
                                new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}
