#ifndef PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H
#define PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H

static bool
transpiler_capture_surface_desc(char *out, size_t out_size,
                                const char *kind,
                                const char *capture_name)
{
    int written;

    if (out == NULL || out_size == 0 || kind == NULL)
        return false;

    written = snprintf(out, out_size, "%s capture '%s'",
        kind,
        capture_name != NULL ? capture_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_capture_surface_desc_too_long(TranspilerCtx *ctx,
                                         const char *kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s capture diagnostic surface is too long for C backend emission",
        kind != NULL ? kind : "parallel/async");
}

void
emit_parallel_block(ASTNode *node, TranspilerCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    unsigned int pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * 1) Generate a context struct that holds pointers to all local
     *    variables currently in scope (slots + non-duplicate typed vars).
     *    Wrapper functions access outer variables through this struct.
     * --------------------------------------------------------------- */
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    for (size_t i = 0; i < count; i++) {
        transpiler_parallel_collect_stmt_captures(node->data.parallel.tasks[i], ctx,
            capture_slot_names, &capture_slot_count,
            capture_typed_names, &capture_typed_count);
    }

    bool has_captures = (capture_slot_count > 0 || capture_typed_count > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers,
            "typedef struct {\n");
        for (int i = 0; i < capture_slot_count; i++) {
            const char *name = capture_slot_names[i];
            const char *inner = lookup_slot_type(ctx, name);
            bool secure = lookup_slot_is_secure(ctx, name);
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "parallel capture '%s' requires concrete Slot<T> metadata",
                    name);
                return;
            }
            codebuf_write(ctx->helpers,
                secure ? "    PgySecureSlot_%s *%s;\n" : "    PgySlot_%s *%s;\n",
                inner, name);
        }
        for (int i = 0; i < capture_typed_count; i++) {
            TypedVarEntry *entry = lookup_typed_entry(ctx, capture_typed_names[i]);
            char surface_desc[256];
            const char *c_type;
            const char *type_name = entry != NULL ? entry->type_name : NULL;
            if ((type_name == NULL || strcmp(type_name, "Unknown") == 0)
                && ctx->current_func_decl != NULL) {
                type_name = transpiler_find_local_type_name(ctx,
                                                            ctx->current_func_decl,
                                                            capture_typed_names[i]);
                if (type_name != NULL && type_name[0] != '\0'
                    && strcmp(type_name, "Unknown") != 0) {
                    register_typed_var(ctx, capture_typed_names[i], type_name);
                    entry = lookup_typed_entry(ctx, capture_typed_names[i]);
                }
            }
            if (!transpiler_capture_surface_desc(surface_desc,
                    sizeof(surface_desc), "parallel",
                    capture_typed_names[i])) {
                transpiler_capture_surface_desc_too_long(ctx, "parallel");
                return;
            }

            /* Function-typed locals need a pointer-to-function-pointer
             * declarator so `(*_pctx->name)` inside the wrapper yields the
             * function pointer (not a primitive deref). */
            ASTNode *local_type_node = ctx->current_func_decl != NULL
                ? transpiler_find_local_let_type_node(
                      ctx->current_func_decl->data.func_decl.body,
                      capture_typed_names[i])
                : NULL;
            if (local_type_node != NULL
                && local_type_node->type == AST_EVENT_HANDLER_TYPE) {
                char ptr_name[sizeof(capture_typed_names[i]) + 1];
                ptr_name[0] = '*';
                memcpy(ptr_name + 1, capture_typed_names[i],
                       sizeof(capture_typed_names[i]));
                ptr_name[sizeof(ptr_name) - 1] = '\0';
                char *decl = pergyra_ast_typed_declarator(
                    local_type_node, ptr_name);
                if (decl != NULL) {
                    codebuf_write(ctx->helpers, "    %s;\n", decl);
                    free(decl);
                    continue;
                }
            }

            c_type = transpiler_require_type_name_c_type(
                ctx,
                type_name,
                surface_desc);
            if (c_type == NULL)
                return;
            codebuf_write(ctx->helpers,
                "    %s *%s;\n", c_type,
                capture_typed_names[i]);
        }
        codebuf_write(ctx->helpers,
            "} _pgy_par_ctx_%u;\n\n", pid);
    }

    /* ---------------------------------------------------------------
     * 2) Generate static wrapper functions for each task.
     *    Variable references inside the wrapper go through _pctx->.
     * --------------------------------------------------------------- */
    for (size_t i = 0; i < count; i++) {
        codebuf_write(ctx->helpers,
            "static void *_pgy_par_%zu_%u(void *_arg) {\n",
            i, pid);
        if (has_captures) {
            codebuf_write(ctx->helpers,
                "    _pgy_par_ctx_%u *_pctx = "
                "(_pgy_par_ctx_%u *)_arg;\n",
                pid, pid);
        } else {
            codebuf_write(ctx->helpers, "    (void)_arg;\n");
        }

        /* Redirect output to helpers and set parallel-capture mode */
        CodeBuf *saved = ctx->out;
        int saved_indent = ctx->indent;
        bool saved_in_pw = ctx->in_parallel_wrapper;
        int saved_slot_count  = ctx->par_capture_slot_count;
        int saved_typed_count = ctx->par_capture_typed_count;
        char saved_slot_names[MAX_SLOT_VARS][64];
        char saved_typed_names[MAX_SLOT_VARS][64];

        ctx->out = ctx->helpers;
        ctx->indent = 1;
        ctx->in_parallel_wrapper  = true;
        memcpy(saved_slot_names, ctx->par_capture_slot_names, sizeof(saved_slot_names));
        memcpy(saved_typed_names, ctx->par_capture_typed_names, sizeof(saved_typed_names));
        memcpy(ctx->par_capture_slot_names, capture_slot_names, sizeof(capture_slot_names));
        memcpy(ctx->par_capture_typed_names, capture_typed_names, sizeof(capture_typed_names));
        ctx->par_capture_slot_count = capture_slot_count;
        ctx->par_capture_typed_count = capture_typed_count;

        emit_statement(node->data.parallel.tasks[i], ctx);

        ctx->out = saved;
        ctx->indent = saved_indent;
        ctx->in_parallel_wrapper  = saved_in_pw;
        memcpy(ctx->par_capture_slot_names, saved_slot_names, sizeof(saved_slot_names));
        memcpy(ctx->par_capture_typed_names, saved_typed_names, sizeof(saved_typed_names));
        ctx->par_capture_slot_count = saved_slot_count;
        ctx->par_capture_typed_count = saved_typed_count;

        codebuf_write(ctx->helpers,
            "    return NULL;\n"
            "}\n\n");
    }

    /* ---------------------------------------------------------------
     * 3) Emit context initialization + spawn + await at call site.
     * --------------------------------------------------------------- */
    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_par_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < capture_slot_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            const char *ssa_name = transpiler_resolve_active_ssa_name(
                ctx, capture_slot_names[i]);
            if (ssa_name != NULL) {
                char *c_name = transpiler_make_c_ssa_name(ctx, ssa_name);
                codebuf_write(ctx->out, "&%s",
                    c_name != NULL ? c_name : capture_slot_names[i]);
                free(c_name);
            } else {
                codebuf_write(ctx->out, "&%s", capture_slot_names[i]);
            }
            first = false;
        }
        for (int i = 0; i < capture_typed_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            const char *ssa_name = transpiler_resolve_active_ssa_name(
                ctx, capture_typed_names[i]);
            if (ssa_name != NULL) {
                char *c_name = transpiler_make_c_ssa_name(ctx, ssa_name);
                codebuf_write(ctx->out, "&%s",
                    c_name != NULL ? c_name : capture_typed_names[i]);
                free(c_name);
            } else {
                codebuf_write(ctx->out, "&%s", capture_typed_names[i]);
            }
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }

    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        if (has_captures) {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, &_pctx%u);\n",
                i, i, pid, pid);
        } else {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, NULL);\n",
                i, i, pid);
        }
    }
    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_await(_ph_%zu);\n", i);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

static void
emit_async_block(ASTNode *node, TranspilerCtx *ctx)
{
    unsigned int pid = ctx->parallel_id++;
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
        transpiler_parallel_collect_stmt_captures(node->data.async_block.statements[i], ctx,
            capture_slot_names, &capture_slot_count,
            capture_typed_names, &capture_typed_count);
    }

    bool has_captures = (capture_slot_count > 0 || capture_typed_count > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers, "typedef struct {\n");
        for (int i = 0; i < capture_slot_count; i++) {
            const char *name = capture_slot_names[i];
            const char *inner = lookup_slot_type(ctx, name);
            bool secure = lookup_slot_is_secure(ctx, name);
            if (inner == NULL || inner[0] == '\0') {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "async capture '%s' requires concrete Slot<T> metadata",
                    name);
                return;
            }
            codebuf_write(ctx->helpers, secure
                ? "    PgySecureSlot_%s *%s;\n"
                : "    PgySlot_%s *%s;\n",
                inner, name);
        }
        for (int i = 0; i < capture_typed_count; i++) {
            TypedVarEntry *entry = lookup_typed_entry(ctx, capture_typed_names[i]);
            char surface_desc[256];
            const char *c_type;
            if (!transpiler_capture_surface_desc(surface_desc,
                    sizeof(surface_desc), "async",
                    capture_typed_names[i])) {
                transpiler_capture_surface_desc_too_long(ctx, "async");
                return;
            }
            c_type = transpiler_require_type_name_c_type(
                ctx,
                entry != NULL ? entry->type_name : NULL,
                surface_desc);
            if (c_type == NULL)
                return;
            codebuf_write(ctx->helpers, "    %s *%s;\n",
                c_type,
                capture_typed_names[i]);
        }
        codebuf_write(ctx->helpers, "} _pgy_async_ctx_%u;\n\n", pid);
    }

    codebuf_write(ctx->helpers, "static void *_pgy_async_%u(void *_arg) {\n", pid);
    if (has_captures) {
        codebuf_write(ctx->helpers,
            "    _pgy_async_ctx_%u *_pctx = (_pgy_async_ctx_%u *)_arg;\n",
            pid, pid);
        codebuf_write(ctx->helpers,
            "    if (_pctx == NULL) return NULL;\n");
    } else {
        codebuf_write(ctx->helpers, "    (void)_arg;\n");
    }

    CodeBuf *saved = ctx->out;
    int saved_indent = ctx->indent;
    bool saved_in_pw = ctx->in_parallel_wrapper;
    int saved_slot_count  = ctx->par_capture_slot_count;
    int saved_typed_count = ctx->par_capture_typed_count;
    char saved_slot_names[MAX_SLOT_VARS][64];
    char saved_typed_names[MAX_SLOT_VARS][64];

    ctx->out = ctx->helpers;
    ctx->indent = 1;
    ctx->in_parallel_wrapper  = true;
    memcpy(saved_slot_names, ctx->par_capture_slot_names, sizeof(saved_slot_names));
    memcpy(saved_typed_names, ctx->par_capture_typed_names, sizeof(saved_typed_names));
    memcpy(ctx->par_capture_slot_names, capture_slot_names, sizeof(capture_slot_names));
    memcpy(ctx->par_capture_typed_names, capture_typed_names, sizeof(capture_typed_names));
    ctx->par_capture_slot_count = capture_slot_count;
    ctx->par_capture_typed_count = capture_typed_count;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++)
        emit_statement(node->data.async_block.statements[i], ctx);

    ctx->out = saved;
    ctx->indent = saved_indent;
    ctx->in_parallel_wrapper  = saved_in_pw;
    memcpy(ctx->par_capture_slot_names, saved_slot_names, sizeof(saved_slot_names));
    memcpy(ctx->par_capture_typed_names, saved_typed_names, sizeof(saved_typed_names));
    ctx->par_capture_slot_count = saved_slot_count;
    ctx->par_capture_typed_count = saved_typed_count;

    if (has_captures)
        codebuf_write(ctx->helpers, "    free(_pctx);\n");
    codebuf_write(ctx->helpers, "    return NULL;\n}\n\n");

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "_pgy_async_ctx_%u *_pctx%u = (_pgy_async_ctx_%u *)malloc(sizeof(_pgy_async_ctx_%u));\n",
            pid, pid, pid, pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (_pctx%u == NULL) { PGY_PANIC(\"async capture allocation failed\"); }\n",
            pid);
        write_indent(ctx);
        codebuf_write(ctx->out, "*_pctx%u = (_pgy_async_ctx_%u){ ", pid, pid);
        bool first = true;
        for (int i = 0; i < capture_slot_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", capture_slot_names[i]);
            first = false;
        }
        for (int i = 0; i < capture_typed_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", capture_typed_names[i]);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }
    write_indent(ctx);
    if (has_captures) {
        codebuf_write(ctx->out,
            "PgyTaskHandle _ah_%u = pgy_async_spawn(_pgy_async_%u, _pctx%u);\n",
            pid, pid, pid);
    } else {
        codebuf_write(ctx->out,
            "PgyTaskHandle _ah_%u = pgy_async_spawn(_pgy_async_%u, NULL);\n",
            pid, pid);
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "pgy_async_detach(_ah_%u);\n", pid);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

#endif /* PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H */
