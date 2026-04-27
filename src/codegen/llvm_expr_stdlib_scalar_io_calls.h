static bool
llvm_emit_stdlib_string_file_call(ASTNode *node, LLVMGenCtx *ctx,
                                  const char *callee_name,
                                  LLVMValueRef *out_result)
{
    if (node == NULL || ctx == NULL || callee_name == NULL || out_result == NULL)
        return false;

    if (strcmp(callee_name, "StringLength") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef s = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMFuncEntry *strlen_fn = llvm_lookup_function(ctx, "strlen");
        LLVMValueRef args[] = { s };
        LLVMValueRef len;
        if (strlen_fn == NULL) {
            LLVMTypeRef params[] = { ctx->type_i8ptr };
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_i64, params, 1, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module, "strlen", ft);
            llvm_register_function(ctx, "strlen", fn, ft, ctx->type_i64);
            strlen_fn = llvm_lookup_function(ctx, "strlen");
        }
        len = LLVMBuildCall2(ctx->builder, strlen_fn->fn_type,
            strlen_fn->fn, args, 1, llvm_tmp_name(ctx));
        *out_result = LLVMBuildTrunc(ctx->builder, len, ctx->type_i32,
            llvm_tmp_name(ctx));
        return true;
    }

    if ((strcmp(callee_name, "Contains") == 0
         || strcmp(callee_name, "StringContains") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringContains");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
            return true;
        }
    }

    if ((strcmp(callee_name, "Replace") == 0
         || strcmp(callee_name, "StringReplace") == 0)
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringReplace");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
            return true;
        }
    }

    if (strcmp(callee_name, "Substring") == 0
        && node->data.call.arg_count == 3) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Substring");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 3);
            return true;
        }
    }

    if ((strcmp(callee_name, "Trim") == 0
         || strcmp(callee_name, "StringTrim") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringTrim");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if ((strcmp(callee_name, "Upper") == 0
         || strcmp(callee_name, "ToUpper") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToUpper");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if ((strcmp(callee_name, "Lower") == 0
         || strcmp(callee_name, "ToLower") == 0)
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToLower");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if ((strcmp(callee_name, "Concat") == 0
         || strcmp(callee_name, "StringConcat") == 0)
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "StringConcat");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
            return true;
        }
    }

    if (strcmp(callee_name, "ReadFile") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_read_file");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "ToString") == 0
        && node->data.call.arg_count == 1) {
        LLVMValueRef value = llvm_emit_expression(node->data.call.arguments[0], ctx);
        *out_result = llvm_coerce_value_to_string(value, ctx);
        return true;
    }

    if (strcmp(callee_name, "ToInt") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToInt");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "ToFloat") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "ToFloat");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "Random") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "Random");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "WriteFile") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_write_file");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
            return true;
        }
    }

    if (strcmp(callee_name, "Input") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    return false;
}

static bool
llvm_emit_stdlib_runtime_io_call(ASTNode *node, LLVMGenCtx *ctx,
                                 const char *callee_name,
                                 LLVMValueRef *out_result)
{
    if (node == NULL || ctx == NULL || callee_name == NULL || out_result == NULL)
        return false;

    if (strcmp(callee_name, "SeedRandom") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "SeedRandom");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "FileOpen") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_open");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
            return true;
        }
    }

    if (strcmp(callee_name, "FileRead") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_read");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "FileWrite") == 0
        && node->data.call.arg_count == 2) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_write");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 2);
            return true;
        }
    }

    if (strcmp(callee_name, "FileClose") == 0
        && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_file_close");
        if (fn != NULL) {
            *out_result = llvm_emit_function_call_args(ctx, fn,
                node->data.call.arguments, 1);
            return true;
        }
    }

    if (strcmp(callee_name, "Print") == 0 && node->data.call.arg_count == 1) {
        LLVMValueRef val = llvm_emit_expression(node->data.call.arguments[0], ctx);
        LLVMTypeRef vt = LLVMTypeOf(val);
        LLVMFuncEntry *pf = llvm_lookup_function(ctx, "printf");
        if (pf != NULL) {
            if (vt == ctx->type_i8ptr) {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%s", ".fmt_s");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            } else {
                LLVMValueRef fmt = LLVMBuildGlobalStringPtr(ctx->builder,
                    "%d", ".fmt_d");
                LLVMValueRef args[] = { fmt, val };
                LLVMBuildCall2(ctx->builder, pf->fn_type, pf->fn, args, 2, "");
            }
        }
        *out_result = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    if (strcmp(callee_name, "ReadLine") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_input");
        if (fn != NULL) {
            LLVMValueRef empty = LLVMBuildGlobalStringPtr(ctx->builder, "",
                ".readline_empty");
            LLVMValueRef args[] = { empty };
            *out_result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, "");
            return true;
        }
    }

    if (strcmp(callee_name, "Now") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_now_ms");
        if (fn != NULL) {
            *out_result = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                NULL, 0, "");
            return true;
        }
    }

    if (strcmp(callee_name, "Sleep") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, "pgy_sleep_ms");
        if (fn != NULL) {
            LLVMValueRef arg = llvm_emit_expression(node->data.call.arguments[0], ctx);
            LLVMValueRef args[] = { arg };
            LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
        }
        *out_result = LLVMConstInt(ctx->type_i32, 0, 0);
        return true;
    }

    return false;
}
