static LLVMValueRef
llvm_emit_checked_result_option_unwrap(LLVMGenCtx *ctx, LLVMValueRef aggregate,
                                       unsigned value_index,
                                       const char *reason)
{
    LLVMValueRef tag;
    LLVMValueRef ok;
    LLVMValueRef current_fn;
    LLVMBasicBlockRef ok_bb;
    LLVMBasicBlockRef fail_bb;

    if (ctx == NULL || aggregate == NULL)
        return NULL;

    tag = LLVMBuildExtractValue(ctx->builder, aggregate, 0, llvm_tmp_name(ctx));
    ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
        LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    current_fn = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    if (current_fn == NULL)
        return LLVMBuildExtractValue(ctx->builder, aggregate, value_index,
            llvm_tmp_name(ctx));

    ok_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "unwrap.ok");
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, current_fn,
        "unwrap.panic");
    LLVMBuildCondBr(ctx->builder, ok, ok_bb, fail_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        LLVMFuncEntry *panic_fn = llvm_lookup_function(ctx,
            "pgy_runtime_panic_internal_invariant_export");
        if (panic_fn != NULL) {
            LLVMValueRef reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
                reason != NULL ? reason : "runtime invariant failed",
                llvm_tmp_name(ctx));
            LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
                &reason_arg, 1, "");
        }
    }
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
    return LLVMBuildExtractValue(ctx->builder, aggregate, value_index,
        llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_emit_result_option_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{    /* Built-in: Ok(value) ??Result<T, E>; prefer active expected layout. */
    if (strcmp(callee_name, "Ok") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 3) {
            result_ty = ctx->current_ret_type;
            LLVMGetStructElementTypes(result_ty, fields);
        } else {
            LLVMTypeRef fallback_fields[] = { ctx->type_i32, LLVMTypeOf(val), ctx->type_i8ptr };
            result_ty = LLVMStructTypeInContext(ctx->context, fallback_fields, 3, 0);
            fields[0] = fallback_fields[0];
            fields[1] = fallback_fields[1];
            fields[2] = fallback_fields[2];
        }
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
    if (strcmp(callee_name, "Err") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef result_ty = NULL;
        LLVMTypeRef fields[3];
        if (ctx->current_ret_type != NULL
            && LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 3) {
            result_ty = ctx->current_ret_type;
            LLVMGetStructElementTypes(result_ty, fields);
        } else {
            LLVMTypeRef fallback_fields[] = { ctx->type_i32, ctx->type_i32, ctx->type_i8ptr };
            result_ty = LLVMStructTypeInContext(ctx->context, fallback_fields, 3, 0);
            fields[0] = fallback_fields[0];
            fields[1] = fallback_fields[1];
            fields[2] = fallback_fields[2];
        }
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
    if (strcmp(callee_name, "IsOk") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) ??!ok */
    if (strcmp(callee_name, "IsErr") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) ??extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return llvm_emit_checked_result_option_unwrap(ctx, r, 1,
            "Result unwrap on Err value");
    }

    /* Built-in: UnwrapOr(result, default) ??ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef def = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) ??{ .tag=PgyOptionSome, .value=value } */
    if (strcmp(callee_name, "Some") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef option_ty = LLVMStructTypeInContext(ctx->context,
            (LLVMTypeRef[]){ ctx->type_i32, LLVMTypeOf(val) }, 2, 0);
        LLVMValueRef o = LLVMGetUndef(option_ty);
        o = LLVMBuildInsertValue(ctx->builder, o,
            LLVMConstInt(ctx->type_i32, 0, 0), 0, llvm_tmp_name(ctx));
        o = LLVMBuildInsertValue(ctx->builder, o, val, 1, llvm_tmp_name(ctx));
        return o;
    }

    /* Built-in: None() ??{ .tag=PgyOptionNone, .value=zero } */
    if (strcmp(callee_name, "None") == 0 && node->data.call.arg_count == 0) {
        LLVMTypeRef value_ty = ctx->type_i32;
        if (LLVMGetTypeKind(ctx->current_ret_type) == LLVMStructTypeKind
            && LLVMCountStructElementTypes(ctx->current_ret_type) == 2) {
            LLVMTypeRef fields[2];
            LLVMGetStructElementTypes(ctx->current_ret_type, fields);
            if (fields[0] == ctx->type_i32)
                value_ty = fields[1];
        }
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
        && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, o, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                strcmp(callee_name, "IsSome") == 0 ? 0 : 1, 0),
            llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOption(option) ??extract value field */
    if (strcmp(callee_name, "UnwrapOption") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return llvm_emit_checked_result_option_unwrap(ctx, o, 1,
            "Option unwrap on None value");
    }

    return NULL;
}
