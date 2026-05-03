/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend resource/type registry helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"

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

void
llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type,
                       bool is_secure)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    PGY_DYNARR_ENSURE(ctx->slot_vars, ctx->slot_var_count,
                      ctx->slot_var_capacity, LLVMSlotVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->slot_vars[ctx->slot_var_count].var_name   = owned_var_name;
    ctx->slot_vars[ctx->slot_var_count].inner_type = owned_inner_type;
    ctx->slot_vars[ctx->slot_var_count].released   = false;
    ctx->slot_vars[ctx->slot_var_count].is_secure  = is_secure;
    ctx->slot_var_count++;
}

void
llvm_register_view_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *source_slot, const char *inner_type,
                       bool is_move_token)
{
    const char *owned_var_name;
    const char *owned_source_slot;
    const char *owned_inner_type;

    PGY_DYNARR_ENSURE(ctx->view_vars, ctx->view_var_count,
                      ctx->view_var_capacity, LLVMViewVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_source_slot = llvm_registry_keep_string(ctx, source_slot);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->view_vars[ctx->view_var_count].var_name = owned_var_name;
    ctx->view_vars[ctx->view_var_count].source_slot = owned_source_slot;
    ctx->view_vars[ctx->view_var_count].inner_type = owned_inner_type;
    ctx->view_vars[ctx->view_var_count].is_move_token = is_move_token;
    ctx->view_var_count++;
}

LLVMViewVarEntry *
llvm_lookup_view_var(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->view_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->view_vars[i].var_name, var_name) == 0)
            return &ctx->view_vars[i];
    }
    return NULL;
}

const char *
llvm_lookup_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].inner_type;
    }
    return NULL;
}

bool
llvm_lookup_slot_is_secure(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->slot_vars[i].var_name, var_name) == 0)
            return ctx->slot_vars[i].is_secure;
    }
    return false;
}

LLVMTypeRef
llvm_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->slot_type_Int;
    case PGY_TK_LONG:   return ctx->slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->slot_type_Double;
    case PGY_TK_BOOL:   return ctx->slot_type_Bool;
    case PGY_TK_STRING: return ctx->slot_type_String;
    default: {
        LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = { inner_ty, LLVMInt1TypeInContext(ctx->context) };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    }
}

LLVMTypeRef
llvm_pinned_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->pinned_slot_type_Int;
    case PGY_TK_LONG:   return ctx->pinned_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->pinned_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->pinned_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->pinned_slot_type_Bool;
    case PGY_TK_STRING: return ctx->pinned_slot_type_String;
    default: {
        LLVMTypeRef slot_ty = llvm_slot_struct_type(ctx, inner);
        LLVMTypeRef fields[] = {
            LLVMPointerType(slot_ty, 0), ctx->type_i1, ctx->type_i1
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    }
}

LLVMTypeRef
llvm_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->secure_slot_type_Int;
    case PGY_TK_LONG:   return ctx->secure_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->secure_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->secure_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->secure_slot_type_Bool;
    case PGY_TK_STRING: return ctx->secure_slot_type_String;
    default: {
        LLVMTypeRef inner_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = {
            inner_ty, LLVMInt1TypeInContext(ctx->context), ctx->type_i64
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    }
}

LLVMTypeRef
llvm_pinned_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->pinned_secure_slot_type_Int;
    case PGY_TK_LONG:   return ctx->pinned_secure_slot_type_Long;
    case PGY_TK_FLOAT:  return ctx->pinned_secure_slot_type_Float;
    case PGY_TK_DOUBLE: return ctx->pinned_secure_slot_type_Double;
    case PGY_TK_BOOL:   return ctx->pinned_secure_slot_type_Bool;
    case PGY_TK_STRING: return ctx->pinned_secure_slot_type_String;
    default: {
        LLVMTypeRef slot_ty = llvm_secure_slot_struct_type(ctx, inner);
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        LLVMTypeRef fields[] = {
            LLVMPointerType(slot_ty, 0),
            LLVMPointerType(token_ty, 0),
            ctx->type_i1,
            ctx->type_i1
        };
        return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
    }
    }
}

LLVMTypeRef
llvm_secure_token_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->secure_token_type_Int;
    case PGY_TK_LONG:   return ctx->secure_token_type_Long;
    case PGY_TK_FLOAT:  return ctx->secure_token_type_Float;
    case PGY_TK_DOUBLE: return ctx->secure_token_type_Double;
    case PGY_TK_BOOL:   return ctx->secure_token_type_Bool;
    case PGY_TK_STRING: return ctx->secure_token_type_String;
    default: {
        LLVMTypeRef fields[] = { ctx->type_i64, ctx->type_i1, ctx->type_i1 };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    }
}

LLVMTypeRef
llvm_array_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->array_type_Int;
    case PGY_TK_LONG:   return ctx->array_type_Long;
    case PGY_TK_FLOAT:  return ctx->array_type_Float;
    case PGY_TK_DOUBLE: return ctx->array_type_Double;
    case PGY_TK_BOOL:   return ctx->array_type_Bool;
    case PGY_TK_STRING: return ctx->array_type_String;
    default: {
        LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = {
            LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
    }
    }
}

LLVMTypeRef
llvm_slice_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    switch (pgy_classify_type(inner)) {
    case PGY_TK_INT:    return ctx->slice_type_Int;
    case PGY_TK_LONG:   return ctx->slice_type_Long;
    case PGY_TK_FLOAT:  return ctx->slice_type_Float;
    case PGY_TK_DOUBLE: return ctx->slice_type_Double;
    case PGY_TK_BOOL:   return ctx->slice_type_Bool;
    case PGY_TK_STRING: return ctx->slice_type_String;
    default: {
        LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
        LLVMTypeRef fields[] = {
            LLVMPointerType(elem_ty, 0), ctx->type_i64
        };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    }
}

LLVMTypeRef
llvm_list_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "List<T>: concrete element type metadata is required");
        return ctx->type_i32;
    }
    LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
}

LLVMTypeRef
llvm_set_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Set<T>: concrete element type metadata is required");
        return ctx->type_i32;
    }
    LLVMTypeRef fields[] = {
        ctx->type_i8ptr,
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
        ctx->type_i64,
        ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
}

LLVMTypeRef
llvm_queue_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Queue<T>: concrete element type metadata is required");
        return ctx->type_i32;
    }
    LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner);
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64,
        ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMTypeRef
llvm_hashmap_struct_type(LLVMGenCtx *ctx, const char *value)
{
    if (value == NULL || value[0] == '\0') {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "HashMap<K, V>: concrete value type metadata is required");
        return ctx->type_i32;
    }
    LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, value);
    LLVMTypeRef fields[] = {
        LLVMPointerType(ctx->type_i8ptr, 0),
        LLVMPointerType(value_ty, 0),
        LLVMPointerType(LLVMInt8TypeInContext(ctx->context), 0),
        ctx->type_i64,
        ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMValueRef
llvm_sizeof_type_i64(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    LLVMValueRef zero_ptr = LLVMConstNull(LLVMPointerType(type, 0));
    LLVMValueRef one_idx = LLVMConstInt(ctx->type_i32, 1, 0);
    LLVMValueRef gep = LLVMConstGEP2(type, zero_ptr, &one_idx, 1);
    return LLVMConstPtrToInt(gep, ctx->type_i64);
}

void
llvm_register_device_slot_var(LLVMGenCtx *ctx, const char *var_name,
                              const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->device_slot_vars, ctx->device_slot_var_count,
                      ctx->device_slot_var_capacity, LLVMDeviceSlotVarEntry);

    ctx->device_slot_vars[ctx->device_slot_var_count].var_name = var_name;
    ctx->device_slot_vars[ctx->device_slot_var_count].inner_type = inner_type;
    ctx->device_slot_vars[ctx->device_slot_var_count].released = false;
    ctx->device_slot_var_count++;
}

const char *
llvm_lookup_device_slot_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->device_slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->device_slot_vars[i].var_name, var_name) == 0)
            return ctx->device_slot_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_mark_device_slot_released(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->device_slot_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->device_slot_vars[i].var_name, var_name) == 0) {
            ctx->device_slot_vars[i].released = true;
            return;
        }
    }
}

LLVMVarEntry *
llvm_lookup_secure_token_var(LLVMGenCtx *ctx, const char *slot_name)
{
    char token_name[256];
    if (slot_name == NULL)
        return NULL;
    snprintf(token_name, sizeof(token_name), "%s_token", slot_name);
    return llvm_scope_lookup(ctx, token_name);
}

void
llvm_register_future_var(LLVMGenCtx *ctx, const char *var_name,
                         const char *inner_type,
                         bool is_remote)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    PGY_DYNARR_ENSURE(ctx->future_vars, ctx->future_var_count,
                      ctx->future_var_capacity, LLVMFutureVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->future_vars[ctx->future_var_count].var_name = owned_var_name;
    ctx->future_vars[ctx->future_var_count].inner_type = owned_inner_type;
    ctx->future_vars[ctx->future_var_count].is_remote = is_remote;
    ctx->future_var_count++;
}

const char *
llvm_lookup_future_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->future_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->future_vars[i].var_name, var_name) == 0)
            return ctx->future_vars[i].inner_type;
    }
    return NULL;
}

bool
llvm_lookup_future_is_remote(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->future_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->future_vars[i].var_name, var_name) == 0)
            return ctx->future_vars[i].is_remote;
    }
    return false;
}

void
llvm_register_channel_var(LLVMGenCtx *ctx, const char *var_name,
                          const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    PGY_DYNARR_ENSURE(ctx->channel_vars, ctx->channel_var_count,
                      ctx->channel_var_capacity, LLVMChannelVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->channel_vars[ctx->channel_var_count].var_name = owned_var_name;
    ctx->channel_vars[ctx->channel_var_count].inner_type = owned_inner_type;
    ctx->channel_var_count++;
}

const char *
llvm_lookup_channel_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->channel_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->channel_vars[i].var_name, var_name) == 0)
            return ctx->channel_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_rc_var(LLVMGenCtx *ctx, const char *var_name,
                     const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    PGY_DYNARR_ENSURE(ctx->rc_vars, ctx->rc_var_count,
                      ctx->rc_var_capacity, LLVMRcVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->rc_vars[ctx->rc_var_count].var_name = owned_var_name;
    ctx->rc_vars[ctx->rc_var_count].inner_type = owned_inner_type;
    ctx->rc_var_count++;
}

const char *
llvm_lookup_rc_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->rc_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->rc_vars[i].var_name, var_name) == 0)
            return ctx->rc_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_weak_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type)
{
    const char *owned_var_name;
    const char *owned_inner_type;

    PGY_DYNARR_ENSURE(ctx->weak_vars, ctx->weak_var_count,
                      ctx->weak_var_capacity, LLVMWeakVarEntry);
    owned_var_name = llvm_registry_keep_string(ctx, var_name);
    owned_inner_type = llvm_registry_keep_string(ctx, inner_type);
    if (owned_var_name == NULL || owned_inner_type == NULL)
        return;

    ctx->weak_vars[ctx->weak_var_count].var_name = owned_var_name;
    ctx->weak_vars[ctx->weak_var_count].inner_type = owned_inner_type;
    ctx->weak_var_count++;
}

const char *
llvm_lookup_weak_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->weak_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->weak_vars[i].var_name, var_name) == 0)
            return ctx->weak_vars[i].inner_type;
    }
    return NULL;
}

#endif
