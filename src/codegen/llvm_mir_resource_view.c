/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR resource view/borrow alias emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_resource_view.h"

#include "llvm_internal.h"
#include "transpiler_mir_resource_name_helpers.h"

#include <stdio.h>
#include <string.h>

#include "../common/string_compat.h"

bool
llvm_mir_def_is_resource_view_alias(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->arg1 != NULL
        && (strcmp(inst->arg1, "ViewRead") == 0
            || strcmp(inst->arg1, "ViewWrite") == 0);
}

static bool
llvm_mir_bind_resource_view_name(LLVMGenCtx *ctx,
                                 const char *name,
                                 LLVMValueRef alloca,
                                 LLVMTypeRef type,
                                 const char *inner,
                                 bool is_secure)
{
    char *owned_name;

    if (ctx == NULL || name == NULL || alloca == NULL || type == NULL
        || inner == NULL) {
        return false;
    }
    owned_name = pgy_arena_strdup(&ctx->persistent, name);
    if (owned_name == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR resource view alias out of memory");
        return false;
    }
    llvm_scope_declare(ctx, owned_name, alloca, type);
    llvm_register_slot_var_binding(ctx, owned_name, alloca, inner, is_secure);
    return !ctx->has_error;
}

static bool
llvm_mir_bind_resource_view_token_alias(LLVMGenCtx *ctx,
                                        const char *source_name,
                                        const char *alias_name)
{
    char source_token_name[256];
    char alias_token_name[256];
    char *owned_alias_token_name;
    LLVMVarEntry *token_entry;
    int source_written;
    int alias_written;

    if (ctx == NULL || source_name == NULL || alias_name == NULL)
        return true;
    source_written = snprintf(source_token_name, sizeof(source_token_name),
        "%s_token", source_name);
    alias_written = snprintf(alias_token_name, sizeof(alias_token_name),
        "%s_token", alias_name);
    if (source_written < 0
        || alias_written < 0
        || (size_t)source_written >= sizeof(source_token_name)
        || (size_t)alias_written >= sizeof(alias_token_name)) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR resource view token alias name is too long");
        return false;
    }
    token_entry = llvm_scope_lookup(ctx, source_token_name);
    if (token_entry == NULL)
        return true;
    owned_alias_token_name = pgy_arena_strdup(&ctx->persistent,
        alias_token_name);
    if (owned_alias_token_name == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR resource view token alias out of memory");
        return false;
    }
    llvm_scope_declare(ctx, owned_alias_token_name, token_entry->alloca,
        token_entry->type);
    return !ctx->has_error;
}

static const char *
llvm_mir_find_resource_view_source(const MIRBasicBlock *block,
                                   const char *alias_name)
{
    if (block == NULL || alias_name == NULL)
        return NULL;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *candidate = &block->instructions[i];
        if (candidate->kind != MIR_INST_RESOURCE_OP
            || candidate->name == NULL
            || candidate->arg0 == NULL
            || candidate->arg1 == NULL
            || strcmp(candidate->arg1, alias_name) != 0) {
            continue;
        }
        if (strcmp(candidate->name, "BorrowRead") == 0
            || strcmp(candidate->name, "BorrowWrite") == 0) {
            return candidate->arg0;
        }
    }
    return NULL;
}

bool
llvm_mir_bind_resource_view_def_alias(const MIRInstruction *inst,
                                      const MIRBasicBlock *block,
                                      LLVMGenCtx *ctx,
                                      LLVMMirVar *vars,
                                      size_t var_count)
{
    char alias_base[128];
    char source_base[128];
    const char *source_name;
    const char *inner;
    bool is_secure;
    LLVMMirVar *source_var;
    LLVMVarEntry *source_entry;
    LLVMValueRef source_alloca;
    LLVMTypeRef source_type;

    if (inst == NULL || ctx == NULL || inst->result_name == NULL
        || block == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR resource view alias requires block metadata");
        return false;
    }
    if (!llvm_mir_base_name_from_versioned(inst->result_name, alias_base,
            sizeof(alias_base))) {
        return true;
    }
    source_name = inst->use_count > 0 && inst->uses != NULL
        ? inst->uses[0]
        : NULL;
    if (source_name == NULL)
        source_name = llvm_mir_find_resource_view_source(block, alias_base);
    if (source_name == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR resource view alias '%s' requires source slot metadata",
            alias_base);
        return false;
    }
    if (!llvm_mir_base_name_from_versioned(source_name, source_base,
            sizeof(source_base))) {
        if (snprintf(source_base, sizeof(source_base), "%s", source_name)
            >= (int)sizeof(source_base)) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR resource view source name is too long");
            return false;
        }
    }

    source_var = llvm_mir_get_var_entry(vars, var_count, source_name);
    source_entry = llvm_scope_lookup(ctx, source_base);
    source_alloca = source_entry != NULL && source_entry->alloca != NULL
        ? source_entry->alloca
        : (source_var != NULL ? source_var->alloca : NULL);
    source_type = source_entry != NULL && source_entry->type != NULL
        ? source_entry->type
        : (source_var != NULL ? source_var->type : NULL);
    inner = llvm_lookup_slot_inner(ctx, source_base);
    if (source_alloca == NULL || source_type == NULL || inner == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR resource view alias '%s' cannot resolve source slot '%s'",
            alias_base, source_base);
        return false;
    }

    is_secure = llvm_lookup_slot_is_secure(ctx, source_base);
    if (!llvm_mir_bind_resource_view_name(ctx, alias_base, source_alloca,
            source_type, inner, is_secure))
        return false;
    if (!llvm_mir_bind_resource_view_name(ctx, inst->result_name,
            source_alloca, source_type, inner, is_secure))
        return false;
    if (is_secure) {
        if (!llvm_mir_bind_resource_view_token_alias(ctx, source_base,
                alias_base))
            return false;
        if (!llvm_mir_bind_resource_view_token_alias(ctx, source_base,
                inst->result_name))
            return false;
    }
    return true;
}

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
