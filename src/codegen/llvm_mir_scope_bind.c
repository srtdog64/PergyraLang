/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_scope_bind.h"

#include "llvm_internal_api.h"

void
llvm_mir_bind_base_local_scope(LLVMGenCtx *ctx,
                               const char *base_name,
                               LLVMValueRef alloca,
                               LLVMTypeRef type,
                               const char *type_name)
{
    char *owned_base;
    LLVMClassTypeEntry *class_entry;

    if (ctx == NULL || base_name == NULL || alloca == NULL || type == NULL)
        return;

    owned_base = pgy_arena_strdup(&ctx->persistent, base_name);
    if (owned_base == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR block emission out of memory binding local scope");
        return;
    }
    llvm_scope_declare(ctx, owned_base, alloca, type);
    if (type_name != NULL && type_name[0] != '\0'
        && llvm_lookup_class(ctx, type_name) != NULL) {
        llvm_register_var_class(ctx, owned_base, type_name);
    } else {
        class_entry = llvm_lookup_class_by_struct_type(ctx, type);
        if (class_entry != NULL && class_entry->class_name != NULL)
            llvm_register_var_class(ctx, owned_base, class_entry->class_name);
    }
}

#endif
