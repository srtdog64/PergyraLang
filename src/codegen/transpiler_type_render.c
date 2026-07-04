#include "transpiler_type_render.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "codegen_type_mapping.h"

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
transpiler_type_render_strdup_fmt(TranspilerCtx *ctx, const char *fmt, ...)
{
    va_list ap;
    int n;
    char *s;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: type-name formatting failed");
        return NULL;
    }

    s = malloc((size_t)n + 1);
    if (s == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: type-name allocation failed");
        return NULL;
    }

    va_start(ap, fmt);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

static bool
append_type_name_in_ctx(TranspilerCtx *ctx, CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || ast_type_name(type_node) == NULL)
        return false;

    if (ast_type_tuple_element_count(type_node) > 0) {
        codebuf_write(buf, "(");
        size_t element_count = ast_type_tuple_element_count(type_node);
        for (size_t i = 0; i < element_count; i++) {
            if (i > 0)
                codebuf_write(buf, ", ");
            if (!append_type_name_in_ctx(ctx, buf,
                    ast_type_tuple_element(type_node, i)))
                return false;
        }
        codebuf_write(buf, ")");
        return true;
    }

    {
        const char *bound = NULL;
        if (ctx != NULL) {
            bound = transpiler_type_render_lookup_generic_binding(
                ctx, ast_type_name(type_node));
        }
        codebuf_write(buf, "%s",
                      bound != NULL ? bound : ast_type_name(type_node));
    }
    {
        GenericParams *generic_args = ast_type_generic_args(type_node);
        size_t generic_count = ast_generic_param_count(generic_args);

        if (generic_count == 0)
            return true;

        codebuf_write(buf, "<");
        for (size_t i = 0; i < generic_count; i++) {
            GenericParam *param = ast_generic_param_at(generic_args, i);
            if (i > 0)
                codebuf_write(buf, ", ");
            if (ast_generic_param_constraint(param) != NULL) {
                if (!append_type_name_in_ctx(ctx, buf,
                        ast_generic_param_constraint(param)))
                    return false;
            } else if (ast_generic_param_name(param) != NULL) {
                const char *arg_name = ast_generic_param_name(param);
                const char *arg_bound = ctx != NULL
                    ? transpiler_type_render_lookup_generic_binding(
                          ctx, arg_name)
                    : NULL;
                codebuf_write(buf, "%s",
                    arg_bound != NULL ? arg_bound : arg_name);
            } else {
                return false;
            }
        }
        codebuf_write(buf, ">");
    }
    return true;
}

char *
render_type_name(ASTNode *type_node)
{
    return render_type_name_in_ctx(NULL, type_node);
}

char *
render_type_name_in_ctx(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (type_node == NULL)
        return NULL;

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name_in_ctx(
            ctx, ast_channel_type_element_type(type_node));
        if (inner == NULL)
            return NULL;
        char *result = transpiler_type_render_strdup_fmt(ctx,
            "Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name_in_ctx(
            ctx, ast_future_type_value_type(type_node));
        if (inner == NULL)
            return NULL;
        char *result = transpiler_type_render_strdup_fmt(ctx,
            "Future<%s>", inner);
        free(inner);
        return result;
    }

    CodeBuf *buf = codebuf_create();
    if (buf == NULL)
        return NULL;
    if (!append_type_name_in_ctx(ctx, buf, type_node)) {
        codebuf_destroy(buf);
        return NULL;
    }
    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
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

bool
pergyra_ast_type_to_c_copy(ASTNode *type_node, char *out, size_t out_size)
{
    return pergyra_ast_type_to_c_copy_in_ctx(NULL, type_node, out, out_size);
}

bool
pergyra_ast_type_to_c_copy_in_ctx(TranspilerCtx *ctx,
                                  ASTNode *type_node,
                                  char *out,
                                  size_t out_size)
{
    char *type_name;
    bool ok;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (type_node == NULL)
        return pergyra_str_copy(out, out_size, "void");

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend function type requires declarator owner, not raw C type copy");
        return false;
    }

    type_name = render_type_name_in_ctx(ctx, type_node);
    ok = pergyra_type_to_c_copy(type_name, out, out_size);
    free(type_name);
    if (!ok)
        out[0] = '\0';
    return ok;
}
