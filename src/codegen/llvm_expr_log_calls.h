static const char *
llvm_log_function_for_type(LLVMGenCtx *ctx, LLVMTypeRef type,
                           bool multiline_log)
{
    if (multiline_log)
        return "pgy_log_banner";
    if (type == ctx->type_i64)
        return "pgy_log_long";
    if (type == ctx->type_f32)
        return "pgy_log_float";
    if (type == ctx->type_f64)
        return "pgy_log_double";
    if (type == ctx->type_i1)
        return "pgy_log_bool";
    if (type == ctx->type_i8ptr)
        return "pgy_log_string";
    return "pgy_log_int";
}

static LLVMFuncEntry *
llvm_required_log_function(LLVMGenCtx *ctx, ASTNode *node,
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
            "LLVM log operation requires registered runtime function '%s'",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMValueRef
llvm_emit_log_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *arg_node;
    bool multiline_log = false;
    LLVMTypeRef arg_type = NULL;
    const char *log_fn_name;
    LLVMValueRef arg = NULL;
    LLVMFuncEntry *log_fn;
    LLVMValueRef args[1];

    if (node->data.call.arg_count < 1)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    arg_node = node->data.call.arguments[0];
    if (arg_node != NULL && arg_node->type == AST_STRING
        && arg_node->data.string.value != NULL) {
        const char *raw = arg_node->data.string.value;
        multiline_log = (strchr(raw, '\n') != NULL) || (strchr(raw, '\r') != NULL);
        if (multiline_log) {
            char *normalized = llvm_normalize_banner_string_literal_scratch(
                raw, &ctx->scratch);
            arg = LLVMBuildGlobalStringPtr(ctx->builder,
                normalized != NULL ? normalized : raw,
                llvm_tmp_name(ctx));
        } else {
            arg = LLVMBuildGlobalStringPtr(ctx->builder, raw,
                                          llvm_tmp_name(ctx));
        }
        arg_type = ctx->type_i8ptr;
    } else {
        arg = llvm_emit_expression(arg_node, ctx);
        if (arg != NULL)
            arg_type = LLVMTypeOf(arg);
    }

    if (arg == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    log_fn_name = llvm_log_function_for_type(ctx, arg_type, multiline_log);
    log_fn = llvm_required_log_function(ctx, node, log_fn_name);
    if (log_fn == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    args[0] = arg;
    LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn, args, 1, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_log_raw_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *arg_node;
    LLVMValueRef arg = NULL;
    LLVMTypeRef arg_type;
    const char *log_fn_name;
    LLVMFuncEntry *log_fn;
    LLVMValueRef args[1];

    if (node->data.call.arg_count < 1)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    arg_node = node->data.call.arguments[0];
    if (arg_node != NULL && arg_node->type == AST_STRING
        && arg_node->data.string.value != NULL) {
        arg = LLVMBuildGlobalStringPtr(ctx->builder,
            arg_node->data.string.value, llvm_tmp_name(ctx));
    } else {
        arg = llvm_emit_expression(arg_node, ctx);
        if (arg == NULL)
            return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    arg_type = LLVMTypeOf(arg);
    log_fn_name = llvm_log_function_for_type(ctx, arg_type, false);
    log_fn = llvm_required_log_function(ctx, node, log_fn_name);
    if (log_fn == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    args[0] = arg;
    LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn, args, 1, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static LLVMValueRef
llvm_emit_log_banner_call(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *arg;
    LLVMValueRef log_arg = NULL;
    LLVMFuncEntry *log_fn;
    LLVMValueRef args[1];

    if (node->data.call.arg_count < 1)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    arg = node->data.call.arguments[0];
    if (arg == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (arg->type == AST_STRING) {
        char *normalized = llvm_normalize_banner_string_literal_scratch(
            arg->data.string.value, &ctx->scratch);
        if (normalized != NULL) {
            log_arg = LLVMBuildGlobalStringPtr(ctx->builder, normalized,
                                               llvm_tmp_name(ctx));
        }
    } else {
        log_arg = llvm_emit_expression(arg, ctx);
        if (log_arg != NULL)
            log_arg = llvm_coerce_value_to_string(log_arg, ctx);
    }

    if (log_arg == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    log_fn = llvm_required_log_function(ctx, node, "pgy_log_banner");
    if (log_fn == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    args[0] = log_arg;
    LLVMBuildCall2(ctx->builder, log_fn->fn_type, log_fn->fn, args, 1, "");
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

static bool
llvm_emit_log_family_call(ASTNode *node, LLVMGenCtx *ctx,
                          const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    if (strcmp(callee_name, "Log") == 0) {
        *out = llvm_emit_log_call(node, ctx);
        return true;
    }
    if (strcmp(callee_name, "LogRaw") == 0) {
        *out = llvm_emit_log_raw_call(node, ctx);
        return true;
    }
    if (strcmp(callee_name, "LogBanner") == 0
        || strcmp(callee_name, "LogBlock") == 0) {
        *out = llvm_emit_log_banner_call(node, ctx);
        return true;
    }
    return false;
}
