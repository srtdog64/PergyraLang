#ifndef PGY_TRANSPILER_CLASS_DECL_EMIT_H
#define PGY_TRANSPILER_CLASS_DECL_EMIT_H

/* Class declaration lowering owner. Included after generic class specialization helpers. */

static bool
transpiler_class_surface_desc(char *out, size_t out_size,
                              const char *surface_kind,
                              const char *class_name,
                              const char *member_name,
                              const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    if (param_name != NULL) {
        written = snprintf(out, out_size, "%s '%s.%s(%s)'",
            surface_kind,
            class_name != NULL ? class_name : "(anonymous)",
            member_name != NULL ? member_name : "(anonymous)",
            param_name);
    } else {
        written = snprintf(out, out_size, "%s '%s.%s'",
            surface_kind,
            class_name != NULL ? class_name : "(anonymous)",
            member_name != NULL ? member_name : "(anonymous)");
    }

    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_class_method_emit_name(char *out, size_t out_size,
                                  const char *class_name,
                                  const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_%s",
        class_name != NULL ? class_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_class_format_too_long(TranspilerCtx *ctx, const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "class generated name");
}

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    /* Generic classes are emitted lazily when first used (monomorphized). */
    if (class_has_generic_params(node))
        return;

    const char *name = ast_class_name(node);
    size_t field_count = 0;
    ClassField **fields = ast_class_fields(node, &field_count);
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for class methods '%s'",
            name != NULL ? name : "(anonymous-class)");
        return;
    }

    for (size_t i = 0; i < field_count; i++) {
        ClassField *f = fields != NULL ? fields[i] : NULL;
        if (f != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out, f->type);
    }
    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            method);
    }

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    for (size_t i = 0; i < field_count; i++) {
        ClassField *f = fields != NULL ? fields[i] : NULL;
        char ft[256];
        char surface_desc[256];
        if (!transpiler_class_surface_desc(surface_desc,
                sizeof(surface_desc), "class field", name,
                f != NULL ? f->name : NULL, NULL)) {
            transpiler_class_format_too_long(ctx, "class field diagnostic surface");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(ctx,
                f != NULL ? f->type : NULL,
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Auto-generate Slot and Result container types for this struct.
     * This allows Slot<MyStruct>, Result<MyStruct> in user code. */
    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for %s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods become free functions over a subject self-cell or class value. */
    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta, method,
            use_self_cell, ctx->out, ctx);
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        const MIRRoutine *mir_method;
        const char *method_name;
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        method_name = transpiler_mir_decl_method_name(method_meta);
        if (method_name == NULL)
            method_name = ast_declaration_name(method);
        mir_method = transpiler_hosted_method_view_routine(ctx, &method_view, i);
        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing routine for class method '%s.%s'",
                name != NULL ? name : "(anonymous-class)",
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }
        if (mir_method != NULL) {
            char emitted_name[256];
            if (!transpiler_class_method_emit_name(emitted_name,
                    sizeof(emitted_name), name, method_name)) {
                transpiler_class_format_too_long(ctx, "class method emitted name");
                return;
            }
            emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
            continue;
        }

        char ret_type_buf[256];
        const char *ret_type = "void";
        if (ast_func_return_type(method) != NULL
            && pergyra_ast_type_to_c_copy(ast_func_return_type(method),
                ret_type_buf,
                sizeof(ret_type_buf))) {
            ret_type = ret_type_buf;
        }

        if (use_self_cell) {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                          ret_type, name, method_name, name);
        } else {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, name, method_name, name);
        }

        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            char pt[256];
            char surface_desc[256];
            if (!transpiler_class_surface_desc(surface_desc,
                    sizeof(surface_desc), "class method parameter", name,
                    method_name, p != NULL ? p->name : NULL)) {
                transpiler_class_format_too_long(
                    ctx, "class method parameter diagnostic surface");
                return;
            }
            if (!transpiler_require_ast_c_type_copy(ctx,
                    p != NULL ? p->type : NULL,
                    surface_desc,
                    pt,
                    sizeof(pt))) {
                return;
            }
            {
                char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
                bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
                if (subj_param)
                    codebuf_write(ctx->out, ", %s *%s", pt, p->name);
                else
                    codebuf_write(ctx->out, ", %s %s", pt, p->name);
                free(ptn);
            }
        }
        codebuf_write(ctx->out, ")\n{\n");

        transpiler_emit_host_method_body_local(
            ctx,
            transpiler_find_decl_in_inventory_local(ctx, AST_CLASS_DECL, name),
            name,
            method,
            NULL,
            true);

        codebuf_write(ctx->out, "}\n");
    }
}
#endif /* PGY_TRANSPILER_CLASS_DECL_EMIT_H */
