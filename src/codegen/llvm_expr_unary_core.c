/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_scalar_core.h"

static LLVMValueRef
llvm_unary_expr_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s",
            message != NULL ? message
                : "LLVM unary expression could not be lowered");
    }
    return NULL;
}

static bool
llvm_emit_try_operator_unwrap_panic(LLVMGenCtx *ctx, ASTNode *node)
{
    LLVMFuncEntry *panic_fn;

    if (ctx == NULL)
        return false;

    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM try operator requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
        LLVMBuildUnreachable(ctx->builder);
        return false;
    }
    if (panic_fn != NULL) {
        LLVMValueRef reason = LLVMBuildGlobalStringPtr(ctx->builder,
            "Result unwrap on Err value", llvm_tmp_name(ctx));
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            &reason, 1, "");
    }
    LLVMBuildUnreachable(ctx->builder);
    return true;
}

LLVMValueRef
llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx)
{
    if (ast_unary_operator(node).type == TOKEN_QUESTION) {
        LLVMValueRef result = llvm_emit_expression(ast_unary_operand(node), ctx);
        LLVMTypeRef result_ty;
        unsigned field_count;

        if (result == NULL)
            return llvm_unary_expr_error(ctx, node,
                "LLVM try operator could not lower operand expression");
        result_ty = LLVMTypeOf(result);
        if (LLVMGetTypeKind(result_ty) != LLVMStructTypeKind)
            return llvm_unary_expr_error(ctx, node,
                "LLVM try operator requires Result-like aggregate operand");
        field_count = LLVMCountStructElementTypes(result_ty);
        if (field_count < 2 || ctx->current_function == NULL)
            return llvm_unary_expr_error(ctx, node,
                "LLVM try operator requires Result payload fields and active function");

        LLVMTypeRef fields[8];
        LLVMGetStructElementTypes(result_ty, fields);

        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, result, 0, llvm_tmp_name(ctx));
        LLVMValueRef is_ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));

        LLVMValueRef ok_alloca = llvm_create_entry_alloca(ctx, fields[1], llvm_tmp_name(ctx));
        if (ok_alloca == NULL)
            return llvm_unary_expr_error(ctx, node,
                "LLVM try operator payload allocation failed");
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.ok");
        LLVMBasicBlockRef err_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.err");
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.cont");

        LLVMBuildCondBr(ctx->builder, is_ok, ok_bb, err_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
        {
            LLVMValueRef ok_value = LLVMBuildExtractValue(ctx->builder, result,
                1, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, ok_value, ok_alloca);
            LLVMBuildBr(ctx->builder, cont_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, err_bb);
        LLVMTypeRef fn_ret_type = ctx->current_ret_type;
        if (ctx->current_func_decl != NULL
            && ctx->current_func_decl->type == AST_FUNC_DECL
            && ast_func_return_type(ctx->current_func_decl) != NULL) {
            LLVMTypeRef declared = ast_type_to_llvm(ctx,
                ast_func_return_type(ctx->current_func_decl));
            if (declared != NULL)
                fn_ret_type = declared;
        }
        if (fn_ret_type == result_ty) {
            LLVMBuildRet(ctx->builder, result);
        } else if (fn_ret_type != NULL
            && LLVMGetTypeKind(fn_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(fn_ret_type) == 3) {
            LLVMTypeRef ret_fields[3];
            LLVMGetStructElementTypes(fn_ret_type, ret_fields);
            LLVMValueRef err_val = LLVMBuildExtractValue(ctx->builder, result,
                2, llvm_tmp_name(ctx));
            if (LLVMTypeOf(err_val) != ret_fields[2]) {
                LLVMTypeRef src_ty = LLVMTypeOf(err_val);
                LLVMTypeRef dst_ty = ret_fields[2];
                if (LLVMGetTypeKind(src_ty) == LLVMIntegerTypeKind
                    && LLVMGetTypeKind(dst_ty) == LLVMIntegerTypeKind) {
                    err_val = (LLVMGetIntTypeWidth(dst_ty) > LLVMGetIntTypeWidth(src_ty))
                        ? LLVMBuildSExt(ctx->builder, err_val, dst_ty, llvm_tmp_name(ctx))
                        : LLVMBuildTrunc(ctx->builder, err_val, dst_ty, llvm_tmp_name(ctx));
                } else if (LLVMGetTypeKind(src_ty) == LLVMPointerTypeKind
                           && LLVMGetTypeKind(dst_ty) == LLVMPointerTypeKind) {
                    err_val = LLVMBuildBitCast(ctx->builder, err_val, dst_ty,
                        llvm_tmp_name(ctx));
                } else {
                    llvm_set_error_at_with_hints(ctx, node,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_ALIGN_RESULT_ERROR_TYPE,
                        "LLVM try operator cannot coerce Result error payload to current function error type");
                    LLVMBuildUnreachable(ctx->builder);
                    LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
                    return NULL;
                }
            }
            LLVMValueRef rebuilt = LLVMGetUndef(fn_ret_type);
            rebuilt = LLVMBuildInsertValue(ctx->builder, rebuilt,
                LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
            rebuilt = LLVMBuildInsertValue(ctx->builder, rebuilt,
                LLVMConstNull(ret_fields[1]), 1, llvm_tmp_name(ctx));
            rebuilt = LLVMBuildInsertValue(ctx->builder, rebuilt,
                err_val, 2, llvm_tmp_name(ctx));
            LLVMBuildRet(ctx->builder, rebuilt);
        } else {
            if (!llvm_emit_try_operator_unwrap_panic(ctx, node)) {
                LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
                return NULL;
            }
        }

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        return LLVMBuildLoad2(ctx->builder, fields[1], ok_alloca, llvm_tmp_name(ctx));
    }

    LLVMValueRef operand = llvm_emit_expression(ast_unary_operand(node), ctx);
    if (operand == NULL)
        return llvm_unary_expr_error(ctx, node,
            "LLVM unary expression could not lower operand expression");

    const char *tmp = llvm_tmp_name(ctx);

    switch (ast_unary_operator(node).type) {
    case TOKEN_MINUS:
        if (LLVMTypeOf(operand) == ctx->type_f64 ||
            LLVMTypeOf(operand) == ctx->type_f32)
            return LLVMBuildFNeg(ctx->builder, operand, tmp);
        return LLVMBuildNeg(ctx->builder, operand, tmp);
    case TOKEN_NOT:
        return LLVMBuildNot(ctx->builder, operand, tmp);
    default:
        return operand;
    }
}

#endif
