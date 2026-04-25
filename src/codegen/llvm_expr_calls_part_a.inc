static LLVMValueRef
llvm_emit_call(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node->data.call.callee == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    /* Method call: obj.method(args) */
    if (node->data.call.callee->type == AST_MEMBER_ACCESS)
        return llvm_emit_member_call(node, ctx);

    /* Get callee name */
    const char *callee_name = NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        callee_name = node->data.call.callee->data.identifier.name;

    if (callee_name == NULL)
        return LLVMConstInt(ctx->type_i32, 0, 0);

    if (strcmp(callee_name, "Clone") == 0 && node->data.call.arg_count == 1) {
        return llvm_emit_expression(node->data.call.arguments[0], ctx);
    }

    {
        LLVMValueRef constructor_value =
            llvm_emit_constructor_call(node, ctx, callee_name);
        if (constructor_value != NULL)
            return constructor_value;
    }

    if ((strcmp(callee_name, "ToTObject") == 0 || strcmp(callee_name, "ToObject") == 0)
        && node->data.call.arg_count == 2) {
        return llvm_emit_subject_projection(node, ctx);
    }

    {
        LLVMValueRef domain_query = NULL;
        if (llvm_emit_domain_query_call(node, ctx, callee_name, &domain_query))
            return domain_query;
    }

    {
        LLVMValueRef log_call = NULL;
        if (llvm_emit_log_family_call(node, ctx, callee_name, &log_call))
            return log_call;
    }

    {
        LLVMValueRef rc_call = NULL;
        if (llvm_emit_rc_builtin_call(node, ctx, callee_name, &rc_call))
            return rc_call;
    }

    {
        LLVMValueRef slot_builtin = NULL;
        if (llvm_emit_slot_builtin_call(node, ctx, callee_name, &slot_builtin))
            return slot_builtin;
    }
