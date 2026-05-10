/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM runtime declarations for task and memory helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

void
llvm_declare_runtime_task_memory(LLVMGenCtx *ctx)
{
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
}

#endif
