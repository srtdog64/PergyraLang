static LLVMValueRef
llvm_emit_task_channel_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{    if (strcmp(callee_name, "Cancel") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_cancel_export");
        LLVMValueRef task = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (fn != NULL) {
            LLVMValueRef args[] = { task };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, llvm_tmp_name(ctx));
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "IsCancelled") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_task_is_cancelled_export");
        if (fn != NULL)
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                NULL, 0, llvm_tmp_name(ctx));
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySend") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "TrySendStatus") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char closed_name[128];
                char send_name[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s",
                    inner != NULL ? inner : "Int");
                snprintf(send_name, sizeof(send_name), "pgy_channel_try_send_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *closed_fn = llvm_lookup_function(ctx, closed_name);
                LLVMFuncEntry *send_fn = llvm_lookup_function(ctx, send_name);
                if (closed_fn != NULL && send_fn != NULL) {
                    LLVMValueRef closed_args[] = { ch_var->alloca };
                    LLVMValueRef send_args[] = { ch_var->alloca, val };
                    LLVMValueRef closed = LLVMBuildCall2(ctx->builder, closed_fn->fn_type,
                        closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
                        send_fn->fn, send_args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
                }
            }
        }
        return llvm_build_option_value(ctx, ctx->type_i1,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstInt(ctx->type_i1, 0, 0));
    }

    if (strcmp(callee_name, "SendTimeout") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, val, timeout };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                }
            }
        }
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    if (strcmp(callee_name, "SendTimeoutStatus") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char closed_name[128];
                char send_name[128];
                LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s",
                    inner != NULL ? inner : "Int");
                snprintf(send_name, sizeof(send_name), "pgy_channel_send_timeout_%s",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *closed_fn = llvm_lookup_function(ctx, closed_name);
                LLVMFuncEntry *send_fn = llvm_lookup_function(ctx, send_name);
                if (closed_fn != NULL && send_fn != NULL) {
                    LLVMValueRef closed_args[] = { ch_var->alloca };
                    LLVMValueRef send_args[] = { ch_var->alloca, val, timeout };
                    LLVMValueRef closed_before = LLVMBuildCall2(ctx->builder,
                        closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
                        send_fn->fn, send_args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef closed_after = LLVMBuildCall2(ctx->builder,
                        closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
                    LLVMValueRef failed = LLVMBuildNot(ctx->builder, ok, llvm_tmp_name(ctx));
                    LLVMValueRef failed_and_closed = LLVMBuildAnd(ctx->builder, failed,
                        closed_after, llvm_tmp_name(ctx));
                    LLVMValueRef closed = LLVMBuildOr(ctx->builder, closed_before,
                        failed_and_closed, llvm_tmp_name(ctx));
                    LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
                }
            }
        }
        return llvm_build_option_value(ctx, ctx->type_i1,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstInt(ctx->type_i1, 0, 0));
    }

    if ((strcmp(callee_name, "TryRecv") == 0 && node->data.call.arg_count == 1)
        || (strcmp(callee_name, "RecvTimeout") == 0 && node->data.call.arg_count == 2)) {
        ASTNode *channel = node->data.call.arguments[0];
        const char *inner = "Int";
        LLVMVarEntry *ch_var = NULL;
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            ch_var = llvm_scope_lookup(ctx, name);
            {
                const char *lookup_inner = llvm_lookup_channel_inner(ctx, name);
                if (lookup_inner != NULL)
                    inner = lookup_inner;
            }
        }

        LLVMTypeRef value_ty = pergyra_type_to_llvm(ctx, inner);
        if (ch_var != NULL) {
            LLVMValueRef tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);

            char fname[128];
            if (strcmp(callee_name, "TryRecv") == 0) {
                snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 2, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            } else {
                LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[1], ctx);
                if (LLVMTypeOf(timeout) != ctx->type_i64) {
                    timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                        ctx->type_i64, llvm_tmp_name(ctx));
                }
                snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", inner);
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca, tmp, timeout };
                    LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 3, llvm_tmp_name(ctx));
                    LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                        llvm_tmp_name(ctx));
                    return llvm_build_option_value(ctx, value_ty, ok, value);
                }
            }
        }

        return llvm_build_option_value(ctx, value_ty,
            LLVMConstInt(ctx->type_i1, 0, 0), LLVMConstNull(value_ty));
    }

    if ((strcmp(callee_name, "ChannelReady") == 0
         || strcmp(callee_name, "ChannelLength") == 0
         || strcmp(callee_name, "ChannelCapacity") == 0
         || strcmp(callee_name, "ChannelSpace") == 0
         || strcmp(callee_name, "ChannelFull") == 0
         || strcmp(callee_name, "ChannelClosed") == 0)
        && node->data.call.arg_count == 1) {
        ASTNode *channel = node->data.call.arguments[0];
        if (channel != NULL && channel->type == AST_IDENTIFIER) {
            const char *name = channel->data.identifier.name;
            LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (ch_var != NULL) {
                char fname[128];
                snprintf(fname, sizeof(fname), "pgy_channel_%s_%s",
                    strcmp(callee_name, "ChannelReady") == 0 ? "ready" :
                    strcmp(callee_name, "ChannelLength") == 0 ? "length" :
                    strcmp(callee_name, "ChannelCapacity") == 0 ? "capacity" :
                    strcmp(callee_name, "ChannelSpace") == 0 ? "space" :
                    strcmp(callee_name, "ChannelFull") == 0 ? "full" :
                    "closed",
                    inner != NULL ? inner : "Int");
                LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
                if (fn != NULL) {
                    LLVMValueRef args[] = { ch_var->alloca };
                    return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                        args, 1, llvm_tmp_name(ctx));
                }
            }
        }

        if (strcmp(callee_name, "ChannelLength") == 0
            || strcmp(callee_name, "ChannelCapacity") == 0
            || strcmp(callee_name, "ChannelSpace") == 0)
            return LLVMConstInt(ctx->type_i32, 0, 0);
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }

    return NULL;
}