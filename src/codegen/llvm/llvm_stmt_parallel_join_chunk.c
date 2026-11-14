#ifdef PGY_LLVM_ENABLED

#include "llvm_stmt_parallel_join_chunk.h"

#include "llvm_stmt_parallel_join_capture.h"
#include "llvm_stmt_parallel_names.h"
#include "llvm_stmt_internal.h"

#include <string.h>

LLVMValueRef
llvm_parallel_join_chunk_wrapper_create(
    ASTNode *site,
    LLVMGenCtx *ctx,
    LLVMValueRef item_wrapper_fn)
{
    char name[64];
    LLVMFuncEntry *cancel_fn;
    LLVMTypeRef type_i8;
    LLVMTypeRef fields[4];
    LLVMTypeRef chunk_type;
    LLVMTypeRef params[] = { ctx->type_i8ptr };
    LLVMTypeRef fn_type;
    LLVMTypeRef item_fn_type;
    LLVMValueRef fn;
    LLVMBasicBlockRef saved_bb;
    LLVMBasicBlockRef cond;
    LLVMBasicBlockRef cancel;
    LLVMBasicBlockRef body;
    LLVMBasicBlockRef done;

    if (ctx == NULL || item_wrapper_fn == NULL)
        return NULL;
    if (!llvm_parallel_counter_name(ctx, name, sizeof(name),
            "_pgy_pjoin_chunk_", ctx->parallel_counter))
        return NULL;
    cancel_fn = llvm_lookup_function(ctx, "pgy_task_is_cancelled_export");
    type_i8 = LLVMInt8TypeInContext(ctx->context);
    item_fn_type = LLVMGlobalGetValueType(item_wrapper_fn);
    if (cancel_fn == NULL || item_fn_type == NULL) {
        llvm_parallel_join_set_error(ctx, site,
            "LLVM specialized parallel chunk requires item and cancellation functions%s",
            "");
        return NULL;
    }

    fields[0] = ctx->type_i8ptr;
    fields[1] = ctx->type_i64;
    fields[2] = ctx->type_i64;
    fields[3] = ctx->type_i64;
    chunk_type = LLVMStructTypeInContext(ctx->context, fields, 4, 0);
    fn_type = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
    fn = LLVMAddFunction(ctx->module, name, fn_type);
    LLVMSetLinkage(fn, LLVMInternalLinkage);
    saved_bb = LLVMGetInsertBlock(ctx->builder);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "entry");
    cond = LLVMAppendBasicBlockInContext(ctx->context, fn, "pj.chunk.cond");
    cancel = LLVMAppendBasicBlockInContext(ctx->context, fn,
        "pj.chunk.cancel");
    body = LLVMAppendBasicBlockInContext(ctx->context, fn, "pj.chunk.item");
    done = LLVMAppendBasicBlockInContext(ctx->context, fn, "pj.chunk.done");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    LLVMValueRef raw = LLVMGetParam(fn, 0);
    LLVMValueRef chunk = LLVMBuildBitCast(ctx->builder, raw,
        LLVMPointerType(chunk_type, 0), "_chunk");
    LLVMValueRef ctxs_ptr = LLVMBuildStructGEP2(ctx->builder, chunk_type,
        chunk, 0, llvm_tmp_name(ctx));
    LLVMValueRef elem_size_ptr = LLVMBuildStructGEP2(ctx->builder, chunk_type,
        chunk, 1, llvm_tmp_name(ctx));
    LLVMValueRef lo_ptr = LLVMBuildStructGEP2(ctx->builder, chunk_type,
        chunk, 2, llvm_tmp_name(ctx));
    LLVMValueRef hi_ptr = LLVMBuildStructGEP2(ctx->builder, chunk_type,
        chunk, 3, llvm_tmp_name(ctx));
    LLVMValueRef ctxs = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
        ctxs_ptr, llvm_tmp_name(ctx));
    LLVMValueRef elem_size = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
        elem_size_ptr, llvm_tmp_name(ctx));
    LLVMValueRef lo = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
        lo_ptr, llvm_tmp_name(ctx));
    LLVMValueRef hi = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
        hi_ptr, llvm_tmp_name(ctx));
    LLVMValueRef index_slot = LLVMBuildAlloca(ctx->builder, ctx->type_i64,
        "_chunk_i");
    LLVMBuildStore(ctx->builder, lo, index_slot);
    LLVMBuildBr(ctx->builder, cond);

    LLVMPositionBuilderAtEnd(ctx->builder, cond);
    LLVMValueRef index = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
        index_slot, llvm_tmp_name(ctx));
    LLVMValueRef has_more = LLVMBuildICmp(ctx->builder, LLVMIntULT,
        index, hi, llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, has_more, cancel, done);

    LLVMPositionBuilderAtEnd(ctx->builder, cancel);
    LLVMValueRef cancelled = LLVMBuildCall2(ctx->builder,
        cancel_fn->fn_type, cancel_fn->fn, NULL, 0, llvm_tmp_name(ctx));
    LLVMBuildCondBr(ctx->builder, cancelled, done, body);

    LLVMPositionBuilderAtEnd(ctx->builder, body);
    index = LLVMBuildLoad2(ctx->builder, ctx->type_i64, index_slot,
        llvm_tmp_name(ctx));
    LLVMValueRef offset = LLVMBuildMul(ctx->builder, index, elem_size,
        llvm_tmp_name(ctx));
    LLVMValueRef item = LLVMBuildGEP2(ctx->builder, type_i8, ctxs,
        &offset, 1, llvm_tmp_name(ctx));
    LLVMValueRef item_args[] = { item };
    LLVMBuildCall2(ctx->builder, item_fn_type, item_wrapper_fn,
        item_args, 1, "");
    LLVMValueRef next = LLVMBuildAdd(ctx->builder, index,
        LLVMConstInt(ctx->type_i64, 1, 0), llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, index_slot);
    LLVMBuildBr(ctx->builder, cond);

    LLVMPositionBuilderAtEnd(ctx->builder, done);
    LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->type_i8ptr));
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    ctx->parallel_counter++;
    return fn;
}

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
    LLVMValueRef fn_ptr = LLVMBuildBitCast(ctx->builder,
        input->chunk_wrapper_fn,
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
