/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_scalar_core.h"

#include <stdio.h>
#include <string.h>

#include "llvm_expr_string_coerce.h"
#include "llvm_internal_api.h"

static LLVMTypeRef
llvm_function_signature_from_event_type(LLVMGenCtx *ctx, ASTNode *type_node)
{
    size_t param_count;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef ret_type = ctx->type_void;
    LLVMTypeRef fn_type;

    if (ctx == NULL || type_node == NULL || type_node->type != AST_EVENT_HANDLER_TYPE)
        return NULL;

    param_count = type_node->data.event_handler_type.param_count;
    if (type_node->data.event_handler_type.return_type != NULL) {
        ret_type = ast_type_to_llvm(ctx, type_node->data.event_handler_type.return_type);
        if (ctx->has_error || ret_type == NULL)
            return NULL;
    }

    if (param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch, param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM event-handler signature parameter allocation failed");
            return NULL;
        }
        for (size_t i = 0; i < param_count; i++) {
            param_types[i] = ast_type_to_llvm(ctx,
                type_node->data.event_handler_type.param_types[i]);
            if (ctx->has_error || param_types[i] == NULL)
                return NULL;
        }
    }

    fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    return fn_type;
}

LLVMTypeRef
llvm_function_signature_from_callable_entry(LLVMGenCtx *ctx,
                                            const LLVMCallableVarEntry *entry)
{
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef ret_type = NULL;

    if (ctx == NULL || entry == NULL)
        return NULL;
    if (entry->type_node != NULL)
        return llvm_function_signature_from_event_type(ctx, entry->type_node);

    ret_type = entry->return_type != NULL
        ? ast_type_to_llvm(ctx, entry->return_type)
        : ctx->type_void;
    if (ctx->has_error || ret_type == NULL)
        return NULL;
    if (entry->param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
                                       entry->param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM callable signature parameter allocation failed");
            return NULL;
        }
        for (size_t i = 0; i < entry->param_count; i++) {
            param_types[i] = ast_type_to_llvm(ctx, entry->param_types[i]);
            if (ctx->has_error || param_types[i] == NULL)
                return NULL;
        }
    }

    return LLVMFunctionType(ret_type, param_types,
                            (unsigned)entry->param_count, 0);
}

static LLVMFuncEntry *
llvm_required_scalar_runtime_function(LLVMGenCtx *ctx,
                                      ASTNode *node,
                                      const char *operation_name,
                                      const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s requires registered runtime function '%s'",
            operation_name != NULL ? operation_name : "scalar operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMFuncEntry *
llvm_required_checked_math_function(LLVMGenCtx *ctx,
                                    ASTNode *node,
                                    const char *function_name)
{
    return llvm_required_scalar_runtime_function(ctx, node,
        "checked arithmetic", function_name);
}

static LLVMValueRef
llvm_scalar_expr_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s",
            message != NULL ? message
                : "LLVM scalar expression could not be lowered");
    }
    return NULL;
}

LLVMValueRef
llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef left  = llvm_emit_expression(node->data.binary.left, ctx);
    LLVMValueRef right = llvm_emit_expression(node->data.binary.right, ctx);
    if (left == NULL || right == NULL)
        return llvm_scalar_expr_error(ctx, node,
            "LLVM binary expression could not lower operand expression");

    LLVMTypeRef left_type  = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);
    {
        const char *suffix = llvm_operator_overload_suffix(
            node->data.binary.op.type);
        const char *type_name = llvm_expr_custom_type_name(
            node->data.binary.left, ctx);

        if (type_name == NULL && left_type == right_type) {
            const char *primitive_suffix = llvm_type_to_suffix(ctx, left_type);
            if (primitive_suffix != NULL
                && strcmp(primitive_suffix, "Unknown") != 0) {
                type_name = primitive_suffix;
            }
        }

        if (type_name != NULL && suffix != NULL) {
            size_t prefix_len = strlen("operator_");
            size_t suffix_len = strlen(suffix);
            size_t type_len = strlen(type_name);
            size_t fn_len;
            if (suffix_len > ((size_t)-1) - prefix_len - 2)
                return llvm_scalar_expr_error(ctx, node,
                    "LLVM operator overload name is too large");
            if (type_len > ((size_t)-1) - prefix_len - suffix_len - 2)
                return llvm_scalar_expr_error(ctx, node,
                    "LLVM operator overload name is too large");
            fn_len = prefix_len + suffix_len + type_len + 2;
            char *fn_name = pgy_arena_alloc(&ctx->scratch, fn_len);
            if (fn_name == NULL)
                return llvm_scalar_expr_error(ctx, node,
                    "LLVM operator overload name allocation failed");
            snprintf(fn_name, fn_len, "operator_%s_%s", suffix, type_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            if (fn != NULL) {
                LLVMValueRef args[] = { left, right };
                if (fn->ret_type == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                }
                return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                      args, 2, llvm_tmp_name(ctx));
            }
        }
    }

    if (node->data.binary.op.type == TOKEN_PLUS
        && (left_type == ctx->type_i8ptr || right_type == ctx->type_i8ptr)) {
        LLVMFuncEntry *fn = llvm_required_scalar_runtime_function(ctx, node,
            "string concatenation", "StringConcat");
        if (left_type != ctx->type_i8ptr)
            left = llvm_coerce_value_to_string(left, ctx);
        if (right_type != ctx->type_i8ptr)
            right = llvm_coerce_value_to_string(right, ctx);
        if (fn != NULL && left != NULL && right != NULL
            && LLVMTypeOf(left) == ctx->type_i8ptr
            && LLVMTypeOf(right) == ctx->type_i8ptr) {
            LLVMValueRef args[] = { left, right };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        }
        return llvm_scalar_expr_error(ctx, node,
            "LLVM string concatenation could not lower/coerce operands");
    }

    if ((node->data.binary.op.type == TOKEN_EQUAL
         || node->data.binary.op.type == TOKEN_NOT_EQUAL)
        && (left_type == ctx->type_i8ptr || right_type == ctx->type_i8ptr)) {
        LLVMFuncEntry *fn = llvm_required_scalar_runtime_function(ctx, node,
            "string comparison", "pgy_string_equals");
        if (left_type != ctx->type_i8ptr)
            left = llvm_coerce_value_to_string(left, ctx);
        if (right_type != ctx->type_i8ptr)
            right = llvm_coerce_value_to_string(right, ctx);
        if (fn != NULL && left != NULL && right != NULL
            && LLVMTypeOf(left) == ctx->type_i8ptr
            && LLVMTypeOf(right) == ctx->type_i8ptr) {
            LLVMValueRef args[] = { left, right };
            LLVMValueRef eq = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                             args, 2, llvm_tmp_name(ctx));
            if (node->data.binary.op.type == TOKEN_EQUAL)
                return eq;
            return LLVMBuildNot(ctx->builder, eq, llvm_tmp_name(ctx));
        }
        return llvm_scalar_expr_error(ctx, node,
            "LLVM string comparison could not lower/coerce operands");
    }

    bool is_float = (left_type == ctx->type_f64 || left_type == ctx->type_f32
                  || right_type == ctx->type_f64 || right_type == ctx->type_f32);

    if (is_float) {
        if (left_type == ctx->type_i32)
            left = LLVMBuildSIToFP(ctx->builder, left, ctx->type_f64,
                                    llvm_tmp_name(ctx));
        if (right_type == ctx->type_i32)
            right = LLVMBuildSIToFP(ctx->builder, right, ctx->type_f64,
                                     llvm_tmp_name(ctx));
    }

    if (!is_float && (node->data.binary.op.type == TOKEN_SLASH
        || node->data.binary.op.type == TOKEN_PERCENT)) {
        bool use_i64 = left_type == ctx->type_i64 || right_type == ctx->type_i64;
        const char *helper = node->data.binary.op.type == TOKEN_SLASH
            ? (use_i64 ? "pgy_checked_div_i64_export"
                       : "pgy_checked_div_i32_export")
            : (use_i64 ? "pgy_checked_mod_i64_export"
                       : "pgy_checked_mod_i32_export");
        LLVMFuncEntry *fn = llvm_required_checked_math_function(ctx, node,
            helper);
        if (fn != NULL) {
            LLVMValueRef args[2];
            if (use_i64) {
                if (left_type == ctx->type_i32)
                    left = LLVMBuildSExt(ctx->builder, left, ctx->type_i64,
                                         llvm_tmp_name(ctx));
                if (right_type == ctx->type_i32)
                    right = LLVMBuildSExt(ctx->builder, right, ctx->type_i64,
                                          llvm_tmp_name(ctx));
            }
            args[0] = left;
            args[1] = right;
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        }
        return llvm_scalar_expr_error(ctx, node,
            "LLVM checked arithmetic requires runtime helper lowering");
    }

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.binary.op.type) {
    case TOKEN_PLUS:
        return is_float
            ? LLVMBuildFAdd(ctx->builder, left, right, tmp)
            : LLVMBuildAdd(ctx->builder, left, right, tmp);
    case TOKEN_MINUS:
        return is_float
            ? LLVMBuildFSub(ctx->builder, left, right, tmp)
            : LLVMBuildSub(ctx->builder, left, right, tmp);
    case TOKEN_STAR:
        return is_float
            ? LLVMBuildFMul(ctx->builder, left, right, tmp)
            : LLVMBuildMul(ctx->builder, left, right, tmp);
    case TOKEN_SLASH:
        return is_float
            ? LLVMBuildFDiv(ctx->builder, left, right, tmp)
            : LLVMBuildSDiv(ctx->builder, left, right, tmp);
    case TOKEN_PERCENT:
        return LLVMBuildSRem(ctx->builder, left, right, tmp);
    case TOKEN_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOEQ, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, tmp);
    case TOKEN_NOT_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealONE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, tmp);
    case TOKEN_LESS:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, tmp);
    case TOKEN_LESS_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOLE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, tmp);
    case TOKEN_GREATER:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGT, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, tmp);
    case TOKEN_GREATER_EQUAL:
        return is_float
            ? LLVMBuildFCmp(ctx->builder, LLVMRealOGE, left, right, tmp)
            : LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, tmp);
    case TOKEN_AND:
        return LLVMBuildAnd(ctx->builder, left, right, tmp);
    case TOKEN_OR:
        return LLVMBuildOr(ctx->builder, left, right, tmp);
    default:
        return llvm_scalar_expr_error(ctx, node,
            "LLVM binary expression uses an unsupported operator");
    }
}

LLVMValueRef
llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.unary.op.type == TOKEN_QUESTION) {
        LLVMValueRef result = llvm_emit_expression(node->data.unary.operand, ctx);
        LLVMTypeRef result_ty;
        unsigned field_count;

        if (result == NULL)
            return llvm_scalar_expr_error(ctx, node,
                "LLVM try operator could not lower operand expression");
        result_ty = LLVMTypeOf(result);
        if (LLVMGetTypeKind(result_ty) != LLVMStructTypeKind)
            return llvm_scalar_expr_error(ctx, node,
                "LLVM try operator requires Result-like aggregate operand");
        field_count = LLVMCountStructElementTypes(result_ty);
        if (field_count < 2 || ctx->current_function == NULL)
            return llvm_scalar_expr_error(ctx, node,
                "LLVM try operator requires Result payload fields and active function");

        LLVMTypeRef fields[8];
        LLVMGetStructElementTypes(result_ty, fields);

        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, result, 0, llvm_tmp_name(ctx));
        LLVMValueRef is_ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));

        LLVMValueRef ok_alloca = llvm_create_entry_alloca(ctx, fields[1], llvm_tmp_name(ctx));
        if (ok_alloca == NULL)
            return llvm_scalar_expr_error(ctx, node,
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
            && ctx->current_func_decl->data.func_decl.return_type != NULL) {
            LLVMTypeRef declared = ast_type_to_llvm(ctx,
                ctx->current_func_decl->data.func_decl.return_type);
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
            LLVMBuildUnreachable(ctx->builder);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        return LLVMBuildLoad2(ctx->builder, fields[1], ok_alloca, llvm_tmp_name(ctx));
    }

    LLVMValueRef operand = llvm_emit_expression(node->data.unary.operand, ctx);
    if (operand == NULL)
        return llvm_scalar_expr_error(ctx, node,
            "LLVM unary expression could not lower operand expression");

    const char *tmp = llvm_tmp_name(ctx);

    switch (node->data.unary.op.type) {
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
