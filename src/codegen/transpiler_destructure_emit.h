#ifndef PGY_TRANSPILER_DESTRUCTURE_EMIT_H
#define PGY_TRANSPILER_DESTRUCTURE_EMIT_H

static inline void
emit_let_destructure_statement(ASTNode *node, TranspilerCtx *ctx)
{
    ASTNode *init = node->data.let_destructure.initializer;
    char *init_expr = emit_expression(init, ctx);
    const char *init_type = infer_expression_type_name(ctx, init);
    char c_init_type_buf[128];
    const char *c_init_type = NULL;
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
    if (pergyra_type_to_c_copy(init_type, c_init_type_buf,
            sizeof(c_init_type_buf))) {
        c_init_type = c_init_type_buf;
    }

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
            transpiler_set_mir_topology_invalid(
                ctx,
                "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                (unsigned long long)node->data.let_destructure.name_count,
                (unsigned long long)arity);
            free(init_expr);
            return;
        }

        tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t j = 0; j < arity; j++) {
            char e_ctype_buf[256];
            const char *e_ctype = NULL;
            if (pergyra_type_to_c_copy(elem_names[j], e_ctype_buf,
                    sizeof(e_ctype_buf))) {
                e_ctype = e_ctype_buf;
            }
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
    char inner_buf[128];
    char elem_c_type_buf[128];
    if (init_type != NULL
        && (strncmp(init_type, "Array<", 6) == 0
            || strncmp(init_type, "Slice<", 6) == 0)) {
        if (slot_inner_type_name_copy(init_type, inner_buf,
                sizeof(inner_buf)))
            inner = inner_buf;
        if (pergyra_type_to_c_copy(inner, elem_c_type_buf,
                sizeof(elem_c_type_buf))) {
            elem_c_type = elem_c_type_buf;
        }
    }
    if (c_init_type == NULL || elem_c_type == NULL || inner == NULL) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot lower destructuring initializer of type '%s' to a concrete array element type",
            init_type != NULL ? init_type : "(unknown)");
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
