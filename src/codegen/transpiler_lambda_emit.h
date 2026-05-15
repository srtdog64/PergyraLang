#ifndef PGY_TRANSPILER_LAMBDA_EMIT_H
#define PGY_TRANSPILER_LAMBDA_EMIT_H

static bool
transpiler_infer_lambda_param_c_type_copy(ASTNode *lambda_node,
                                          ASTNode *param_node,
                                          char *out,
                                          size_t out_size)
{
    const char *param_name = NULL;
    ASTNode *body = NULL;
    ASTNode *ret_value = NULL;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (lambda_node == NULL || param_node == NULL)
        return false;
    if (param_node->type == AST_IDENTIFIER)
        param_name = param_node->data.identifier.name;
    else if (param_node->type == AST_LET_DECL)
        param_name = param_node->data.let_decl.name;
    if (param_name == NULL || lambda_node->data.lambda_expr.return_type == NULL)
        return false;

    body = lambda_node->data.lambda_expr.body;
    if (body == NULL)
        return false;
    if (body->type == AST_IDENTIFIER) {
        ret_value = body;
    } else if (body->type == AST_BLOCK
               && body->data.block.count == 1
               && body->data.block.statements[0] != NULL
               && body->data.block.statements[0]->type == AST_RETURN) {
        ret_value = ast_return_value(body->data.block.statements[0]);
    }

    if (ret_value != NULL
        && ret_value->type == AST_IDENTIFIER
        && ret_value->data.identifier.name != NULL
        && strcmp(ret_value->data.identifier.name, param_name) == 0) {
        return pergyra_ast_type_to_c_copy(
            lambda_node->data.lambda_expr.return_type,
            out,
            out_size);
    }
    return false;
}

static bool
transpiler_lambda_param_c_type_copy(ASTNode *lambda_node, ASTNode *param,
                                    char *out, size_t out_size,
                                    const char **param_name_out)
{
    const char *param_name = NULL;
    const char *param_type = NULL;

    if (param_name_out != NULL)
        *param_name_out = NULL;
    if (param == NULL || out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (param->type == AST_LET_DECL) {
        param_name = param->data.let_decl.name;
        if (param->data.let_decl.type != NULL
            && pergyra_ast_type_to_c_copy(param->data.let_decl.type,
                out,
                out_size)) {
            param_type = out;
        }
    } else {
        param_name = param->data.identifier.name;
        if (transpiler_infer_lambda_param_c_type_copy(lambda_node,
                param,
                out,
                out_size)) {
            param_type = out;
        }
    }
    if (param_name_out != NULL)
        *param_name_out = param_name;
    return param_type != NULL;
}

static bool
transpiler_emit_lambda_signature(ASTNode *node, TranspilerCtx *ctx,
                                 CodeBuf *out, const char *return_type,
                                 const char *lambda_name,
                                 bool declaration_only)
{
    codebuf_write(out, "\nstatic %s %s(", return_type, lambda_name);
    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        char param_type_buf[256];
        if (i > 0)
            codebuf_write(out, ", ");
        if (!transpiler_lambda_param_c_type_copy(node, param,
                param_type_buf, sizeof(param_type_buf), &param_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot determine lambda parameter type for '%s' at argument %llu",
                lambda_name,
                (unsigned long long) i);
            return false;
        }
        codebuf_write(out, "%s %s", param_type_buf, param_name);
    }
    codebuf_write(out, declaration_only ? ");\n" : ")\n{\n");
    return true;
}

char *
emit_lambda_expr(ASTNode *node, TranspilerCtx *ctx)
{
    int lambda_id = ++ctx->tmp_counter;
    const char *return_type = NULL;
    char inferred_return_c_type_buf[256];
    char *return_type_owned = NULL;
    int saved_typed_var_count = ctx->typed_var_count;

    for (size_t i = 0; i < node->data.lambda_expr.param_count; i++) {
        ASTNode *param = node->data.lambda_expr.params[i];
        const char *param_name = NULL;
        char *param_type_name = NULL;

        if (param == NULL)
            continue;
        if (param->type == AST_LET_DECL) {
            param_name = param->data.let_decl.name;
            if (param->data.let_decl.type != NULL)
                param_type_name = render_type_name(param->data.let_decl.type);
        } else if (param->type == AST_IDENTIFIER) {
            param_name = param->data.identifier.name;
        }
        if (param_name != NULL && param_type_name != NULL)
            register_typed_var(ctx, param_name, param_type_name);
        free(param_type_name);
    }

    if (node->data.lambda_expr.return_type != NULL) {
        if (pergyra_ast_type_to_c_copy(node->data.lambda_expr.return_type,
                inferred_return_c_type_buf,
                sizeof(inferred_return_c_type_buf))) {
            return_type = inferred_return_c_type_buf;
        }
    } else if (node->data.lambda_expr.body != NULL
               && node->data.lambda_expr.body->type == AST_BLOCK) {
        return_type = "void";
    } else if (node->data.lambda_expr.body != NULL) {
        const char *inferred_return_type =
            infer_expression_type_name(ctx, node->data.lambda_expr.body);
        if (inferred_return_type != NULL
            && pergyra_type_to_c_copy(inferred_return_type,
                inferred_return_c_type_buf,
                sizeof(inferred_return_c_type_buf))) {
            return_type = inferred_return_c_type_buf;
        }
    }
    if (return_type == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot determine lambda return type; explicit return type is required for non-block lambda bodies");
        ctx->typed_var_count = saved_typed_var_count;
        return pergyra_strdup("0");
    }
    return_type_owned = pergyra_strdup(return_type);
    if (return_type_owned == NULL) {
        ctx->typed_var_count = saved_typed_var_count;
        return pergyra_strdup("0");
    }
    return_type = return_type_owned;

    char *lambda_name = strdup_fmt("pgy_lambda_%d", lambda_id);
    if (lambda_name == NULL) {
        free(return_type_owned);
        ctx->typed_var_count = saved_typed_var_count;
        return pergyra_strdup("0");
    }

    if (!transpiler_emit_lambda_signature(node, ctx, ctx->decls,
            return_type, lambda_name, true)
        || !transpiler_emit_lambda_signature(node, ctx, ctx->helpers,
            return_type, lambda_name, false)) {
        free(lambda_name);
        free(return_type_owned);
        ctx->typed_var_count = saved_typed_var_count;
        return pergyra_strdup("0");
    }

    if (node->data.lambda_expr.body != NULL
        && node->data.lambda_expr.body->type == AST_BLOCK) {
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        emit_block(node->data.lambda_expr.body, ctx);
        ctx->indent = saved_indent;
        ctx->out = saved_out;
    } else if (node->data.lambda_expr.body != NULL) {
        char *expr = emit_expression(node->data.lambda_expr.body, ctx);
        write_indent_to(ctx->helpers, 1);
        codebuf_write(ctx->helpers, "return %s;\n", expr);
        free(expr);
    }

    codebuf_write(ctx->helpers, "}\n");
    free(return_type_owned);
    ctx->typed_var_count = saved_typed_var_count;
    return lambda_name;
}

#endif /* PGY_TRANSPILER_LAMBDA_EMIT_H */
