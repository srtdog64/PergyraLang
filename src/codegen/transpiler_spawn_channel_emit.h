#ifndef PGY_TRANSPILER_SPAWN_CHANNEL_EMIT_H
#define PGY_TRANSPILER_SPAWN_CHANNEL_EMIT_H

#include "../parser/ast_api.h"

char *
emit_spawn_expr(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *target = ast_spawn_function(node);
    ASTNode *call = NULL;
    ASTNode *callee = NULL;
    const char *function_name = NULL;
    const char *emitted_function_name = NULL;
    ASTNode *decl = NULL;
    size_t arg_count = 0;
    int wrapper_id = ++ctx->tmp_counter;
    char *wrapper_name = strdup_fmt("pgy_spawn_wrapper_%d", wrapper_id);
    char *args_type_name = NULL;
    char *return_type_name = infer_spawn_return_type_name(ctx, node);
    char return_c_type_buf[256];
    char *return_c_type = NULL;
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (pergyra_type_to_c_copy(return_type_name, return_c_type_buf,
            sizeof(return_c_type_buf))) {
        return_c_type = pergyra_strdup(return_c_type_buf);
    }

    if (target == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C spawn expression requires a target expression");
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    if (target->type == AST_CALL) {
        call = target;
        callee = ast_call_callee(target);
        arg_count = ast_call_arg_count(target);
    } else {
        callee = target;
    }

    if (callee != NULL && callee->type == AST_IDENTIFIER)
        function_name = ast_identifier_name(callee);
    if (function_name == NULL) {
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "C backend: unsupported spawn target at line %d", target->line);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }
    if (return_type_name == NULL || return_type_name[0] == '\0'
        || strcmp(return_type_name, "Unknown") == 0
        || return_c_type == NULL || return_c_type[0] == '\0'
        || strcmp(return_c_type, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C spawn expression requires concrete Future<T> return metadata for '%s'",
            function_name);
        free(wrapper_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    decl = find_function_decl(ctx, function_name);
    emitted_function_name = function_name;
    if (call != NULL && func_has_generic_params(decl)
        && infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count)) {
        const char *specialized = ensure_generic_specialization(ctx, decl, call);
        if (specialized != NULL)
            emitted_function_name = specialized;
    }
    if (arg_count > 0)
        args_type_name = strdup_fmt("PgySpawnArgs_%d", wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            char arg_type_buf[256];
            const char *arg_type = NULL;
            FuncParam *param = ast_func_param(decl, i);
            if (param != NULL && param->type != NULL) {
                if (binding_count > 0) {
                    char *bound_type = render_type_name_with_bindings(ctx,
                        param->type, bindings, binding_count);
                    if (pergyra_type_to_c_copy(bound_type, arg_type_buf,
                            sizeof(arg_type_buf))) {
                        arg_type = arg_type_buf;
                    }
                    if (bound_type == NULL || bound_type[0] == '\0'
                        || strcmp(bound_type, "Unknown") == 0
                        || arg_type == NULL || arg_type[0] == '\0'
                        || strcmp(arg_type, "Unknown") == 0) {
                        transpiler_set_backend_error_with_hints(ctx,
                            PGY_CODE_C_TYPE_UNSUPPORTED,
                            PGY_CAUSE_C_TYPE_UNSUPPORTED,
                            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                            "C spawn wrapper argument %llu requires concrete parameter metadata for call '%s'",
                            (unsigned long long)i,
                            function_name != NULL ? function_name : "<function>");
                        free(bound_type);
                        free(args_type_name);
                        free(wrapper_name);
                        free(return_type_name);
                        free(return_c_type);
                        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
                    }
                    codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
                    free(bound_type);
                    continue;
                }
                if (pergyra_ast_type_to_c_copy(
                        param->type,
                        arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            } else if (call != NULL) {
                const char *inferred_arg_type = infer_expression_type_name(
                    ctx, ast_call_argument(call, i));
                if (pergyra_type_to_c_copy(inferred_arg_type, arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            }
            if (arg_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine spawn wrapper argument type for call '%s' at argument %llu",
                    function_name != NULL ? function_name : "<function>",
                    (unsigned long long) i);
                free(args_type_name);
                free(wrapper_name);
                free(return_type_name);
                free(return_c_type);
                return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
            }
            if (arg_type[0] == '\0' || strcmp(arg_type, "Unknown") == 0) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C spawn wrapper argument %llu requires concrete C type metadata for call '%s'",
                    (unsigned long long)i,
                    function_name != NULL ? function_name : "<function>");
                free(args_type_name);
                free(wrapper_name);
                free(return_type_name);
                free(return_c_type);
                return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
            }
            codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
        }
        codebuf_write(ctx->decls, "} %s;\n", args_type_name);
    }

    codebuf_write(ctx->decls, "static void *%s(void *raw);\n", wrapper_name);
    codebuf_write(ctx->helpers, "\nstatic void *%s(void *raw)\n{\n", wrapper_name);
    if (args_type_name != NULL) {
        codebuf_write(ctx->helpers, "    %s *args = (%s *)raw;\n",
            args_type_name, args_type_name);
    } else {
        codebuf_write(ctx->helpers, "    (void)raw;\n");
    }

    if (strcmp(return_type_name, "Void") == 0) {
        codebuf_write(ctx->helpers, "    %s(", emitted_function_name);
    } else {
        codebuf_write(ctx->helpers,
            "    %s *result = (%s *)malloc(sizeof(%s));\n",
            return_c_type, return_c_type, return_c_type);
        codebuf_write(ctx->helpers,
            "    if (result == NULL) {\n"
            "        PGY_PANIC(\"spawn result allocation failed\");\n"
            "    }\n"
            "    *result = %s(",
            emitted_function_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0)
            codebuf_write(ctx->helpers, ", ");
        codebuf_write(ctx->helpers, "args->arg%zu", i);
    }
    codebuf_write(ctx->helpers, ");\n");

    if (args_type_name != NULL)
        codebuf_write(ctx->helpers, "    free(args);\n");

    if (strcmp(return_type_name, "Void") == 0)
        codebuf_write(ctx->helpers, "    return NULL;\n");
    else
        codebuf_write(ctx->helpers, "    return result;\n");
    codebuf_write(ctx->helpers, "}\n");

    CodeBuf *expr = codebuf_create();
    if (expr == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C spawn expression could not allocate wrapper call buffer");
        free(wrapper_name);
        free(args_type_name);
        free(return_type_name);
        free(return_c_type);
        return pergyra_strdup("/* spawn alloc failed */");
    }

    {
        const char *spawn_fn = ast_spawn_is_blocking(node)
            ? "pgy_spawn_blocking" : "pgy_async_spawn";
        if (args_type_name == NULL) {
            codebuf_write(expr, "%s(%s, NULL)", spawn_fn, wrapper_name);
        } else {
            codebuf_write(expr,
                "({ %s *_pgy_args = (%s *)malloc(sizeof(%s)); "
                "if (_pgy_args == NULL) { PGY_PANIC(\"spawn arg allocation failed\"); } ",
                args_type_name, args_type_name, args_type_name);
            for (size_t i = 0; i < arg_count; i++) {
                char *arg = emit_expression(ast_call_argument(call, i), ctx);
                codebuf_write(expr, "_pgy_args->arg%zu = %s; ", i, arg);
                free(arg);
            }
            codebuf_write(expr, "%s(%s, _pgy_args); })", spawn_fn, wrapper_name);
        }
    }

    char *result = pergyra_strdup(expr->data);
    codebuf_destroy(expr);
    free(wrapper_name);
    free(args_type_name);
    free(return_type_name);
    free(return_c_type);
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *channel = ast_channel_send_channel(node);
    ASTNode *value = ast_channel_send_value(node);
    char *ch  = emit_expression(channel, ctx);
    char *val = emit_expression(value, ctx);
    const char *inner = NULL;
    char inner_buf[128];

    if (channel != NULL
        && channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            ast_identifier_name(channel));
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            if (slot_inner_type_name_copy(type_name, inner_buf,
                    sizeof(inner_buf)))
                inner = inner_buf;
    }
    if (inner == NULL || inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: channel send requires concrete Channel<T> metadata");
        free(ch);
        free(val);
        return pergyra_strdup("0");
    }
    if (strcmp(inner, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: channel send requires concrete Channel<T> payload metadata");
        free(ch);
        free(val);
        return pergyra_strdup("0");
    }

    char *result = strdup_fmt("pgy_channel_send_%s(&%s, %s)", inner, ch, val);
    free(ch);
    free(val);
    return result;
}

char *
emit_channel_recv(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *channel = ast_channel_recv_channel(node);
    char *ch = emit_expression(channel, ctx);
    const char *inner = NULL;
    char inner_buf[128];

    if (channel != NULL
        && channel->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx,
            ast_identifier_name(channel));
        if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
            if (slot_inner_type_name_copy(type_name, inner_buf,
                    sizeof(inner_buf)))
                inner = inner_buf;
    }
    if (inner == NULL || inner[0] == '\0') {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: channel receive requires concrete Channel<T> metadata");
        free(ch);
        return pergyra_strdup("0");
    }
    if (strcmp(inner, "Unknown") == 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C backend: channel receive requires concrete Channel<T> payload metadata");
        free(ch);
        return pergyra_strdup("0");
    }

    char *result = strdup_fmt("pgy_channel_recv_val_%s(&%s)", inner, ch);
    free(ch);
    return result;
}

#endif /* PGY_TRANSPILER_SPAWN_CHANNEL_EMIT_H */
