#ifdef PGY_LLVM_ENABLED
/*
 * Copyright (c) 2026 Pergyra Language Project
 * Result materialization for the expression-form parallel join
 * (docs/181 R2): after the all-join, per-task give slots become one
 * Array<R> value in INDEX order -- never completion order, which is what
 * lets byte-equal compare bite a parallel result. Split out of
 * llvm_stmt_parallel_join.c under the 550-line responsibility rule.
 */

#include "llvm_internal.h"

#include <stdio.h>

void
llvm_pjoin_materialize_result(LLVMGenCtx *ctx, ASTNode *node,
                              const char *give_name, LLVMTypeRef give_type,
                              LLVMTypeRef ctx_struct_type, LLVMValueRef ctxs,
                              size_t n_captured, LLVMValueRef n_val,
                              LLVMValueRef i_slot, LLVMValueRef *result_out)
{
    /* give_name is a bare primitive ("Int"), so the runtime suffix is
     * the name itself. */
    char fn_name[64];
    LLVMFuncEntry *new_fn = NULL;
    LLVMFuncEntry *push_fn = NULL;
    LLVMTypeRef arr_struct = llvm_array_struct_type(ctx, give_name);
    LLVMValueRef one = LLVMConstInt(ctx->type_i64, 1, 0);
    LLVMValueRef zero = LLVMConstInt(ctx->type_i64, 0, 0);

    if (snprintf(fn_name, sizeof(fn_name), "pgy_array_new_%s",
                 give_name) < (int)sizeof(fn_name))
        new_fn = llvm_required_runtime_function(ctx, node,
            "parallel join result", "ArrayNew", fn_name);
    if (snprintf(fn_name, sizeof(fn_name), "pgy_array_push_%s",
                 give_name) < (int)sizeof(fn_name))
        push_fn = llvm_required_runtime_function(ctx, node,
            "parallel join result", "ArrayPush", fn_name);
    if (arr_struct == NULL || new_fn == NULL || push_fn == NULL)
        return;

    LLVMValueRef res_alloca = LLVMBuildAlloca(ctx->builder, arr_struct,
        "_pj_res");
    LLVMValueRef new_args[] = { n_val };
    LLVMValueRef res_init = LLVMBuildCall2(ctx->builder,
        new_fn->fn_type, new_fn->fn, new_args, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, res_init, res_alloca);
    LLVMBuildStore(ctx->builder, zero, i_slot);

    LLVMBasicBlockRef res_cond = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.res.cond");
    LLVMBasicBlockRef res_body = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.res.body");
    LLVMBasicBlockRef res_done = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.res.done");
    LLVMBuildBr(ctx->builder, res_cond);

    LLVMPositionBuilderAtEnd(ctx->builder, res_cond);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef cont = LLVMBuildICmp(ctx->builder, LLVMIntULT,
            i_val, n_val, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, cont, res_body, res_done);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, res_body);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef ctx_i = LLVMBuildGEP2(ctx->builder,
            ctx_struct_type, ctxs, &i_val, 1, llvm_tmp_name(ctx));
        LLVMValueRef give_gep = LLVMBuildStructGEP2(ctx->builder,
            ctx_struct_type, ctx_i, (unsigned)(n_captured + 1),
            llvm_tmp_name(ctx));
        LLVMValueRef give_val = LLVMBuildLoad2(ctx->builder, give_type,
            give_gep, llvm_tmp_name(ctx));
        LLVMValueRef push_args[] = { res_alloca, give_val };
        LLVMBuildCall2(ctx->builder, push_fn->fn_type, push_fn->fn,
            push_args, 2, "");
        LLVMValueRef next = LLVMBuildAdd(ctx->builder, i_val, one,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, next, i_slot);
        LLVMBuildBr(ctx->builder, res_cond);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, res_done);
    *result_out = LLVMBuildLoad2(ctx->builder, arr_struct, res_alloca,
        "_pj_res_val");
}

#endif /* PGY_LLVM_ENABLED */
