#ifndef PGY_TRANSPILER_DESTRUCTURE_EMIT_H
#define PGY_TRANSPILER_DESTRUCTURE_EMIT_H

static inline void
emit_let_destructure_statement(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *init = node->data.let_destructure.initializer;
    char *init_expr = emit_expression(init, ctx);
    const char *init_type = infer_expression_type_name(ctx, init);
    const char *c_init_type;
    int tmp_id;

    if ((init_type == NULL || strcmp(init_type, "Unknown") == 0)
        && init != NULL
        && init->type == AST_IDENTIFIER
        && init->data.identifier.name != NULL) {
        const char *resolved =
            transpiler_current_local_type_name(ctx, init->data.identifier.name);
        if (resolved != NULL)
            init_type = resolved;
    }
    c_init_type = pergyra_type_to_c(init_type);

    if (init_type != NULL && init_type[0] == '(') {
        char elem_names[8][64];
        size_t arity = 0;
        size_t i = 1;
        size_t n = strlen(init_type);

        while (i < n && init_type[i] != ')' && arity < 8) {
            size_t eo = 0;
            int depth = 0;

            while (i < n && (init_type[i] == ' ' || init_type[i] == '\t'))
                i++;
            while (i < n && eo + 1 < sizeof(elem_names[0])) {
                char c = init_type[i];
                if (depth == 0 && (c == ',' || c == ')'))
                    break;
                if (c == '<' || c == '(')
                    depth++;
                if (c == '>' || c == ')')
                    depth--;
                elem_names[arity][eo++] = c;
                i++;
            }
            elem_names[arity][eo] = '\0';
            while (eo > 0 && (elem_names[arity][eo - 1] == ' '
                           || elem_names[arity][eo - 1] == '\t')) {
                elem_names[arity][--eo] = '\0';
            }
            arity++;
            if (i < n && init_type[i] == ',')
                i++;
        }

        if (arity != node->data.let_destructure.name_count) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                    (unsigned long long)node->data.let_destructure.name_count,
                    (unsigned long long)arity);
            }
            free(init_expr);
            return;
        }

        tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t j = 0; j < arity; j++) {
            const char *e_ctype = pergyra_type_to_c(elem_names[j]);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.f%zu;\n",
                e_ctype != NULL ? e_ctype : elem_names[j],
                node->data.let_destructure.names[j], tmp_id, j);
            register_typed_var(ctx, node->data.let_destructure.names[j],
                elem_names[j]);
        }
        free(init_expr);
        return;
    }

    const char *elem_c_type = NULL;
    const char *inner = NULL;
    if (init_type != NULL
        && (strncmp(init_type, "Array<", 6) == 0
            || strncmp(init_type, "Slice<", 6) == 0)) {
        inner = slot_inner_type_name(init_type);
        elem_c_type = pergyra_type_to_c(inner);
    }
    if (c_init_type == NULL || elem_c_type == NULL || inner == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                init_type != NULL ? init_type : "(unknown)");
        }
        free(init_expr);
        return;
    }

    tmp_id = ++ctx->tmp_counter;
    write_indent(ctx);
    codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
        c_init_type, tmp_id, init_expr);
    for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.data[%zu];\n",
            elem_c_type, node->data.let_destructure.names[i], tmp_id, i);
        register_typed_var(ctx, node->data.let_destructure.names[i], inner);
    }
    free(init_expr);
}

#endif /* PGY_TRANSPILER_DESTRUCTURE_EMIT_H */
