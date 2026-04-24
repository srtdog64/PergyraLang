static bool
llvm_emit_scalar_math_call(ASTNode *node, LLVMGenCtx *ctx,
                           const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;

    /* Built-in: Abs(x) -> select(x < 0, -x, x) */
    if (strcmp(callee_name, "Abs") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef x = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef zero = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef neg = LLVMBuildNeg(ctx->builder, x, llvm_tmp_name(ctx));
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, x, zero,
                                          llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, cmp, neg, x, llvm_tmp_name(ctx));
        return true;
    }

    /* Built-in: Min(a, b) -> select(a < b, a, b) */
    if (strcmp(callee_name, "Min") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSLT, a, b,
                                          llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
        return true;
    }

    /* Built-in: Max(a, b) -> select(a > b, a, b) */
    if (strcmp(callee_name, "Max") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef a = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef b = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntSGT, a, b,
                                          llvm_tmp_name(ctx));
        *out = LLVMBuildSelect(ctx->builder, cmp, a, b, llvm_tmp_name(ctx));
        return true;
    }

    return false;
}
