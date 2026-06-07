/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM native backend: Phase 1 expressions, functions, and control flow.
 *
 * This file is only compiled when PGY_LLVM_ENABLED is defined.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_internal.h"
#include "../common/string_compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Transforms/PassBuilder.h>

/* Types, context, and constants are defined in llvm_internal.h */

/* =================================================================
 * Context lifecycle
 * ================================================================= */

LLVMGenCtx *
llvm_ctx_create(const char *module_name)
{
    LLVMGenCtx *ctx = calloc(1, sizeof(LLVMGenCtx));
    if (ctx == NULL)
        return NULL;

    ctx->context = LLVMContextCreate();
    ctx->module  = LLVMModuleCreateWithNameInContext(module_name, ctx->context);
    ctx->builder = LLVMCreateBuilderInContext(ctx->context);

    ctx->type_i32   = LLVMInt32TypeInContext(ctx->context);
    ctx->type_i64   = LLVMInt64TypeInContext(ctx->context);
    ctx->type_f32   = LLVMFloatTypeInContext(ctx->context);
    ctx->type_f64   = LLVMDoubleTypeInContext(ctx->context);
    ctx->type_i1    = LLVMInt1TypeInContext(ctx->context);
    ctx->type_i8ptr = LLVMPointerTypeInContext(ctx->context, 0);
    ctx->type_void  = LLVMVoidTypeInContext(ctx->context);

    pgy_arena_init(&ctx->scratch, 0);
    pgy_arena_init(&ctx->persistent, 0);

    ctx->scope_depth   = 0;
    ctx->tmp_counter   = 0;
    ctx->func_count    = 0;
    ctx->slot_var_count = 0;
    ctx->class_type_count = 0;
    ctx->event_type_count = 0;
    ctx->lambda_counter = 0;
    ctx->var_class_count = 0;
    ctx->array_var_count = 0;
    ctx->enum_variant_count = 0;
    ctx->loop_depth = 0;
    ctx->current_ret_type = ctx->type_i32;
    ctx->current_function_ret_type = ctx->type_i32;
    ctx->current_return_type_name = NULL;
    ctx->current_return_callable_type = NULL;
    ctx->current_within_zone_name = NULL;
    ctx->result_spec_count = 0;
    ctx->expected_type_name = NULL;

    /* Slot struct types: { value_type, i1 (occupied) } */
    {
        LLVMTypeRef fields_int[] = { ctx->type_i32, ctx->type_i1 };
        ctx->slot_type_Int = LLVMStructCreateNamed(ctx->context, "PgySlot_Int");
        LLVMStructSetBody(ctx->slot_type_Int, fields_int, 2, 0);

        LLVMTypeRef fields_long[] = { ctx->type_i64, ctx->type_i1 };
        ctx->slot_type_Long = LLVMStructCreateNamed(ctx->context, "PgySlot_Long");
        LLVMStructSetBody(ctx->slot_type_Long, fields_long, 2, 0);

        LLVMTypeRef fields_float[] = { ctx->type_f32, ctx->type_i1 };
        ctx->slot_type_Float = LLVMStructCreateNamed(ctx->context, "PgySlot_Float");
        LLVMStructSetBody(ctx->slot_type_Float, fields_float, 2, 0);

        LLVMTypeRef fields_double[] = { ctx->type_f64, ctx->type_i1 };
        ctx->slot_type_Double = LLVMStructCreateNamed(ctx->context, "PgySlot_Double");
        LLVMStructSetBody(ctx->slot_type_Double, fields_double, 2, 0);

        LLVMTypeRef fields_bool[] = { ctx->type_i1, ctx->type_i1 };
        ctx->slot_type_Bool = LLVMStructCreateNamed(ctx->context, "PgySlot_Bool");
        LLVMStructSetBody(ctx->slot_type_Bool, fields_bool, 2, 0);

        LLVMTypeRef fields_str[] = { ctx->type_i8ptr, ctx->type_i1 };
        ctx->slot_type_String = LLVMStructCreateNamed(ctx->context, "PgySlot_String");
        LLVMStructSetBody(ctx->slot_type_String, fields_str, 2, 0);

        LLVMTypeRef pinned_fields_int[] = {
            LLVMPointerType(ctx->slot_type_Int, 0), ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_slot_type_Int = LLVMStructCreateNamed(ctx->context, "PgyPinnedSlotView_Int");
        LLVMStructSetBody(ctx->pinned_slot_type_Int, pinned_fields_int, 3, 0);

        LLVMTypeRef pinned_fields_long[] = {
            LLVMPointerType(ctx->slot_type_Long, 0), ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_slot_type_Long = LLVMStructCreateNamed(ctx->context, "PgyPinnedSlotView_Long");
        LLVMStructSetBody(ctx->pinned_slot_type_Long, pinned_fields_long, 3, 0);

        LLVMTypeRef pinned_fields_float[] = {
            LLVMPointerType(ctx->slot_type_Float, 0), ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_slot_type_Float = LLVMStructCreateNamed(ctx->context, "PgyPinnedSlotView_Float");
        LLVMStructSetBody(ctx->pinned_slot_type_Float, pinned_fields_float, 3, 0);

        LLVMTypeRef pinned_fields_double[] = {
            LLVMPointerType(ctx->slot_type_Double, 0), ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_slot_type_Double = LLVMStructCreateNamed(ctx->context, "PgyPinnedSlotView_Double");
        LLVMStructSetBody(ctx->pinned_slot_type_Double, pinned_fields_double, 3, 0);

        LLVMTypeRef pinned_fields_bool[] = {
            LLVMPointerType(ctx->slot_type_Bool, 0), ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_slot_type_Bool = LLVMStructCreateNamed(ctx->context, "PgyPinnedSlotView_Bool");
        LLVMStructSetBody(ctx->pinned_slot_type_Bool, pinned_fields_bool, 3, 0);

        LLVMTypeRef pinned_fields_string[] = {
            LLVMPointerType(ctx->slot_type_String, 0), ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_slot_type_String = LLVMStructCreateNamed(ctx->context, "PgyPinnedSlotView_String");
        LLVMStructSetBody(ctx->pinned_slot_type_String, pinned_fields_string, 3, 0);

        LLVMTypeRef secure_fields_int[] = { ctx->type_i32, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Int = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Int");
        LLVMStructSetBody(ctx->secure_slot_type_Int, secure_fields_int, 3, 0);

        LLVMTypeRef secure_fields_long[] = { ctx->type_i64, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Long = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Long");
        LLVMStructSetBody(ctx->secure_slot_type_Long, secure_fields_long, 3, 0);

        LLVMTypeRef secure_fields_float[] = { ctx->type_f32, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Float = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Float");
        LLVMStructSetBody(ctx->secure_slot_type_Float, secure_fields_float, 3, 0);

        LLVMTypeRef secure_fields_double[] = { ctx->type_f64, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Double = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Double");
        LLVMStructSetBody(ctx->secure_slot_type_Double, secure_fields_double, 3, 0);

        LLVMTypeRef secure_fields_bool[] = { ctx->type_i1, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_Bool = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_Bool");
        LLVMStructSetBody(ctx->secure_slot_type_Bool, secure_fields_bool, 3, 0);

        LLVMTypeRef secure_fields_str[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i64 };
        ctx->secure_slot_type_String = LLVMStructCreateNamed(ctx->context, "PgySecureSlot_String");
        LLVMStructSetBody(ctx->secure_slot_type_String, secure_fields_str, 3, 0);

        LLVMTypeRef token_fields[] = { ctx->type_i64, ctx->type_i1, ctx->type_i1 };
        ctx->secure_token_type_Int = LLVMStructCreateNamed(ctx->context, "PgyToken_Int");
        LLVMStructSetBody(ctx->secure_token_type_Int, token_fields, 3, 0);
        ctx->secure_token_type_Long = LLVMStructCreateNamed(ctx->context, "PgyToken_Long");
        LLVMStructSetBody(ctx->secure_token_type_Long, token_fields, 3, 0);
        ctx->secure_token_type_Float = LLVMStructCreateNamed(ctx->context, "PgyToken_Float");
        LLVMStructSetBody(ctx->secure_token_type_Float, token_fields, 3, 0);
        ctx->secure_token_type_Double = LLVMStructCreateNamed(ctx->context, "PgyToken_Double");
        LLVMStructSetBody(ctx->secure_token_type_Double, token_fields, 3, 0);
        ctx->secure_token_type_Bool = LLVMStructCreateNamed(ctx->context, "PgyToken_Bool");
        LLVMStructSetBody(ctx->secure_token_type_Bool, token_fields, 3, 0);
        ctx->secure_token_type_String = LLVMStructCreateNamed(ctx->context, "PgyToken_String");
        LLVMStructSetBody(ctx->secure_token_type_String, token_fields, 3, 0);

        LLVMTypeRef pinned_secure_fields_int[] = {
            LLVMPointerType(ctx->secure_slot_type_Int, 0),
            LLVMPointerType(ctx->secure_token_type_Int, 0),
            ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_secure_slot_type_Int = LLVMStructCreateNamed(ctx->context, "PgyPinnedSecureSlotView_Int");
        LLVMStructSetBody(ctx->pinned_secure_slot_type_Int, pinned_secure_fields_int, 4, 0);

        LLVMTypeRef pinned_secure_fields_long[] = {
            LLVMPointerType(ctx->secure_slot_type_Long, 0),
            LLVMPointerType(ctx->secure_token_type_Long, 0),
            ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_secure_slot_type_Long = LLVMStructCreateNamed(ctx->context, "PgyPinnedSecureSlotView_Long");
        LLVMStructSetBody(ctx->pinned_secure_slot_type_Long, pinned_secure_fields_long, 4, 0);

        LLVMTypeRef pinned_secure_fields_float[] = {
            LLVMPointerType(ctx->secure_slot_type_Float, 0),
            LLVMPointerType(ctx->secure_token_type_Float, 0),
            ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_secure_slot_type_Float = LLVMStructCreateNamed(ctx->context, "PgyPinnedSecureSlotView_Float");
        LLVMStructSetBody(ctx->pinned_secure_slot_type_Float, pinned_secure_fields_float, 4, 0);

        LLVMTypeRef pinned_secure_fields_double[] = {
            LLVMPointerType(ctx->secure_slot_type_Double, 0),
            LLVMPointerType(ctx->secure_token_type_Double, 0),
            ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_secure_slot_type_Double = LLVMStructCreateNamed(ctx->context, "PgyPinnedSecureSlotView_Double");
        LLVMStructSetBody(ctx->pinned_secure_slot_type_Double, pinned_secure_fields_double, 4, 0);

        LLVMTypeRef pinned_secure_fields_bool[] = {
            LLVMPointerType(ctx->secure_slot_type_Bool, 0),
            LLVMPointerType(ctx->secure_token_type_Bool, 0),
            ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_secure_slot_type_Bool = LLVMStructCreateNamed(ctx->context, "PgyPinnedSecureSlotView_Bool");
        LLVMStructSetBody(ctx->pinned_secure_slot_type_Bool, pinned_secure_fields_bool, 4, 0);

        LLVMTypeRef pinned_secure_fields_string[] = {
            LLVMPointerType(ctx->secure_slot_type_String, 0),
            LLVMPointerType(ctx->secure_token_type_String, 0),
            ctx->type_i1, ctx->type_i1
        };
        ctx->pinned_secure_slot_type_String = LLVMStructCreateNamed(ctx->context, "PgyPinnedSecureSlotView_String");
        LLVMStructSetBody(ctx->pinned_secure_slot_type_String, pinned_secure_fields_string, 4, 0);

        LLVMTypeRef arr_fields_int[] = {
            LLVMPointerType(ctx->type_i32, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Int = LLVMStructCreateNamed(ctx->context, "PgyArray_Int");
        LLVMStructSetBody(ctx->array_type_Int, arr_fields_int, 4, 0);

        LLVMTypeRef arr_fields_long[] = {
            LLVMPointerType(ctx->type_i64, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Long = LLVMStructCreateNamed(ctx->context, "PgyArray_Long");
        LLVMStructSetBody(ctx->array_type_Long, arr_fields_long, 4, 0);

        LLVMTypeRef arr_fields_float[] = {
            LLVMPointerType(ctx->type_f32, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Float = LLVMStructCreateNamed(ctx->context, "PgyArray_Float");
        LLVMStructSetBody(ctx->array_type_Float, arr_fields_float, 4, 0);

        LLVMTypeRef arr_fields_double[] = {
            LLVMPointerType(ctx->type_f64, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Double = LLVMStructCreateNamed(ctx->context, "PgyArray_Double");
        LLVMStructSetBody(ctx->array_type_Double, arr_fields_double, 4, 0);

        LLVMTypeRef arr_fields_bool[] = {
            LLVMPointerType(ctx->type_i1, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_Bool = LLVMStructCreateNamed(ctx->context, "PgyArray_Bool");
        LLVMStructSetBody(ctx->array_type_Bool, arr_fields_bool, 4, 0);

        LLVMTypeRef arr_fields_string[] = {
            LLVMPointerType(ctx->type_i8ptr, 0), ctx->type_i64, ctx->type_i64, ctx->type_i8ptr
        };
        ctx->array_type_String = LLVMStructCreateNamed(ctx->context, "PgyArray_String");
        LLVMStructSetBody(ctx->array_type_String, arr_fields_string, 4, 0);

        LLVMTypeRef slice_fields_int[] = { LLVMPointerType(ctx->type_i32, 0), ctx->type_i64 };
        ctx->slice_type_Int = LLVMStructCreateNamed(ctx->context, "PgySlice_Int");
        LLVMStructSetBody(ctx->slice_type_Int, slice_fields_int, 2, 0);

        LLVMTypeRef slice_fields_long[] = { LLVMPointerType(ctx->type_i64, 0), ctx->type_i64 };
        ctx->slice_type_Long = LLVMStructCreateNamed(ctx->context, "PgySlice_Long");
        LLVMStructSetBody(ctx->slice_type_Long, slice_fields_long, 2, 0);

        LLVMTypeRef slice_fields_float[] = { LLVMPointerType(ctx->type_f32, 0), ctx->type_i64 };
        ctx->slice_type_Float = LLVMStructCreateNamed(ctx->context, "PgySlice_Float");
        LLVMStructSetBody(ctx->slice_type_Float, slice_fields_float, 2, 0);

        LLVMTypeRef slice_fields_double[] = { LLVMPointerType(ctx->type_f64, 0), ctx->type_i64 };
        ctx->slice_type_Double = LLVMStructCreateNamed(ctx->context, "PgySlice_Double");
        LLVMStructSetBody(ctx->slice_type_Double, slice_fields_double, 2, 0);

        LLVMTypeRef slice_fields_bool[] = { LLVMPointerType(ctx->type_i1, 0), ctx->type_i64 };
        ctx->slice_type_Bool = LLVMStructCreateNamed(ctx->context, "PgySlice_Bool");
        LLVMStructSetBody(ctx->slice_type_Bool, slice_fields_bool, 2, 0);

        LLVMTypeRef slice_fields_string[] = { LLVMPointerType(ctx->type_i8ptr, 0), ctx->type_i64 };
        ctx->slice_type_String = LLVMStructCreateNamed(ctx->context, "PgySlice_String");
        LLVMStructSetBody(ctx->slice_type_String, slice_fields_string, 2, 0);
    }

    return ctx;
}

void
llvm_ctx_destroy(LLVMGenCtx *ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->builder != NULL)
        LLVMDisposeBuilder(ctx->builder);
    if (ctx->module != NULL)
        LLVMDisposeModule(ctx->module);
    if (ctx->context != NULL)
        LLVMContextDispose(ctx->context);

    for (int i = 0; i < ctx->list_var_count; i++) {
        free((char *)ctx->list_vars[i].var_name);
        free((char *)ctx->list_vars[i].inner_type);
    }
    for (int i = 0; i < ctx->set_var_count; i++) {
        free((char *)ctx->set_vars[i].var_name);
        free((char *)ctx->set_vars[i].inner_type);
    }
    for (int i = 0; i < ctx->queue_var_count; i++) {
        free((char *)ctx->queue_vars[i].var_name);
        free((char *)ctx->queue_vars[i].inner_type);
    }
    for (int i = 0; i < ctx->map_var_count; i++) {
        free((char *)ctx->map_vars[i].var_name);
        free((char *)ctx->map_vars[i].key_type);
        free((char *)ctx->map_vars[i].value_type);
    }

    /* Free dynamic arrays */
    for (int i = 0; i < MAX_SCOPE_DEPTH; i++)
        free(ctx->scopes[i].entries);
    free(ctx->functions);
    free(ctx->slot_vars);
    free(ctx->view_vars);
    free(ctx->device_slot_vars);
    free(ctx->future_vars);
    free(ctx->channel_vars);
    free(ctx->rc_vars);
    free(ctx->weak_vars);
    free(ctx->class_types);
    free(ctx->var_classes);
    free(ctx->projection_borrows);
    for (int i = 0; i < ctx->array_var_count; i++)
        free((char *)ctx->array_vars[i].var_name);
    free(ctx->array_vars);
    free(ctx->list_vars);
    free(ctx->set_vars);
    free(ctx->queue_vars);
    free(ctx->map_vars);
    free(ctx->callable_vars);
    free(ctx->event_types);
    free(ctx->enum_variants);
    free(ctx->generic_templates);

    /* Free heap-allocated mono instance names */
    for (int i = 0; i < ctx->mono_count; i++)
        free(ctx->mono_instances[i].name);
    free(ctx->mono_instances);

    pgy_arena_destroy(&ctx->scratch);
    pgy_arena_destroy(&ctx->persistent);
    free(ctx);
}

/* =================================================================
 * Scope management
 * ================================================================= */

/* =================================================================
 * Runtime function declarations
 * ================================================================= */

#else /* !PGY_LLVM_ENABLED - stub implementations */

#include "llvm_backend.h"
#include <stdlib.h>
#include <string.h>

LLVMGenResult *llvm_codegen_from_mir(const void *mir, const char *module_name) {
    LLVMGenResult *res = malloc(sizeof(LLVMGenResult));
    (void)mir;
    (void)module_name;
    if (res) {
        memset(res, 0, sizeof(LLVMGenResult));
        pgy_arena_init(&res->owned_arena, 0);
        res->error_message = pgy_arena_strdup(&res->owned_arena,
            "LLVM backend not enabled");
    }
    return res;
}

LLVMGenResult *llvm_codegen_to_object_from_mir(const void *mir, const char *module_name, const char *output_path, bool release_opt) {
    LLVMGenResult *res = malloc(sizeof(LLVMGenResult));
    (void)mir;
    (void)module_name;
    (void)output_path;
    (void)release_opt;
    if (res) {
        memset(res, 0, sizeof(LLVMGenResult));
        pgy_arena_init(&res->owned_arena, 0);
        res->error_message = pgy_arena_strdup(&res->owned_arena,
            "LLVM backend not enabled");
    }
    return res;
}

void llvm_gen_result_destroy(LLVMGenResult *res) {
    if (res) {
        pgy_arena_destroy(&res->owned_arena);
        free(res);
    }
}

#endif /* PGY_LLVM_ENABLED */
