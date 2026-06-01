/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend collection registry helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static char *
llvm_collection_registry_strdup(LLVMGenCtx *ctx, const char *value,
                                const char *what)
{
    char *copy;

    if (value == NULL)
        return NULL;
    copy = pergyra_strdup(value);
    if (copy == NULL && ctx != NULL && !ctx->has_error)
        llvm_set_error(ctx, what != NULL
            ? what : "out of memory copying LLVM collection registry string");
    return copy;
}

static LLVMValueRef
llvm_collection_active_binding(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMVarEntry *entry;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    entry = llvm_scope_lookup(ctx, var_name);
    return entry != NULL ? entry->alloca : NULL;
}

void
llvm_register_list_var_binding(LLVMGenCtx *ctx, const char *var_name,
                               LLVMValueRef binding,
                               const char *inner_type)
{
    char *owned_name;
    char *owned_inner;

    PGY_DYNARR_ENSURE(ctx->list_vars, ctx->list_var_count,
                      ctx->list_var_capacity, LLVMListVarEntry);
    owned_name = llvm_collection_registry_strdup(ctx, var_name,
        "out of memory copying LLVM List registry name");
    owned_inner = llvm_collection_registry_strdup(ctx, inner_type,
        "out of memory copying LLVM List registry inner type");
    if (owned_name == NULL || owned_inner == NULL) {
        free(owned_name);
        free(owned_inner);
        return;
    }
    ctx->list_vars[ctx->list_var_count].var_name = owned_name;
    ctx->list_vars[ctx->list_var_count].binding = binding;
    ctx->list_vars[ctx->list_var_count].inner_type = owned_inner;
    ctx->list_var_count++;
}

void
llvm_register_list_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type)
{
    llvm_register_list_var_binding(ctx, var_name,
        llvm_collection_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_list_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_collection_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->list_var_count - 1; i >= 0; i--) {
        if (ctx->list_vars[i].binding == binding
            && strcmp(ctx->list_vars[i].var_name, var_name) == 0)
            return ctx->list_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_set_var_binding(LLVMGenCtx *ctx, const char *var_name,
                              LLVMValueRef binding,
                              const char *inner_type)
{
    char *owned_name;
    char *owned_inner;

    PGY_DYNARR_ENSURE(ctx->set_vars, ctx->set_var_count,
                      ctx->set_var_capacity, LLVMSetVarEntry);
    owned_name = llvm_collection_registry_strdup(ctx, var_name,
        "out of memory copying LLVM Set registry name");
    owned_inner = llvm_collection_registry_strdup(ctx, inner_type,
        "out of memory copying LLVM Set registry inner type");
    if (owned_name == NULL || owned_inner == NULL) {
        free(owned_name);
        free(owned_inner);
        return;
    }
    ctx->set_vars[ctx->set_var_count].var_name = owned_name;
    ctx->set_vars[ctx->set_var_count].binding = binding;
    ctx->set_vars[ctx->set_var_count].inner_type = owned_inner;
    ctx->set_var_count++;
}

void
llvm_register_set_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *inner_type)
{
    llvm_register_set_var_binding(ctx, var_name,
        llvm_collection_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_set_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_collection_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->set_var_count - 1; i >= 0; i--) {
        if (ctx->set_vars[i].binding == binding
            && strcmp(ctx->set_vars[i].var_name, var_name) == 0)
            return ctx->set_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_queue_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                LLVMValueRef binding,
                                const char *inner_type)
{
    char *owned_name;
    char *owned_inner;

    PGY_DYNARR_ENSURE(ctx->queue_vars, ctx->queue_var_count,
                      ctx->queue_var_capacity, LLVMQueueVarEntry);
    owned_name = llvm_collection_registry_strdup(ctx, var_name,
        "out of memory copying LLVM Queue registry name");
    owned_inner = llvm_collection_registry_strdup(ctx, inner_type,
        "out of memory copying LLVM Queue registry inner type");
    if (owned_name == NULL || owned_inner == NULL) {
        free(owned_name);
        free(owned_inner);
        return;
    }
    ctx->queue_vars[ctx->queue_var_count].var_name = owned_name;
    ctx->queue_vars[ctx->queue_var_count].binding = binding;
    ctx->queue_vars[ctx->queue_var_count].inner_type = owned_inner;
    ctx->queue_var_count++;
}

void
llvm_register_queue_var(LLVMGenCtx *ctx, const char *var_name,
                        const char *inner_type)
{
    llvm_register_queue_var_binding(ctx, var_name,
        llvm_collection_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_queue_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_collection_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->queue_var_count - 1; i >= 0; i--) {
        if (ctx->queue_vars[i].binding == binding
            && strcmp(ctx->queue_vars[i].var_name, var_name) == 0)
            return ctx->queue_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_map_var_binding(LLVMGenCtx *ctx, const char *var_name,
                              LLVMValueRef binding,
                              const char *key_type,
                              const char *value_type)
{
    char *owned_name;
    char *owned_key;
    char *owned_value;

    PGY_DYNARR_ENSURE(ctx->map_vars, ctx->map_var_count,
                      ctx->map_var_capacity, LLVMMapVarEntry);
    owned_name = llvm_collection_registry_strdup(ctx, var_name,
        "out of memory copying LLVM HashMap registry name");
    owned_key = llvm_collection_registry_strdup(ctx, key_type,
        "out of memory copying LLVM HashMap registry key type");
    owned_value = llvm_collection_registry_strdup(ctx, value_type,
        "out of memory copying LLVM HashMap registry value type");
    if (owned_name == NULL || owned_key == NULL || owned_value == NULL) {
        free(owned_name);
        free(owned_key);
        free(owned_value);
        return;
    }
    ctx->map_vars[ctx->map_var_count].var_name = owned_name;
    ctx->map_vars[ctx->map_var_count].binding = binding;
    ctx->map_vars[ctx->map_var_count].key_type = owned_key;
    ctx->map_vars[ctx->map_var_count].value_type = owned_value;
    ctx->map_var_count++;
}

void
llvm_register_map_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *key_type, const char *value_type)
{
    llvm_register_map_var_binding(ctx, var_name,
        llvm_collection_active_binding(ctx, var_name), key_type, value_type);
}

const char *
llvm_lookup_map_key(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_collection_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->map_var_count - 1; i >= 0; i--) {
        if (ctx->map_vars[i].binding == binding
            && strcmp(ctx->map_vars[i].var_name, var_name) == 0)
            return ctx->map_vars[i].key_type;
    }
    return NULL;
}

const char *
llvm_lookup_map_value(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_collection_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->map_var_count - 1; i >= 0; i--) {
        if (ctx->map_vars[i].binding == binding
            && strcmp(ctx->map_vars[i].var_name, var_name) == 0)
            return ctx->map_vars[i].value_type;
    }
    return NULL;
}

#endif
