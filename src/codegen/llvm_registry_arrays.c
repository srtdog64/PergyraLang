/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend Array/Slice registry owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

#include <string.h>

void
llvm_register_array_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                LLVMValueRef binding,
                                LLVMTypeRef elem_type, const char *elem_name,
                                int64_t length)
{
    char *owned_name;
    char *owned_elem_name = NULL;

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
    if (elem_name == NULL || elem_name[0] == '\0')
        elem_name = llvm_type_to_suffix(ctx, elem_type);
    if (elem_name != NULL && elem_name[0] != '\0') {
        owned_elem_name = pergyra_strdup(elem_name);
        if (owned_elem_name == NULL) {
            free(owned_name);
            llvm_set_error(ctx,
                "out of memory copying LLVM Array registry element name");
            return;
        }
    }
    ctx->array_vars[ctx->array_var_count].var_name = owned_name;
    ctx->array_vars[ctx->array_var_count].binding = binding;
    ctx->array_vars[ctx->array_var_count].elem_type = elem_type;
    ctx->array_vars[ctx->array_var_count].elem_name = owned_elem_name;
    ctx->array_vars[ctx->array_var_count].length = length;
    ctx->array_var_count++;
}

void
llvm_register_array_var(LLVMGenCtx *ctx, const char *var_name,
                        LLVMTypeRef elem_type, const char *elem_name,
                        int64_t length)
{
    LLVMVarEntry entry;
    bool has_entry = llvm_scope_lookup_snapshot(ctx, var_name, &entry);

    llvm_register_array_var_binding(ctx, var_name,
        has_entry ? entry.alloca : NULL, elem_type, elem_name, length);
}

LLVMArrayVarEntry *
llvm_lookup_array_var(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMVarEntry entry;

    if (ctx == NULL || var_name == NULL
        || !llvm_scope_lookup_snapshot(ctx, var_name, &entry)
        || entry.alloca == NULL) {
        return NULL;
    }
    for (int i = ctx->array_var_count - 1; i >= 0; i--) {
        if (ctx->array_vars[i].binding == entry.alloca
            && strcmp(ctx->array_vars[i].var_name, var_name) == 0)
            return &ctx->array_vars[i];
    }
    return NULL;
}

LLVMArrayVarEntry *
llvm_lookup_array_var_binding(LLVMGenCtx *ctx, const char *var_name,
                              LLVMValueRef binding)
{
    if (ctx == NULL || var_name == NULL || binding == NULL)
        return NULL;
    for (int i = ctx->array_var_count - 1; i >= 0; i--) {
        if (ctx->array_vars[i].binding == binding
            && strcmp(ctx->array_vars[i].var_name, var_name) == 0)
            return &ctx->array_vars[i];
    }
    return NULL;
}

const char *
llvm_array_access_element_class_name(LLVMGenCtx *ctx, ASTNode *array_access)
{
    ASTNode *array;
    LLVMTypeRef elem_type = NULL;

    if (ctx == NULL || array_access == NULL
        || array_access->type != AST_ARRAY_ACCESS) {
        return NULL;
    }

    array = ast_array_access_array(array_access);
    if (array != NULL && array->type == AST_IDENTIFIER
        && ast_identifier_name(array) != NULL) {
        LLVMArrayVarEntry *entry =
            llvm_lookup_array_var(ctx, ast_identifier_name(array));
        if (entry != NULL && entry->elem_name != NULL
            && llvm_lookup_class(ctx, entry->elem_name) != NULL) {
            return entry->elem_name;
        }
        if (entry != NULL)
            elem_type = entry->elem_type;
    }

    if (elem_type == NULL)
        elem_type = llvm_stmt_resolve_array_elem_type(ctx, array, NULL);
    if (elem_type != NULL) {
        LLVMClassTypeEntry *elem_cls =
            llvm_lookup_class_by_type(ctx, elem_type);
        if (elem_cls != NULL)
            return elem_cls->class_name;
    }
    return NULL;
}

#endif
