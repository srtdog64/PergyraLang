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
        || type_node->data.type.name == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    if (type_node->data.type.tuple_elements != NULL
        && type_node->data.type.tuple_element_count > 0) {
        codebuf_write(buf, "(");
        for (size_t i = 0; i < type_node->data.type.tuple_element_count; i++) {
            if (i > 0)
                codebuf_write(buf, ", ");
            append_type_name(buf, type_node->data.type.tuple_elements[i]);
        }
        codebuf_write(buf, ")");
        return;
    }

    {
        const char *bound = NULL;
        TranspilerCtx *render_ctx = transpiler_type_render_ctx_current();
        if (render_ctx != NULL) {
            bound = transpiler_type_render_lookup_generic_binding(
                render_ctx, type_node->data.type.name);
        }
        codebuf_write(buf, "%s",
                      bound != NULL ? bound : type_node->data.type.name);
    }
    if (type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0) {
        codebuf_write(buf, "<");
        for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
            GenericParam *param = type_node->data.type.generic_args->params[i];
            if (i > 0)
                codebuf_write(buf, ", ");
            if (param != NULL && param->constraint != NULL) {
                append_type_name(buf, param->constraint);
            } else if (param != NULL && param->name != NULL) {
                codebuf_write(buf, "%s", param->name);
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
        char *inner = render_type_name(type_node->data.channel_type.element_type);
        char *result = transpiler_type_render_strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(type_node->data.future_type.value_type);
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
