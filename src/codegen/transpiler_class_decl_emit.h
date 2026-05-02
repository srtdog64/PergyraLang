/* Class declaration lowering owner. Included after generic class specialization helpers. */

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    /* Generic classes are emitted lazily when first used (monomorphized). */
    if (class_has_generic_params(node))
        return;

    const char *name = node->data.class_decl.name;
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_MIR_TOPOLOGY_INVALID,
            PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING,
            PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
            "MIR-only C path missing declaration metadata for class methods '%s'",
            name != NULL ? name : "(anonymous-class)");
        return;
    }

    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        if (f != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out, f->type);
    }
    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            method);
    }

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "class field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL)
            return;
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
        ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_named(name, method, use_self_cell,
                                              ctx->out, ctx);
    }

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        const MIRRoutine *mir_method;
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        mir_method = transpiler_find_mir_method(ctx, name, method);
        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing routine for class method '%s.%s'",
                    name != NULL ? name : "(anonymous-class)",
                    method->data.func_decl.name != NULL
                        ? method->data.func_decl.name
                        : "(anonymous)");
            }
            return;
        }
        if (mir_method != NULL) {
            char emitted_name[256];
            snprintf(emitted_name, sizeof(emitted_name), "%s_%s", name,
                method->data.func_decl.name);
            emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
            continue;
        }

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                          ret_type, name, method_name, name);
        } else {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, name, method_name, name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "class method parameter '%s.%s(%s)'",
                name != NULL ? name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
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
