#include "mir_fact_validate.h"
#include "mir_fact_validate_internal.h"
#include "mir_speculation_facts.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "mir_surface_usage.h"

char *
mir_fact_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    int written;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    written = vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != length) {
        free(result);
        return NULL;
    }
    return result;
}

#define mir_strdup_fmt mir_fact_strdup_fmt

static bool
mir_has_inventory_payload(const MIRProgram *mir)
{
    return mir != NULL
        && (mir->extern_count > 0
            || mir->type_count > 0
            || mir->ability_count > 0
            || mir->role_count > 0
            || mir->party_count > 0
            || mir->roster_count > 0
            || mir->world_count > 0
            || mir->relation_count > 0
            || mir->effect_count > 0
            || mir->zone_count > 0
            || mir->event_count > 0
            || mir->intent_count > 0
            || mir->function_count > 0);
}

bool
mir_validate_instruction_inventory_shape(const MIRRoutine *routine,
                                         const MIRBasicBlock *block,
                                         size_t block_index,
                                         const char *validator,
                                         char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;
    if (block->instruction_count == 0 || block->instructions != NULL)
        return true;
    if (error_message != NULL) {
        *error_message = mir_strdup_fmt(
            "MIR routine '%s' block[%zu] has instruction count without instruction inventory during %s",
            routine->name != NULL ? routine->name : "(anonymous)",
            block_index,
            validator != NULL ? validator : "fact validation");
    }
    return false;
}

bool
mir_validate_inventory_surface_usage(const MIRProgram *mir, char **error_message)
{
    MIRSurfaceUsageSummary summary;

    if (mir == NULL)
        return false;

    if (!mir_has_inventory_payload(mir))
        return true;

    if (!mir_program_has_inventory_surface_usage_facts(mir)) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program is missing inventory surface usage facts");
        }
        return false;
    }

    summary = mir_inventory_surface_usage_summary(mir);
    if (mir_program_recorded_inventory_uses_thread_pool_surface(mir)
        != summary.uses_thread_pool) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program has stale thread-pool inventory surface usage fact");
        }
        return false;
    }

    if (mir_program_recorded_inventory_uses_intent_observability_surface(mir)
        != summary.uses_intent_observability) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program has stale intent observability inventory surface usage fact");
        }
        return false;
    }

    return true;
}

bool
mir_validate_statement_inventory(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (!mir_validate_instruction_inventory_shape(routine,
                                                  block,
                                                  block_index,
                                                  "statement inventory validation",
                                                  error_message))
        return false;

    if (block->source_statement_inventory.count > 0
        && block->source_statement_inventory.items == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] statement inventory has %zu item(s) but no storage",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index,
                block->source_statement_inventory.count);
        }
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (!mir_instruction_has_source_statement_order(inst))
            continue;
        size_t source_statement_index =
            mir_instruction_source_statement_index_or(inst, SIZE_MAX);
        if (source_statement_index >= block->source_statement_inventory.count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source statement index %zu exceeds inventory count %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    source_statement_index,
                    block->source_statement_inventory.count);
            }
            return false;
        }
    }

    return true;
}

bool
mir_validate_routine_emission_facts(const MIRRoutine *routine,
                                    char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (routine == NULL) {
        if (error_message != NULL)
            *error_message = mir_strdup_fmt("MIR routine is null during emission fact validation");
        return false;
    }
    if (routine->block_count > 0 && routine->blocks == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' records %zu block(s) without block inventory during emission fact validation",
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->block_count);
        }
        return false;
    }
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        if (!mir_validate_statement_inventory(routine, block, i, error_message))
            return false;
        if (!mir_validate_instruction_surface_usage(routine, block, i, error_message))
            return false;
        if (!mir_validate_intent_instruction_fact(routine, block, i, error_message))
            return false;
        if (!mir_validate_terminator_provenance(routine, block, i, error_message))
            return false;
    }

    return mir_validate_speculation_facts(routine, error_message);
}
