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
    if (type_node->data.event_handler_type.return_type != NULL)
        ret_type = ast_type_to_llvm(ctx, type_node->data.event_handler_type.return_type);

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(LLVMTypeRef));
        if (param_types == NULL)
            return LLVMFunctionType(ret_type, NULL, 0, 0);
        for (size_t i = 0; i < param_count; i++)
            param_types[i] = ast_type_to_llvm(ctx,
                type_node->data.event_handler_type.param_types[i]);
    }

    fn_type = LLVMFunctionType(ret_type, param_types, (unsigned)param_count, 0);
    free(param_types);
    return fn_type;
}

static LLVMValueRef
llvm_coerce_value_to_string(LLVMValueRef value, LLVMGenCtx *ctx)
{
    LLVMTypeRef value_type;
    LLVMFuncEntry *fn;
    LLVMValueRef args[1];

    if (value == NULL || ctx == NULL)
        return NULL;

    value_type = LLVMTypeOf(value);
    if (value_type == ctx->type_i8ptr)
        return value;

    if (LLVMGetTypeKind(value_type) == LLVMIntegerTypeKind) {
        unsigned width = LLVMGetIntTypeWidth(value_type);
        if (width == 1) {
            LLVMValueRef true_str = LLVMBuildGlobalStringPtr(
                ctx->builder, "true", llvm_tmp_name(ctx));
            LLVMValueRef false_str = LLVMBuildGlobalStringPtr(
                ctx->builder, "false", llvm_tmp_name(ctx));
            return LLVMBuildSelect(ctx->builder, value, true_str, false_str,
                llvm_tmp_name(ctx));
        } else if (width < 32) {
            value = LLVMBuildSExt(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        } else if (width > 32) {
            value = LLVMBuildTrunc(ctx->builder, value, ctx->type_i32,
                llvm_tmp_name(ctx));
        }

        fn = llvm_lookup_function(ctx, "pgy_int_to_string");
        if (fn == NULL)
            return NULL;
        args[0] = value;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
            llvm_tmp_name(ctx));
    }

    if (value_type == ctx->type_f32 || value_type == ctx->type_f64) {
        if (value_type == ctx->type_f64) {
            value = LLVMBuildFPTrunc(ctx->builder, value, ctx->type_f32,
                llvm_tmp_name(ctx));
        }
        fn = llvm_lookup_function(ctx, "pgy_float_to_string");
        if (fn == NULL)
            return NULL;
        args[0] = value;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1,
            llvm_tmp_name(ctx));
    }

    return NULL;
}

static LLVMValueRef
llvm_emit_binary(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef left  = llvm_emit_expression(node->data.binary.left, ctx);
    LLVMValueRef right = llvm_emit_expression(node->data.binary.right, ctx);
    if (left == NULL || right == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

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
            size_t fn_len = strlen("operator_") + strlen(suffix) + 1 + strlen(type_name) + 1;
            char *fn_name = malloc(fn_len);
            if (fn_name == NULL)
                return LLVMConstInt(ctx->type_i32, 0, 0);
            snprintf(fn_name, fn_len, "operator_%s_%s", suffix, type_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
            free(fn_name);
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
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
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
    }

    if ((node->data.binary.op.type == TOKEN_EQUAL
         || node->data.binary.op.type == TOKEN_NOT_EQUAL)
        && (left_type == ctx->type_i8ptr || right_type == ctx->type_i8ptr)) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_string_equals");
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
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
}

static LLVMValueRef
llvm_emit_unary(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.unary.op.type == TOKEN_QUESTION) {
        LLVMValueRef result = llvm_emit_expression(node->data.unary.operand, ctx);
        LLVMTypeRef result_ty = LLVMTypeOf(result);
        unsigned field_count = LLVMCountStructElementTypes(result_ty);

        if (result == NULL || LLVMGetTypeKind(result_ty) != LLVMStructTypeKind
            || field_count < 2 || ctx->current_function == NULL) {
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }

        LLVMTypeRef fields[8];
        LLVMGetStructElementTypes(result_ty, fields);

        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, result, 0, llvm_tmp_name(ctx));
        LLVMValueRef is_ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));

        LLVMValueRef ok_alloca = llvm_create_entry_alloca(ctx, fields[1], llvm_tmp_name(ctx));
        LLVMBasicBlockRef ok_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.ok");
        LLVMBasicBlockRef err_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.err");
        LLVMBasicBlockRef cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
            ctx->current_function, "try.cont");

        LLVMBuildCondBr(ctx->builder, is_ok, ok_bb, err_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
        {
            LLVMValueRef ok_value = LLVMBuildExtractValue(ctx->builder, result, 1,
                llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, ok_value, ok_alloca);
            LLVMBuildBr(ctx->builder, cont_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, err_bb);
        if (ctx->current_ret_type == result_ty) {
            LLVMBuildRet(ctx->builder, result);
        } else {
            LLVMBuildUnreachable(ctx->builder);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
        return LLVMBuildLoad2(ctx->builder, fields[1], ok_alloca, llvm_tmp_name(ctx));
    }

    LLVMValueRef operand = llvm_emit_expression(node->data.unary.operand, ctx);
    if (operand == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

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
