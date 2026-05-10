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

    if (inner == NULL || strcmp(inner, "Void") == 0) {
        if (!is_remote)
            return LLVMConstInt(ctx->type_i32, 0, 0);

        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, ctx->type_i32, ctx->type_i8ptr },
            3, 0);
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return r;
    }

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || inner_ty == NULL)
        return NULL;
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

    if (!is_remote)
        return value;

    {
        LLVMTypeRef result_fields[] = { ctx->type_i32, inner_ty,
            ctx->type_i8ptr };
        LLVMTypeRef result_ty = LLVMStructTypeInContext(ctx->context,
            result_fields, 3, 0);
        LLVMValueRef result = LLVMGetUndef(result_ty);
        result = LLVMBuildInsertValue(ctx->builder, result,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result,
            value, 1, llvm_tmp_name(ctx));
        result = LLVMBuildInsertValue(ctx->builder, result,
            LLVMConstNull(ctx->type_i8ptr), 2, llvm_tmp_name(ctx));
        return result;
    }
}

#endif
