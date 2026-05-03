
static char *
infer_spawn_return_type_name(TranspilerCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    const char *function_name = NULL;
    ASTNode *call = NULL;

    if (target == NULL)
        return pergyra_strdup("Void");

    if (target->type == AST_CALL
        && target->data.call.callee != NULL
        && target->data.call.callee->type == AST_IDENTIFIER) {
        call = target;
        function_name = target->data.call.callee->data.identifier.name;
    } else if (target->type == AST_IDENTIFIER) {
        function_name = target->data.identifier.name;
    } else if (target->type == AST_FUNC_DECL) {
        if (target->data.func_decl.return_type != NULL)
            return render_type_name(target->data.func_decl.return_type);
        return pergyra_strdup("Void");
    }

    if (function_name == NULL)
        return pergyra_strdup("Void");

    ASTNode *decl = find_function_decl(ctx, function_name);
    if (decl != NULL && decl->data.func_decl.return_type != NULL) {
        if (call != NULL && func_has_generic_params(decl)) {
            GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
            size_t binding_count = 0;
            if (infer_generic_call_bindings(ctx, decl, call, bindings, &binding_count))
                return render_type_name_with_bindings(ctx, decl->data.func_decl.return_type,
                    bindings, binding_count);
        }
        return render_type_name(decl->data.func_decl.return_type);
    }

    return pergyra_strdup("Void");
}

static bool
is_remote_future_expr(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL) return false;
    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        return type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0;
    }
    return false;
}

static const char *
lookup_future_inner_type(TranspilerCtx *ctx, ASTNode *expr)
{
    if (expr == NULL)
        return "Void";

    if (expr->type == AST_IDENTIFIER) {
        const char *type_name = lookup_typed_var(ctx, expr->data.identifier.name);
        if (type_name != NULL && strncmp(type_name, "Future<", 7) == 0)
            return slot_inner_type_name(type_name);
        if (type_name != NULL && strncmp(type_name, "RemoteFuture<", 13) == 0)
            return slot_inner_type_name(type_name);
    }

    if (expr->type == AST_SPAWN_EXPR) {
        char *inner = infer_spawn_return_type_name(ctx, expr);
        static char buf[128];
        snprintf(buf, sizeof(buf), "%s", inner);
        free(inner);
        return buf;
    }

    return "Void";
}

const char *
pergyra_ast_type_to_c(ASTNode *type_node)
{
    static char mapped[128];
    if (type_node == NULL)
        return "void";

    if (type_node->type == AST_EVENT_HANDLER_TYPE)
        return "void *";

    char *type_name = render_type_name(type_node);
    snprintf(mapped, sizeof(mapped), "%s", pergyra_type_to_c(type_name));
    free(type_name);
    return mapped;
}

static bool
transpiler_can_forward_declare_type_early(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return true;

    const char *name = type_node->data.type.name;
    if (strcmp(name, "Int") == 0
        || strcmp(name, "Float") == 0
        || strcmp(name, "Bool") == 0
        || strcmp(name, "String") == 0
        || strcmp(name, "Char") == 0
        || strcmp(name, "Byte") == 0
        || strcmp(name, "Void") == 0
        || strcmp(name, "Qubit") == 0)
        return true;

    if (strcmp(name, "Result") == 0
        || strcmp(name, "Option") == 0
        || strcmp(name, "Slot") == 0
        || strcmp(name, "SecureSlot") == 0
        || strcmp(name, "DeviceSlot") == 0
        || strcmp(name, "RemoteFuture") == 0
        || strcmp(name, "Array") == 0
        || strcmp(name, "Slice") == 0
        || strcmp(name, "Channel") == 0
        || strcmp(name, "Box") == 0
        || strcmp(name, "Rc") == 0
        || strcmp(name, "Weak") == 0
        || strcmp(name, "Future") == 0)
        return true;

    return find_class_decl(ctx, name) != NULL;
}

static bool
transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_early(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_early(ctx, p->type))
            return false;
    }
    return true;
}

static bool
transpiler_can_forward_declare_type_after_zones(TranspilerCtx *ctx, ASTNode *type_node)
{
    const char *name = NULL;

    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return true;
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL)
        return false;

    name = type_node->data.type.name;
    if (find_world_decl(ctx, name) != NULL)
        return false;
    return transpiler_has_known_nominal_type(ctx, name);
}

static bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    if (func->data.func_decl.generic_params != NULL
        && func->data.func_decl.generic_params->count > 0)
        return false;
    if (!transpiler_can_forward_declare_type_after_zones(ctx,
            func->data.func_decl.return_type))
        return false;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *p = func->data.func_decl.params[i];
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type))
            return false;
    }
    return true;
}

static void
append_mangled_type_name(CodeBuf *buf, const char *type_name)
{
    bool wrote = false;

    for (const unsigned char *p = (const unsigned char *)type_name; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || (*p >= '0' && *p <= '9')) {
            codebuf_write(buf, "%c", *p);
            wrote = true;
        } else if (wrote && buf->len > 0 && buf->data[buf->len - 1] != '_') {
            codebuf_write(buf, "_");
        }
    }

    if (!wrote)
        codebuf_write(buf, "Type");
}

static int
find_generic_param_index(ASTNode *decl, const char *name)
{
    if (!func_has_generic_params(decl) || name == NULL)
        return -1;

    for (size_t i = 0; i < decl->data.func_decl.generic_params->count; i++) {
        GenericParam *param = decl->data.func_decl.generic_params->params[i];
        if (param != NULL && param->name != NULL && strcmp(param->name, name) == 0)
            return (int)i;
    }

    return -1;
}

static bool

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
emit_hosted_method_forward_decl_from_metadata(const char *host_name,
                                              const MIRDeclMethod *method_meta,
                                              ASTNode *method,
                                              bool pointer_self, CodeBuf *buf,
                                              TranspilerCtx *ctx);

static void
emit_hosted_method_forward_decl_from_metadata(const char *host_name,
                                              const MIRDeclMethod *method_meta,
                                              ASTNode *method,
                                              bool pointer_self, CodeBuf *buf,
                                              TranspilerCtx *ctx)
{
    if (host_name == NULL || method == NULL || buf == NULL || ctx == NULL
        || method->type != AST_FUNC_DECL)
        return;

    const char *method_name = transpiler_mir_decl_method_name(method_meta);
    ASTNode *return_type = transpiler_mir_decl_method_return_type(method_meta);
    size_t param_count = transpiler_mir_decl_method_param_count(method_meta);
    const char *ret_type = "void";
    if (method_name == NULL)
        method_name = method->data.func_decl.name;
    if (return_type == NULL)
        return_type = method->data.func_decl.return_type;
    if (param_count == 0 && method_meta == NULL)
        param_count = method->data.func_decl.param_count;
    if (method_name == NULL)
        return;
    ensure_type_specializations_from_ast(ctx, return_type);
    if (return_type != NULL)
        ret_type = pergyra_ast_type_to_c(return_type);

    codebuf_write(buf, "\n%s\n%s_%s(%s%s",
                  ret_type, host_name, method_name, host_name,
                  pointer_self ? " *self" : " self");

    for (size_t j = 0; j < param_count; j++) {
        FuncParam *p = transpiler_mir_decl_method_param(method_meta, j);
        if (p == NULL && method_meta == NULL)
            p = method->data.func_decl.params[j];
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
