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
    ctx->scopes[ctx->scope_depth].last_lookup_name = NULL;
    ctx->scopes[ctx->scope_depth].last_lookup = NULL;
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
    frame->last_lookup_name = name;
    frame->last_lookup = &frame->entries[frame->count];
    frame->count++;
}

LLVMVarEntry *
llvm_scope_lookup(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->scope_depth - 1; i >= 0; i--) {
        LLVMScopeFrame *frame = &ctx->scopes[i];
        if (frame->last_lookup_name != NULL
            && frame->last_lookup != NULL
            && strcmp(frame->last_lookup_name, name) == 0) {
            return frame->last_lookup;
        }
        for (int j = frame->count - 1; j >= 0; j--) {
            if (strcmp(frame->entries[j].name, name) == 0) {
                frame->last_lookup_name = frame->entries[j].name;
                frame->last_lookup = &frame->entries[j];
                return &frame->entries[j];
            }
        }
    }
    return NULL;
}

void
llvm_register_function(LLVMGenCtx *ctx, const char *name,
                       LLVMValueRef fn, LLVMTypeRef fn_type,
                       LLVMTypeRef ret_type)
{
    const char *stable_name = name;

    if (ctx == NULL)
        return;

    if (name != NULL) {
        char *copied_name = pgy_arena_strdup(&ctx->persistent, name);
        if (copied_name == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_OOM,
                PGY_CAUSE_LLVM_MEMORY_EXHAUSTED,
                PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT,
                "out of memory copying LLVM function registry name '%s'",
                name);
            return;
        }
        stable_name = copied_name;
    }

    PGY_DYNARR_ENSURE(ctx->functions, ctx->func_count,
                      ctx->func_capacity, LLVMFuncEntry);

    ctx->functions[ctx->func_count].name     = stable_name;
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
    if (ctx == NULL || name == NULL || ctx->functions == NULL)
        return NULL;

    for (int i = 0; i < ctx->func_count; i++) {
        if (ctx->functions[i].name != NULL
            && strcmp(ctx->functions[i].name, name) == 0)
            return &ctx->functions[i];
    }
    return NULL;
}

LLVMFuncEntry *
llvm_lookup_or_declare_function(LLVMGenCtx *ctx, const char *name,
                                LLVMTypeRef decl_type,
                                LLVMTypeRef decl_ret_type)
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
    LLVMTypeRef ret_type = decl_ret_type;

    if (fn != NULL) {
        LLVMTypeRef value_type = LLVMTypeOf(fn);
        if (LLVMGetTypeKind(value_type) == LLVMPointerTypeKind)
            fn_type = LLVMGetElementType(value_type);
        else
            fn_type = value_type;
        if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
            ret_type = LLVMGetReturnType(fn_type);
    } else if (decl_type != NULL && ctx->module != NULL) {
        fn = LLVMAddFunction(ctx->module, name, decl_type);
        fn_type = decl_type;
        if (ret_type == NULL)
            ret_type = LLVMGetReturnType(decl_type);
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
    if (ctx == NULL || class_name == NULL)
        return NULL;
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

LLVMClassTypeEntry *
llvm_lookup_vtable_class_with_method(LLVMGenCtx *ctx,
                                     const char *method_name,
                                     int *out_method_index)
{
    if (out_method_index != NULL)
        *out_method_index = -1;
    if (ctx == NULL || method_name == NULL)
        return NULL;

    for (int i = 0; i < ctx->class_type_count; i++) {
        const char *class_name = ctx->class_types[i].class_name;
        int method_index;

        if (class_name == NULL || strstr(class_name, "_vtable") == NULL)
            continue;
        method_index = llvm_class_field_index(&ctx->class_types[i], method_name);
        if (method_index < 0)
            continue;
        if (out_method_index != NULL)
            *out_method_index = method_index;
        return &ctx->class_types[i];
    }

    return NULL;
}

int
llvm_class_field_index(LLVMClassTypeEntry *entry, const char *field_name)
{
    if (entry == NULL || field_name == NULL)
        return -1;
    for (int i = 0; i < entry->field_count; i++) {
        if (entry->fields[i].field_name != NULL
            && strcmp(entry->fields[i].field_name, field_name) == 0)
            return entry->fields[i].index;
    }
    return -1;
}

LLVMTypeRef
llvm_class_field_type_at_index(LLVMClassTypeEntry *entry, int struct_index)
{
    if (entry == NULL)
        return NULL;
    for (int i = 0; i < entry->field_count; i++) {
        if (entry->fields[i].index == struct_index)
            return entry->fields[i].field_type;
    }
    return NULL;
}

int
llvm_class_field_count(LLVMClassTypeEntry *entry)
{
    return entry != NULL ? entry->field_count : 0;
}

const char *
llvm_class_field_name_at(LLVMClassTypeEntry *entry, int ordinal)
{
    if (entry == NULL || ordinal < 0 || ordinal >= entry->field_count)
        return NULL;
    return entry->fields[ordinal].field_name;
}

LLVMTypeRef
llvm_class_field_type_at(LLVMClassTypeEntry *entry, int ordinal)
{
    if (entry == NULL || ordinal < 0 || ordinal >= entry->field_count)
        return NULL;
    return entry->fields[ordinal].field_type;
}

int
llvm_class_field_struct_index_at(LLVMClassTypeEntry *entry, int ordinal)
{
    if (entry == NULL || ordinal < 0 || ordinal >= entry->field_count)
        return -1;
    return entry->fields[ordinal].index;
}

bool
llvm_class_field_is_subject_slot_at(LLVMClassTypeEntry *entry, int ordinal)
{
    if (entry == NULL || ordinal < 0 || ordinal >= entry->field_count)
        return false;
    return entry->fields[ordinal].is_subject_slot;
}

void
llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                        const char *class_name)
{
    if (ctx == NULL || var_name == NULL || class_name == NULL)
        return;

    PGY_DYNARR_ENSURE(ctx->var_classes, ctx->var_class_count,
                      ctx->var_class_capacity, LLVMVarClassEntry);

    ctx->var_classes[ctx->var_class_count].var_name   = var_name;
    ctx->var_classes[ctx->var_class_count].class_name = class_name;
    ctx->var_class_count++;
}

const char *
llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name)
{
    if (ctx == NULL || var_name == NULL)
        return NULL;

    for (int i = ctx->var_class_count - 1; i >= 0; i--) {
        if (ctx->var_classes[i].var_name != NULL
            && strcmp(ctx->var_classes[i].var_name, var_name) == 0)
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
    if (llvm_scope_lookup(ctx, var_name) == NULL)
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
        || llvm_scope_lookup(ctx, var_name) == NULL) {
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
