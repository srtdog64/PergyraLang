static bool
llvm_emit_slot_builtin_call(ASTNode *node, LLVMGenCtx *ctx,
                            const char *callee_name, LLVMValueRef *out)
{
    if (out == NULL)
        return false;
    *out = NULL;

    if (strcmp(callee_name, "ClaimSlot") == 0
        || strcmp(callee_name, "ClaimSecureSlot") == 0) {
        llvm_set_error_at_with_hints(ctx, node, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM standalone %s requires an explicitly typed binding; use 'let value: %s<T> = %s()'",
            callee_name,
            strcmp(callee_name, "ClaimSecureSlot") == 0 ? "SecureSlot" : "Slot",
            callee_name);
        return true;
    }

    if (strcmp(callee_name, "ClaimDeviceSlot") == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_claim_device_Int");
        if (fn == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                              NULL, 0, llvm_tmp_name(ctx));
        return true;
    }

    if (strcmp(callee_name, "Write") == 0) {
        if (node->data.call.arg_count < 2) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_write_%s" : "pgy_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            if (is_secure)
                llvm_direct_secure_slot_write(ctx, slot_var, val);
            else
                llvm_direct_slot_write(ctx, slot_var, val);
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL) {
                *out = LLVMConstInt(ctx->type_i32, 0, 0);
                return true;
            }
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                val,
                token_var->alloca
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 3, "");
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                val
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        }
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "Read") == 0) {
        if (node->data.call.arg_count < 1) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_read_%s" : "pgy_read_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            *out = is_secure
                ? llvm_direct_secure_slot_read(ctx, slot_var, inner)
                : llvm_direct_slot_read(ctx, slot_var, inner);
            return true;
        }

        if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL) {
                *out = LLVMConstInt(ctx->type_i32, 0, 0);
                return true;
            }
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                token_var->alloca
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 2, llvm_tmp_name(ctx));
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var)
            };
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 1, llvm_tmp_name(ctx));
        }
        return true;
    }

    if (strcmp(callee_name, "Release") == 0) {
        if (node->data.call.arg_count < 1) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        const char *inner = "Int";
        const char *source_name = NULL;
        bool is_secure = false;
        ASTNode *slot_arg = node->data.call.arguments[0];
        LLVMVarEntry *slot_var = llvm_resolve_slot_target(ctx, slot_arg, &inner,
            &source_name, &is_secure);
        if (slot_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name),
            is_secure ? "pgy_secure_release_%s" : "pgy_release_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL && is_secure) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        if (fn == NULL) {
            if (is_secure)
                llvm_direct_secure_slot_release(ctx, slot_var);
            else
                llvm_direct_slot_release(ctx, slot_var);
        } else if (is_secure) {
            LLVMVarEntry *token_var = llvm_lookup_secure_token_var(ctx, source_name);
            if (token_var == NULL) {
                *out = LLVMConstInt(ctx->type_i32, 0, 0);
                return true;
            }
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var),
                token_var->alloca
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        } else {
            LLVMValueRef args[] = {
                llvm_slot_runtime_arg(ctx, slot_var)
            };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }

        if (source_name != NULL) {
            const char *sname = source_name;
            for (int ri = 0; ri < ctx->slot_var_count; ri++) {
                if (strcmp(ctx->slot_vars[ri].var_name, sname) == 0) {
                    ctx->slot_vars[ri].released = true;
                    break;
                }
            }
        }

        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "DeviceWrite") == 0) {
        if (node->data.call.arg_count < 2) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM DeviceWrite on '%s' requires a concrete DeviceSlot<T> inner type",
                slot_arg->data.identifier.name);
            return true;
        }
        if (slot_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        char fn_name[64];
        snprintf(fn_name, sizeof(fn_name), "pgy_device_write_%s", inner);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (fn == NULL || val == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMValueRef args[] = { slot_var->alloca, val };
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
        *out = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "DeviceRead") == 0
        || strcmp(callee_name, "ReleaseDeviceSlot") == 0
        || strcmp(callee_name, "SubmitDeviceRead") == 0) {
        if (node->data.call.arg_count < 1) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        ASTNode *slot_arg = node->data.call.arguments[0];
        const char *inner = llvm_call_arg_device_inner(ctx, slot_arg);
        LLVMVarEntry *slot_var = NULL;
        if (slot_arg != NULL && slot_arg->type == AST_IDENTIFIER)
            slot_var = llvm_scope_lookup(ctx, slot_arg->data.identifier.name);
        if (inner == NULL && slot_arg != NULL && slot_arg->type == AST_IDENTIFIER) {
            llvm_set_error_at_with_hints(ctx, slot_arg, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "LLVM %s on '%s' requires a concrete DeviceSlot<T> inner type",
                callee_name, slot_arg->data.identifier.name);
            return true;
        }
        if (slot_var == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        char fn_name[64];
        if (strcmp(callee_name, "DeviceRead") == 0)
            snprintf(fn_name, sizeof(fn_name), "pgy_device_read_%s", inner);
        else if (strcmp(callee_name, "ReleaseDeviceSlot") == 0)
            snprintf(fn_name, sizeof(fn_name), "pgy_release_device_%s", inner);
        else
            snprintf(fn_name, sizeof(fn_name), "pgy_submit_device_read_%s", inner);

        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fn_name);
        if (fn == NULL) {
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
            return true;
        }

        LLVMValueRef args[] = { slot_var->alloca };
        if (strcmp(callee_name, "DeviceRead") == 0
            || strcmp(callee_name, "SubmitDeviceRead") == 0) {
            *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                                  args, 1, llvm_tmp_name(ctx));
        } else {
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
            if (slot_arg->type == AST_IDENTIFIER)
                llvm_mark_device_slot_released(ctx, slot_arg->data.identifier.name);
            *out = LLVMConstInt(ctx->type_i32, 0, 0);
        }
        return true;
    }

    return false;
}
