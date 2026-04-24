static bool
llvm_emit_event_invocation_call(ASTNode *node, LLVMGenCtx *ctx,
                                const char *callee_name, LLVMValueRef *out)
{
    LLVMEventTypeEntry *evt = llvm_lookup_event(ctx, callee_name);
    char fname[256];
    LLVMFuncEntry *fn;
    LLVMValueRef ev_ptr;
    size_t ac;
    LLVMValueRef *args;

    if (out == NULL || evt == NULL)
        return false;

    snprintf(fname, sizeof(fname), "%s_INVOKE", callee_name);
    fn = llvm_lookup_function(ctx, fname);
    ev_ptr = LLVMGetNamedGlobal(ctx->module, callee_name);
    if (ev_ptr == NULL) {
        LLVMVarEntry *ev = llvm_scope_lookup(ctx, callee_name);
        if (ev != NULL)
            ev_ptr = ev->alloca;
    }
    if (fn == NULL || ev_ptr == NULL)
        return false;

    ac = node->data.call.arg_count;
    args = pgy_arena_calloc(&ctx->scratch, (ac + 1) * sizeof(LLVMValueRef));
    args[0] = ev_ptr;
    for (size_t j = 0; j < ac; j++) {
        args[j + 1] = llvm_emit_expression(node->data.call.arguments[j], ctx);
    }
    LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args,
        (unsigned)(ac + 1), "");
    *out = LLVMConstInt(ctx->type_i32, 0, 0);
    return true;
}
