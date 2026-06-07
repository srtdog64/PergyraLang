/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend runtime declaration registry.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

static bool
llvm_runtime_export_name(char *out,
    size_t out_size,
    const char *op,
    const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || op == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "pgy_%s_%s", op, suffix);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_runtime_slot_name(char *out,
    size_t out_size,
    const char *op,
    const char *suffix)
{
    return llvm_runtime_export_name(out, out_size, op, suffix);
}

void
llvm_declare_runtime(LLVMGenCtx *ctx)
{
    if (ctx->type_task_handle == NULL) {
        LLVMTypeRef task_handle_fields[] = { ctx->type_i8ptr };
        ctx->type_task_handle = LLVMStructCreateNamed(ctx->context,
            "PgyTaskHandle");
        LLVMStructSetBody(ctx->type_task_handle, task_handle_fields, 1, 0);
    }

    llvm_declare_runtime_core_builtins(ctx);

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
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim", suffix)) {
              llvm_set_error(ctx, "slot claim runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, slot_ty); }
        { LLVMTypeRef params[] = { ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "write", suffix)) {
              llvm_set_error(ctx, "slot write runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "read", suffix)) {
              llvm_set_error(ctx, "slot read runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release", suffix)) {
              llvm_set_error(ctx, "slot release runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_slot_struct_type(ctx, suffix);
          LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(pinned_ty, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "pin_read", suffix)) {
              llvm_set_error(ctx, "slot pin-read runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "pin_write", suffix)) {
              llvm_set_error(ctx, "slot pin-write runtime name is too long");
              return;
          }
          fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty); }
        { LLVMTypeRef pinned_ty = llvm_pinned_slot_struct_type(ctx, suffix);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0), ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "pin_read_init", suffix)) {
              llvm_set_error(ctx, "slot pin-read init runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "pin_write_init", suffix)) {
              llvm_set_error(ctx, "slot pin-write init runtime name is too long");
              return;
          }
          fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_slot_struct_type(ctx, suffix);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "unpin", suffix)) {
              llvm_set_error(ctx, "slot unpin runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef ft = LLVMFunctionType(slot_ty, NULL, 0, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "claim_device", suffix)) {
              llvm_set_error(ctx, "device slot claim runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, slot_ty); }
        { LLVMTypeRef params[] = { ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_write", suffix)) {
              llvm_set_error(ctx, "device slot write runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "device_read", suffix)) {
              llvm_set_error(ctx, "device slot read runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "release_device", suffix)) {
              llvm_set_error(ctx, "device slot release runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 1, 0);
          if (!llvm_runtime_slot_name(fn_name, sizeof(fn_name), "submit_device_read", suffix)) {
              llvm_set_error(ctx, "device slot submit-read runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_task_handle); }
    }

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        LLVMTypeRef val_ptr_ty = LLVMPointerType(val_ty, 0);
        LLVMTypeRef handle_ptr_ty = LLVMPointerType(ctx->type_i8ptr, 0);
        char fn_name[64];

        { LLVMTypeRef params[] = { val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_new", suffix)) {
              llvm_set_error(ctx, "Rc new runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_clone", suffix)) {
              llvm_set_error(ctx, "Rc clone runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(val_ptr_ty, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_get", suffix)) {
              llvm_set_error(ctx, "Rc get runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ptr_ty); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_downgrade", suffix)) {
              llvm_set_error(ctx, "Rc downgrade runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { handle_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_drop", suffix)) {
              llvm_set_error(ctx, "Rc drop runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "weak_upgrade", suffix)) {
              llvm_set_error(ctx, "Weak upgrade runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { handle_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "weak_drop", suffix)) {
              llvm_set_error(ctx, "Weak drop runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef arr_ty = llvm_array_struct_type(ctx, suffix);
        LLVMTypeRef arr_ptr_ty = LLVMPointerType(arr_ty, 0);
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        char fn_name[64];

        { LLVMTypeRef params[] = { ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(arr_ty, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_new", suffix)) {
              llvm_set_error(ctx, "Array new runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, arr_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_push", suffix)) {
              llvm_set_error(ctx, "Array push runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_get", suffix)) {
              llvm_set_error(ctx, "Array get runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_set", suffix)) {
              llvm_set_error(ctx, "Array set runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { arr_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_pop", suffix)) {
              llvm_set_error(ctx, "Array pop runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        /* Closure #81: declare pgy_array_sort_<T>(val_ty *data, i64 len)
         * so ArraySort can lower to a runtime call. The runtime function
         * sorts in place; the LLVM emit threads the original array value
         * back as the call result to match the C-backend statement-expr
         * `({ pgy_array_sort_X(arr.data, arr.length); arr; })`. */
        { LLVMTypeRef val_ptr_ty = LLVMPointerType(val_ty, 0);
          LLVMTypeRef params[] = { val_ptr_ty, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_sort", suffix)) {
              llvm_set_error(ctx, "Array sort runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(llvm_slice_struct_type(ctx, suffix), 0),
                                   ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "slice_get", suffix)) {
              llvm_set_error(ctx, "Slice get runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { LLVMPointerType(llvm_slice_struct_type(ctx, suffix), 0) };
          LLVMTypeRef ft = LLVMFunctionType(arr_ty, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "slice_copy", suffix)) {
              llvm_set_error(ctx, "Slice copy runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, arr_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(llvm_slice_struct_type(ctx, suffix), params, 3, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_slice", suffix)) {
              llvm_set_error(ctx, "Array slice runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
              llvm_slice_struct_type(ctx, suffix)); }
    }

    llvm_declare_runtime_task_memory(ctx);
    llvm_declare_runtime_raw_collections(ctx);
    llvm_declare_runtime_channels(ctx);
    llvm_declare_runtime_secure_slots(ctx);
}

#endif
