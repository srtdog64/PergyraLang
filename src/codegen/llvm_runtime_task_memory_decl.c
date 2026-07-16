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
    { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_lane_spawn_dispatch_export", ft);
      llvm_register_function(ctx, "pgy_lane_spawn_dispatch_export", fn, ft, ctx->type_task_handle); }
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
    /* Auto-chunked index fan-out (docs/186 P-B3): chunk-count policy,
     * caller-owned chunk-ctx table, and the fill+spawn combiner. */
    { LLVMTypeRef params[] = { ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_parallel_chunk_count_export", ft);
      llvm_register_function(ctx, "pgy_parallel_chunk_count_export", fn, ft, ctx->type_i64); }
    { LLVMTypeRef params[] = { ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_parallel_chunk_ctxs_alloc_export", ft);
      llvm_register_function(ctx, "pgy_parallel_chunk_ctxs_alloc_export", fn, ft, ctx->type_i8ptr); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i64,
                               ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64,
                               ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 7, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_parallel_spawn_chunk_at_export", ft);
      llvm_register_function(ctx, "pgy_parallel_spawn_chunk_at_export", fn, ft, ctx->type_task_handle); }
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
