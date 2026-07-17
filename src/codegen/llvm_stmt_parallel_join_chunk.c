#ifdef PGY_LLVM_ENABLED

#include "llvm_stmt_parallel_join_chunk.h"

#include "llvm_stmt_parallel_join_capture.h"
#include "llvm_stmt_internal.h"

#include <string.h>

bool
llvm_parallel_join_chunk_plan_init(
    ASTNode *site,
    LLVMGenCtx *ctx,
    LLVMValueRef item_count,
    LLVMValueRef zero,
    LLVMValueRef one,
    LLVMParallelJoinChunkPlan *plan)
{
    LLVMFuncEntry *count_fn;
    LLVMFuncEntry *alloc_fn;
    LLVMValueRef count_args[] = { item_count };

    if (ctx == NULL || plan == NULL)
        return false;
    memset(plan, 0, sizeof(*plan));
    count_fn = llvm_lookup_function(ctx, "pgy_parallel_chunk_count_export");
    alloc_fn = llvm_lookup_function(ctx,
        "pgy_parallel_chunk_ctxs_alloc_export");
    plan->spawn_fn = llvm_lookup_function(ctx,
        "pgy_parallel_spawn_chunk_at_export");
    plan->free_fn = llvm_lookup_function(ctx, "free");
    if (count_fn == NULL || alloc_fn == NULL || plan->spawn_fn == NULL
        || plan->free_fn == NULL) {
        llvm_parallel_join_set_error(ctx, site,
            "LLVM parallel join requires registered chunk runtime functions%s",
            "");
        return false;
    }
    plan->count = LLVMBuildCall2(ctx->builder, count_fn->fn_type,
        count_fn->fn, count_args, 1, "_pj_nch");
    LLVMValueRef count_is_zero = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
        plan->count, zero, llvm_tmp_name(ctx));
    plan->allocation_count = LLVMBuildSelect(ctx->builder, count_is_zero,
        one, plan->count, llvm_tmp_name(ctx));
    LLVMValueRef alloc_args[] = { plan->count };
    plan->storage = LLVMBuildCall2(ctx->builder, alloc_fn->fn_type,
        alloc_fn->fn, alloc_args, 1, "_pj_cc");
    return true;
}

bool
llvm_parallel_join_chunk_fanout(
    ASTNode *site,
    LLVMGenCtx *ctx,
    const LLVMParallelJoinChunkPlan *plan,
    const LLVMParallelJoinChunkFanoutInput *input)
{
    LLVMBasicBlockRef cond;
    LLVMBasicBlockRef body;
    LLVMBasicBlockRef done;

    if (ctx == NULL || plan == NULL || input == NULL)
        return false;
    cond = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.chunk.cond");
    body = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.chunk.body");
    done = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.chunk.done");
    LLVMBuildBr(ctx->builder, cond);

    LLVMPositionBuilderAtEnd(ctx->builder, cond);
    LLVMValueRef index = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
        input->loop_slot, llvm_tmp_name(ctx));
    LLVMValueRef has_more = LLVMBuildICmp(ctx->builder, LLVMIntULT, index,
        plan->count, llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, has_more, body, done);

    LLVMPositionBuilderAtEnd(ctx->builder, body);
    index = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
        input->loop_slot, llvm_tmp_name(ctx));
    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder, input->wrapper_fn,
        ctx->type_i8ptr, llvm_tmp_name(ctx));
    LLVMValueRef contexts = LLVMBuildBitCast(ctx->builder,
        input->item_contexts, ctx->type_i8ptr, llvm_tmp_name(ctx));
    LLVMValueRef spawn_args[] = {
        plan->storage,
        index,
        plan->count,
        fn_ptr,
        contexts,
        LLVMSizeOf(input->item_context_type),
        input->item_count
    };
    LLVMValueRef handle = LLVMBuildCall2(ctx->builder,
        plan->spawn_fn->fn_type, plan->spawn_fn->fn, spawn_args, 7,
        llvm_tmp_name(ctx));
    if (!llvm_emit_task_handle_nonnull_guard(ctx, site, handle,
            "LLVM parallel join task spawn failed"))
        return false;
    LLVMValueRef handle_ptr = LLVMBuildGEP2(ctx->builder, input->handle_type,
        input->handles, &index, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, handle, handle_ptr);
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, index, input->one,
        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, input->loop_slot);
    LLVMBuildBr(ctx->builder, cond);

    LLVMPositionBuilderAtEnd(ctx->builder, done);
    LLVMBuildStore(ctx->builder, input->zero, input->loop_slot);
    return true;
}

void
llvm_parallel_join_chunk_plan_dispose(
    LLVMGenCtx *ctx,
    const LLVMParallelJoinChunkPlan *plan)
{
    LLVMValueRef args[] = { plan->storage };

    LLVMBuildCall2(ctx->builder, plan->free_fn->fn_type, plan->free_fn->fn,
        args, 1, "");
}

#endif
