/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA classification helpers.
 */

#include <string.h>

#include "codegen_slot_type_policy.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_ssa_utils.h"

bool
transpiler_name_is_token_local(const char *name)
{
    const char *tag;
    if (name == NULL)
        return false;
    tag = strstr(name, "_token");
    if (tag == NULL)
        return false;
    tag += 6;
    while (*tag != '\0') {
        if (*tag < '0' || *tag > '9')
            return false;
        tag++;
    }
    return true;
}

bool
transpiler_type_name_is_slot_like(const char *type_name)
{
    return pgy_codegen_type_name_is_slot_family(type_name);
}

bool
transpiler_type_name_is_view_like(const char *type_name)
{
    return pgy_codegen_type_name_is_view(type_name);
}

bool
transpiler_type_name_is_claim_shape(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return transpiler_type_name_is_slot_like(type_name)
        || strncmp(type_name, "Token<", 6) == 0;
}

bool
transpiler_block_has_claim_for_slot_local(const MIRBasicBlock *block,
                                          const char *slot_name)
{
    if (block == NULL || slot_name == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        const char *claim_name;
        TranspilerMIRResourceOp op;
        if (inst->kind != MIR_INST_RESOURCE_OP) {
            continue;
        }
        op = transpiler_mir_resource_op_lookup(inst->name);
        if (op != TRANS_MIR_RESOURCE_OP_CLAIM) {
            continue;
        }
        claim_name = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
        if (claim_name != NULL && strcmp(claim_name, slot_name) == 0)
            return true;
    }
    return false;
}

bool
transpiler_mir_routine_has_explicit_cfg(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    if (routine->block_count > 1)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_BRANCH
                || inst->kind == MIR_INST_PHI
                || inst->kind == MIR_INST_CLEANUP_EDGE) {
                return true;
            }
        }
    }
    return false;
}

bool
transpiler_versioned_name_list_contains(const char **names,
                                        size_t count,
                                        const char *name)
{
    if (names == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

bool
transpiler_versioned_name_list_add(const char **names,
                                   size_t *count,
                                   size_t capacity,
                                   const char *name)
{
    if (names == NULL || count == NULL || name == NULL)
        return false;
    if (transpiler_versioned_name_list_contains(names, *count, name))
        return true;
    if (*count >= capacity)
        return false;
    names[(*count)++] = name;
    return true;
}

bool
transpiler_c_type_uses_scalar_zero(const char *c_type)
{
    if (c_type == NULL)
        return true;
    if (strchr(c_type, '*') != NULL)
        return true;
    return strcmp(c_type, "int") == 0
           || strcmp(c_type, "int32_t") == 0
           || strcmp(c_type, "int64_t") == 0
           || strcmp(c_type, "size_t") == 0
           || strcmp(c_type, "bool") == 0
           || strcmp(c_type, "float") == 0
           || strcmp(c_type, "double") == 0;
}
