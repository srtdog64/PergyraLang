#ifndef PGY_SRC_CODEGEN_TRANSPILER_CALL_RESULT_OPTION_BUILTIN_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_CALL_RESULT_OPTION_BUILTIN_EMIT_H

static bool
transpiler_option_type_has_concrete_inner(const char *opt_type)
{
    const char *inner = NULL;
    char inner_buf[128];
    if (opt_type == NULL || strncmp(opt_type, "Option<", 7) != 0)
        return false;
    if (!slot_inner_type_name_copy(opt_type, inner_buf, sizeof(inner_buf)))
        return false;
    inner = inner_buf;
    return inner != NULL
        && inner[0] != '\0'
        && strcmp(inner, "Unknown") != 0;
}

static char *
emit_call_result_option_builtin(ASTNode *call, ASTNode *callee, TranspilerCtx *ctx)
{
    /* Result<T, E> built-in functions:
     * - Ok/Err require explicit Result context from the surrounding type.
     * - IsOk/IsErr/Unwrap/UnwrapOr may also derive suffix from their Result
     *   operand when the surrounding expression context is not specific. */
    if (callee->type == AST_IDENTIFIER) {
        const char *fn = ast_identifier_name(callee);
        size_t argc = ast_call_arg_count(call);
        ASTNode *arg0 = ast_call_argument(call, 0);
        ASTNode *arg1 = ast_call_argument(call, 1);
        bool is_result_ctor = false;
        bool is_result_consumer = false;
        char result_suffix[128] = {0};
        bool have_result_suffix = transpiler_result_suffix_from_context(
            ctx, result_suffix, sizeof(result_suffix));
        is_result_ctor = strcmp(fn, "Ok") == 0 || strcmp(fn, "Err") == 0;
        is_result_consumer = strcmp(fn, "IsOk") == 0
            || strcmp(fn, "IsErr") == 0
            || strcmp(fn, "Unwrap") == 0
            || strcmp(fn, "UnwrapOr") == 0;

        if (!have_result_suffix && is_result_consumer
            && argc >= 1
            && arg0 != NULL) {
            const char *arg_type = infer_expression_type_name(
                ctx, arg0);
            have_result_suffix = transpiler_result_suffix_from_type_name(
                arg_type, result_suffix, sizeof(result_suffix));
        }

        if ((is_result_ctor || is_result_consumer) && !have_result_suffix) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot derive Result<T, E> specialization for %s(); add explicit Result<T, E> type context",
                fn);
            return pergyra_strdup("0");
        }

        if (strcmp(fn, "Ok") == 0 && argc == 1) {
            char *arg = emit_expression(arg0, ctx);
            char *result = strdup_fmt("Ok_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Err") == 0 && argc == 1) {
            char *arg = emit_expression(arg0, ctx);
            char *result = strdup_fmt("Err_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsOk") == 0 && argc == 1) {
            char *arg = emit_expression(arg0, ctx);
            char *result = strdup_fmt("IsOk_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsErr") == 0 && argc == 1) {
            char *arg = emit_expression(arg0, ctx);
            char *result = strdup_fmt("IsErr_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "Unwrap") == 0 && argc == 1) {
            char *arg = emit_expression(arg0, ctx);
            char *result = strdup_fmt("Unwrap_%s(%s)", result_suffix, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOr") == 0 && argc == 2) {
            char *arg = emit_expression(arg0, ctx);
            char *fallback = emit_expression(arg1, ctx);
            char *result = strdup_fmt("UnwrapOr_%s(%s, %s)", result_suffix, arg, fallback);
            free(arg);
            free(fallback);
            return result;
        }
        if (strcmp(fn, "Some") == 0 && argc == 1) {
            char *arg = emit_expression(arg0, ctx);
            const char *inner = infer_expression_type_name(ctx, arg0);
            char inner_buf[128];
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                if (transpiler_contextual_option_inner_type_copy(ctx,
                        inner_buf, sizeof(inner_buf))) {
                    inner = inner_buf;
                }
            }
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                free(arg);
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Some requires concrete payload type during C emission");
                return pergyra_strdup("0");
            }
            char *result = strdup_fmt("Some_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "None") == 0 && argc == 0) {
            return transpiler_emit_none_with_context(ctx, call);
        }
        if (strcmp(fn, "IsSome") == 0 && argc == 1) {
            const char *opt_type = infer_expression_type_name(ctx, arg0);
            if (!transpiler_option_type_has_concrete_inner(opt_type)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: IsSome requires concrete Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return pergyra_strdup("false");
            }
            char *arg = emit_expression(arg0, ctx);
            char inner_buf[128];
            const char *inner = inner_buf;
            (void)slot_inner_type_name_copy(opt_type, inner_buf,
                sizeof(inner_buf));
            char *result = strdup_fmt("IsSome_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "IsNone") == 0 && argc == 1) {
            const char *opt_type = infer_expression_type_name(ctx, arg0);
            if (!transpiler_option_type_has_concrete_inner(opt_type)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: IsNone requires concrete Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return pergyra_strdup("false");
            }
            char *arg = emit_expression(arg0, ctx);
            char inner_buf[128];
            const char *inner = inner_buf;
            (void)slot_inner_type_name_copy(opt_type, inner_buf,
                sizeof(inner_buf));
            char *result = strdup_fmt("IsNone_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
        if (strcmp(fn, "UnwrapOption") == 0 && argc == 1) {
            const char *opt_type = infer_expression_type_name(ctx, arg0);
            if (!transpiler_option_type_has_concrete_inner(opt_type)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: UnwrapOption requires concrete Option<T>; inferred '%s'",
                    opt_type != NULL ? opt_type : "<unknown>");
                return pergyra_strdup("0");
            }
            char *arg = emit_expression(arg0, ctx);
            char inner_buf[128];
            const char *inner = inner_buf;
            (void)slot_inner_type_name_copy(opt_type, inner_buf,
                sizeof(inner_buf));
            char *result = strdup_fmt("UnwrapOption_%s(%s)", inner, arg);
            free(arg);
            return result;
        }
    }

    return NULL;
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_CALL_RESULT_OPTION_BUILTIN_EMIT_H */
