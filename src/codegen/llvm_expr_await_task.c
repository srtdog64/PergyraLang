/*
 * LLVM await expression task-handle lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_spawn_call_helpers.h"

#include <string.h>

LLVMValueRef
llvm_await_task_handle(LLVMGenCtx *ctx, ASTNode *node, LLVMValueRef task,
                       const char *inner, bool is_remote)
{
    LLVMFuncEntry *await_fn = llvm_lookup_function(ctx, "pgy_await_export");
    LLVMValueRef args[1];
    LLVMValueRef raw;
    LLVMTypeRef inner_ty;
    LLVMValueRef value;
    LLVMFuncEntry *free_fn;

    if (await_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM await expression requires registered runtime function '%s'",
            "pgy_await_export");
        return NULL;
    }
    if (task == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM await expression could not lower task handle expression");
        return NULL;
    }

    args[0] = task;
    raw = LLVMBuildCall2(ctx->builder, await_fn->fn_type,
        await_fn->fn, args, 1, llvm_tmp_name(ctx));

    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM await expression requires registered Future<T> result metadata");
        return NULL;
    }

    if (strcmp(inner, "Void") == 0) {
        if (!is_remote)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM await expression does not lower RemoteFuture<Void>; "
            "semantic analysis must reject it until Result<Void> ABI is frozen");
        return NULL;
    }

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || inner_ty == NULL)
        return NULL;
    free_fn = llvm_lookup_function(ctx, "free");
    if (free_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM await expression requires registered runtime function '%s'",
            "free");
        return NULL;
    }

    if (is_remote) {
        LLVMValueRef is_null;
        LLVMValueRef ok_result;
        LLVMValueRef err_result;
        LLVMValueRef phi;
        LLVMValueRef free_args[1];
        LLVMTypeRef result_fields[] = { ctx->type_i32, inner_ty,
            ctx->type_i8ptr };
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            result_fields, 3, 0);
        LLVMValueRef null_result = LLVMConstNull(ctx->type_i8ptr);
        LLVMValueRef err_msg = LLVMBuildGlobalStringPtr(ctx->builder,
            "remote operation failed", llvm_tmp_name(ctx));
        LLVMValueRef current_fn = LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(ctx->builder));
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(
            ctx->context, current_fn, "await.remote.ok");
        LLVMBasicBlockRef err_bb = LLVMAppendBasicBlockInContext(
            ctx->context, current_fn, "await.remote.err");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
            ctx->context, current_fn, "await.remote.merge");

        is_null = LLVMBuildICmp(ctx->builder, LLVMIntEQ, raw,
            null_result, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, is_null, err_bb, ok_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
        if (strcmp(inner, "String") == 0) {
            LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
                LLVMPointerType(ctx->type_i8ptr, 0), llvm_tmp_name(ctx));
            value = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
                typed_ptr, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
                LLVMPointerType(inner_ty, 0), llvm_tmp_name(ctx));
            value = LLVMBuildLoad2(ctx->builder, inner_ty, typed_ptr,
                llvm_tmp_name(ctx));
        }
        free_args[0] = raw;
        LLVMBuildCall2(ctx->builder, free_fn->fn_type, free_fn->fn,
            free_args, 1, "");
        ok_result = LLVMGetUndef(result_ty);
        ok_result = LLVMBuildInsertValue(ctx->builder, ok_result,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        ok_result = LLVMBuildInsertValue(ctx->builder, ok_result,
            value, 1, llvm_tmp_name(ctx));
        ok_result = LLVMBuildInsertValue(ctx->builder, ok_result,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        LLVMBuildBr(ctx->builder, merge_bb);
        ok_bb = LLVMGetInsertBlock(ctx->builder);

        LLVMPositionBuilderAtEnd(ctx->builder, err_bb);
        err_result = LLVMGetUndef(result_ty);
        err_result = LLVMBuildInsertValue(ctx->builder, err_result,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        err_result = LLVMBuildInsertValue(ctx->builder, err_result,
            LLVMConstNull(inner_ty), 1, llvm_tmp_name(ctx));
        err_result = LLVMBuildInsertValue(ctx->builder, err_result,
            err_msg, 2, llvm_tmp_name(ctx));
        LLVMBuildBr(ctx->builder, merge_bb);
        err_bb = LLVMGetInsertBlock(ctx->builder);

        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        phi = LLVMBuildPhi(ctx->builder, result_ty, llvm_tmp_name(ctx));
        {
            LLVMValueRef incoming_vals[] = { ok_result, err_result };
            LLVMBasicBlockRef incoming_bbs[] = { ok_bb, err_bb };
            LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);
        }
        return phi;
    }

    {
        LLVMValueRef is_null;
        LLVMValueRef null_value = LLVMConstNull(inner_ty);
        LLVMValueRef phi;
        LLVMValueRef null_ptr = LLVMConstNull(ctx->type_i8ptr);
        LLVMValueRef current_fn = LLVMGetBasicBlockParent(
            LLVMGetInsertBlock(ctx->builder));
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(
            ctx->context, current_fn, "await.local.ok");
        LLVMBasicBlockRef null_bb = LLVMAppendBasicBlockInContext(
            ctx->context, current_fn, "await.local.null");
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
            ctx->context, current_fn, "await.local.merge");

        is_null = LLVMBuildICmp(ctx->builder, LLVMIntEQ, raw,
            null_ptr, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, is_null, null_bb, ok_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
        if (strcmp(inner, "String") == 0) {
            LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
                LLVMPointerType(ctx->type_i8ptr, 0), llvm_tmp_name(ctx));
            value = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
                typed_ptr, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef typed_ptr = LLVMBuildBitCast(ctx->builder, raw,
                LLVMPointerType(inner_ty, 0), llvm_tmp_name(ctx));
            value = LLVMBuildLoad2(ctx->builder, inner_ty, typed_ptr,
                llvm_tmp_name(ctx));
        }
        {
            LLVMValueRef free_args[] = { raw };
            LLVMBuildCall2(ctx->builder, free_fn->fn_type, free_fn->fn,
                free_args, 1, "");
        }
        LLVMBuildBr(ctx->builder, merge_bb);
        ok_bb = LLVMGetInsertBlock(ctx->builder);

        LLVMPositionBuilderAtEnd(ctx->builder, null_bb);
        LLVMBuildBr(ctx->builder, merge_bb);
        null_bb = LLVMGetInsertBlock(ctx->builder);

        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        phi = LLVMBuildPhi(ctx->builder, inner_ty, llvm_tmp_name(ctx));
        {
            LLVMValueRef incoming_vals[] = { value, null_value };
            LLVMBasicBlockRef incoming_bbs[] = { ok_bb, null_bb };
            LLVMAddIncoming(phi, incoming_vals, incoming_bbs, 2);
        }
        return phi;
    }
}

#endif
