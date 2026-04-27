/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend registry/state helpers.
 *
 * This file is only compiled when PGY_LLVM_ENABLED is defined.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

void
llvm_scope_push(LLVMGenCtx *ctx)
{
    if (ctx->scope_depth >= MAX_SCOPE_DEPTH) {
        llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_SCOPE_LIMIT, PGY_CAUSE_LLVM_SCOPE_CAPACITY, PGY_FIX_REFACTOR_OR_RAISE_LIMIT, "Scope depth overflow (max %d)", MAX_SCOPE_DEPTH);
        return;
    }
    ctx->scopes[ctx->scope_depth].count = 0;
    ctx->scope_depth++;
}

void
llvm_scope_pop(LLVMGenCtx *ctx)
{
    if (ctx->scope_depth > 0)
        ctx->scope_depth--;
}

void
llvm_scope_declare(LLVMGenCtx *ctx, const char *name,
                   LLVMValueRef alloca_val, LLVMTypeRef type)
{
    if (ctx->scope_depth == 0)
        return;

    LLVMScopeFrame *frame = &ctx->scopes[ctx->scope_depth - 1];
    if (frame->count >= MAX_SCOPE_VARS) {
        llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_SCOPE_LIMIT, PGY_CAUSE_LLVM_SCOPE_CAPACITY, PGY_FIX_REFACTOR_OR_RAISE_LIMIT, "Too many variables in scope (max %d)", MAX_SCOPE_VARS);
        return;
    }

    frame->entries[frame->count].name   = name;
    frame->entries[frame->count].alloca = alloca_val;
    frame->entries[frame->count].type   = type;
    frame->count++;
}

LLVMVarEntry *
llvm_scope_lookup(LLVMGenCtx *ctx, const char *name)
{
    for (int i = ctx->scope_depth - 1; i >= 0; i--) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        for (int j = frame->count - 1; j >= 0; j--) {
            if (strcmp(frame->entries[j].name, name) == 0)
                return &frame->entries[j];
        }
    }
    return NULL;
}

void
llvm_register_function(LLVMGenCtx *ctx, const char *name,
                       LLVMValueRef fn, LLVMTypeRef fn_type,
                       LLVMTypeRef ret_type)
{
    PGY_DYNARR_ENSURE(ctx->functions, ctx->func_count,
                      ctx->func_capacity, LLVMFuncEntry);

    ctx->functions[ctx->func_count].name     = name;
    ctx->functions[ctx->func_count].fn       = fn;
    ctx->functions[ctx->func_count].fn_type  = fn_type;
    ctx->functions[ctx->func_count].ret_type = ret_type;
    ctx->functions[ctx->func_count].is_action = false;
    ctx->functions[ctx->func_count].action_self_only = false;
    ctx->func_count++;
}

void
llvm_set_function_flags(LLVMGenCtx *ctx, const char *name,
                        bool is_action, bool action_self_only)
{
    LLVMFuncEntry *entry;

    if (ctx == NULL || name == NULL)
        return;
    entry = llvm_lookup_function(ctx, name);
    if (entry == NULL)
        return;
    entry->is_action = is_action;
    entry->action_self_only = action_self_only;
}

LLVMFuncEntry *
llvm_lookup_function(LLVMGenCtx *ctx, const char *name)
{
    for (int i = 0; i < ctx->func_count; i++) {
        if (strcmp(ctx->functions[i].name, name) == 0)
            return &ctx->functions[i];
    }
    return NULL;
}

LLVMFuncEntry *
llvm_lookup_or_create_function(LLVMGenCtx *ctx, const char *name,
                               LLVMTypeRef fallback_type,
                               LLVMTypeRef fallback_ret_type)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry != NULL)
        return entry;

    if (ctx->module == NULL)
        return NULL;

    LLVMValueRef fn = LLVMGetNamedFunction(ctx->module, name);
    LLVMTypeRef fn_type = NULL;
    LLVMTypeRef ret_type = fallback_ret_type;

    if (fn != NULL) {
        LLVMTypeRef value_type = LLVMTypeOf(fn);
        if (LLVMGetTypeKind(value_type) == LLVMPointerTypeKind)
            fn_type = LLVMGetElementType(value_type);
        else
            fn_type = value_type;
        if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
            ret_type = LLVMGetReturnType(fn_type);
    } else if (fallback_type != NULL && ctx->module != NULL) {
        fn = LLVMAddFunction(ctx->module, name, fallback_type);
        fn_type = fallback_type;
        if (ret_type == NULL)
            ret_type = LLVMGetReturnType(fallback_type);
    }

    if (fn == NULL || fn_type == NULL
        || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind || ret_type == NULL)
        return NULL;

    llvm_register_function(ctx, name, fn, fn_type, ret_type);
    return llvm_lookup_function(ctx, name);
}

void
llvm_mark_function_as_used(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry == NULL || entry->fn == NULL)
        return;

    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->context);
    LLVMTypeRef i8_ptr = LLVMPointerType(i8_type, 0);
    LLVMValueRef casted = LLVMConstBitCast(entry->fn, i8_ptr);

    LLVMValueRef used_elems[] = { casted };
    LLVMTypeRef used_ty = LLVMArrayType(i8_ptr, 1);
    LLVMValueRef used_init = LLVMConstArray(i8_ptr, used_elems, 1);

    const char *used_names[] = { "llvm.used", "llvm.compiler.used" };
    for (size_t i = 0; i < 2; i++) {
        const char *used_name = used_names[i];
        if (LLVMGetNamedGlobal(ctx->module, used_name) != NULL)
            continue;
        LLVMValueRef used_global = LLVMAddGlobal(ctx->module, used_ty, used_name);
        LLVMSetInitializer(used_global, used_init);
        LLVMSetGlobalConstant(used_global, true);
        LLVMSetSection(used_global, "llvm.metadata");
        LLVMSetLinkage(used_global, LLVMAppendingLinkage);
    }
}

void
llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type,
                       bool is_secure)
{
    PGY_DYNARR_ENSURE(ctx->slot_vars, ctx->slot_var_count,
                      ctx->slot_var_capacity, LLVMSlotVarEntry);

    ctx->slot_vars[ctx->slot_var_count].var_name   = var_name;
    ctx->slot_vars[ctx->slot_var_count].inner_type = inner_type;
    ctx->slot_vars[ctx->slot_var_count].released   = false;
    ctx->slot_vars[ctx->slot_var_count].is_secure  = is_secure;
    ctx->slot_var_count++;
}

void
llvm_register_view_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *source_slot, const char *inner_type,
                       bool is_move_token)
{
    PGY_DYNARR_ENSURE(ctx->view_vars, ctx->view_var_count,
                      ctx->view_var_capacity, LLVMViewVarEntry);

    ctx->view_vars[ctx->view_var_count].var_name = var_name;
    ctx->view_vars[ctx->view_var_count].source_slot = source_slot;
    ctx->view_vars[ctx->view_var_count].inner_type = inner_type;
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
    LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner != NULL ? inner : "Int");
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
}

LLVMTypeRef
llvm_set_struct_type(LLVMGenCtx *ctx, const char *inner)
{
    (void)inner;
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
    LLVMTypeRef elem_ty = pergyra_type_to_llvm(ctx, inner != NULL ? inner : "Int");
    LLVMTypeRef fields[] = {
        LLVMPointerType(elem_ty, 0), ctx->type_i64, ctx->type_i64,
        ctx->type_i64, ctx->type_i64
    };
    return LLVMStructTypeInContext(ctx->context, fields, 5, 0);
}

LLVMTypeRef
llvm_hashmap_struct_type(LLVMGenCtx *ctx, const char *value)
{
    LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, value != NULL ? value : "Int");
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
    PGY_DYNARR_ENSURE(ctx->future_vars, ctx->future_var_count,
                      ctx->future_var_capacity, LLVMFutureVarEntry);

    ctx->future_vars[ctx->future_var_count].var_name = var_name;
    ctx->future_vars[ctx->future_var_count].inner_type = inner_type;
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
    PGY_DYNARR_ENSURE(ctx->channel_vars, ctx->channel_var_count,
                      ctx->channel_var_capacity, LLVMChannelVarEntry);

    ctx->channel_vars[ctx->channel_var_count].var_name = var_name;
    ctx->channel_vars[ctx->channel_var_count].inner_type = inner_type;
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
    PGY_DYNARR_ENSURE(ctx->rc_vars, ctx->rc_var_count,
                      ctx->rc_var_capacity, LLVMRcVarEntry);

    ctx->rc_vars[ctx->rc_var_count].var_name = var_name;
    ctx->rc_vars[ctx->rc_var_count].inner_type = inner_type;
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
    PGY_DYNARR_ENSURE(ctx->weak_vars, ctx->weak_var_count,
                      ctx->weak_var_capacity, LLVMWeakVarEntry);

    ctx->weak_vars[ctx->weak_var_count].var_name = var_name;
    ctx->weak_vars[ctx->weak_var_count].inner_type = inner_type;
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

LLVMClassTypeEntry *
llvm_register_class(LLVMGenCtx *ctx, const char *class_name,
                    LLVMTypeRef struct_type,
                    bool is_subject,
                    bool is_pointer_self_host)
{
    PGY_DYNARR_ENSURE_RET(ctx->class_types, ctx->class_type_count,
                          ctx->class_type_capacity, LLVMClassTypeEntry);

    LLVMClassTypeEntry *entry = &ctx->class_types[ctx->class_type_count++];
    entry->class_name  = class_name;
    entry->struct_type = struct_type;
    entry->is_subject  = is_subject;
    entry->is_pointer_self_host = is_pointer_self_host;
    entry->is_immutable = false;
    entry->is_boundary_transfer_contract = false;
    entry->domain_kind = LLVM_DOMAIN_NONE;
    entry->sync_function_name = NULL;
    entry->field_count = 0;
    return entry;
}

void
llvm_class_add_field(LLVMClassTypeEntry *entry, const char *field_name,
                     LLVMTypeRef field_type, int index)
{
    llvm_class_add_field_ex(entry, field_name, field_type, index, false);
}

void
llvm_class_add_field_ex(LLVMClassTypeEntry *entry, const char *field_name,
                        LLVMTypeRef field_type, int index,
                        bool is_subject_slot)
{
    if (entry->field_count >= MAX_CLASS_FIELDS)
        return;

    entry->fields[entry->field_count].field_name = field_name;
    entry->fields[entry->field_count].field_type = field_type;
    entry->fields[entry->field_count].index      = index;
    entry->fields[entry->field_count].is_subject_slot = is_subject_slot;
    entry->field_count++;
}

LLVMClassTypeEntry *
llvm_lookup_class(LLVMGenCtx *ctx, const char *class_name)
{
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (strcmp(ctx->class_types[i].class_name, class_name) == 0)
            return &ctx->class_types[i];
    }
    return NULL;
}

LLVMClassTypeEntry *
llvm_lookup_class_by_struct_type(LLVMGenCtx *ctx, LLVMTypeRef struct_type)
{
    if (ctx == NULL || struct_type == NULL)
        return NULL;
    for (int i = 0; i < ctx->class_type_count; i++) {
        if (ctx->class_types[i].struct_type == struct_type)
            return &ctx->class_types[i];
    }
    return NULL;
}

int
llvm_class_field_index(LLVMClassTypeEntry *entry, const char *field_name)
{
    for (int i = 0; i < entry->field_count; i++) {
        if (strcmp(entry->fields[i].field_name, field_name) == 0)
            return entry->fields[i].index;
    }
    return -1;
}

void
llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                        const char *class_name)
{
    PGY_DYNARR_ENSURE(ctx->var_classes, ctx->var_class_count,
                      ctx->var_class_capacity, LLVMVarClassEntry);

    ctx->var_classes[ctx->var_class_count].var_name   = var_name;
    ctx->var_classes[ctx->var_class_count].class_name = class_name;
    ctx->var_class_count++;
}

const char *
llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->var_class_count - 1; i >= 0; i--) {
        if (strcmp(ctx->var_classes[i].var_name, var_name) == 0)
            return ctx->var_classes[i].class_name;
    }
    return NULL;
}

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

    for (int i = ctx->projection_borrow_count - 1; i >= 0; i--) {
        if (strcmp(ctx->projection_borrows[i].var_name, var_name) == 0)
            return &ctx->projection_borrows[i];
    }
    return NULL;
}

void
llvm_register_array_var(LLVMGenCtx *ctx, const char *var_name,
                        LLVMTypeRef elem_type, int64_t length)
{
    PGY_DYNARR_ENSURE(ctx->array_vars, ctx->array_var_count,
                      ctx->array_var_capacity, LLVMArrayVarEntry);

    ctx->array_vars[ctx->array_var_count].var_name = var_name;
    ctx->array_vars[ctx->array_var_count].elem_type = elem_type;
    ctx->array_vars[ctx->array_var_count].length = length;
    ctx->array_var_count++;
}

LLVMArrayVarEntry *
llvm_lookup_array_var(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->array_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->array_vars[i].var_name, var_name) == 0)
            return &ctx->array_vars[i];
    }
    return NULL;
}

void
llvm_register_list_var(LLVMGenCtx *ctx, const char *var_name,
                       const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->list_vars, ctx->list_var_count,
                      ctx->list_var_capacity, LLVMListVarEntry);
    ctx->list_vars[ctx->list_var_count].var_name = var_name;
    ctx->list_vars[ctx->list_var_count].inner_type = inner_type;
    ctx->list_var_count++;
}

const char *
llvm_lookup_list_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->list_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->list_vars[i].var_name, var_name) == 0)
            return ctx->list_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_set_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->set_vars, ctx->set_var_count,
                      ctx->set_var_capacity, LLVMSetVarEntry);
    ctx->set_vars[ctx->set_var_count].var_name = var_name;
    ctx->set_vars[ctx->set_var_count].inner_type = inner_type;
    ctx->set_var_count++;
}

const char *
llvm_lookup_set_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->set_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->set_vars[i].var_name, var_name) == 0)
            return ctx->set_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_queue_var(LLVMGenCtx *ctx, const char *var_name,
                        const char *inner_type)
{
    PGY_DYNARR_ENSURE(ctx->queue_vars, ctx->queue_var_count,
                      ctx->queue_var_capacity, LLVMQueueVarEntry);
    ctx->queue_vars[ctx->queue_var_count].var_name = var_name;
    ctx->queue_vars[ctx->queue_var_count].inner_type = inner_type;
    ctx->queue_var_count++;
}

const char *
llvm_lookup_queue_inner(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->queue_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->queue_vars[i].var_name, var_name) == 0)
            return ctx->queue_vars[i].inner_type;
    }
    return NULL;
}

void
llvm_register_map_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *key_type, const char *value_type)
{
    PGY_DYNARR_ENSURE(ctx->map_vars, ctx->map_var_count,
                      ctx->map_var_capacity, LLVMMapVarEntry);
    ctx->map_vars[ctx->map_var_count].var_name = var_name;
    ctx->map_vars[ctx->map_var_count].key_type = key_type;
    ctx->map_vars[ctx->map_var_count].value_type = value_type;
    ctx->map_var_count++;
}

const char *
llvm_lookup_map_key(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->map_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->map_vars[i].var_name, var_name) == 0)
            return ctx->map_vars[i].key_type;
    }
    return NULL;
}

const char *
llvm_lookup_map_value(LLVMGenCtx *ctx, const char *var_name)
{
    for (int i = ctx->map_var_count - 1; i >= 0; i--) {
        if (strcmp(ctx->map_vars[i].var_name, var_name) == 0)
            return ctx->map_vars[i].value_type;
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
