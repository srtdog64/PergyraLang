#ifndef PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H
#define PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H

char *
emit_builtin_log(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (call->data.call.arg_count == 1) {
        ASTNode *arg_node = call->data.call.arguments[0];
        if (arg_node != NULL && arg_node->type == AST_STRING
            && arg_node->data.string.value != NULL) {
            const char *raw = arg_node->data.string.value;
            bool multiline = (strchr(raw, '\n') != NULL)
                           || (strchr(raw, '\r') != NULL);
            char *escaped = escape_c_string_literal(arg_node->data.string.value);
            if (escaped != NULL) {
                char *result;
                if (multiline) {
                    char *normalized = normalize_banner_string_literal(raw);
                    if (normalized == NULL)
                        normalized = pergyra_strdup(raw);
                    free(escaped);
                    escaped = escape_c_string_literal(normalized);
                    free(normalized);
                    if (escaped == NULL) {
                        return pergyra_strdup("/* Log: failed to normalize string */");
                    }
                }
                result = strdup_fmt("pgy_log%s(\"%s\")", multiline ? "_banner" : "", escaped);
                free(escaped);
                return result;
            }
            return pergyra_strdup("/* Log: failed to escape string */");
        }
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_builtin_log_raw(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count == 0)
        return pergyra_strdup("printf(\"\\n\")");

    if (call->data.call.arg_count == 1) {
        ASTNode *arg_node = call->data.call.arguments[0];

        if (arg_node != NULL && arg_node->type == AST_STRING
            && arg_node->data.string.value != NULL) {
            char *escaped = escape_c_string_literal(arg_node->data.string.value);
            if (escaped != NULL) {
                char *result = strdup_fmt("pgy_log(\"%s\")", escaped);
                free(escaped);
                return result;
            }
            return pergyra_strdup("/* LogRaw: failed to escape string */");
        }

        char *arg = emit_expression(arg_node, ctx);
        char *result = strdup_fmt("pgy_log(%s)", arg);
        free(arg);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    codebuf_write(buf, "do { ");
    for (size_t i = 0; i < call->data.call.arg_count; i++) {
        char *arg = emit_expression(call->data.call.arguments[i], ctx);
        codebuf_write(buf, "pgy_log(%s); ", arg);
        free(arg);
    }
    codebuf_write(buf, "} while(0)");
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
emit_builtin_log_banner(ASTNode *call, TranspilerCtx *ctx)
{
    if (call->data.call.arg_count < 1 || call->data.call.arguments[0] == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: LogBanner requires one argument");
        return pergyra_strdup("0");
    }

    if (call->data.call.arg_count != 1)
        return emit_builtin_log(call, ctx);

    ASTNode *arg_node = call->data.call.arguments[0];
    if (arg_node->type != AST_STRING)
        return emit_builtin_log(call, ctx);

    char *normalized = normalize_banner_string_literal(arg_node->data.string.value);
    if (normalized == NULL)
        return emit_builtin_log(call, ctx);

    char *escaped = escape_c_string_literal(normalized);
    if (escaped == NULL) {
        free(normalized);
        return emit_builtin_log(call, ctx);
    }
    char *result = strdup_fmt("pgy_log_banner(\"%s\")", escaped);
    free(normalized);
    free(escaped);
    return result;
}

static const char *
lookup_wrapped_inner_type(TranspilerCtx *ctx, ASTNode *arg, const char *wrapper)
{
    if (arg != NULL && arg->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, arg->data.identifier.name);
        size_t wrapper_len = strlen(wrapper);
        if (type_name != NULL && strncmp(type_name, wrapper, wrapper_len) == 0
            && type_name[wrapper_len] == '<') {
            return slot_inner_type_name(type_name);
        }
    }
    return NULL;
}

static const char *
expected_wrapped_inner_type(TranspilerCtx *ctx, const char *wrapper)
{
    size_t wrapper_len;

    if (ctx == NULL || ctx->expected_type == NULL || wrapper == NULL)
        return NULL;
    wrapper_len = strlen(wrapper);
    if (strncmp(ctx->expected_type, wrapper, wrapper_len) == 0
        && ctx->expected_type[wrapper_len] == '<')
        return slot_inner_type_name(ctx->expected_type);
    return NULL;
}

char *
emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = NULL;
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_RC_NEW:
        if (call->data.call.arg_count != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: RcNew requires exactly one argument");
            return pergyra_strdup("0");
        }
        {
            const char *expected_inner =
                expected_wrapped_inner_type(ctx, "Rc");
            const char *arg_type = NULL;
            if (expected_inner != NULL)
                inner = expected_inner;
            else if (arg != NULL)
                arg_type = infer_expression_type_name(ctx, arg);
            if (expected_inner == NULL && arg_type != NULL
                && strcmp(arg_type, "Unknown") != 0)
                inner = arg_type;
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: RcNew requires concrete Rc<T> metadata or a typed initializer");
                return pergyra_strdup("0");
            }
        }
        break;
    case BUILTIN_RC_CLONE:
    case BUILTIN_RC_DROP:
    case BUILTIN_RC_GET:
    case BUILTIN_RC_DOWNGRADE:
        inner = lookup_wrapped_inner_type(ctx, arg, "Rc");
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Rc operation requires concrete Rc<T> metadata");
            return pergyra_strdup("0");
        }
        break;
    case BUILTIN_WEAK_UPGRADE:
    case BUILTIN_WEAK_DROP:
        inner = lookup_wrapped_inner_type(ctx, arg, "Weak");
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Weak operation requires concrete Weak<T> metadata");
            return pergyra_strdup("0");
        }
        break;
    default:
        break;
    }

    if (kind == BUILTIN_RC_NEW) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_CLONE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_clone_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("(*pgy_rc_get_%s(&%s))", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_RC_DOWNGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_rc_downgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_UPGRADE) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_upgrade_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_WEAK_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_weak_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported Rc builtin kind %d", (int)kind);
    return pergyra_strdup("0");
}

char *
emit_builtin_box(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    const char *inner = NULL;
    ASTNode *arg = call->data.call.arg_count > 0 ? call->data.call.arguments[0] : NULL;

    switch (kind) {
    case BUILTIN_BOX:
        if (call->data.call.arg_count != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: Box requires exactly one argument");
            return pergyra_strdup("0");
        }
        if (arg->type == AST_NUMBER) inner = "Int";
        else if (arg->type == AST_STRING) inner = "String";
        else if (arg->type == AST_BOOLEAN) inner = "Bool";
        else if (arg->type == AST_CALL
                 && arg->data.call.callee != NULL
                 && arg->data.call.callee->type == AST_IDENTIFIER) {
            const char *callee_name = arg->data.call.callee->data.identifier.name;
            ASTNode *class_decl = find_class_decl(ctx, callee_name);
            if (class_decl != NULL && class_decl->type == AST_CLASS_DECL)
                inner = callee_name;
            else {
                const char *arg_type = infer_expression_type_name(ctx, arg);
                if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                    inner = arg_type;
            }
        }
        else if (arg->type == AST_IDENTIFIER) {
            const char *arg_type = lookup_typed_var(ctx, arg->data.identifier.name);
            if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                inner = arg_type;
        } else {
            const char *arg_type = infer_expression_type_name(ctx, arg);
            if (arg_type != NULL && strcmp(arg_type, "Unknown") != 0)
                inner = arg_type;
        }
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Box requires concrete Box<T> metadata or a typed initializer");
            return pergyra_strdup("0");
        }
        break;
    case BUILTIN_BOX_GET:
    case BUILTIN_BOX_SET:
    case BUILTIN_BOX_DROP:
    case BUILTIN_BOX_IS_VALID:
        inner = lookup_wrapped_inner_type(ctx, arg, "Box");
        if (inner == NULL || inner[0] == '\0') {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: Box operation requires concrete Box<T> metadata");
            return pergyra_strdup("0");
        }
        break;
    case BUILTIN_BOX_ARRAY:
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: BoxArray requires an array argument");
            return pergyra_strdup("0");
        }
        {
            const char *arr_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
            if (arr_type != NULL && strncmp(arr_type, "Array<", 6) == 0)
                inner = arr_type;
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: BoxArray requires concrete Array<T> metadata");
                return pergyra_strdup("0");
            }
        }
        break;
    default:
        break;
    }

    if (kind == BUILTIN_BOX) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_new_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_GET) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_get_%s(%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_SET) {
        char *box_expr = emit_expression(arg, ctx);
        char *value = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("pgy_box_set_%s(&%s, %s)", inner, box_expr, value);
        free(box_expr);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_DROP) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_drop_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_IS_VALID) {
        char *value = emit_expression(arg, ctx);
        char *result = strdup_fmt("pgy_box_is_valid_%s(&%s)", inner, value);
        free(value);
        return result;
    }

    if (kind == BUILTIN_BOX_ARRAY) {
        if (call->data.call.arg_count < 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: BoxArray requires an array argument");
            return pergyra_strdup("0");
        }
        char *arr_expr = emit_expression(call->data.call.arguments[0], ctx);
        const char *elem = slot_inner_type_name(inner);
        char *result = strdup_fmt("pgy_box_new_Array_%s(%s)", elem, arr_expr);
        free(arr_expr);
        return result;
    }

    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported Box builtin kind %d", (int)kind);
    return pergyra_strdup("0");
}

char *
emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    switch (kind) {
    case BUILTIN_ALLOCATOR_SYSTEM:
        return pergyra_strdup("pgy_allocator_system()");
    case BUILTIN_ALLOCATOR_TRACING:
        return pergyra_strdup("pgy_allocator_tracing()");
    case BUILTIN_ALLOCATOR_DEBUG:
        return pergyra_strdup("pgy_allocator_debug()");
    case BUILTIN_ALLOCATOR_POOL:
        if (call->data.call.arg_count != 1) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: AllocatorPool requires exactly one capacity argument");
            return pergyra_strdup("0");
        }
        {
            char *cap = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("pgy_allocator_pool(%s)", cap);
            free(cap);
            return result;
        }
    default:
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported allocator builtin kind %d", (int)kind);
        return pergyra_strdup("0");
    }
}

#endif /* PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H */
