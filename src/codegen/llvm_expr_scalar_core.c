/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_scalar_core.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "codegen_scalar_arithmetic_policy.h"
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

    param_count = ast_event_handler_param_count(type_node);
    ASTNode *return_type = ast_event_handler_return_type(type_node);
    if (return_type != NULL) {
        ret_type = ast_type_to_llvm(ctx, return_type);
        if (ctx->has_error || ret_type == NULL)
            return NULL;
    }

    if (param_count > 0) {
        if (param_count > (size_t)UINT_MAX
            || param_count > SIZE_MAX / sizeof(LLVMTypeRef)) {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                "LLVM event-handler signature parameter count exceeds backend ABI limits");
            return NULL;
        }
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
                ast_event_handler_param_type(type_node, i));
            if (ctx->has_error || param_types[i] == NULL)
                return NULL;
        }
    }

    fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    return fn_type;
}

static LLVMTypeRef
llvm_binary_float_target_type(LLVMGenCtx *ctx, LLVMTypeRef left_type,
                              LLVMTypeRef right_type, ASTNode *left_expr,
                              ASTNode *right_expr)
{
    if (left_type == ctx->type_f32 && right_type == ctx->type_f64
        && right_expr != NULL && right_expr->type == AST_NUMBER)
        return ctx->type_f32;
    if (right_type == ctx->type_f32 && left_type == ctx->type_f64
        && left_expr != NULL && left_expr->type == AST_NUMBER)
        return ctx->type_f32;
    if (left_type == ctx->type_f64 || right_type == ctx->type_f64)
        return ctx->type_f64;
    return ctx->type_f32;
}

static LLVMValueRef
llvm_coerce_numeric_to_fp(LLVMGenCtx *ctx, LLVMValueRef value,
                          LLVMTypeRef target_type)
{
    LLVMTypeRef source_type;

    if (ctx == NULL || value == NULL || target_type == NULL)
        return value;
    source_type = LLVMTypeOf(value);
    if (source_type == target_type)
        return value;
    if (source_type == ctx->type_i32 || source_type == ctx->type_i64)
        return LLVMBuildSIToFP(ctx->builder, value, target_type,
                               llvm_tmp_name(ctx));
    if (source_type == ctx->type_f32 && target_type == ctx->type_f64)
        return LLVMBuildFPExt(ctx->builder, value, target_type,
                              llvm_tmp_name(ctx));
    if (source_type == ctx->type_f64 && target_type == ctx->type_f32)
        return LLVMBuildFPTrunc(ctx->builder, value, target_type,
                                llvm_tmp_name(ctx));
    return value;
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

static LLVMValueRef
llvm_scalar_coerce_payload(LLVMGenCtx *ctx, LLVMValueRef val,
                           LLVMTypeRef target_ty)
{
    LLVMTypeRef val_ty;

    if (ctx == NULL || val == NULL || target_ty == NULL)
        return val;
    val_ty = LLVMTypeOf(val);
    if (val_ty == target_ty)
        return val;
    if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
        && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
        return (LLVMGetIntTypeWidth(target_ty) > LLVMGetIntTypeWidth(val_ty))
            ? LLVMBuildSExt(ctx->builder, val, target_ty, llvm_tmp_name(ctx))
            : LLVMBuildTrunc(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    }
    if ((target_ty == ctx->type_f32 || target_ty == ctx->type_f64)
        && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64))
        return LLVMBuildSIToFP(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    if ((val_ty == ctx->type_f32 || val_ty == ctx->type_f64)
        && (target_ty == ctx->type_i32 || target_ty == ctx->type_i64))
        return LLVMBuildFPToSI(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    if ((val_ty == ctx->type_f32 && target_ty == ctx->type_f64))
        return LLVMBuildFPExt(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    if ((val_ty == ctx->type_f64 && target_ty == ctx->type_f32))
        return LLVMBuildFPTrunc(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    return val;
}

static LLVMValueRef
llvm_emit_option_coalesce(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef left;
    LLVMTypeRef left_type;
    LLVMTypeRef fields[2];
    LLVMValueRef current_fn;
    LLVMBasicBlockRef some_bb;
    LLVMBasicBlockRef fallback_bb;
    LLVMBasicBlockRef merge_bb;
    LLVMBasicBlockRef incoming_blocks[2];
    LLVMValueRef incoming_values[2];
    unsigned incoming_count = 0;
    LLVMValueRef tag;
    LLVMValueRef is_some;
    LLVMValueRef value;
    LLVMValueRef fallback;
    LLVMValueRef phi;

    left = llvm_emit_expression(ast_binary_left(node), ctx);
    if (left == NULL)
        return llvm_scalar_expr_error(ctx, node,
            "LLVM coalesce operator could not lower left Option operand");

    left_type = LLVMTypeOf(left);
    if (LLVMGetTypeKind(left_type) != LLVMStructTypeKind
        || LLVMCountStructElementTypes(left_type) != 2) {
        return llvm_scalar_expr_error(ctx, node,
            "LLVM coalesce operator requires concrete Option<T> aggregate operand");
    }

    current_fn = ctx != NULL ? ctx->current_function : NULL;
    if (current_fn == NULL && ctx != NULL && ctx->builder != NULL) {
        LLVMBasicBlockRef block = LLVMGetInsertBlock(ctx->builder);
        current_fn = block != NULL ? LLVMGetBasicBlockParent(block) : NULL;
    }
    if (current_fn == NULL)
        return llvm_scalar_expr_error(ctx, node,
            "LLVM coalesce operator requires an active function for lazy fallback lowering");

    LLVMGetStructElementTypes(left_type, fields);
    tag = LLVMBuildExtractValue(ctx->builder, left, 0, llvm_tmp_name(ctx));
    is_some = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
        LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));

    some_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
                                            "coalesce.some");
    fallback_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
                                                "coalesce.fallback");
    merge_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
                                             "coalesce.merge");

    LLVMBuildCondBr(ctx->builder, is_some, some_bb, fallback_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, some_bb);
    value = LLVMBuildExtractValue(ctx->builder, left, 1, llvm_tmp_name(ctx));
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        incoming_blocks[incoming_count] = LLVMGetInsertBlock(ctx->builder);
        incoming_values[incoming_count] = value;
        incoming_count++;
        LLVMBuildBr(ctx->builder, merge_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fallback_bb);
    fallback = llvm_emit_expression(ast_binary_right(node), ctx);
    if (fallback == NULL)
        return llvm_scalar_expr_error(ctx, node,
            "LLVM coalesce operator could not lower fallback expression");
    fallback = llvm_scalar_coerce_payload(ctx, fallback, fields[1]);
    if (LLVMTypeOf(fallback) != fields[1])
        return llvm_scalar_expr_error(ctx, node,
            "LLVM coalesce operator fallback type does not match Option payload type");
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        incoming_blocks[incoming_count] = LLVMGetInsertBlock(ctx->builder);
        incoming_values[incoming_count] = fallback;
        incoming_count++;
        LLVMBuildBr(ctx->builder, merge_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
    if (incoming_count == 0)
        return LLVMGetUndef(fields[1]);

    phi = LLVMBuildPhi(ctx->builder, fields[1], llvm_tmp_name(ctx));
    LLVMAddIncoming(phi, incoming_values, incoming_blocks, incoming_count);
    return phi;
}

LLVMValueRef
llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx)
{
    PgyTokenType op_type = ast_binary_operator(node).type;
    ASTNode *left_expr = ast_binary_left(node);
    ASTNode *right_expr = ast_binary_right(node);

    if (op_type == TOKEN_COALESCE)
        return llvm_emit_option_coalesce(node, ctx);

    if (op_type == TOKEN_AND || op_type == TOKEN_OR) {
        LLVMValueRef left = llvm_emit_expression(left_expr, ctx);
        if (left == NULL)
            return llvm_scalar_expr_error(ctx, node,
                "LLVM logical operator could not lower left operand");
        LLVMBasicBlockRef entry_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMValueRef parent_fn = LLVMGetBasicBlockParent(entry_bb);
        LLVMBasicBlockRef rhs_bb = LLVMAppendBasicBlockInContext(
            ctx->context, parent_fn, llvm_tmp_name(ctx));
        LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
            ctx->context, parent_fn, llvm_tmp_name(ctx));
        if (op_type == TOKEN_AND) {
            LLVMBuildCondBr(ctx->builder, left, rhs_bb, merge_bb);
        } else {
            LLVMBuildCondBr(ctx->builder, left, merge_bb, rhs_bb);
        }
        LLVMPositionBuilderAtEnd(ctx->builder, rhs_bb);
        LLVMValueRef right_val = llvm_emit_expression(right_expr, ctx);
        if (right_val == NULL)
            return llvm_scalar_expr_error(ctx, node,
                "LLVM logical operator could not lower right operand");
        LLVMBasicBlockRef rhs_end_bb = LLVMGetInsertBlock(ctx->builder);
        LLVMBuildBr(ctx->builder, merge_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
        LLVMValueRef phi = LLVMBuildPhi(ctx->builder, ctx->type_i1,
            llvm_tmp_name(ctx));
        LLVMValueRef short_val = LLVMConstInt(ctx->type_i1,
            op_type == TOKEN_AND ? 0 : 1, 0);
        LLVMValueRef incoming_vals[2] = { short_val, right_val };
        LLVMBasicBlockRef incoming_blocks[2] = { entry_bb, rhs_end_bb };
        LLVMAddIncoming(phi, incoming_vals, incoming_blocks, 2);
        return phi;
    }

    LLVMValueRef left  = llvm_emit_expression(left_expr, ctx);
    LLVMValueRef right = llvm_emit_expression(right_expr, ctx);
    if (left == NULL || right == NULL)
        return llvm_scalar_expr_error(ctx, node,
            "LLVM binary expression could not lower operand expression");

    LLVMTypeRef left_type  = LLVMTypeOf(left);
    LLVMTypeRef right_type = LLVMTypeOf(right);

    {
        const char *suffix = llvm_operator_overload_suffix(
            op_type);
        const char *type_name = llvm_expr_custom_type_name(
            left_expr, ctx);

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

    if (op_type == TOKEN_PLUS
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

    if ((op_type == TOKEN_EQUAL
         || op_type == TOKEN_NOT_EQUAL)
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
            if (op_type == TOKEN_EQUAL)
                return eq;
            return LLVMBuildNot(ctx->builder, eq, llvm_tmp_name(ctx));
        }
        return llvm_scalar_expr_error(ctx, node,
            "LLVM string comparison could not lower/coerce operands");
    }

    bool is_float = (left_type == ctx->type_f64 || left_type == ctx->type_f32
                  || right_type == ctx->type_f64 || right_type == ctx->type_f32);

    if (is_float) {
        LLVMTypeRef target_type = llvm_binary_float_target_type(
            ctx, left_type, right_type, left_expr, right_expr);
        left = llvm_coerce_numeric_to_fp(ctx, left, target_type);
        right = llvm_coerce_numeric_to_fp(ctx, right, target_type);
        left_type = LLVMTypeOf(left);
        right_type = LLVMTypeOf(right);
        if (left_type != target_type || right_type != target_type)
            return llvm_scalar_expr_error(ctx, node,
                "LLVM floating binary expression could not align operand types");
    }

    if (!is_float && (op_type == TOKEN_SLASH
        || op_type == TOKEN_PERCENT)) {
        bool use_i64 = left_type == ctx->type_i64 || right_type == ctx->type_i64;
        bool rhs_is_nonzero_literal = !use_i64
            && pgy_codegen_ast_number_is_nonzero_i32_literal(
                right_expr);
        if (!rhs_is_nonzero_literal) {
            const char *helper = op_type == TOKEN_SLASH
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
    }

    const char *tmp = llvm_tmp_name(ctx);

    switch (op_type) {
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

#endif
