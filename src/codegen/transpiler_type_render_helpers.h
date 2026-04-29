/* -----------------------------------------------------------------
 * Type-name rendering
 * ----------------------------------------------------------------- */

static void
append_type_name(CodeBuf *buf, ASTNode *type_node)
{
    if (type_node == NULL
        || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        codebuf_write(buf, "Int");
        return;
    }

    /* Tuple type: (T0, T1, ...) */
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
        if (g_type_render_ctx != NULL) {
            bound = lookup_generic_binding(g_type_render_ctx,
                                           type_node->data.type.name);
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

static char *
render_type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return pergyra_strdup("Int");

    if (type_node->type == AST_CHANNEL_TYPE) {
        char *inner = render_type_name(type_node->data.channel_type.element_type);
        char *result = strdup_fmt("Channel<%s>", inner);
        free(inner);
        return result;
    }

    if (type_node->type == AST_FUTURE_TYPE) {
        char *inner = render_type_name(type_node->data.future_type.value_type);
        char *result = strdup_fmt("Future<%s>", inner);
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

const char *
transpiler_render_type_name_local(TranspilerCtx *ctx, ASTNode *type_node)
{
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char *owned;
    char *stable;

    if (ctx != NULL)
        g_type_render_ctx = ctx;
    owned = render_type_name(type_node);
    g_type_render_ctx = saved_render_ctx;
    if (owned == NULL)
        return NULL;

    stable = ctx != NULL ? pgy_arena_strdup(&ctx->arena, owned) : NULL;
    free(owned);
    return stable;
}
