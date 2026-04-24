infer_generic_call_bindings(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call,
                            GenericBindingEntry *bindings, size_t *binding_count)
{
    if (!func_has_generic_params(decl)
        || call == NULL
        || call->type != AST_CALL
        || bindings == NULL
        || binding_count == NULL) {
        return false;
    }

    size_t generic_count = decl->data.func_decl.generic_params->count;
    memset(bindings, 0, sizeof(GenericBindingEntry) * generic_count);

    for (size_t i = 0; i < generic_count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL) {
            strncpy(bindings[i].name, param->name, sizeof(bindings[i].name) - 1);
            bindings[i].name[sizeof(bindings[i].name) - 1] = '\0';
        }
    }

    for (size_t i = 0; i < decl->data.func_decl.param_count && i < call->data.call.arg_count; i++) {
        FuncParam *param = decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE)
            continue;
        if (param->type->data.type.generic_args != NULL)
            continue;

        int generic_index = find_generic_param_index(decl, param->type->data.type.name);
        if (generic_index < 0)
            continue;

        const char *arg_type = infer_expression_type_name(ctx, call->data.call.arguments[i]);
        if (arg_type == NULL)
            continue;

        if (bindings[generic_index].concrete_type[0] != '\0'
            && strcmp(bindings[generic_index].concrete_type, arg_type) != 0) {
            return false;
        }

        strncpy(bindings[generic_index].concrete_type, arg_type,
            sizeof(bindings[generic_index].concrete_type) - 1);
        bindings[generic_index].concrete_type[sizeof(bindings[generic_index].concrete_type) - 1] = '\0';
    }

    for (size_t i = 0; i < generic_count; i++) {
        if (bindings[i].name[0] == '\0' || bindings[i].concrete_type[0] == '\0')
            return false;
    }

    *binding_count = generic_count;
    return true;
}

static void
emit_func_forward_decl_named(ASTNode *node, const char *emitted_name,
                             CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx);

static void
emit_hosted_method_forward_decl_named(const char *host_name, ASTNode *method,
                                      bool pointer_self, CodeBuf *buf,
                                      TranspilerCtx *ctx)
{
    if (host_name == NULL || method == NULL || buf == NULL || ctx == NULL
        || method->type != AST_FUNC_DECL)
        return;

    const char *method_name = method->data.func_decl.name;
    const char *ret_type = "void";
    if (method_name == NULL)
        return;
    ensure_type_specializations_from_ast(ctx, method->data.func_decl.return_type);
    if (method->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

    codebuf_write(buf, "\n%s\n%s_%s(%s%s",
                  ret_type, host_name, method_name, host_name,
                  pointer_self ? " *self" : " self");

    for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
        FuncParam *p = method->data.func_decl.params[j];
        if (p == NULL || p->name == NULL)
            continue;
        if (strcmp(p->name, "self") == 0)
            continue;

        const char *pt = NULL;
        char surface_desc[256];
        if (p->type != NULL)
            ensure_type_specializations_from_ast(ctx, p->type);
        snprintf(surface_desc, sizeof(surface_desc),
            "hosted method parameter '%s.%s(%s)'",
            host_name != NULL ? host_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)",
            p->name != NULL ? p->name : "(anonymous)");
        pt = transpiler_require_ast_c_type(ctx, p->type, surface_desc);
        if (pt == NULL)
            return;
        {
            char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
            bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
            if (subj_param)
                codebuf_write(buf, ", %s *%s", pt, p->name);
            else
                codebuf_write(buf, ", %s %s", pt, p->name);
            free(ptn);
        }
    }
    codebuf_write(buf, ");\n");
}

static const char *
ensure_generic_specialization(TranspilerCtx *ctx, ASTNode *decl, ASTNode *call)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (!infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
        return NULL;

    CodeBuf *name_buf = codebuf_create();
    if (name_buf == NULL)
        return NULL;

    codebuf_write(name_buf, "%s", decl->data.func_decl.name);
    for (size_t i = 0; i < binding_count; i++) {
        codebuf_write(name_buf, "_");
        append_mangled_type_name(name_buf, bindings[i].concrete_type);
    }

    for (int i = 0; i < ctx->generic_specialization_count; i++) {
        GenericSpecializationEntry *entry = &ctx->generic_specializations[i];
        if (entry->func_decl == decl
            && strcmp(entry->specialized_name, name_buf->data) == 0) {
            codebuf_destroy(name_buf);
            return entry->specialized_name;
        }
    }

    if (ctx->generic_specialization_count >= MAX_GENERIC_SPECIALIZATIONS) {
        codebuf_destroy(name_buf);
        return NULL;
    }

    GenericSpecializationEntry *entry =
        &ctx->generic_specializations[ctx->generic_specialization_count++];
    memset(entry, 0, sizeof(*entry));
    entry->func_decl = decl;
    strncpy(entry->specialized_name, name_buf->data,
        sizeof(entry->specialized_name) - 1);
    entry->specialized_name[sizeof(entry->specialized_name) - 1] = '\0';
    entry->emitting = true;
    codebuf_destroy(name_buf);

    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < binding_count && ctx->generic_binding_count < MAX_GENERIC_BINDINGS; i++) {
        ctx->generic_bindings[ctx->generic_binding_count++] = bindings[i];
    }

    emit_func_forward_decl_named(decl, entry->specialized_name, ctx->decls, ctx);
    emit_func_decl_named(decl, entry->specialized_name, ctx->helpers, ctx);

    ctx->generic_binding_count = saved_binding_count;
    entry->emitting = false;
    return entry->specialized_name;
}
