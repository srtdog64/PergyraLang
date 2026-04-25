/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend — runtime declaration registry
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

void
llvm_declare_runtime(LLVMGenCtx *ctx)
{
    struct { const char *name; LLVMTypeRef param; } log_fns[] = {
        { "pgy_log_int",    ctx->type_i32 },
        { "pgy_log_long",   ctx->type_i64 },
        { "pgy_log_float",  ctx->type_f32 },
        { "pgy_log_double", ctx->type_f64 },
        { "pgy_log_bool",   ctx->type_i1  },
        { "pgy_log_string", ctx->type_i8ptr },
        { "pgy_log_banner", ctx->type_i8ptr },
    };

    if (ctx->type_task_handle == NULL) {
        LLVMTypeRef task_handle_fields[] = { ctx->type_i8ptr };
        ctx->type_task_handle = LLVMStructCreateNamed(ctx->context,
            "PgyTaskHandle");
        LLVMStructSetBody(ctx->type_task_handle, task_handle_fields, 1, 0);
    }

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
            { "StringReplace", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr }, 3 },
            { "Substring", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i32, ctx->type_i32 }, 3 },
            { "StringTrim", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "ToUpper", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "ToLower", ctx->type_i8ptr,
              { ctx->type_i8ptr }, 1 },
            { "StringConcat", ctx->type_i8ptr,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_string_equals", ctx->type_i1,
              { ctx->type_i8ptr, ctx->type_i8ptr }, 2 },
            { "pgy_runtime_panic_internal_invariant_export", ctx->type_void,
              { ctx->type_i8ptr }, 1 },
            { "pgy_checked_div_i32_export", ctx->type_i32,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_checked_div_i64_export", ctx->type_i64,
              { ctx->type_i64, ctx->type_i64 }, 2 },
            { "pgy_checked_mod_i32_export", ctx->type_i32,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_checked_mod_i64_export", ctx->type_i64,
              { ctx->type_i64, ctx->type_i64 }, 2 },
            { "ToInt", ctx->type_i32,
              { ctx->type_i8ptr }, 1 },
            { "ToFloat", ctx->type_f32,
              { ctx->type_i8ptr }, 1 },
            { "Random", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_int_to_string", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_float_to_string", ctx->type_i8ptr,
              { ctx->type_f32 }, 1 },
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
            { "pgy_intent_last_trace_export", ctx->type_i8ptr,
              { }, 0 },
            { "pgy_intent_last_failure_export", ctx->type_i8ptr,
              { }, 0 },
            { "pgy_intent_last_name_export", ctx->type_i8ptr,
              { }, 0 },
            { "pgy_intent_last_handle_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_last_trace_id_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_last_step_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_last_failed_export", ctx->type_i1,
              { }, 0 },
            { "pgy_intent_history_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_history_step_name_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_zone_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_phase_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_participant_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_slot_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_from_zone_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_from_slot_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_to_zone_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_to_slot_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_ok_export", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_history_step_failure_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_active_name_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_handle_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_trace_id_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_priority_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_concurrent_export", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_trace_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_parent_handle_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_subject_count_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_step_count_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_failed_export", ctx->type_i1,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_failure_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_active_step_name_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_zone_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_phase_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_participant_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_slot_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_from_zone_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_from_slot_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_to_zone_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_to_slot_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_ok_export", ctx->type_i1,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_active_step_failure_export", ctx->type_i8ptr,
              { ctx->type_i32, ctx->type_i32 }, 2 },
            { "pgy_intent_current_handle_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_recent_count_export", ctx->type_i32,
              { }, 0 },
            { "pgy_intent_recent_handle_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_recent_trace_id_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_recent_name_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_recent_trace_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_recent_failure_export", ctx->type_i8ptr,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_recent_step_count_export", ctx->type_i32,
              { ctx->type_i32 }, 1 },
            { "pgy_intent_recent_failed_export", ctx->type_i1,
              { ctx->type_i32 }, 1 },
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
            { "pgy_zone_authority_validate_flags_export", ctx->type_i1,
              { ctx->type_i1, ctx->type_i1, ctx->type_i8ptr, ctx->type_i8ptr }, 4 },
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
            LLVMTypeRef ft = LLVMFunctionType(
                builtins[i].ret, builtins[i].params,
                builtins[i].param_count, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, builtins[i].name, ft);
            llvm_register_function(ctx, builtins[i].name, fn, ft, builtins[i].ret);
        }
    }

    struct {
        const char *suffix;
        LLVMTypeRef slot_ty;
        LLVMTypeRef val_ty;
    } slot_types[] = {
        { "Int",    ctx->slot_type_Int,    ctx->type_i32   },
        { "Long",   ctx->slot_type_Long,   ctx->type_i64   },
        { "Float",  ctx->slot_type_Float,  ctx->type_f32   },
        { "Double", ctx->slot_type_Double, ctx->type_f64   },
        { "Bool",   ctx->slot_type_Bool,   ctx->type_i1    },
        { "String", ctx->slot_type_String, ctx->type_i8ptr },
    };

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef slot_ty = slot_types[i].slot_ty;
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        LLVMTypeRef ptr_ty = LLVMPointerType(slot_ty, 0);
        char fn_name[64];

        { LLVMTypeRef ft = LLVMFunctionType(slot_ty, NULL, 0, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_claim_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, slot_ty); }
        { LLVMTypeRef params[] = { ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_write_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_read_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_release_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef ft = LLVMFunctionType(slot_ty, NULL, 0, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_claim_device_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, slot_ty); }
        { LLVMTypeRef params[] = { ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_device_read_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_release_device_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 1, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_submit_device_read_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_task_handle); }
    }

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef arr_ty = llvm_array_struct_type(ctx, suffix);
        LLVMTypeRef arr_ptr_ty = LLVMPointerType(arr_ty, 0);
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        char fn_name[64];

        { LLVMTypeRef params[] = { ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(arr_ty, params, 1, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_array_new_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, arr_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_array_push_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 2, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_array_get_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_array_set_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(llvm_slice_struct_type(ctx, suffix), 0),
                                   ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 2, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_slice_get_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(llvm_slice_struct_type(ctx, suffix), params, 3, 0);
          snprintf(fn_name, sizeof(fn_name), "pgy_array_slice_%s", suffix);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
              llvm_slice_struct_type(ctx, suffix)); }
    }

    { LLVMTypeRef params[] = { ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_pool_init_export", ft);
      llvm_register_function(ctx, "pgy_pool_init_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, NULL, 0, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_pool_shutdown_export", ft);
      llvm_register_function(ctx, "pgy_pool_shutdown_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_spawn_export", ft);
      llvm_register_function(ctx, "pgy_spawn_export", fn, ft, ctx->type_task_handle); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_async_spawn_export", ft);
      llvm_register_function(ctx, "pgy_async_spawn_export", fn, ft, ctx->type_task_handle); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_spawn_blocking_export", ft);
      llvm_register_function(ctx, "pgy_spawn_blocking_export", fn, ft, ctx->type_task_handle); }
    { LLVMTypeRef params[] = { ctx->type_task_handle };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_async_detach_export", ft);
      llvm_register_function(ctx, "pgy_async_detach_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_task_handle };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_await_export", ft);
      llvm_register_function(ctx, "pgy_await_export", fn, ft, ctx->type_i8ptr); }
    { LLVMTypeRef params[] = { ctx->type_task_handle };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_task_cancel_export", ft);
      llvm_register_function(ctx, "pgy_task_cancel_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, NULL, 0, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_task_is_cancelled_export", ft);
      llvm_register_function(ctx, "pgy_task_is_cancelled_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "malloc", ft);
      llvm_register_function(ctx, "malloc", fn, ft, ctx->type_i8ptr); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "free", ft);
      llvm_register_function(ctx, "free", fn, ft, ctx->type_void); }

    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_new_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_set_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_new_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_new_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_map_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_new_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_add_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_add_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_has_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_has_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_remove_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_remove_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_size_raw_export", fn, ft, ctx->type_i32); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_push_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_push_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_push_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_push_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_get_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_get_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_list_set_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_set_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_size_raw_export", fn, ft, ctx->type_i32);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_size_raw_export", fn, ft, ctx->type_i32);
      fn = LLVMAddFunction(ctx->module, "pgy_map_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_size_raw_export", fn, ft, ctx->type_i32);
      ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_empty_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_empty_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_remove_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_remove_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_queue_pop_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_pop_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_i32_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_i64_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_bool_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_bool_export", fn, ft, ctx->type_void); }

    struct {
        const char *suffix;
        LLVMTypeRef val_type;
    } chan_types[] = {
        { "Int",    ctx->type_i32 },
        { "String", ctx->type_i8ptr },
    };

    for (size_t ci = 0; ci < sizeof(chan_types) / sizeof(chan_types[0]); ci++) {
        const char *suf = chan_types[ci].suffix;
        LLVMTypeRef vt = chan_types[ci].val_type;
        char fname[128];

        { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_init_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0), ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_ready_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_length_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_capacity_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_space_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_full_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_closed_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_close_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_destroy_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }

    for (size_t si = 0; si < sizeof(slot_types) / sizeof(slot_types[0]); si++) {
        const char *suf = slot_types[si].suffix;
        LLVMTypeRef sty = llvm_secure_slot_struct_type(ctx, suf);
        LLVMTypeRef tty = llvm_secure_token_type(ctx, suf);
        LLVMTypeRef vt = slot_types[si].val_ty;
        char fname[128];

        { LLVMTypeRef params[] = { LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(sty, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_claim_secure_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), vt, LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_write_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_read_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_release_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif
