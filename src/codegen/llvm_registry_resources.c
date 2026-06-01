/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend resource/type registry owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"

#include <stdio.h>
#include <string.h>

static const char *
llvm_registry_keep_string(LLVMGenCtx *ctx, const char *value)
{
    size_t len;
    char *copy;

    if (value == NULL)
        return NULL;
    if (ctx == NULL)
        return value;

    len = strlen(value);
    copy = pgy_arena_alloc(&ctx->persistent, len + 1);
    if (copy == NULL) {
        if (!ctx->has_error)
            llvm_set_error(ctx, "out of memory copying LLVM registry string");
        return NULL;
    }
    memcpy(copy, value, len + 1);
    return copy;
}

static LLVMValueRef
llvm_resource_active_binding(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMVarEntry *entry;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    entry = llvm_scope_lookup(ctx, var_name);
    return entry != NULL ? entry->alloca : NULL;
}

void
llvm_register_slot_var_binding(LLVMGenCtx *ctx, const char *var_name,
                               LLVMValueRef binding,
                               const char *inner_type,
                               bool is_secure)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->slot_vars, ctx->slot_var_count,
                      ctx->slot_var_capacity, LLVMSlotVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->slot_vars[ctx->slot_var_count].var_name = owned_var_name;
    ctx->slot_vars[ctx->slot_var_count].binding = binding;
    ctx->slot_vars[ctx->slot_var_count].inner_type = owned_inner_type;
    ctx->slot_vars[ctx->slot_var_count].released = false;
    ctx->slot_vars[ctx->slot_var_count].is_secure = is_secure;
    ctx->slot_var_count++;
}

void
llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type,
                       bool is_secure)
{
    llvm_register_slot_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), inner_type, is_secure);
}

const char *
llvm_lookup_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (ctx->slot_vars[i].binding == binding
            && strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
    }
    return NULL;
}

bool
llvm_lookup_slot_is_secure(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return false;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return false;
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (ctx->slot_vars[i].binding == binding
            && strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].is_secure;
    }
    return false;
}

void
llvm_mark_slot_released(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return;
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (ctx->slot_vars[i].binding == binding
            && strcmp(ctx->slot_vars[i].var_name, var_name) == 0) {
            ctx->slot_vars[i].released = true;
            return;
        }
    }
}

void
llvm_register_view_var_binding(LLVMGenCtx *ctx, const char *var_name,
                               LLVMValueRef binding,
                               const char *source_slot,
                               const char *inner_type,
                               bool is_move_token)
{
    const char *owned_var_name;
    const char *owned_source_slot;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->view_vars, ctx->view_var_count,
                      ctx->view_var_capacity, LLVMViewVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_source_slot = llvm_registry_keep_string(ctx, source_slot);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->view_vars[ctx->view_var_count].var_name = owned_var_name;
    ctx->view_vars[ctx->view_var_count].binding = binding;
    ctx->view_vars[ctx->view_var_count].source_slot = owned_source_slot;
    ctx->view_vars[ctx->view_var_count].inner_type = owned_inner_type;
    ctx->view_vars[ctx->view_var_count].is_move_token = is_move_token;
    ctx->view_var_count++;
}

void
llvm_register_view_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *source_slot, const char *inner_type,
                       bool is_move_token)
{
    llvm_register_view_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), source_slot, inner_type,
        is_move_token);
}

LLVMViewVarEntry *
llvm_lookup_view_var(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->view_var_count - 1; i >= 0; i--) {
        if (ctx->view_vars[i].binding == binding
            && strcmp(ctx->view_vars[i].var_name, var_name) == 0)
            return &ctx->view_vars[i];
    }
    return NULL;
}

void
llvm_register_device_slot_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                      LLVMValueRef binding,
                                      const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->device_slot_vars, ctx->device_slot_var_count,
                      ctx->device_slot_var_capacity, LLVMDeviceSlotVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->device_slot_vars[ctx->device_slot_var_count].var_name = owned_var_name;
    ctx->device_slot_vars[ctx->device_slot_var_count].binding = binding;
    ctx->device_slot_vars[ctx->device_slot_var_count].inner_type =
        owned_inner_type;
    ctx->device_slot_vars[ctx->device_slot_var_count].released = false;
    ctx->device_slot_var_count++;
}

void
llvm_register_device_slot_var(LLVMGenCtx *ctx, const char *var_name,
                              const char *inner_type)
{
    llvm_register_device_slot_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_device_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->device_slot_var_count - 1; i >= 0; i--) {
        if (ctx->device_slot_vars[i].binding == binding
            && strcmp(ctx->device_slot_vars[i].var_name, var_name) == 0)
            return ctx->device_slot_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_mark_device_slot_released(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return;
    for (int i = ctx->device_slot_var_count - 1; i >= 0; i--) {
        if (ctx->device_slot_vars[i].binding == binding
            && strcmp(ctx->device_slot_vars[i].var_name, var_name) == 0) {
            ctx->device_slot_vars[i].released = true;
            return;
        }
    }
}

LLVMVarEntry *
llvm_lookup_secure_token_var(LLVMGenCtx *ctx, const char *slot_name)
{
    char token_name[256];
    int written;

    if (ctx == NULL || slot_name == NULL)
        return NULL;
    written = snprintf(token_name, sizeof(token_name), "%s_token", slot_name);
    if (written < 0 || (size_t)written >= sizeof(token_name))
        return NULL;
    return llvm_scope_lookup(ctx, token_name);
}

void
llvm_register_future_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                 LLVMValueRef binding,
                                 const char *inner_type,
                                 bool is_remote)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->future_vars, ctx->future_var_count,
                      ctx->future_var_capacity, LLVMFutureVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->future_vars[ctx->future_var_count].var_name = owned_var_name;
    ctx->future_vars[ctx->future_var_count].binding = binding;
    ctx->future_vars[ctx->future_var_count].inner_type = owned_inner_type;
    ctx->future_vars[ctx->future_var_count].is_remote = is_remote;
    ctx->future_var_count++;
}

void
llvm_register_future_var(LLVMGenCtx *ctx, const char *var_name,
                         const char *inner_type,
                         bool is_remote)
{
    llvm_register_future_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), inner_type, is_remote);
}

const char *
llvm_lookup_future_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->future_var_count - 1; i >= 0; i--) {
        if (ctx->future_vars[i].binding == binding
            && strcmp(ctx->future_vars[i].var_name, var_name) == 0)
            return ctx->future_vars[i].inner_type;
    }
    return NULL;
}

bool
llvm_lookup_future_is_remote(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return false;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return false;
    for (int i = ctx->future_var_count - 1; i >= 0; i--) {
        if (ctx->future_vars[i].binding == binding
            && strcmp(ctx->future_vars[i].var_name, var_name) == 0)
            return ctx->future_vars[i].is_remote;
    }
    return false;
}

void
llvm_register_channel_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                  LLVMValueRef binding,
                                  const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->channel_vars, ctx->channel_var_count,
                      ctx->channel_var_capacity, LLVMChannelVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->channel_vars[ctx->channel_var_count].var_name = owned_var_name;
    ctx->channel_vars[ctx->channel_var_count].binding = binding;
    ctx->channel_vars[ctx->channel_var_count].inner_type = owned_inner_type;
    ctx->channel_var_count++;
}

void
llvm_register_channel_var(LLVMGenCtx *ctx, const char *var_name,
                          const char *inner_type)
{
    llvm_register_channel_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_channel_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->channel_var_count - 1; i >= 0; i--) {
        if (ctx->channel_vars[i].binding == binding
            && strcmp(ctx->channel_vars[i].var_name, var_name) == 0)
            return ctx->channel_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_rc_var_binding(LLVMGenCtx *ctx, const char *var_name,
                             LLVMValueRef binding,
                             const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->rc_vars, ctx->rc_var_count,
                      ctx->rc_var_capacity, LLVMRcVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->rc_vars[ctx->rc_var_count].var_name = owned_var_name;
    ctx->rc_vars[ctx->rc_var_count].binding = binding;
    ctx->rc_vars[ctx->rc_var_count].inner_type = owned_inner_type;
    ctx->rc_var_count++;
}

void
llvm_register_rc_var(LLVMGenCtx *ctx, const char *var_name,
                     const char *inner_type)
{
    llvm_register_rc_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_rc_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->rc_var_count - 1; i >= 0; i--) {
        if (ctx->rc_vars[i].binding == binding
            && strcmp(ctx->rc_vars[i].var_name, var_name) == 0)
            return ctx->rc_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_weak_var_binding(LLVMGenCtx *ctx, const char *var_name,
                               LLVMValueRef binding,
                               const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    if (ctx == NULL || var_name == NULL || inner_type == NULL)
        return;
    PGY_DYNARR_ENSURE(ctx->weak_vars, ctx->weak_var_count,
                      ctx->weak_var_capacity, LLVMWeakVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->weak_vars[ctx->weak_var_count].var_name = owned_var_name;
    ctx->weak_vars[ctx->weak_var_count].binding = binding;
    ctx->weak_vars[ctx->weak_var_count].inner_type = owned_inner_type;
    ctx->weak_var_count++;
}

void
llvm_register_weak_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type)
{
    llvm_register_weak_var_binding(ctx, var_name,
        llvm_resource_active_binding(ctx, var_name), inner_type);
}

const char *
llvm_lookup_weak_inner(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMValueRef binding;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    binding = llvm_resource_active_binding(ctx, var_name);
    if (binding == NULL)
        return NULL;
    for (int i = ctx->weak_var_count - 1; i >= 0; i--) {
        if (ctx->weak_vars[i].binding == binding
            && strcmp(ctx->weak_vars[i].var_name, var_name) == 0)
            return ctx->weak_vars[i].inner_type;
    }
    return NULL;
}

#endif
