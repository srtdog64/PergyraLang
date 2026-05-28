#include "transpiler_spawn_channel_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

#include "transpiler_context.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_future_type_query.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_generic_specialization_emit.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

static char *
emit_channel_endpoint_lvalue(ASTNode *channel, TranspilerCtx *ctx)
{
    const void *saved_active_ssa_map = NULL;
    char *result = NULL;

    if (ctx == NULL)
        return emit_expression(channel, ctx);

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = NULL;
    result = emit_expression(channel, ctx);
    ctx->active_ssa_map = saved_active_ssa_map;
    return result;
}

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
    const char *wrapper_name = transpiler_scratch_fmt(
        ctx, "pgy_spawn_wrapper_%d", wrapper_id);
    const char *args_type_name = NULL;
    const char *return_type_name = infer_spawn_return_type_name_scratch(
        ctx, node);
    char return_c_type_buf[256];
    const char *return_c_type = NULL;
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (transpiler_require_type_name_c_type_copy(ctx, return_type_name,
            "spawn return metadata", return_c_type_buf,
            sizeof(return_c_type_buf))) {
        return_c_type = return_c_type_buf;
    }

    if (target == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "C spawn expression requires a target expression");
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
        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
    }

    decl = find_function_decl(ctx, function_name);
    emitted_function_name = function_name;
    if (call != NULL && transpiler_func_has_generic_params(decl)
        && transpiler_infer_generic_call_bindings(ctx, decl, call, bindings,
            &binding_count)) {
        const char *specialized = ensure_generic_specialization(ctx, decl, call);
        if (specialized != NULL)
            emitted_function_name = specialized;
    }
    if (arg_count > 0)
        args_type_name = transpiler_scratch_fmt(ctx,
                                                "PgySpawnArgs_%d",
                                                wrapper_id);

    if (args_type_name != NULL) {
        codebuf_write(ctx->decls, "\ntypedef struct {\n");
        for (size_t i = 0; i < arg_count; i++) {
            char arg_type_buf[256];
            const char *arg_type = NULL;
            FuncParam *param = ast_func_param(decl, i);
            if (param != NULL && param->type != NULL) {
                if (binding_count > 0) {
                    char *bound_type =
                        transpiler_render_type_name_with_bindings(ctx,
                        param->type, bindings, binding_count);
                    if (bound_type == NULL || bound_type[0] == '\0'
                        || strcmp(bound_type, "Unknown") == 0
                        || !transpiler_require_type_name_c_type_copy(ctx,
                            bound_type,
                            "spawn wrapper generic argument",
                            arg_type_buf,
                            sizeof(arg_type_buf))) {
                        transpiler_set_backend_error_with_hints(ctx,
                            PGY_CODE_C_TYPE_UNSUPPORTED,
                            PGY_CAUSE_C_TYPE_UNSUPPORTED,
                            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                            "C spawn wrapper argument %llu requires concrete parameter metadata for call '%s'",
                            (unsigned long long)i,
                            function_name != NULL ? function_name : "<function>");
                        free(bound_type);
                        return pergyra_strdup("pgy_async_spawn(NULL, NULL)");
                    }
                    arg_type = arg_type_buf;
                    codebuf_write(ctx->decls, "    %s arg%zu;\n", arg_type, i);
                    free(bound_type);
                    continue;
                }
                if (pergyra_ast_type_to_c_copy_in_ctx(ctx,
                        param->type,
                        arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            } else if (call != NULL) {
                const char *inferred_arg_type = infer_expression_type_name(
                    ctx, ast_call_argument(call, i));
                if (inferred_arg_type != NULL
                    && transpiler_require_type_name_c_type_copy(ctx,
                        inferred_arg_type,
                        "spawn wrapper inferred argument",
                        arg_type_buf,
                        sizeof(arg_type_buf))) {
                    arg_type = arg_type_buf;
                }
            }
            if (arg_type == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot determine spawn wrapper argument type for call '%s' at argument %llu",
                    function_name != NULL ? function_name : "<function>",
                    (unsigned long long) i);
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
    return result;
}

char *
emit_channel_send(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *channel = ast_channel_send_channel(node);
    ASTNode *value = ast_channel_send_value(node);
    if (!transpiler_channel_expr_is_c_lvalue(channel)) {
        transpiler_set_channel_lvalue_error(ctx, "channel send");
        return pergyra_strdup("0");
    }

    char *ch  = emit_channel_endpoint_lvalue(channel, ctx);
    char *val = emit_expression(value, ctx);
    char inner_buf[128];
    const char *inner = transpiler_require_channel_inner_type(
        ctx, channel, "channel send", inner_buf, sizeof(inner_buf));

    if (inner == NULL) {
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
    if (!transpiler_channel_expr_is_c_lvalue(channel)) {
        transpiler_set_channel_lvalue_error(ctx, "channel receive");
        return pergyra_strdup("0");
    }

    char *ch = emit_channel_endpoint_lvalue(channel, ctx);
    char inner_buf[128];
    const char *inner = transpiler_require_channel_inner_type(
        ctx, channel, "channel receive", inner_buf, sizeof(inner_buf));

    if (inner == NULL) {
        free(ch);
        return pergyra_strdup("0");
    }

    char *result = strdup_fmt("pgy_channel_recv_val_%s(&%s)", inner, ch);
    free(ch);
    return result;
}
