#include "mir_fact_validate_internal.h"

#include <string.h>

#include "../parser/ast_analysis.h"

#define mir_strdup_fmt mir_fact_strdup_fmt

static bool
mir_def_source_requires_initializer_fact(const MIRInstruction *inst)
{
    return inst != NULL
        && (mir_instruction_source_is_local_decl(inst)
            || mir_instruction_source_is_assignment(inst));
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
        && mir_instruction_is_first_source_statement(inst)
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
mir_instruction_has_surface_payload_or_shape(const MIRInstruction *inst)
{
    return inst != NULL
        && (mir_instruction_source_payload(inst) != NULL
            || inst->expr0 != NULL
            || inst->expr1 != NULL
            || mir_instruction_has_source_location(inst));
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
        if (mir_instruction_resource_op_is_write(inst)
            && mir_instruction_source_matches_ast_type(inst, AST_CALL)
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
            && mir_instruction_source_is_defer_stmt(inst)
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
        if (inst->kind == MIR_INST_STMT
            && !mir_instruction_is_intent_semantic_carrier(inst)
            && (mir_instruction_source_payload(inst) == NULL
                || !mir_instruction_has_source_statement_order(inst))) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] STMT fallback is missing source statement inventory fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_STMT
            && !mir_instruction_source_stmt_fallback_is_allowed(inst)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] STMT fallback is outside allowed residual statement policy (%s)",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    mir_source_ast_type_name((ASTNodeType)
                        mir_instruction_source_ast_type_or(inst, AST_PROGRAM)));
            }
            return false;
        }
        if (mir_instruction_source_is_with_slot_claim(inst)) {
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
                || mir_instruction_source_payload(inst) == NULL
                || !mir_instruction_has_source_location(inst)
                || (!mir_instruction_source_is_local_decl(inst)
                    && !mir_instruction_source_is_assignment(inst)))) {
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
                || !mir_instruction_source_is_local_decl(inst)
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
            && mir_instruction_source_is_local_decl(inst)
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
            (ast_uses_thread_pool_surface(mir_instruction_source_payload(inst))
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
            (ast_uses_intent_observability_surface(
                 mir_instruction_source_payload(inst))
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
