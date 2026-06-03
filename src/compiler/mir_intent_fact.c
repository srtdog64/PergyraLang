#include "mir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_intent_fact_strdup_fmt(const char *fmt, ...)
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

bool
mir_instruction_is_intent_stmt(const MIRInstruction *inst, const char *name)
{
    return inst != NULL
        && inst->kind == MIR_INST_STMT
        && inst->name != NULL
        && name != NULL
        && strcmp(inst->name, name) == 0;
}

static const char *const k_mir_intent_semantic_carrier_names[] = {
    "IntentAuthorizedBy",
    "IntentCauses",
    "IntentCheck",
    "IntentDispatch",
    "IntentEval",
    "IntentInvalidationTarget",
    "IntentParticipant",
    "IntentStep",
    "IntentValue",
    "IntentWho",
    "IntentZoneAlias",
    "IntentZoneFrom",
    "IntentZoneWhere",
};

static int
mir_intent_carrier_name_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const char *const *candidate = (const char *const *)entry;

    return strcmp(name, *candidate);
}

bool
mir_instruction_is_intent_semantic_carrier(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->name == NULL)
        return false;

    return bsearch(inst->name, k_mir_intent_semantic_carrier_names,
                   sizeof(k_mir_intent_semantic_carrier_names)
                       / sizeof(k_mir_intent_semantic_carrier_names[0]),
                   sizeof(k_mir_intent_semantic_carrier_names[0]),
                   mir_intent_carrier_name_compare) != NULL;
}

bool
mir_instruction_intent_step_matches(const MIRInstruction *inst,
                                    const char *step_name)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (step_name != NULL)
        return inst->arg1 != NULL && strcmp(inst->arg1, step_name) == 0;
    return inst->arg1 == NULL;
}

bool
mir_instruction_intent_phase_matches(const MIRInstruction *inst,
                                     const char *phase_name)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || phase_name == NULL)
        return false;
    return inst->arg0 != NULL && strcmp(inst->arg0, phase_name) == 0;
}

const char *
mir_instruction_intent_payload(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return NULL;
    return inst->arg0;
}

const char *
mir_instruction_intent_step_name(const MIRInstruction *inst)
{
    if (!mir_instruction_is_intent_stmt(inst, "IntentStep"))
        return NULL;
    return inst->arg0;
}

static bool
mir_intent_fact_requires_ast_step(const MIRInstruction *inst)
{
    return mir_instruction_is_intent_stmt(inst, "IntentStep")
        || mir_instruction_is_intent_stmt(inst, "IntentZoneWhere")
        || mir_instruction_is_intent_stmt(inst, "IntentZoneAlias")
        || mir_instruction_is_intent_stmt(inst, "IntentZoneFrom")
        || mir_instruction_is_intent_stmt(inst, "IntentInvalidationTarget")
        || mir_instruction_is_intent_stmt(inst, "IntentWho")
        || mir_instruction_is_intent_stmt(inst, "IntentAuthorizedBy")
        || mir_instruction_is_intent_stmt(inst, "IntentCauses")
        || mir_instruction_is_intent_stmt(inst, "IntentDispatch");
}

static bool
mir_intent_fact_requires_phase(const MIRInstruction *inst)
{
    return mir_instruction_is_intent_stmt(inst, "IntentCheck")
        || mir_instruction_is_intent_stmt(inst, "IntentEval");
}

bool
mir_validate_intent_instruction_fact(const MIRRoutine *routine,
                                     const MIRBasicBlock *block,
                                     size_t block_index,
                                     char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (mir_intent_fact_requires_ast_step(inst)) {
            if (!mir_instruction_source_is_intent_step(inst)) {
                if (error_message != NULL) {
                    *error_message = mir_intent_fact_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] intent fact '%s' is not anchored to an intent step source",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i,
                        inst->name != NULL ? inst->name : "(unnamed)");
                }
                return false;
            }
        }
        if (mir_instruction_is_intent_stmt(inst, "IntentStep")
            && inst->arg0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] IntentStep is missing MIR step name fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if ((mir_intent_fact_requires_ast_step(inst)
             && !mir_instruction_is_intent_stmt(inst, "IntentStep"))
            && inst->arg0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] intent fact '%s' is missing MIR payload fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->name != NULL ? inst->name : "(unnamed)");
            }
            return false;
        }
        if ((mir_intent_fact_requires_ast_step(inst)
             && !mir_instruction_is_intent_stmt(inst, "IntentStep"))
            && inst->arg1 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] intent fact '%s' is missing step link fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->name != NULL ? inst->name : "(unnamed)");
            }
            return false;
        }
        if (mir_intent_fact_requires_phase(inst) && inst->arg0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] intent fact '%s' is missing phase fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->name != NULL ? inst->name : "(unnamed)");
            }
            return false;
        }
        if (mir_intent_fact_requires_phase(inst) && inst->arg1 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] intent fact '%s' is missing step link fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->name != NULL ? inst->name : "(unnamed)");
            }
            return false;
        }
        if (mir_intent_fact_requires_phase(inst) && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] intent fact '%s' is missing expression payload fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->name != NULL ? inst->name : "(unnamed)");
            }
            return false;
        }
    }

    return true;
}
