/* -----------------------------------------------------------------
 * Generic class monomorphization
 * ----------------------------------------------------------------- */

static bool
class_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_CLASS_DECL
        && node->data.class_decl.generic_params != NULL
        && node->data.class_decl.generic_params->count > 0;
}

/* Ensure a monomorphized specialization of a generic class exists.
 * Returns the specialized name (e.g. "Node_Int") that should be used
 * as the C struct type name. The struct + methods are emitted into
 * ctx->helpers on first invocation.
 *
 * `ann` is the AST_TYPE node for the annotation (e.g. Node<Int>).
 * We extract generic_args from it and match them to class_decl's
 * generic_params to build the bindings. */
static const char *
ensure_generic_class_specialization(TranspilerCtx *ctx,
                                     ASTNode *class_decl,
                                     ASTNode *ann)
{
    GenericParams *gp = class_decl->data.class_decl.generic_params;
    GenericParams *ga = ann->data.type.generic_args;
    bool has_effective_args = false;

    if (gp == NULL)
        return class_decl->data.class_decl.name;

    CodeBuf *nbuf = codebuf_create();
    codebuf_write(nbuf, "%s", class_decl->data.class_decl.name);
    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *formal = gp->params[i];
        GenericParam *garg = (ga != NULL && i < ga->count) ? ga->params[i] : NULL;
        const char *effective_name = NULL;

        if (garg != NULL && garg->name != NULL)
            effective_name = garg->name;
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            effective_name = garg->constraint->data.type.name;
        else if (formal != NULL && formal->default_type != NULL
                 && formal->default_type->type == AST_TYPE
                 && formal->default_type->data.type.name != NULL)
            effective_name = formal->default_type->data.type.name;
        else
            return class_decl->data.class_decl.name;

        has_effective_args = true;
        codebuf_write(nbuf, "_");
        append_mangled_type_name(nbuf, effective_name);
    }

    if (!has_effective_args) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, nbuf->data) == 0) {
            const char *result = ctx->generic_class_specs[i].specialized_name;
            codebuf_destroy(nbuf);
            return result;
        }
    }

    if (ctx->generic_class_spec_count >= MAX_GENERIC_CLASS_SPECIALIZATIONS) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    GenericClassSpecEntry *entry = &ctx->generic_class_specs[ctx->generic_class_spec_count++];
    entry->class_decl = class_decl;
    snprintf(entry->specialized_name, sizeof(entry->specialized_name), "%s", nbuf->data);
    entry->emitted = true;
    const char *spec_name = entry->specialized_name;

    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *formal = gp->params[i];
        GenericParam *garg = (ga != NULL && i < ga->count) ? ga->params[i] : NULL;
        const char *effective_name = NULL;

        if (ctx->generic_binding_count >= MAX_GENERIC_BINDINGS)
            break;
        GenericBindingEntry *b = &ctx->generic_bindings[ctx->generic_binding_count++];
        snprintf(b->name, sizeof(b->name), "%s",
                 gp->params[i] != NULL ? gp->params[i]->name : "T");
        if (garg != NULL && garg->name != NULL)
            effective_name = garg->name;
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            effective_name = garg->constraint->data.type.name;
        else if (formal != NULL && formal->default_type != NULL
                 && formal->default_type->type == AST_TYPE
                 && formal->default_type->data.type.name != NULL)
            effective_name = formal->default_type->data.type.name;

        if (effective_name != NULL)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     effective_name);
        else {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot resolve generic class binding '%s' for specialization '%s'",
                    gp->params[i] != NULL && gp->params[i]->name != NULL
                        ? gp->params[i]->name
                        : "(anonymous)",
                    class_decl != NULL && class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name
                        : "(anonymous)");
            }
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }

        entry->bindings[i] = *b;
    }
    entry->binding_count = gp->count;

    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    g_type_render_ctx = ctx;

    codebuf_write(ctx->helpers, "\ntypedef struct %s\n{\n", spec_name);
    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *f = class_decl->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "generic class field '%s.%s'",
            spec_name != NULL ? spec_name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL) {
            g_type_render_ctx = saved_render_ctx;
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, f->name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    codebuf_write(ctx->helpers,
        "\n#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        spec_name, spec_name,
        spec_name, spec_name,
        spec_name, spec_name);

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        emit_hosted_method_forward_decl_named(spec_name, method, use_self_cell,
                                              ctx->helpers, ctx);
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        const MIRRoutine *mir_method = transpiler_find_mir_method(ctx,
            class_decl->data.class_decl.name, method);
        if (method->type != AST_FUNC_DECL)
            continue;

        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing routine for generic class method '%s.%s' specialization '%s'",
                    class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name
                        : "(anonymous-class)",
                    method->data.func_decl.name != NULL
                        ? method->data.func_decl.name
                        : "(anonymous)",
                    spec_name != NULL ? spec_name : "(anonymous-specialization)");
            }
            g_type_render_ctx = saved_render_ctx;
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }

        if (mir_method != NULL) {
            char emitted_name[256];
            snprintf(emitted_name, sizeof(emitted_name), "%s_%s", spec_name,
                method->data.func_decl.name != NULL
                    ? method->data.func_decl.name
                    : "(anonymous)");
            emit_func_decl_from_mir_named(method, mir_method, emitted_name,
                ctx->helpers, ctx);
            continue;
        }

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s *self",
                          ret_type, spec_name, method_name, spec_name);
        } else {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s self",
                          ret_type, spec_name, method_name, spec_name);
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
                "generic class method parameter '%s.%s(%s)'",
                spec_name != NULL ? spec_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL) {
                g_type_render_ctx = saved_render_ctx;
                ctx->generic_binding_count = saved_binding_count;
                codebuf_destroy(nbuf);
                return NULL;
            }
            codebuf_write(ctx->helpers, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->helpers, ")\n{\n");

        transpiler_emit_host_method_body_local(
            ctx, class_decl, spec_name, method, ctx->helpers, false);

        codebuf_write(ctx->helpers, "}\n");
    }

    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    codebuf_destroy(nbuf);

    return spec_name;
}
