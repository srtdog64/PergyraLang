/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

static bool
transpiler_condition_is_already_parenthesized(const char *expr)
{
    size_t len;

    if (expr == NULL)
        return false;
    len = strlen(expr);
    return len >= 2 && expr[0] == '(' && expr[len - 1] == ')';
}

static void
transpiler_write_condition_head(TranspilerCtx *ctx,
                                const char *keyword,
                                const char *expr,
                                const char *suffix)
{
    const char *safe_expr = expr != NULL ? expr : "false";
    const char *safe_suffix = suffix != NULL ? suffix : "";

    if (transpiler_condition_is_already_parenthesized(safe_expr)) {
        codebuf_write(ctx->out, "%s %s%s", keyword, safe_expr, safe_suffix);
        return;
    }

    codebuf_write(ctx->out, "%s (%s)%s", keyword, safe_expr, safe_suffix);
}

void
emit_if_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.if_stmt.condition, ctx);
    write_indent(ctx);
    transpiler_write_condition_head(ctx, "if", cond, "\n");
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.if_stmt.then_branch != NULL)
        emit_block(node->data.if_stmt.then_branch, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (node->data.if_stmt.else_branch != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_statement(node->data.if_stmt.else_branch, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

static int
transpiler_find_loop_label_depth(const TranspilerCtx *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    const char *var = node->data.for_loop.variable;
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;

    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = node->data.for_loop.label;
        snprintf(ctx->loop_break_labels[loop_slot],
                 sizeof(ctx->loop_break_labels[loop_slot]),
                 "_pgy_loop_break_%d", loop_id);
        snprintf(ctx->loop_continue_labels[loop_slot],
                 sizeof(ctx->loop_continue_labels[loop_slot]),
                 "_pgy_loop_continue_%d", loop_id);
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_defer_base_depth[loop_slot] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }

    if (node->data.for_loop.iterable != NULL) {
        char *coll = emit_expression(node->data.for_loop.iterable, ctx);
        const char *coll_type = infer_expression_type_name(ctx,
            node->data.for_loop.iterable);
        const char *elem_type = NULL;
        const char *length_field = "count";
        if (coll_type != NULL
            && (strncmp(coll_type, "Array<", 6) == 0
                || strncmp(coll_type, "Slice<", 6) == 0)) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
            length_field = "length";
        } else if (coll_type != NULL && strncmp(coll_type, "List<", 5) == 0) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
            length_field = "count";
        }
        if (elem_type == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot derive concrete element type for for-in iterable '%s'",
                    coll_type != NULL ? coll_type : "(unknown)");
            }
            free(coll);
            return;
        }

        int idx_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pgy_idx_%d = 0; "
            "_pgy_idx_%d < %s.%s; "
            "_pgy_idx_%d++)\n",
            idx_id, idx_id, coll, length_field, idx_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s.data[_pgy_idx_%d];\n",
            elem_type, var, coll, idx_id);
        register_typed_var(ctx, var, slot_inner_type_name(coll_type));
        if (node->data.for_loop.body != NULL)
            emit_block(node->data.for_loop.body, ctx);
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_continue_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_continue_labels[loop_slot]);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_break_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_break_labels[loop_slot]);
        }
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
            ctx->loop_depth--;
            ctx->loop_labels[loop_slot] = NULL;
        }
        free(coll);
        return;
    }

    char *start = emit_expression(node->data.for_loop.range_start, ctx);
    char *end   = emit_expression(node->data.for_loop.range_end,   ctx);

    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (int32_t %s = %s; %s < %s; %s++)\n",
        var, start, var, end, var);
    free(start);
    free(end);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.for_loop.body != NULL)
        emit_block(node->data.for_loop.body, ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.while_loop.condition, ctx);
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = node->data.while_loop.label;
        snprintf(ctx->loop_break_labels[loop_slot],
                 sizeof(ctx->loop_break_labels[loop_slot]),
                 "_pgy_loop_break_%d", loop_id);
        snprintf(ctx->loop_continue_labels[loop_slot],
                 sizeof(ctx->loop_continue_labels[loop_slot]),
                 "_pgy_loop_continue_%d", loop_id);
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_defer_base_depth[loop_slot] = ctx->defer_scope_depth;
        ctx->loop_depth++;
    }
    write_indent(ctx);
    transpiler_write_condition_head(ctx, "while", cond, "\n");
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.while_loop.body != NULL)
        emit_block(node->data.while_loop.body, ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}
