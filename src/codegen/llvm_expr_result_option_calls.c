#include "llvm_internal.h"

static LLVMValueRef
llvm_result_option_error(LLVMGenCtx *ctx, ASTNode *node,
                         const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s", message != NULL ? message
                : "LLVM Result/Option lowering requires concrete metadata");
    }
    return NULL;
}

static LLVMValueRef
llvm_emit_checked_result_option_unwrap(LLVMGenCtx *ctx, ASTNode *node,
                                       LLVMValueRef aggregate,
                                       unsigned value_index,
                                       const char *reason)
{
    LLVMValueRef tag;
    LLVMValueRef ok;
    LLVMValueRef current_fn;
    LLVMFuncEntry *panic_fn;
    LLVMBasicBlockRef ok_bb;
    LLVMBasicBlockRef fail_bb;

    if (ctx == NULL || aggregate == NULL)
        return NULL;

    tag = LLVMBuildExtractValue(ctx->builder, aggregate, 0, llvm_tmp_name(ctx));
    ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
        LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    current_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    if (current_fn == NULL)
        return llvm_result_option_error(ctx, node,
            "LLVM checked unwrap requires an active function insertion block");
    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM checked unwrap requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
        return NULL;
    }

    ok_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "unwrap.ok");
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "unwrap.panic");
    LLVMBuildCondBr(ctx->builder, ok, ok_bb, fail_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
            reason != NULL ? reason : "runtime invariant failed",
            llvm_tmp_name(ctx));
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            &reason_arg, 1, "");
    }
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
    return LLVMBuildExtractValue(ctx->builder, aggregate, value_index,
        llvm_tmp_name(ctx));
}

static bool
llvm_result_option_context_struct(LLVMGenCtx *ctx, unsigned field_count,
                                  LLVMTypeRef *fields_out)
{
    if (ctx == NULL || ctx->current_ret_type == NULL)
        return false;
    if (LLVMGetTypeKind(ctx->current_ret_type) != LLVMStructTypeKind)
        return false;
    if (LLVMCountStructElementTypes(ctx->current_ret_type) != field_count)
        return false;
    if (fields_out != NULL)
        LLVMGetStructElementTypes(ctx->current_ret_type, fields_out);
    return true;
}

static bool
llvm_result_option_value_struct(LLVMValueRef aggregate, unsigned field_count,
                                LLVMTypeRef *fields_out)
{
    LLVMTypeRef aggregate_ty;

    if (aggregate == NULL)
        return false;
    aggregate_ty = LLVMTypeOf(aggregate);
    if (aggregate_ty == NULL
        || LLVMGetTypeKind(aggregate_ty) != LLVMStructTypeKind)
        return false;
    if (LLVMCountStructElementTypes(aggregate_ty) != field_count)
        return false;
    if (fields_out != NULL)
        LLVMGetStructElementTypes(aggregate_ty, fields_out);
    return true;
}

static LLVMValueRef
llvm_coerce_result_option_payload(LLVMGenCtx *ctx,
                                  LLVMValueRef val,
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
    if ((target_ty == ctx->type_i32 || target_ty == ctx->type_i64)
        && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64))
        return LLVMBuildFPToSI(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(target_ty) == LLVMPointerTypeKind
        && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind)
        return LLVMBuildBitCast(ctx->builder, val, target_ty, llvm_tmp_name(ctx));
    return val;
}

LLVMValueRef
llvm_emit_result_option_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{    /* Built-in: Ok(value) ??Result<T, E>; prefer active expected layout. */
    size_t argc = ast_call_arg_count(node);
    if (strcmp(callee_name, "Ok") == 0 && argc == 1) {
        LLVMValueRef val;
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        if (llvm_result_option_context_struct(ctx, 3, fields)) {
            result_ty = ctx->current_ret_type;
        } else {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Ok(value) requires contextual Result<T, E>; anonymous Result layout fallback is disabled");
            return NULL;
        }
        val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (val == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM Ok(value) could not lower payload expression");
        LLVMValueRef r = LLVMGetUndef(result_ty);
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        if (LLVMTypeOf(val) != fields[1]) {
            LLVMTypeRef val_ty = LLVMTypeOf(val);
            if ((fields[1] == ctx->type_i32 || fields[1] == ctx->type_i64)
                && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(fields[1]) > LLVMGetIntTypeWidth(val_ty))
                    ? LLVMBuildSExt(ctx->builder, val, fields[1], llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if ((fields[1] == ctx->type_f32 || fields[1] == ctx->type_f64)
                       && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if ((fields[1] == ctx->type_i32 || fields[1] == ctx->type_i64)
                       && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            } else if (LLVMGetTypeKind(fields[1]) == LLVMPointerTypeKind
                       && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r, val, 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(fields[2]), 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) ??Result<T, E>; prefer active expected layout. */
    if (strcmp(callee_name, "Err") == 0 && argc == 1) {
        LLVMValueRef val;
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        if (llvm_result_option_context_struct(ctx, 3, fields)) {
            result_ty = ctx->current_ret_type;
        } else {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Err(value) requires contextual Result<T, E>; anonymous Result layout fallback is disabled");
            return NULL;
        }
        val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (val == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM Err(value) could not lower payload expression");
        LLVMValueRef r = LLVMGetUndef(result_ty);
        if (LLVMTypeOf(val) != fields[2]) {
            LLVMTypeRef val_ty = LLVMTypeOf(val);
            if ((fields[2] == ctx->type_i32 || fields[2] == ctx->type_i64)
                && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = (LLVMGetIntTypeWidth(fields[2]) > LLVMGetIntTypeWidth(val_ty))
                    ? LLVMBuildSExt(ctx->builder, val, fields[2], llvm_tmp_name(ctx))
                    : LLVMBuildTrunc(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            } else if ((fields[2] == ctx->type_f32 || fields[2] == ctx->type_f64)
                       && (val_ty == ctx->type_i32 || val_ty == ctx->type_i64)) {
                val = LLVMBuildSIToFP(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            } else if ((fields[2] == ctx->type_i32 || fields[2] == ctx->type_i64)
                       && (val_ty == ctx->type_f32 || val_ty == ctx->type_f64)) {
                val = LLVMBuildFPToSI(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            } else if (LLVMGetTypeKind(fields[2]) == LLVMPointerTypeKind
                       && LLVMGetTypeKind(val_ty) == LLVMPointerTypeKind) {
                val = LLVMBuildBitCast(ctx->builder, val, fields[2], llvm_tmp_name(ctx));
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(fields[1]), 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r, val, 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: IsOk(result) ??extract ok field */
    if (strcmp(callee_name, "IsOk") == 0 && argc == 1) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[3];
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM IsOk(result) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) ??!ok */
    if (strcmp(callee_name, "IsErr") == 0 && argc == 1) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[3];
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM IsErr(result) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) ??extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && argc == 1) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[3];
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Unwrap(result) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        return llvm_emit_checked_result_option_unwrap(ctx, node, r, 1,
            "Result unwrap on Err value");
    }

    /* Built-in: UnwrapOr(result, default) ??ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && argc == 2) {
        LLVMValueRef r = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMValueRef def = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        LLVMTypeRef fields[3];
        if (r == NULL || def == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM UnwrapOr(result, default) could not lower operand expression");
        if (!llvm_result_option_value_struct(r, 3, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM UnwrapOr(result, default) requires concrete Result<T, E> aggregate operand");
            return NULL;
        }
        def = llvm_coerce_result_option_payload(ctx, def, fields[1]);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) ??{ .tag=PgyOptionSome, .value=value } */
    if (strcmp(callee_name, "Some") == 0 && argc == 1) {
        LLVMValueRef val;
        LLVMTypeRef option_ty = NULL;
        LLVMTypeRef fields[2];
        if (!llvm_result_option_context_struct(ctx, 2, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Some(value) requires contextual Option<T>; anonymous Option layout fallback is disabled");
            return NULL;
        }
        val = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        if (val == NULL)
            return llvm_result_option_error(ctx, node,
                "LLVM Some(value) could not lower payload expression");
        option_ty = ctx->current_ret_type;
        val = llvm_coerce_result_option_payload(ctx, val, fields[1]);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o, val, 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: None() ??{ .tag=PgyOptionNone, .value=zero } */
    if (strcmp(callee_name, "None") == 0 && argc == 0) {
        LLVMTypeRef fields[2];
        if (!llvm_result_option_context_struct(ctx, 2, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM None() requires contextual Option<T>; Option<Int> fallback is disabled");
            return NULL;
        }
        LLVMTypeRef value_ty = fields[1];
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, value_ty }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 1, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstNull(value_ty), 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: IsSome(option) / IsNone(option) */
    if ((strcmp(callee_name, "IsSome") == 0 || strcmp(callee_name, "IsNone") == 0)
        && argc == 1) {
        LLVMValueRef o = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[2];
        if (!llvm_result_option_value_struct(o, 2, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s(option) requires concrete Option<T> aggregate operand",
                callee_name);
            return NULL;
        }
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, o, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                strcmp(callee_name, "IsSome") == 0 ? 0 : 1, 0),
            llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOption(option) ??extract value field */
    if (strcmp(callee_name, "UnwrapOption") == 0 && argc == 1) {
        LLVMValueRef o = llvm_emit_expression(ast_call_argument(node, 0), ctx);
        LLVMTypeRef fields[2];
        if (!llvm_result_option_value_struct(o, 2, fields)
            || fields[0] != ctx->type_i32) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM UnwrapOption(option) requires concrete Option<T> aggregate operand");
            return NULL;
        }
        return llvm_emit_checked_result_option_unwrap(ctx, node, o, 1,
            "Option unwrap on None value");
    }

    return NULL;
}
