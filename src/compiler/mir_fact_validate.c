#include "mir_fact_validate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_analysis.h"
#include "mir_surface_usage.h"

static char *
mir_fact_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
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
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
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

static bool
mir_def_source_requires_initializer_fact(const MIRInstruction *inst)
{
    return inst != NULL
        && (inst->source_ast_type == AST_LET_DECL
            || inst->source_ast_type == AST_ASSIGNMENT);
}

static bool
mir_def_source_requires_channel_receive_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->expr0 != NULL
        && inst->expr0->type == AST_CHANNEL_RECV;
}

static bool
mir_def_source_requires_select_receive_emit(const MIRBasicBlock *block,
                                            const MIRInstruction *inst)
{
    return block != NULL
        && block->is_select_case_body
        && inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->has_source_statement_index
        && inst->source_statement_index == 0
        && mir_def_source_requires_channel_receive_emit(inst);
}

static bool
mir_claim_abi_type_is_slot_family(const MIRInstruction *inst)
{
    const char *abi_name = inst != NULL && inst->type_layout != NULL
        ? inst->type_layout->abi_type_name
        : NULL;
    return abi_name != NULL
        && (strncmp(abi_name, "Slot<", 5) == 0
            || strncmp(abi_name, "SecureSlot<", 11) == 0
            || strncmp(abi_name, "DeviceSlot<", 11) == 0);
}

static bool
mir_branch_shape_requires_source_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && (inst->branch_shape == MIR_BRANCH_MATCH_CASE
            || inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH);
}

static bool
mir_branch_source_ast_type_matches_shape(const MIRInstruction *inst)
{
    if (inst == NULL || !inst->has_source_location)
        return false;
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE)
        return inst->source_ast_type == AST_MATCH_CASE;
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        return inst->source_ast_type == AST_BLOCK;
    return true;
}

static bool
mir_instruction_has_surface_payload_or_shape(const MIRInstruction *inst)
{
    return inst != NULL
        && (inst->ast != NULL
            || inst->expr0 != NULL
            || inst->expr1 != NULL
            || inst->has_source_location);
}

static bool
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
    if (mir == NULL)
        return false;

    if (!mir_has_inventory_payload(mir))
        return true;

    if (!mir->has_inventory_surface_usage_facts) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program is missing inventory surface usage facts");
        }
        return false;
    }

    if (mir->inventory_uses_thread_pool_surface
        != mir_inventory_uses_thread_pool_surface(mir)) {
        if (error_message != NULL) {
            *error_message =
                mir_strdup_fmt("MIR program has stale thread-pool inventory surface usage fact");
        }
        return false;
    }

    if (mir->inventory_uses_intent_observability_surface
        != mir_inventory_uses_intent_observability_surface(mir)) {
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
        if (!inst->has_source_statement_index)
            continue;
        if (inst->source_statement_index >= block->source_statement_inventory.count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source statement index %zu exceeds inventory count %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->source_statement_index,
                    block->source_statement_inventory.count);
            }
            return false;
        }
    }

    return true;
}

bool
mir_validate_instruction_surface_usage(const MIRRoutine *routine,
                                       const MIRBasicBlock *block,
                                       size_t block_index,
                                       char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (!mir_validate_instruction_inventory_shape(routine,
                                                  block,
                                                  block_index,
                                                  "instruction surface usage validation",
                                                  error_message))
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (!mir_instruction_has_surface_payload_or_shape(inst))
            continue;
        if (inst->kind == MIR_INST_DEF
            && mir_def_source_requires_initializer_fact(inst)
            && !inst->requires_source_statement_emit) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] DEF is missing source-statement emit fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_DEF
            && mir_def_source_requires_initializer_fact(inst)
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] DEF is missing MIR initializer expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RESOURCE_OP
            && inst->name != NULL
            && strcmp(inst->name, "Write") == 0
            && inst->source_ast_type == AST_CALL
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] Write resource op is missing MIR value expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_STMT
            && inst->source_ast_type == AST_DEFER_STMT
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] defer statement is missing MIR body expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RESOURCE_OP
            && inst->name != NULL
            && strcmp(inst->name, "Claim") == 0
            && inst->source_ast_type == AST_WITH_STMT) {
            if (inst->type_layout == NULL
                || inst->type_layout->abi_type_name == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] with-slot Claim resource op is missing MIR ABI type layout fact",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i);
                }
                return false;
            }
            if (!mir_claim_abi_type_is_slot_family(inst)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] with-slot Claim resource op has invalid MIR ABI type layout fact",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i);
                }
                return false;
            }
        }
        if (inst->requires_source_statement_emit
            && (inst->kind != MIR_INST_DEF
                || inst->ast == NULL
                || !inst->has_source_location
                || (inst->source_ast_type != AST_LET_DECL
                    && inst->source_ast_type != AST_ASSIGNMENT))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-statement emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->requires_source_local_decl_emit
            && (!inst->requires_source_statement_emit
                || inst->kind != MIR_INST_DEF
                || inst->source_ast_type != AST_LET_DECL
                || inst->arg0 == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-local-decl emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->requires_source_statement_emit
            && inst->source_ast_type == AST_LET_DECL
            && !inst->requires_source_local_decl_emit) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-statement LET emit is missing local-decl fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (mir_def_source_requires_channel_receive_emit(inst)
            && !inst->requires_channel_receive_statement_emit) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] channel receive DEF is missing source-statement receive emit fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->requires_channel_receive_statement_emit
            && (!inst->requires_source_statement_emit
                || !mir_def_source_requires_channel_receive_emit(inst))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-statement receive emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (mir_def_source_requires_select_receive_emit(block, inst)
            && !inst->requires_select_receive_statement_emit) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] select receive DEF is missing select receive emit fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->requires_select_receive_statement_emit
            && (!inst->requires_channel_receive_statement_emit
                || !mir_def_source_requires_select_receive_emit(block, inst))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] select receive emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (!inst->has_surface_usage_facts) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has source payload without surface usage facts",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->uses_thread_pool_surface !=
            (ast_uses_thread_pool_surface(inst->ast)
             || ast_uses_thread_pool_surface(inst->expr0)
             || ast_uses_thread_pool_surface(inst->expr1))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has stale thread-pool surface usage fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->uses_intent_observability_surface !=
            (ast_uses_intent_observability_surface(inst->ast)
             || ast_uses_intent_observability_surface(inst->expr0)
             || ast_uses_intent_observability_surface(inst->expr1))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has stale intent observability surface usage fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
    }

    return true;
}

bool
mir_validate_terminator_provenance(const MIRRoutine *routine,
                                   const MIRBasicBlock *block,
                                   size_t block_index,
                                   char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (!mir_validate_instruction_inventory_shape(routine,
                                                  block,
                                                  block_index,
                                                  "terminator provenance validation",
                                                  error_message))
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_BRANCH && inst->kind != MIR_INST_RETURN)
            continue;
        if (!inst->has_source_terminator_kind) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has CFG terminator without HIR source terminator kind",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && inst->source_terminator_kind != HIR_BLOCK_BRANCH) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch source terminator is not HIR_BLOCK_BRANCH",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RETURN
            && inst->source_terminator_kind != HIR_BLOCK_RETURN) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] return source terminator is not HIR_BLOCK_RETURN",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && inst->branch_shape == MIR_BRANCH_EXPR
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch is missing MIR terminator expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && mir_branch_shape_requires_source_emit(inst)
            && !inst->requires_source_branch_emit) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch is missing source-branch emit fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->requires_source_branch_emit
            && (inst->kind != MIR_INST_BRANCH
                || inst->ast == NULL
                || !mir_branch_shape_requires_source_emit(inst)
                || !mir_branch_source_ast_type_matches_shape(inst))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-branch emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && mir_branch_shape_requires_source_emit(inst)
            && (inst->ast == NULL
                || !mir_branch_source_ast_type_matches_shape(inst))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-branch emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RETURN
            && inst->source_terminator_has_value
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] return is missing MIR terminator expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
    }

    return true;
}
