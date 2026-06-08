/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend - MIR-backed intent metadata readers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

const char *
llvm_find_mir_intent_meta_arg(const MIRRoutine *routine,
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

bool
llvm_mir_intent_has_stmt(const MIRRoutine *routine,
                         const char *step_name,
                         const char *inst_name,
                         const char *arg0)
{
    if (routine == NULL || inst_name == NULL)
        return false;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, inst_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            if (arg0 != NULL) {
                if (payload == NULL || strcmp(payload, arg0) != 0)
                    continue;
            }
            return true;
        }
    }

    return false;
}

size_t
llvm_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                    LLVMGenCtx *ctx,
                                    const char *step_name,
                                    const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL || ctx == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentWho"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            count++;
        }
    }

    if (count == 0)
        return 0;
    aliases = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    if (aliases == NULL)
        return 0;

    count = 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentWho"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

size_t
llvm_collect_mir_intent_authorized_aliases(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           const char *step_name,
                                           const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL || ctx == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentAuthorizedBy"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            count++;
        }
    }

    if (count == 0)
        return 0;
    aliases = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    if (aliases == NULL)
        return 0;

    count = 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentAuthorizedBy"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

size_t
llvm_collect_mir_intent_bindings(const MIRRoutine *routine,
                                 LLVMGenCtx *ctx,
                                 IntentBindingMetadataView *bindings_out)
{
    const char **kinds = NULL;
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;

    if (bindings_out != NULL) {
        bindings_out->kinds = NULL;
        bindings_out->aliases = NULL;
        bindings_out->types = NULL;
        bindings_out->count = 0;
        bindings_out->owns_storage = false;
    }
    if (routine == NULL || ctx == NULL || bindings_out == NULL) {
        return 0;
    }

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentBinding"))
                continue;
            if (inst->slot_anchor == NULL || payload == NULL
                || inst->arg1 == NULL) {
                continue;
            }
            count++;
        }
    }

    if (count == 0)
        return 0;
    kinds = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    aliases = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    types = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    if (kinds == NULL || aliases == NULL || types == NULL)
        return 0;

    count = 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentBinding"))
                continue;
            if (inst->slot_anchor == NULL || payload == NULL
                || inst->arg1 == NULL) {
                continue;
            }
            kinds[count] = inst->slot_anchor;
            aliases[count] = payload;
            types[count] = inst->arg1;
            count++;
        }
    }

    bindings_out->kinds = kinds;
    bindings_out->aliases = aliases;
    bindings_out->types = types;
    bindings_out->count = count;
    bindings_out->owns_storage = false;
    return count;
}

#endif
