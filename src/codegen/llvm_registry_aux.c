/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend auxiliary registry helpers split from
 * llvm_registry.c (2026-06 owner-size closure):
 *   projection borrows, callable vars/signatures, enum variants.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

void
llvm_register_projection_borrow(LLVMGenCtx *ctx,
                                const char *var_name,
                                const char *class_name,
                                const char *source_name)
{
    if (ctx == NULL || var_name == NULL || class_name == NULL || source_name == NULL)
        return;

    PGY_DYNARR_ENSURE(ctx->projection_borrows, ctx->projection_borrow_count,
                      ctx->projection_borrow_capacity, LLVMProjectionBorrowEntry);

    ctx->projection_borrows[ctx->projection_borrow_count].var_name = var_name;
    ctx->projection_borrows[ctx->projection_borrow_count].class_name = class_name;
    ctx->projection_borrows[ctx->projection_borrow_count].source_name = source_name;
    ctx->projection_borrow_count++;
}

LLVMProjectionBorrowEntry *
llvm_lookup_projection_borrow(LLVMGenCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return NULL;
    if (!llvm_scope_contains(ctx, var_name))
        return NULL;

    for (int i = ctx->projection_borrow_count - 1; i >= 0; i--) {
        if (strcmp(ctx->projection_borrows[i].var_name, var_name) == 0)
            return &ctx->projection_borrows[i];
    }
    return NULL;
}

void
llvm_register_callable_var(LLVMGenCtx *ctx, const char *var_name,
                           ASTNode *type_node)
{
    PGY_DYNARR_ENSURE(ctx->callable_vars, ctx->callable_var_count,
                      ctx->callable_var_capacity, LLVMCallableVarEntry);
    ctx->callable_vars[ctx->callable_var_count].var_name = var_name;
    ctx->callable_vars[ctx->callable_var_count].type_node = type_node;
    ctx->callable_vars[ctx->callable_var_count].param_types = NULL;
    ctx->callable_vars[ctx->callable_var_count].param_count = 0;
    ctx->callable_vars[ctx->callable_var_count].return_type = NULL;
    ctx->callable_var_count++;
}

void
llvm_register_callable_signature(LLVMGenCtx *ctx, const char *var_name,
                                 size_t param_count,
                                 ASTNode *const *param_types,
                                 ASTNode *return_type)
{
    ASTNode **stored_param_types = NULL;

    if (ctx == NULL || var_name == NULL)
        return;

    if (param_count > 0) {
        stored_param_types = pgy_arena_calloc(&ctx->persistent,
                                              param_count * sizeof(ASTNode *));
        if (stored_param_types == NULL) {
            llvm_set_error(ctx, "out of memory registering callable signature");
            return;
        }
        for (size_t i = 0; i < param_count; i++)
            stored_param_types[i] = param_types != NULL ? param_types[i] : NULL;
    }

    PGY_DYNARR_ENSURE(ctx->callable_vars, ctx->callable_var_count,
                      ctx->callable_var_capacity, LLVMCallableVarEntry);
    ctx->callable_vars[ctx->callable_var_count].var_name = var_name;
    ctx->callable_vars[ctx->callable_var_count].type_node = NULL;
    ctx->callable_vars[ctx->callable_var_count].param_types = stored_param_types;
    ctx->callable_vars[ctx->callable_var_count].param_count = param_count;
    ctx->callable_vars[ctx->callable_var_count].return_type = return_type;
    ctx->callable_var_count++;
}

LLVMCallableVarEntry *
llvm_lookup_callable_entry(LLVMGenCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL
        || !llvm_scope_contains(ctx, var_name)) {
        return NULL;
    }
    for (int i = ctx->callable_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->callable_vars[i].var_name, var_name) == 0)
            return &ctx->callable_vars[i];
    }
    return NULL;
}

ASTNode *
llvm_lookup_callable_var(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMCallableVarEntry *entry = llvm_lookup_callable_entry(ctx, var_name);
    return entry != NULL ? entry->type_node : NULL;
}

void
llvm_register_enum_variant(LLVMGenCtx *ctx, const char *enum_name,
                           const char *variant_name, int value)
{
    PGY_DYNARR_ENSURE(ctx->enum_variants, ctx->enum_variant_count,
                      ctx->enum_variant_capacity, LLVMEnumVariantEntry);

    ctx->enum_variants[ctx->enum_variant_count].enum_name = enum_name;
    ctx->enum_variants[ctx->enum_variant_count].variant_name = variant_name;
    ctx->enum_variants[ctx->enum_variant_count].value = value;
    ctx->enum_variant_count++;
}

bool
llvm_enum_type_exists(LLVMGenCtx *ctx, const char *enum_name)
{
    if (ctx == NULL || enum_name == NULL)
        return false;
    for (int i = ctx->enum_variant_count - 1; i >= 0; i--) {
        if (ctx->enum_variants[i].enum_name != NULL
            && strcmp(ctx->enum_variants[i].enum_name, enum_name) == 0) {
            return true;
        }
    }
    return false;
}

LLVMEnumVariantEntry *
llvm_lookup_enum_variant(LLVMGenCtx *ctx, const char *variant_name)
{
    for (int i = ctx->enum_variant_count - 1; i >= 0; i--) {
        if (strcmp(ctx->enum_variants[i].variant_name, variant_name) == 0)
            return &ctx->enum_variants[i];
    }
    return NULL;
}

LLVMEnumVariantEntry *
llvm_lookup_enum_variant_qualified(LLVMGenCtx *ctx, const char *enum_name,
                                   const char *variant_name)
{
    for (int i = ctx->enum_variant_count - 1; i >= 0; i--) {
        if (strcmp(ctx->enum_variants[i].enum_name, enum_name) == 0
            && strcmp(ctx->enum_variants[i].variant_name, variant_name) == 0)
            return &ctx->enum_variants[i];
    }
    return NULL;
}

#endif
