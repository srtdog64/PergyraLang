#include "transpiler_type_render.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"

static TranspilerCtx *g_type_render_ctx = NULL;

static const char *
transpiler_type_render_lookup_generic_binding(TranspilerCtx *ctx,
                                              const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
        if (strcmp(ctx->generic_bindings[i].name, name) == 0)
            return ctx->generic_bindings[i].concrete_type;
    }

    return NULL;
}

static char *
transpiler_type_render_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0)
        return pergyra_strdup("");

    s = malloc((size_t)n + 1);
    if (s == NULL)
        return pergyra_strdup("");

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

TranspilerCtx *
transpiler_type_render_ctx_current(void)
{
    return g_type_render_ctx;
}

void
transpiler_type_render_ctx_bind(TranspilerCtx *ctx)
{
    if (ctx != NULL)
        g_type_render_ctx = ctx;
}

TranspilerCtx *
transpiler_type_render_ctx_push(TranspilerCtx *ctx)
{
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    transpiler_type_render_ctx_bind(ctx);
    return saved_render_ctx;
}

void
transpiler_type_render_ctx_restore(TranspilerCtx *saved)
{
    g_type_render_ctx = saved;
}

static void
append_type_name(CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || ast_type_name(type_node) == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    if (ast_type_tuple_element_count(type_node) > 0) {
        codebuf_write(buf, "(");
        size_t element_count = ast_type_tuple_element_count(type_node);
        for (size_t i = 0; i < element_count; i++) {
            if (i > 0)
                codebuf_write(buf, ", ");
            append_type_name(buf, ast_type_tuple_element(type_node, i));
        }
        codebuf_write(buf, ")");
        return;
    }

    {
        const char *bound = NULL;
        TranspilerCtx *render_ctx = transpiler_type_render_ctx_current();
        if (render_ctx != NULL) {
            bound = transpiler_type_render_lookup_generic_binding(
                render_ctx, ast_type_name(type_node));
        }
        codebuf_write(buf, "%s",
                      bound != NULL ? bound : ast_type_name(type_node));
    }
    {
        GenericParams *generic_args = ast_type_generic_args(type_node);
        size_t generic_count = ast_generic_param_count(generic_args);

        if (generic_count == 0)
            return;

        codebuf_write(buf, "<");
        for (size_t i = 0; i < generic_count; i++) {
            GenericParam *param = ast_generic_param_at(generic_args, i);
            if (i > 0)
                codebuf_write(buf, ", ");
            if (ast_generic_param_constraint(param) != NULL) {
                append_type_name(buf, ast_generic_param_constraint(param));
            } else if (ast_generic_param_name(param) != NULL) {
                codebuf_write(buf, "%s", ast_generic_param_name(param));
            } else {
                codebuf_write(buf, "Int");
            }
        }
        codebuf_write(buf, ">");
    }
}

char *
render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name(ast_channel_type_element_type(type_node));
        char *result = transpiler_type_render_strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(ast_future_type_value_type(type_node));
        char *result = transpiler_type_render_strdup_fmt("Future<%s>", inner);
        free(inner);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    if (buf == NULL)
        return pergyra_strdup("Int");
    append_type_name(buf, type_node);
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

char *
render_type_name_in_ctx(TranspilerCtx *ctx, ASTNode *type_node)
{
    TranspilerCtx *saved_render_ctx =
        transpiler_type_render_ctx_push(ctx);
    char *result = render_type_name(type_node);
    transpiler_type_render_ctx_restore(saved_render_ctx);
    return result;
}

const char *
transpiler_render_type_name_local(TranspilerCtx *ctx, ASTNode *type_node)
{
    char *owned;
    char *stable;

    owned = render_type_name_in_ctx(ctx, type_node);
    if (owned == NULL)
        return NULL;

    stable = ctx != NULL ? pgy_arena_strdup(&ctx->arena, owned) : NULL;
    free(owned);
    return stable;
}
