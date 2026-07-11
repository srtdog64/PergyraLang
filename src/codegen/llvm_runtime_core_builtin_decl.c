/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM runtime core/log/intent/authority declaration owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"
#include "../common/intent_observability_abi.h"
#include "../compiler/mir_abi_layout.h"

#include <string.h>

static void
llvm_declare_text_builder_builtins(LLVMGenCtx *ctx)
{
    LLVMTypeRef builder_ptr = LLVMPointerType(ctx->type_text_builder, 0);
    LLVMTypeRef allocator_ptr = LLVMPointerType(ctx->type_allocator, 0);

    for (size_t i = 0; i < mir_text_builder_runtime_row_count(); i++) {
        const MIRTextBuilderRuntimeRow *row =
            mir_text_builder_runtime_row_at(i);
        LLVMTypeRef params[3] = { NULL, NULL, NULL };
        LLVMTypeRef ret = ctx->type_void;
        unsigned count = 1;

        if (row == NULL)
            continue;
        params[0] = builder_ptr;
        if (row->llvm_call_shape
            == MIR_TEXT_BUILDER_CALL_BUILDER_STRING_TO_VOID) {
            params[1] = ctx->type_i8ptr;
            count = 2;
        } else if (row->llvm_call_shape
                   == MIR_TEXT_BUILDER_CALL_BUILDER_ALLOCATOR_TO_STRING) {
            params[1] = allocator_ptr;
            ret = ctx->type_i8ptr;
            count = 2;
        } else if (row->llvm_call_shape
                   == MIR_TEXT_BUILDER_CALL_OUT_CAPACITY_TO_VOID) {
            params[1] = ctx->type_i64;
            count = 2;
        }
        LLVMTypeRef ft = LLVMFunctionType(ret, params, count, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, row->llvm_export_fn, ft);
        llvm_register_function(ctx, row->llvm_export_fn, fn, ft, ret);
    }
}

static LLVMTypeRef
llvm_intent_observability_return_type(
    LLVMGenCtx *ctx, PgyIntentObservabilityReturnKind kind)
{
    switch (kind) {
    case PGY_INTENT_OBSERVABILITY_RETURN_INT:
        return ctx->type_i32;
    case PGY_INTENT_OBSERVABILITY_RETURN_BOOL:
        return ctx->type_i1;
    case PGY_INTENT_OBSERVABILITY_RETURN_STRING:
        return ctx->type_i8ptr;
    }
    return NULL;
}

static LLVMTypeRef
llvm_intent_observability_argument_type(
    LLVMGenCtx *ctx, PgyIntentObservabilityArgumentKind kind)
{
    switch (kind) {
    case PGY_INTENT_OBSERVABILITY_ARGUMENT_INT:
        return ctx->type_i32;
    case PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID:
        break;
    }
    return NULL;
}

static void
llvm_declare_intent_observability_builtins(LLVMGenCtx *ctx)
{
    size_t row_count = pgy_intent_observability_abi_row_count();

    for (size_t i = 0; i < row_count; i++) {
        const PgyIntentObservabilityAbiRow *row =
            pgy_intent_observability_abi_row_at(i);
        LLVMTypeRef params[2] = { NULL, NULL };
        LLVMTypeRef ret = llvm_intent_observability_return_type(
            ctx, row->return_kind);
        for (size_t j = 0; j < row->arg_count; j++) {
            params[j] = llvm_intent_observability_argument_type(
                ctx, pgy_intent_observability_argument_kind_at(row, j));
        }
        LLVMTypeRef ft = LLVMFunctionType(
            ret, params, (unsigned)row->arg_count, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_name, ft);
        llvm_register_function(ctx, row->runtime_name, fn, ft, ret);
    }
}

void
llvm_declare_runtime_core_builtins(LLVMGenCtx *ctx)
{
    llvm_declare_text_builder_builtins(ctx);
    struct { const char *name; LLVMTypeRef param; } log_fns[] = {
        { "pgy_log_int",    ctx->type_i32 },
        { "pgy_log_long",   ctx->type_i64 },
        { "pgy_log_float",  ctx->type_f32 },
        { "pgy_log_double", ctx->type_f64 },
        { "pgy_log_bool",   ctx->type_i1  },
        { "pgy_log_string", ctx->type_i8ptr },
        { "pgy_log_banner", ctx->type_i8ptr },
    };

    for (size_t i = 0; i < sizeof(log_fns) / sizeof(log_fns[0]); i++) {
        LLVMTypeRef params[] = { log_fns[i].param };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, log_fns[i].name, ft);
        llvm_register_function(ctx, log_fns[i].name, fn, ft, ctx->type_void);
    }

    {
        LLVMTypeRef params[] = { ctx->type_i8ptr };
        LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 1);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "printf", ft);
        llvm_register_function(ctx, "printf", fn, ft, ctx->type_i32);
    }

    {
        struct {
            const char *name;
            LLVMTypeRef ret;
            LLVMTypeRef params[6];
            unsigned param_count;
        } builtins[] = {
            { "StringContains", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "StringIndexOf", ctx->type_i32,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "StringReplace", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "Substring", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32 }, 3 },
            { "CharAtN", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32 }, 3 },
            { "CharCode", ctx->type_i32,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32 }, 3 },
            { "SubIndexOf", ctx->type_i32,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 4 },
            { "SubIndexOfWithLen", ctx->type_i32,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 5 },
            { "SubEquals", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 4 },
            { "SubEqualsWithLen", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 5 },
            { "SubContains", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 4 },
            { "SubContainsWithLen", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 5 },
            { "SubStartsWith", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr }, 3 },
            { "SubStartsWithLen", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr }, 4 },
            { "StringTrim", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "ToUpper", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "ToLower", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "StringConcat", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "StringSplit", ctx->array_type_String,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "StringJoin", ctx->type_i8ptr,
              { LLVMPointerType(ctx->array_type_String, 0), ctx->type_i8ptr }, 2 },
            { "pgy_args", ctx->array_type_String,
              { 0 }, 0 },
            { "pgy_args_init", ctx->type_void,
              { ctx->type_i32, LLVMPointerType(ctx->type_i8ptr, 0) }, 2 },
            { "pgy_exit", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "pgy_string_equals", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_runtime_panic_internal_invariant_export", ctx->type_void,
              { ctx->type_i8ptr }, 1 },
            { "pgy_runtime_panic_out_of_bounds_export", ctx->type_void,
              { ctx->type_i8ptr }, 1 },
            { "pgy_runtime_lifecycle_set_export", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i32 }, 2 },
            { "pgy_runtime_lifecycle_guard_export", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr }, 5 },
            { "pgy_cap_require_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_cap_set_manifest_export", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "pgy_cap_grant_all_export", ctx->type_void,
              { 0 }, 0 },
            { "pgy_cap_granted_export", ctx->type_i32,
              { 0 }, 0 },
            { "pgy_budget_wall_arm_export", ctx->type_void,
              { 0 }, 0 },
            { "pgy_checked_div_i32_export", ctx->type_i32,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_checked_div_i64_export", ctx->type_i64,
              { ctx->type_i64, ctx->type_i64 }, 2 },
            { "pgy_checked_mod_i32_export", ctx->type_i32,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_checked_mod_i64_export", ctx->type_i64,
              { ctx->type_i64, ctx->type_i64 }, 2 },
            { "pgy_checked_add_i32_export", ctx->type_i32,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_checked_add_i64_export", ctx->type_i64,
              { ctx->type_i64, ctx->type_i64 }, 2 },
            { "pgy_checked_mul_i32_export", ctx->type_i32,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_checked_mul_i64_export", ctx->type_i64,
              { ctx->type_i64, ctx->type_i64 }, 2 },
            { "ToInt", ctx->type_i32,
              { ctx->type_i8ptr }, 1 },
            { "ToFloat", ctx->type_f32,
              { ctx->type_i8ptr }, 1 },
            { "Sqrt", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Pow", ctx->type_f32,
              { ctx->type_f32, ctx->type_f32 }, 2 },
            { "Floor", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Ceil", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Round", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Sin", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Cos", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Tan", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Asin", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Acos", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Atan", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Atan2", ctx->type_f32,
              { ctx->type_f32, ctx->type_f32 }, 2 },
            { "Exp", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "MathLog", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Log10", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Log2", ctx->type_f32,
              { ctx->type_f32 }, 1 },
            { "Random", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_int_to_string", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_long_to_string", ctx->type_i8ptr,
              { ctx->type_i64 }, 1 },
            { "pgy_float_to_string", ctx->type_i8ptr,
              { ctx->type_f32 }, 1 },
            { "pgy_double_to_string", ctx->type_i8ptr,
              { ctx->type_f64 }, 1 },
            { "pgy_intent_enter_export", ctx->type_i32,
              { ctx->type_i8ptr, LLVMPointerType(ctx->type_i8ptr, 0), ctx->type_i32, ctx->type_i1, ctx->type_i32 }, 5 },
            { "pgy_intent_exit_export", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_trace_step_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "pgy_intent_trace_bind_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "pgy_intent_trace_step_ok_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_intent_trace_fail_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_intent_trace_materialize_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
            { "pgy_intent_trace_transfer_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 6 },
            { "pgy_mir_resource_op_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
            { "pgy_mir_cleanup_op_export", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
            { "pgy_zone_authority_check_export", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
            { "pgy_zone_authority_check_token_export", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64, ctx->type_i64, ctx->type_i8ptr, ctx->type_i8ptr }, 6 },
            { "pgy_zone_authority_validate_flags_export", ctx->type_i1,
              { ctx->type_i1, ctx->type_i1, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
            { "pgy_zone_authority_validate_token_flags_export", ctx->type_i1,
              { ctx->type_i1, ctx->type_i1, ctx->type_i64, ctx->type_i64, ctx->type_i8ptr, ctx->type_i8ptr }, 6 },
            { "pgy_zone_authority_last_ok_rt_export", ctx->type_i1,
              { 0 }, 0 },
            { "pgy_zone_authority_last_zone_rt_export", ctx->type_i8ptr,
              { 0 }, 0 },
            { "pgy_zone_authority_last_participant_rt_export", ctx->type_i8ptr,
              { 0 }, 0 },
            { "pgy_zone_authority_last_code_rt_export", ctx->type_i8ptr,
              { 0 }, 0 },
            { "pgy_zone_authority_last_reason_rt_export", ctx->type_i8ptr,
              { 0 }, 0 },
            { "pgy_read_file", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "pgy_read_stdin", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_dir_walk", ctx->array_type_String,
              { ctx->type_i8ptr }, 1 },
            { "pgy_file_exists", ctx->type_i1,
              { ctx->type_i8ptr }, 1 },
            { "pgy_write_file", ctx->type_void,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_input", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "pgy_now_ms", ctx->type_i32,
              { 0 }, 0 },
            { "pgy_sleep_ms", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "SeedRandom", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "pgy_file_open", ctx->type_i32,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_file_read", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_file_write", ctx->type_void,
              { ctx->type_i32, ctx->type_i8ptr }, 2 },
            { "pgy_file_close", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "ClaimQubit", ctx->type_i32,
              { 0 }, 0 },
            { "Measure", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "Entangle", ctx->type_void,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "QubitState", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "IsCollapsed", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "ReleaseQubit", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "H", ctx->type_void,
              { ctx->type_i32 }, 1 },
            { "IntoClassical", ctx->type_i1,
              { ctx->type_i32 }, 1 },
        };

        for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
            LLVMTypeRef ret = builtins[i].ret;
            LLVMTypeRef params[6];
            unsigned param_count = builtins[i].param_count;
            for (unsigned j = 0; j < param_count; j++)
                params[j] = builtins[i].params[j];
            llvm_runtime_aggregate_return_apply_decl_shape(ctx,
                builtins[i].name, &ret, params, &param_count,
                sizeof(params) / sizeof(params[0]));
            if (ctx->has_error)
                return;
            LLVMTypeRef ft = LLVMFunctionType(
                ret, params, param_count, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, builtins[i].name, ft);
            llvm_register_function(ctx, builtins[i].name, fn, ft, ret);
        }
    }
    llvm_declare_intent_observability_builtins(ctx);
}

#endif
