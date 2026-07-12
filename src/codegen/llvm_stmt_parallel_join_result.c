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
#include <string.h>

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

/* R4 reduce (docs/181): fold the per-task give slots into ONE scalar,
 * in INDEX order (a fixed left fold -- Float determinism across
 * backends and completion orders). The Int/Long sum/product lanes call
 * the same checked-arith exports as surface '+'/'*'; min/max seed from
 * slot 0 after an empty-fan-out fail-closed panic (identity extremes
 * would leak fake domain values); and the min/max select keeps the
 * accumulator on NaN (ordered compare), matching the C ternary twin. */
void
llvm_pjoin_materialize_reduce(LLVMGenCtx *ctx, ASTNode *node,
                              const char *give_name, LLVMTypeRef give_type,
                              LLVMTypeRef ctx_struct_type, LLVMValueRef ctxs,
                              size_t n_captured, LLVMValueRef n_val,
                              LLVMValueRef i_slot, LLVMValueRef *result_out)
{
    const char *rop = ast_parallel_join_reduce_op(node);
    bool float_lane = give_name != NULL
        && (strcmp(give_name, "Float") == 0
            || strcmp(give_name, "Double") == 0);
    bool is_sum = rop != NULL && strcmp(rop, "sum") == 0;
    bool is_product = rop != NULL && strcmp(rop, "product") == 0;
    bool is_min = rop != NULL && strcmp(rop, "min") == 0;
    bool seeded = !is_sum && !is_product;
    LLVMFuncEntry *fold_fn = NULL;
    LLVMValueRef one = LLVMConstInt(ctx->type_i64, 1, 0);
    LLVMValueRef zero = LLVMConstInt(ctx->type_i64, 0, 0);

    if (!float_lane && (is_sum || is_product)) {
        char fn_name[64];

        /* Duration gives are Long-backed (docs/181 SS2.3): the i32
         * checked helpers would mismatch the i64 accumulator. */
        snprintf(fn_name, sizeof(fn_name), "pgy_checked_%s_%s_export",
                 is_sum ? "add" : "mul",
                 strcmp(give_name, "Long") == 0
                     || strcmp(give_name, "Duration") == 0
                     ? "i64" : "i32");
        fold_fn = llvm_required_runtime_function(ctx, node,
            "parallel join reduce", "CheckedArith", fn_name);
        if (fold_fn == NULL)
            return;
    }

    LLVMValueRef acc = LLVMBuildAlloca(ctx->builder, give_type, "_pj_acc");

    if (seeded) {
        LLVMFuncEntry *panic_fn = llvm_required_runtime_function(ctx, node,
            "parallel join reduce", "PanicOutOfBounds",
            "pgy_runtime_panic_out_of_bounds_export");
        if (panic_fn == NULL)
            return;

        LLVMBasicBlockRef red_empty = LLVMAppendBasicBlockInContext(
            ctx->context, ctx->current_function, "pj.red.empty");
        LLVMBasicBlockRef red_seed = LLVMAppendBasicBlockInContext(
            ctx->context, ctx->current_function, "pj.red.seed");
        LLVMValueRef is_empty = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
            n_val, zero, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, is_empty, red_empty, red_seed);

        LLVMPositionBuilderAtEnd(ctx->builder, red_empty);
        {
            /* The message is the shared observable with the C twin. */
            LLVMValueRef msg = LLVMBuildGlobalStringPtr(ctx->builder,
                "min/max reduce over an empty parallel join range",
                llvm_tmp_name(ctx));
            LLVMValueRef panic_args[] = { msg };
            LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
                panic_args, 1, "");
            LLVMBuildUnreachable(ctx->builder);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, red_seed);
        {
            LLVMValueRef ctx0 = LLVMBuildGEP2(ctx->builder,
                ctx_struct_type, ctxs, &zero, 1, llvm_tmp_name(ctx));
            LLVMValueRef give_gep0 = LLVMBuildStructGEP2(ctx->builder,
                ctx_struct_type, ctx0, (unsigned)(n_captured + 1),
                llvm_tmp_name(ctx));
            LLVMValueRef seed = LLVMBuildLoad2(ctx->builder, give_type,
                give_gep0, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, seed, acc);
            LLVMBuildStore(ctx->builder, one, i_slot);
        }
    } else {
        LLVMValueRef init = float_lane
            ? LLVMConstReal(give_type, is_product ? 1.0 : 0.0)
            : LLVMConstInt(give_type, is_product ? 1 : 0, 0);
        LLVMBuildStore(ctx->builder, init, acc);
        LLVMBuildStore(ctx->builder, zero, i_slot);
    }

    LLVMBasicBlockRef red_cond = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.red.cond");
    LLVMBasicBlockRef red_body = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.red.body");
    LLVMBasicBlockRef red_done = LLVMAppendBasicBlockInContext(
        ctx->context, ctx->current_function, "pj.red.done");
    LLVMBuildBr(ctx->builder, red_cond);

    LLVMPositionBuilderAtEnd(ctx->builder, red_cond);
    {
        LLVMValueRef i_val = LLVMBuildLoad2(ctx->builder, ctx->type_i64,
            i_slot, llvm_tmp_name(ctx));
        LLVMValueRef cont = LLVMBuildICmp(ctx->builder, LLVMIntULT,
            i_val, n_val, llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, cont, red_body, red_done);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, red_body);
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
        LLVMValueRef acc_val = LLVMBuildLoad2(ctx->builder, give_type,
            acc, llvm_tmp_name(ctx));
        LLVMValueRef combined;

        if (fold_fn != NULL) {
            LLVMValueRef fold_args[] = { acc_val, give_val };
            combined = LLVMBuildCall2(ctx->builder, fold_fn->fn_type,
                fold_fn->fn, fold_args, 2, llvm_tmp_name(ctx));
        } else if (is_sum) {
            combined = LLVMBuildFAdd(ctx->builder, acc_val, give_val,
                llvm_tmp_name(ctx));
        } else if (is_product) {
            combined = LLVMBuildFMul(ctx->builder, acc_val, give_val,
                llvm_tmp_name(ctx));
        } else {
            LLVMValueRef take_give = float_lane
                ? LLVMBuildFCmp(ctx->builder,
                      is_min ? LLVMRealOLT : LLVMRealOGT,
                      give_val, acc_val, llvm_tmp_name(ctx))
                : LLVMBuildICmp(ctx->builder,
                      is_min ? LLVMIntSLT : LLVMIntSGT,
                      give_val, acc_val, llvm_tmp_name(ctx));
            combined = LLVMBuildSelect(ctx->builder, take_give, give_val,
                acc_val, llvm_tmp_name(ctx));
        }
        LLVMBuildStore(ctx->builder, combined, acc);
        LLVMValueRef next = LLVMBuildAdd(ctx->builder, i_val, one,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, next, i_slot);
        LLVMBuildBr(ctx->builder, red_cond);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, red_done);
    *result_out = LLVMBuildLoad2(ctx->builder, give_type, acc,
        "_pj_red_val");
}

#endif /* PGY_LLVM_ENABLED */
