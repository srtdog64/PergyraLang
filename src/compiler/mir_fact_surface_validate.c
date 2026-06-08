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
mir_resource_kind_is_borrow_view(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->rir_op != NULL
        && (inst->rir_op->kind == RIR_OP_BORROW_READ
            || inst->rir_op->kind == RIR_OP_BORROW_WRITE);
}

static bool
mir_resource_kind_consumes_view(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->rir_op != NULL
        && (inst->rir_op->kind == RIR_OP_READ
            || inst->rir_op->kind == RIR_OP_WRITE
            || inst->rir_op->kind == RIR_OP_MOVE);
}

static bool
mir_resource_borrow_targets_view(const MIRInstruction *inst,
                                 const char *view_name)
{
    if (inst == NULL || view_name == NULL || view_name[0] == '\0')
        return false;
    return inst->arg1 != NULL && strcmp(inst->arg1, view_name) == 0;
}

static const char *
mir_prior_borrow_source_for_view(const MIRRoutine *routine,
                                 size_t before_block,
                                 size_t before_inst,
                                 const char *view_name)
{
    const char *source_slot = NULL;

    if (routine == NULL || view_name == NULL || view_name[0] == '\0')
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *candidate = &block->instructions[ii];
            if (bi == before_block && ii == before_inst)
                return source_slot;
            if (mir_resource_kind_is_borrow_view(candidate)
                && mir_resource_borrow_targets_view(candidate, view_name)
                && (candidate->resource_owner_slot_anchor != NULL
                    || candidate->arg0 != NULL)) {
                source_slot = candidate->resource_owner_slot_anchor != NULL
                    ? candidate->resource_owner_slot_anchor
                    : candidate->arg0;
            }
        }
    }
    return NULL;
}

static bool
mir_resource_owner_layout_is_slot_family(const MIRInstruction *inst)
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
        if (inst->kind == MIR_INST_RESOURCE_OP) {
            const char *expected_owner = mir_resource_kind_consumes_view(inst)
                ? mir_prior_borrow_source_for_view(
                    routine, block_index, i, inst->slot_anchor)
                : NULL;
            if (expected_owner != NULL || inst->resource_owner_requires_metadata) {
                if (inst->resource_owner_slot_anchor == NULL
                    || (expected_owner != NULL
                        && strcmp(inst->resource_owner_slot_anchor, expected_owner) != 0)
                    || !mir_resource_owner_layout_is_slot_family(inst)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] view-backed resource op is missing owner slot ABI metadata",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            i);
                    }
                    return false;
                }
            }
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
        if (inst->kind == MIR_INST_RESOURCE_OP
            && inst->rir_op != NULL
            && (inst->rir_op->kind == RIR_OP_AWAIT_LOCAL
                || inst->rir_op->kind == RIR_OP_AWAIT_REMOTE)) {
            const char *expected = inst->rir_op->kind == RIR_OP_AWAIT_LOCAL
                ? "Future"
                : "RemoteFuture";
            const char *actual = inst->type_layout != NULL
                ? inst->type_layout->abi_type_name
                : NULL;
            if (actual == NULL || strcmp(actual, expected) != 0) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] await resource op has invalid MIR ABI type layout fact",
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
