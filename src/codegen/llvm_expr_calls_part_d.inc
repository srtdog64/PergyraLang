                val = LLVMBuildBitCast(ctx->builder, val, fields[1], llvm_tmp_name(ctx));
            }
        }
        r = LLVMBuildInsertValue(ctx->builder, r, val, 1, llvm_tmp_name(ctx));
        r = LLVMBuildInsertValue(ctx->builder, r,
            LLVMConstNull(fields[2]), 2, llvm_tmp_name(ctx));
        return r;
    }

    /* Built-in: Err(value) → Result<T, E>; prefer active expected layout. */
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

    /* Built-in: IsOk(result) → extract ok field */
    if (strcmp(callee_name, "IsOk") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: IsErr(result) → !ok */
    if (strcmp(callee_name, "IsErr") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 1, 0), llvm_tmp_name(ctx));
    }

    /* Built-in: Unwrap(result) → extract value field */
    if (strcmp(callee_name, "Unwrap") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
    }

    /* Built-in: UnwrapOr(result, default) → ok ? value : default */
    if (strcmp(callee_name, "UnwrapOr") == 0 && node->data.call.arg_count == 2) {
        LLVMValueRef r = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMValueRef def = llvm_emit_expression(node->data.call.arguments[1], ctx);
        LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, r, 0, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildExtractValue(ctx->builder, r, 1, llvm_tmp_name(ctx));
        return LLVMBuildSelect(ctx->builder, ok, val, def, llvm_tmp_name(ctx));
    }

    /* Built-in: Some(value) → { .tag=PgyOptionSome, .value=value } */
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

    /* Built-in: None() → { .tag=PgyOptionNone, .value=zero } */
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

    /* Built-in: UnwrapOption(option) → extract value field */
    if (strcmp(callee_name, "UnwrapOption") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef o = llvm_emit_expression(node->data.call.arguments[0], ctx);
        return LLVMBuildExtractValue(ctx->builder, o, 1, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "Cancel") == 0 && node->data.call.arg_count == 1) {
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

    if (node->data.call.callee->type == AST_IDENTIFIER) {
        ASTNode *host_decl = llvm_current_host_decl(ctx);
        const char *host_name = llvm_decl_node_name(host_decl);
        ASTNode *host_method = llvm_current_host_method_decl(ctx, callee_name);
        if (host_name != NULL && host_method != NULL) {
            char full_name[256];
            snprintf(full_name, sizeof(full_name), "%s_%s", host_name, callee_name);
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, full_name);
            if (fn != NULL) {
                size_t argc = node->data.call.arg_count;
                LLVMValueRef *args = pgy_arena_calloc(&ctx->scratch,
                    (argc + 1) * sizeof(LLVMValueRef));
                if (args == NULL)
                    return LLVMConstInt(ctx->type_i32, 0, 0);
                args[0] = llvm_current_self_call_arg(ctx);
                for (size_t i = 0; i < argc; i++) {
                    LLVMValueRef arg_val = llvm_emit_expression(node->data.call.arguments[i], ctx);
                    size_t logical_idx = 0;
                    for (size_t pk = 0; pk < host_method->data.func_decl.param_count; pk++) {
                        FuncParam *p = host_method->data.func_decl.params[pk];
                        const char *ptn = NULL;
                        LLVMClassTypeEntry *param_cls = NULL;
                        if (p->type == NULL && strcmp(p->name, "self") == 0)
                            continue;
                        if (logical_idx == i) {
                            if (p->type != NULL && p->type->type == AST_TYPE)
                                ptn = p->type->data.type.name;
                            param_cls = ptn != NULL ? llvm_lookup_class(ctx, ptn) : NULL;
                            if (param_cls != NULL && param_cls->is_pointer_self_host
                                && node->data.call.arguments[i] != NULL
                                && node->data.call.arguments[i]->type == AST_IDENTIFIER) {
                                const char *arg_name =
                                    node->data.call.arguments[i]->data.identifier.name;
                                LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                                if (arg_var != NULL) {
                                    if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                        arg_val = LLVMBuildLoad2(ctx->builder,
                                            arg_var->type, arg_var->alloca, llvm_tmp_name(ctx));
                                    else
                                        arg_val = arg_var->alloca;
                                }
                            }
                            break;
                        }
                        logical_idx++;
                    }
                    args[i + 1] = arg_val;
                }
                {
                    LLVMValueRef result;
                    if (fn->ret_type == ctx->type_void) {
                        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), "");
                        result = LLVMConstInt(ctx->type_i32, 0, 0);
                    } else {
                        result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                            args, (unsigned)(argc + 1), llvm_tmp_name(ctx));
                    }
                    return result;
                }
            }
        }
    }

    size_t argc = node->data.call.arg_count;
    ASTNode *decl = llvm_find_function_decl(ctx, callee_name);
    ASTNode *intent_decl = decl == NULL ? llvm_find_intent_decl(ctx, callee_name) : NULL;
    unsigned emitted_argc = 0;
    LLVMValueRef *args = NULL;

    if (decl != NULL)
        args = llvm_build_boundary_call_args(ctx, decl, node->data.call.arguments,
            argc, &emitted_argc);
    if (args == NULL && intent_decl != NULL) {
        args = pgy_arena_calloc(&ctx->scratch,
            (argc > 0 ? argc : 1) * sizeof(LLVMValueRef));
        if (args != NULL) {
            for (size_t i = 0; i < argc; i++) {
                const char *type_name = NULL;
                bool pointer_self = false;
                ASTNode *arg_node = node->data.call.arguments[i];

                {
                    size_t binding_count = intent_decl->data.intent_decl.binding_count > 0
                        ? intent_decl->data.intent_decl.binding_count
                        : (intent_decl->data.intent_decl.involve_count
                            + intent_decl->data.intent_decl.value_count);
                    if (i < binding_count) {
                        ASTNode *binding = intent_decl->data.intent_decl.binding_count > 0
                            ? intent_decl->data.intent_decl.bindings[i]
                            : (i < intent_decl->data.intent_decl.involve_count
                                ? intent_decl->data.intent_decl.involves[i]
                                : intent_decl->data.intent_decl.values[
                                    i - intent_decl->data.intent_decl.involve_count]);
                        if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                            && binding->data.intent_involves.subject_type != NULL
                            && binding->data.intent_involves.subject_type->type == AST_TYPE) {
                            type_name = binding->data.intent_involves.subject_type->data.type.name;
                        } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                            && binding->data.intent_value.value_type != NULL
                            && binding->data.intent_value.value_type->type == AST_TYPE) {
                            type_name = binding->data.intent_value.value_type->data.type.name;
                        }
                    }
                }
                pointer_self = llvm_type_name_uses_pointer_self(ctx, type_name);
                if (pointer_self) {
                    if (arg_node != NULL && arg_node->type == AST_IDENTIFIER) {
                        const char *arg_name = arg_node->data.identifier.name;
                        LLVMVarEntry *arg_var = llvm_scope_lookup(ctx, arg_name);
                        if (arg_var != NULL) {
                            if (LLVMGetTypeKind(arg_var->type) == LLVMPointerTypeKind)
                                args[i] = LLVMBuildLoad2(ctx->builder, arg_var->type,
                                    arg_var->alloca, llvm_tmp_name(ctx));
                            else
                                args[i] = arg_var->alloca;
                        } else {
                            ASTNode *host_decl = llvm_current_host_decl(ctx);
                            const char *host_name = llvm_decl_node_name(host_decl);
                            LLVMClassTypeEntry *host_cls =
                                host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
                            if (host_cls != NULL) {
                                int field_idx = llvm_class_field_index(host_cls, arg_name);
                                if (field_idx >= 0) {
                                    LLVMValueRef base_ptr = llvm_current_self_base_ptr(ctx, host_cls);
                                    if (base_ptr != NULL) {
                                        args[i] = LLVMBuildStructGEP2(ctx->builder,
                                            host_cls->struct_type, base_ptr,
                                            (unsigned)field_idx, llvm_tmp_name(ctx));
                                    }
                                }
                            }
                        }
                    } else if (arg_node != NULL && arg_node->type == AST_MEMBER_ACCESS) {
                        args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
                    }
                }
                if (args[i] == NULL)
                    args[i] = llvm_emit_expression(arg_node, ctx);
            }
            emitted_argc = (unsigned)argc;
        }
    }
    if (args == NULL) {
        args = pgy_arena_calloc(&ctx->scratch,
            (argc > 0 ? argc : 1) * sizeof(LLVMValueRef));
        for (size_t i = 0; i < argc; i++)
            args[i] = llvm_emit_expression(node->data.call.arguments[i], ctx);
        emitted_argc = (unsigned)argc;
    }

    LLVMFuncEntry *func = llvm_resolve_callee_entry(ctx, callee_name, args, argc);
    if (func == NULL && decl != NULL && decl->type == AST_FUNC_DECL) {
        llvm_forward_declare_func(decl, ctx);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL && intent_decl != NULL) {
        LLVMTypeRef *param_types = NULL;
        LLVMTypeRef fn_type;
        LLVMValueRef fn;

        if (intent_decl->data.intent_decl.binding_count > 0
            || intent_decl->data.intent_decl.involve_count > 0
            || intent_decl->data.intent_decl.value_count > 0) {
            size_t param_count = intent_decl->data.intent_decl.binding_count > 0
                ? intent_decl->data.intent_decl.binding_count
                : (intent_decl->data.intent_decl.involve_count
                    + intent_decl->data.intent_decl.value_count);
            param_types = pgy_arena_calloc(&ctx->scratch,
                param_count * sizeof(LLVMTypeRef));
            for (size_t i = 0; i < param_count; i++) {
                LLVMTypeRef pt = ctx->type_i8ptr;
                ASTNode *binding = intent_decl->data.intent_decl.binding_count > 0
                    ? intent_decl->data.intent_decl.bindings[i]
                    : (i < intent_decl->data.intent_decl.involve_count
                        ? intent_decl->data.intent_decl.involves[i]
                        : intent_decl->data.intent_decl.values[
                            i - intent_decl->data.intent_decl.involve_count]);
                const char *type_name = NULL;

                if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                    && binding->data.intent_involves.subject_type != NULL
                    && binding->data.intent_involves.subject_type->type == AST_TYPE) {
                    type_name = binding->data.intent_involves.subject_type->data.type.name;
                    pt = ast_type_to_llvm(ctx, binding->data.intent_involves.subject_type);
                    if (llvm_type_name_uses_pointer_self(ctx, type_name))
                        pt = LLVMPointerType(pt, 0);
                } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                    && binding->data.intent_value.value_type != NULL) {
                    pt = ast_type_to_llvm(ctx, binding->data.intent_value.value_type);
                }
                if (pt == NULL) {
                    pt = ctx->type_i8ptr;
                }
                param_types[i] = pt;
            }
        }

        fn_type = LLVMFunctionType(ctx->type_i1, param_types,
            (unsigned)(intent_decl->data.intent_decl.involve_count
                + intent_decl->data.intent_decl.value_count), 0);
        fn = LLVMAddFunction(ctx->module, callee_name, fn_type);
        llvm_register_function(ctx, callee_name, fn, fn_type, ctx->type_i1);
        func = llvm_lookup_function(ctx, callee_name);
    }
    if (func == NULL) {
        LLVMVarEntry *callee_var = NULL;
        LLVMValueRef fn_ptr = NULL;
        LLVMTypeRef fn_type = NULL;
        LLVMValueRef result;
        LLVMCallableVarEntry *callable_entry = NULL;

        if (node->data.call.callee->type == AST_IDENTIFIER)
            callee_var = llvm_scope_lookup(ctx, callee_name);
        callable_entry = llvm_lookup_callable_entry(ctx, callee_name);
        if (callee_var != NULL) {
            LLVMTypeRef callable_ptr_ty = NULL;
            if (LLVMGetTypeKind(callee_var->type) == LLVMPointerTypeKind)
                fn_type = LLVMGetElementType(callee_var->type);
            if (fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind) {
                if (callable_entry != NULL)
                    fn_type = llvm_function_signature_from_callable_entry(ctx, callable_entry);
            }
            if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind)
                callable_ptr_ty = LLVMPointerType(fn_type, 0);
            if ((fn_type == NULL || LLVMGetTypeKind(fn_type) != LLVMFunctionTypeKind)
                && ctx->current_function != NULL) {
                const char *current_name = LLVMGetValueName(ctx->current_function);
                ASTNode *current_decl = current_name != NULL
                    ? llvm_find_function_decl(ctx, current_name) : NULL;
                if (current_decl != NULL && current_decl->type == AST_FUNC_DECL) {
                    for (size_t i = 0; i < current_decl->data.func_decl.param_count; i++) {
                        FuncParam *p = current_decl->data.func_decl.params[i];
                        if (p == NULL || p->name == NULL || p->type == NULL)
                            continue;
                        if (strcmp(p->name, callee_name) != 0)
                            continue;
                        if (p->type->type == AST_EVENT_HANDLER_TYPE) {
                            size_t pc = p->type->data.event_handler_type.param_count;
                            LLVMTypeRef *pts = NULL;
                            LLVMTypeRef ret = ctx->type_void;
                            if (p->type->data.event_handler_type.return_type != NULL)
                                ret = ast_type_to_llvm(ctx,
                                    p->type->data.event_handler_type.return_type);
                            if (pc > 0) {
                                pts = pgy_arena_calloc(&ctx->scratch,
                                    pc * sizeof(LLVMTypeRef));
                                if (pts != NULL) {
                                    for (size_t pi = 0; pi < pc; pi++) {
                                        pts[pi] = ast_type_to_llvm(ctx,
                                            p->type->data.event_handler_type.param_types[pi]);
                                    }
                                }
                            }
                            fn_type = LLVMFunctionType(ret, pts, (unsigned)pc, 0);
                        } else {
                            LLVMTypeRef declared_ptr_ty = ast_type_to_llvm(ctx, p->type);
                            if (LLVMGetTypeKind(declared_ptr_ty) == LLVMPointerTypeKind)
                                fn_type = LLVMGetElementType(declared_ptr_ty);
                            else
                                fn_type = declared_ptr_ty;
                        }
                        break;
                    }
                }
            }
            if (fn_type != NULL
                && LLVMGetTypeKind(fn_type) == LLVMPointerTypeKind) {
                callable_ptr_ty = fn_type;
                fn_type = LLVMGetElementType(fn_type);
            }
            if (callable_ptr_ty == NULL
                && fn_type != NULL
                && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
                callable_ptr_ty = LLVMPointerType(fn_type, 0);
            }
            fn_ptr = llvm_emit_expression(node->data.call.callee, ctx);
            if (fn_ptr != NULL && callable_ptr_ty != NULL
                && LLVMTypeOf(fn_ptr) != callable_ptr_ty) {
                fn_ptr = LLVMBuildBitCast(ctx->builder, fn_ptr, callable_ptr_ty,
                    llvm_tmp_name(ctx));
            }
            if (fn_type == NULL && LLVMGetTypeKind(LLVMTypeOf(fn_ptr)) == LLVMFunctionTypeKind)
                fn_type = LLVMTypeOf(fn_ptr);
            if (fn_type != NULL && LLVMGetTypeKind(fn_type) == LLVMFunctionTypeKind) {
                if (LLVMGetReturnType(fn_type) == ctx->type_void) {
                    LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                        args, emitted_argc, "");
                    result = LLVMConstInt(ctx->type_i32, 0, 0);
                } else {
                    result = LLVMBuildCall2(ctx->builder, fn_type, fn_ptr,
                        args, emitted_argc, llvm_tmp_name(ctx));
                }
                return result;
            }
        }

        fprintf(stderr, "[llvm] warning: unknown function '%s'\n", callee_name);
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }

    for (size_t i = 0; decl == NULL && i < argc; i++) {
        ASTNode *arg_node = node->data.call.arguments[i];
        unsigned param_count = LLVMCountParams(func->fn);
        LLVMTypeRef param_ty = (i < param_count)
            ? LLVMTypeOf(LLVMGetParam(func->fn, (unsigned)i))
            : NULL;
            if (param_ty != NULL
            && LLVMGetTypeKind(param_ty) == LLVMPointerTypeKind) {
            if (arg_node->type == AST_IDENTIFIER) {
                LLVMVarEntry *v = llvm_scope_lookup(ctx,
                    arg_node->data.identifier.name);
                LLVMCallableVarEntry *callable_entry =
                    llvm_lookup_callable_entry(ctx, arg_node->data.identifier.name);
                if (v != NULL) {
                    if (callable_entry != NULL) {
                        LLVMTypeRef callable_sig =
                            llvm_function_signature_from_callable_entry(ctx, callable_entry);
                        LLVMTypeRef callable_ptr_ty = callable_sig != NULL
                            ? LLVMPointerType(callable_sig, 0)
                            : v->type;
                        args[i] = LLVMBuildLoad2(ctx->builder, callable_ptr_ty,
                            v->alloca, llvm_tmp_name(ctx));
                    } else if (LLVMGetTypeKind(v->type) == LLVMPointerTypeKind) {
                        args[i] = LLVMBuildLoad2(ctx->builder, v->type,
                            v->alloca, llvm_tmp_name(ctx));
                    } else {
                        args[i] = v->alloca;
                    }
                }
            } else if (arg_node->type == AST_MEMBER_ACCESS) {
                args[i] = llvm_emit_member_lvalue_ptr(arg_node, ctx, NULL);
            }
            if (args[i] == NULL && arg_node->type == AST_CALL) {
                LLVMValueRef maybe_value = llvm_emit_expression(arg_node, ctx);
                if (maybe_value != NULL
                    && LLVMGetTypeKind(LLVMTypeOf(maybe_value)) == LLVMPointerTypeKind) {
                    args[i] = maybe_value;
                }
            }
        }
    }

    LLVMValueRef result;
    if (func->ret_type == ctx->type_void) {
        LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                       args, emitted_argc, "");
        result = LLVMConstInt(ctx->type_i32, 0, 0);
    } else {
        result = LLVMBuildCall2(ctx->builder, func->fn_type, func->fn,
                                args, emitted_argc, llvm_tmp_name(ctx));
    }

    return result;
}
