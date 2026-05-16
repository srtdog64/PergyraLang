/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR resource view/borrow alias emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "transpiler_mir_resource_name_helpers.h"

#include <stdio.h>
#include <string.h>

#include "../common/string_compat.h"

void
llvm_mir_emit_borrow_view_alias(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    LLVMVarEntry *source_entry;
    const char *inner;
    bool is_secure;
    TranspilerMIRResourceOp op;

    if (inst == NULL || ctx == NULL || inst->name == NULL
        || inst->arg0 == NULL || inst->arg1 == NULL)
        return;
    op = transpiler_mir_resource_op_lookup(inst->name);
    if (op != TRANS_MIR_RESOURCE_OP_BORROW_READ
        && op != TRANS_MIR_RESOURCE_OP_BORROW_WRITE)
        return;

    source_entry = llvm_scope_lookup(ctx, inst->arg0);
    inner = llvm_lookup_slot_inner(ctx, inst->arg0);
    is_secure = llvm_lookup_slot_is_secure(ctx, inst->arg0);
    if (source_entry == NULL || inner == NULL)
        return;

    if (llvm_scope_lookup(ctx, inst->arg1) == NULL) {
        llvm_scope_declare(ctx, pergyra_strdup(inst->arg1),
                           source_entry->alloca, source_entry->type);
    }
    llvm_register_slot_var(ctx, pergyra_strdup(inst->arg1), inner, is_secure);
    if (is_secure) {
        char source_token_name[256];
        char view_token_name[256];
        LLVMVarEntry *token_entry;

        snprintf(source_token_name, sizeof(source_token_name), "%s_token",
                 inst->arg0);
        snprintf(view_token_name, sizeof(view_token_name), "%s_token",
                 inst->arg1);
        token_entry = llvm_scope_lookup(ctx, source_token_name);
        if (token_entry != NULL
            && llvm_scope_lookup(ctx, view_token_name) == NULL) {
            llvm_scope_declare(ctx, pergyra_strdup(view_token_name),
                               token_entry->alloca, token_entry->type);
        }
    }
}

#endif /* PGY_LLVM_ENABLED */
