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
    "IntentBinding",
    "IntentCauses",
    "IntentCheck",
    "IntentDispatch",
    "IntentEval",
    "IntentInvalidationTarget",
    "IntentOutcomeBinding",
    "IntentParticipant",
    "IntentStep",
    "IntentStepCompleted",
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
        || mir_instruction_is_intent_stmt(inst, "IntentOutcomeBinding")
        || mir_instruction_is_intent_stmt(inst, "IntentDispatch");
}

static bool
mir_intent_fact_requires_phase(const MIRInstruction *inst)
{
    return mir_instruction_is_intent_stmt(inst, "IntentCheck")
        || mir_instruction_is_intent_stmt(inst, "IntentEval");
}

static bool
mir_intent_fact_is_binding_kind(const char *kind)
{
    return kind != NULL
        && (strcmp(kind, "participant") == 0
            || strcmp(kind, "value") == 0);
}

static bool
mir_intent_outcome_carrier_matches_ast(const MIRInstruction *inst)
{
    ASTNode *step;
    const char *binding_name;
    const char *type_name;
    uint32_t action_id;
    char expected_action_id[16];
    int written;

    if (!mir_instruction_is_intent_stmt(inst, "IntentOutcomeBinding")
        || !mir_instruction_source_is_intent_step(inst)) {
        return false;
    }
    step = inst->ast;
    binding_name = ast_intent_step_outcome_binding_name(step);
    type_name = ast_intent_step_outcome_binding_type_name(step);
    action_id = ast_intent_step_outcome_action_decl_syntax_id(step);
    written = snprintf(expected_action_id, sizeof(expected_action_id), "%u",
        (unsigned)action_id);
    return binding_name != NULL && binding_name[0] != '\0'
        && type_name != NULL && type_name[0] != '\0'
        && strcmp(type_name, "Void") != 0
        && action_id != 0
        && ast_intent_step_on_expr_count(step) == 1
        && written > 0 && (size_t)written < sizeof(expected_action_id)
        && inst->slot_anchor != NULL
        && strcmp(inst->slot_anchor, binding_name) == 0
        && inst->result_name != NULL
        && strcmp(inst->result_name, binding_name) == 0
        && inst->abi_type_name != NULL
        && strcmp(inst->abi_type_name, type_name) == 0
        && inst->arg0 != NULL
        && strcmp(inst->arg0, expected_action_id) == 0
        && inst->arg1 != NULL
        && ast_intent_step_name(step) != NULL
        && strcmp(inst->arg1, ast_intent_step_name(step)) == 0;
}

static bool
mir_intent_validate_outcome_step(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 ASTNode *step,
                                 char **error_message)
{
    const char *step_name;
    const char *binding_name;
    const char *binding_type;
    size_t step_fact_count = 0;
    size_t carrier_count = 0;
    size_t on_eval_count = 0;
    const MIRInstruction *carrier = NULL;
    const MIRInstruction *on_eval = NULL;

    if (step == NULL || step->type != AST_INTENT_STEP)
        return false;
    step_name = ast_intent_step_name(step);
    binding_name = ast_intent_step_outcome_binding_name(step);
    binding_type = ast_intent_step_outcome_binding_type_name(step);
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (mir_instruction_is_intent_stmt(inst, "IntentStep")
            && inst->arg0 != NULL && step_name != NULL
            && strcmp(inst->arg0, step_name) == 0) {
            step_fact_count++;
        }
        if (mir_instruction_is_intent_stmt(inst, "IntentOutcomeBinding")
            && mir_instruction_intent_step_matches(inst, step_name)) {
            carrier_count++;
            carrier = inst;
        }
        if (mir_instruction_is_intent_stmt(inst, "IntentEval")
            && mir_instruction_intent_step_matches(inst, step_name)
            && mir_instruction_intent_phase_matches(inst, "on")) {
            on_eval_count++;
            on_eval = inst;
        }
    }
    if (step_fact_count == 0)
        return true;
    if (step_fact_count != 1) {
        if (error_message != NULL) {
            *error_message = mir_intent_fact_strdup_fmt(
                "MIR routine '%s' block[%zu] step '%s' has duplicate IntentStep facts",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index, step_name != NULL ? step_name : "-");
        }
        return false;
    }
    if (binding_name == NULL) {
        if (carrier_count != 0) {
            if (error_message != NULL) {
                *error_message = mir_intent_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] step '%s' has an outcome carrier without a semantic binding",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index, step_name != NULL ? step_name : "-");
            }
            return false;
        }
        return true;
    }
    if (ast_intent_step_on_expr_count(step) != 1
        || carrier_count != 1 || on_eval_count != 1
        || carrier == NULL || on_eval == NULL
        || !mir_intent_outcome_carrier_matches_ast(carrier)
        || on_eval->result_name == NULL
        || strcmp(on_eval->result_name, binding_name) != 0
        || on_eval->abi_type_name == NULL || binding_type == NULL
        || strcmp(on_eval->abi_type_name, binding_type) != 0) {
        if (error_message != NULL) {
            *error_message = mir_intent_fact_strdup_fmt(
                "MIR routine '%s' block[%zu] step '%s' outcome binding requires one exact carrier and one matching on DEF",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index, step_name != NULL ? step_name : "-");
        }
        return false;
    }
    return true;
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
        if (mir_instruction_is_intent_stmt(inst, "IntentBinding")) {
            if (!mir_intent_fact_is_binding_kind(inst->slot_anchor)
                || inst->arg0 == NULL
                || inst->arg1 == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_intent_fact_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] IntentBinding is missing ordered binding kind, alias, or type metadata",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i);
                }
                return false;
            }
        }
        if (mir_instruction_is_intent_stmt(inst,
                                            "IntentOutcomeBinding")) {
            if (!mir_intent_outcome_carrier_matches_ast(inst)) {
                if (error_message != NULL) {
                    *error_message = mir_intent_fact_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] IntentOutcomeBinding has missing or mismatched step/name/type/action-id metadata",
                        routine->name != NULL
                            ? routine->name : "(anonymous)",
                        block_index, i);
                }
                return false;
            }
            for (size_t j = 0; j < i; j++) {
                const MIRInstruction *prior = &block->instructions[j];
                if (mir_instruction_is_intent_stmt(
                        prior, "IntentOutcomeBinding")
                    && prior->result_name != NULL
                    && inst->result_name != NULL
                    && strcmp(prior->result_name,
                              inst->result_name) == 0) {
                    if (error_message != NULL) {
                        *error_message = mir_intent_fact_strdup_fmt(
                            "MIR routine '%s' block[%zu] outcome binding '%s' is duplicated across intent steps",
                            routine->name != NULL
                                ? routine->name : "(anonymous)",
                            block_index, inst->result_name);
                    }
                    return false;
                }
            }
        }
    }

    if (routine->hir_routine != NULL
        && routine->hir_routine->ast != NULL
        && routine->hir_routine->ast->type == AST_INTENT_DECL) {
        ASTNode **steps;
        size_t step_count;

        steps = ast_intent_decl_steps(
            routine->hir_routine->ast, &step_count);
        for (size_t i = 0; i < step_count; i++) {
            if (!mir_intent_validate_outcome_step(
                    routine, block, block_index, steps[i],
                    error_message)) {
                return false;
            }
        }
    }

    return true;
}
