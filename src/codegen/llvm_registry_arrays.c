/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend Array/Slice registry owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <string.h>

void
llvm_register_array_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                LLVMValueRef binding,
                                LLVMTypeRef elem_type, int64_t length)
{
    char *owned_name;

    if (ctx == NULL)
        return;
    if (var_name == NULL || var_name[0] == '\0') {
        llvm_set_error(ctx, "LLVM Array registry requires a variable name");
        return;
    }

    PGY_DYNARR_ENSURE(ctx->array_vars, ctx->array_var_count,
                      ctx->array_var_capacity, LLVMArrayVarEntry);

    owned_name = pergyra_strdup(var_name);
    if (owned_name == NULL) {
        llvm_set_error(ctx, "out of memory copying LLVM Array registry name");
        return;
    }
    ctx->array_vars[ctx->array_var_count].var_name = owned_name;
    ctx->array_vars[ctx->array_var_count].binding = binding;
    ctx->array_vars[ctx->array_var_count].elem_type = elem_type;
    ctx->array_vars[ctx->array_var_count].length = length;
    ctx->array_var_count++;
}

void
llvm_register_array_var(LLVMGenCtx *ctx, const char *var_name,
                        LLVMTypeRef elem_type, int64_t length)
{
    LLVMVarEntry *entry = llvm_scope_lookup(ctx, var_name);
    llvm_register_array_var_binding(ctx, var_name,
        entry != NULL ? entry->alloca : NULL, elem_type, length);
}

LLVMArrayVarEntry *
llvm_lookup_array_var(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMVarEntry *entry;

    if (ctx == NULL || var_name == NULL
        || (entry = llvm_scope_lookup(ctx, var_name)) == NULL
        || entry->alloca == NULL) {
        return NULL;
    }
    for (int i = ctx->array_var_count - 1; i >= 0; i--) {
        if (ctx->array_vars[i].binding == entry->alloca
            && strcmp(ctx->array_vars[i].var_name, var_name) == 0)
            return &ctx->array_vars[i];
    }
    return NULL;
}

#endif
