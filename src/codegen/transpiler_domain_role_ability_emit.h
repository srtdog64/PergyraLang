#ifndef PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H

/* =================================================================
 * Role/Ability system emitters
 * ================================================================= */

static void
emit_hosted_methods_from_mir_or_error_local(const char *host_name,
                                            const char *anonymous_host_name,
                                            const char *host_kind,
                                            const TranspilerHostedMethodView *method_view,
                                            TranspilerCtx *ctx)
{
    size_t method_count = method_view != NULL ? method_view->count : 0;

    if (transpiler_hosted_method_view_missing_mir_metadata(method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for %s methods '%s'",
            host_kind != NULL ? host_kind : "host",
            host_name != NULL ? host_name : anonymous_host_name);
        return;
    }

    for (size_t i = 0; i < method_count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(method_view, i);
        const MIRRoutine *mir_method = NULL;
        const char *method_name = NULL;
        char emitted_name[256];

        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        method_name = transpiler_mir_decl_method_name(method_meta);
        if (method_name == NULL)
            method_name = method->data.func_decl.name;

        mir_method = transpiler_mir_decl_method_routine(ctx, method_meta);
        if (mir_method == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing routine for %s method '%s.%s'",
                host_kind != NULL ? host_kind : "host",
                host_name != NULL ? host_name : anonymous_host_name,
                method_name != NULL
                    ? method_name
                    : "(anonymous)");
            return;
        }

        snprintf(emitted_name, sizeof(emitted_name), "%s_%s", host_name,
            method_name != NULL ? method_name : "(anonymous)");
        emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
        if (ctx != NULL && ctx->backend_error != NULL)
            return;
    }
}

static void
build_ability_ref_bindings(ASTNode *ability_decl,
                           ASTNode *ability_ref,
                           TranspilerCtx *ctx,
                           GenericBindingEntry *bindings,
                           size_t *binding_count)
{
    size_t out = 0;

    if (binding_count != NULL)
        *binding_count = 0;
    if (ability_decl == NULL || ability_ref == NULL
        || ability_decl->type != AST_ABILITY_DECL
        || ability_ref->type != AST_TYPE
        || ability_decl->data.ability_decl.generic_params == NULL) {
        return;
    }

    for (size_t i = 0;
         i < ability_decl->data.ability_decl.generic_params->count
         && out < MAX_GENERIC_BINDINGS;
         i++) {
        GenericParam *formal = ability_decl->data.ability_decl.generic_params->params[i];
        GenericParam *actual = NULL;
        ASTNode *actual_type = NULL;
        char *rendered = NULL;

        if (formal == NULL || formal->name == NULL)
            continue;
        if (ability_ref->data.type.generic_args != NULL
            && i < ability_ref->data.type.generic_args->count) {
            actual = ability_ref->data.type.generic_args->params[i];
            if (actual != NULL)
                actual_type = actual->constraint;
        }
        if (actual_type == NULL)
            actual_type = formal->default_type;
        if (actual_type == NULL)
            continue;

        rendered = render_type_name(actual_type);
        if (rendered == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render generic ability binding '%s' for ability '%s'",
                formal->name != NULL ? formal->name : "<param>",
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name
                    : "<ability>");
            return;
        }
        snprintf(bindings[out].name, sizeof(bindings[out].name), "%s", formal->name);
        snprintf(bindings[out].concrete_type, sizeof(bindings[out].concrete_type),
            "%s", rendered);
        free(rendered);
        out++;
    }

    if (binding_count != NULL)
        *binding_count = out;
}

static char *
render_effective_ability_ref_vtable_tag(ASTNode *ability_decl,
                                        ASTNode *ability_ref,
                                        TranspilerCtx *ctx)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;
    char *rendered = NULL;
    char suffix[128];
    size_t len;

    if (ability_ref == NULL)
        return NULL;

    if (ability_decl != NULL && ability_decl->type == AST_ABILITY_DECL
        && ability_decl->data.ability_decl.generic_params != NULL
        && ability_decl->data.ability_decl.generic_params->count > 0) {
        CodeBuf *buf = codebuf_create();
        if (buf == NULL)
            return NULL;
        build_ability_ref_bindings(ability_decl, ability_ref, ctx, bindings, &binding_count);
        if (ctx != NULL && ctx->backend_error != NULL) {
            codebuf_destroy(buf);
            return NULL;
        }
        codebuf_write(buf, "%s", ability_ref->data.type.name != NULL
                               ? ability_ref->data.type.name
                               : "Ability");
        if (binding_count > 0) {
            codebuf_write(buf, "<");
            for (size_t i = 0; i < binding_count; i++) {
                if (i > 0)
                    codebuf_write(buf, ", ");
                codebuf_write(buf, "%s", bindings[i].concrete_type);
            }
            codebuf_write(buf, ">");
        }
        rendered = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
    } else {
        rendered = render_type_name(ability_ref);
    }

    if (rendered == NULL)
        return NULL;
    sanitize_c_suffix(rendered, suffix, sizeof(suffix));
    len = strlen(suffix);
    while (len > 0 && suffix[len - 1] == '_')
        suffix[--len] = '\0';
    free(rendered);
    if (len == 0)
        return NULL;
    return pergyra_strdup(suffix);
}

static bool
ability_ref_vtable_typedef_name(ASTNode *ability_ref,
                                char *buf,
                                size_t buf_size,
                                TranspilerCtx *ctx)
{
    char *tag;
    ASTNode *ability_decl = NULL;

    if (buf == NULL || buf_size == 0)
        return false;

    if (ctx != NULL && ability_ref != NULL && ability_ref->type == AST_TYPE)
        ability_decl = find_ability_decl(ctx, ability_ref->data.type.name);
    tag = render_effective_ability_ref_vtable_tag(ability_decl, ability_ref, ctx);
    if (tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for ability reference");
        return false;
    }
    snprintf(buf, buf_size, "%s_vtable", tag);
    free(tag);
    return true;
}

static void
ensure_ability_ref_vtable_decl(ASTNode *ability_ref, TranspilerCtx *ctx)
{
    ASTNode *ability_decl;
    const char *ability_name;
    char typedef_name[128];
    char *tag = NULL;
    bool already_emitted = false;
    CodeBuf *target;

    if (ctx == NULL || ability_ref == NULL
        || ability_ref->type != AST_TYPE || ability_ref->data.type.name == NULL) {
        return;
    }
    target = ctx->out != NULL ? ctx->out : ctx->decls;
    if (target == NULL)
        return;

    ability_name = ability_ref->data.type.name;
    ability_decl = find_ability_decl(ctx, ability_name);
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL)
        return;

    if (ability_decl->data.ability_decl.generic_params == NULL
        || ability_decl->data.ability_decl.generic_params->count == 0) {
        return;
    }

    tag = render_effective_ability_ref_vtable_tag(ability_decl, ability_ref, ctx);
    if (tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for ability '%s'",
            ability_name != NULL ? ability_name : "<ability>");
        return;
    }
    for (int i = 0; i < ctx->ability_vtable_spec_count; i++) {
        if (strcmp(ctx->ability_vtable_specs[i].name, tag) == 0) {
            already_emitted = true;
            break;
        }
    }
    if (already_emitted) {
        free(tag);
        return;
    }

    if (ctx->ability_vtable_spec_count < MAX_ABILITY_VTABLE_SPECIALIZATIONS) {
        snprintf(ctx->ability_vtable_specs[ctx->ability_vtable_spec_count++].name,
            sizeof(ctx->ability_vtable_specs[0].name), "%s", tag);
    }

    if (!ability_ref_vtable_typedef_name(ability_ref, typedef_name, sizeof(typedef_name), ctx)) {
        free(tag);
        return;
    }
    codebuf_write(target, "\ntypedef struct\n{\n");

    for (size_t i = 0; i < ability_decl->data.ability_decl.method_count; i++) {
        ASTNode *method = ability_decl->data.ability_decl.methods[i];
        GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
        size_t binding_count = 0;
        char *ret_name = NULL;
        const char *ret_type = "void";

        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        build_ability_ref_bindings(ability_decl, ability_ref, ctx, bindings, &binding_count);
        if (ctx != NULL && ctx->backend_error != NULL) {
            free(tag);
            return;
        }
        if (method->data.func_decl.return_type != NULL) {
            ret_name = render_type_name_with_bindings(ctx,
                method->data.func_decl.return_type, bindings, binding_count);
            ret_type = pergyra_type_to_c(ret_name);
        }

        codebuf_write(target, "    %s (*%s)(void *self",
            ret_type, method->data.func_decl.name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            char *param_name = NULL;
            const char *pt = NULL;
            bool pointer_param = false;
            char surface_desc[256];
            if (p == NULL)
                continue;
            if (p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            if (p->type != NULL) {
                param_name = render_type_name_with_bindings(ctx, p->type, bindings, binding_count);
                pointer_param = param_name != NULL
                    && is_pointer_self_host_type_name(ctx, param_name);
            }
            snprintf(surface_desc, sizeof(surface_desc),
                "ability vtable parameter '%s.%s(%s)'",
                ability_name != NULL ? ability_name : "(anonymous)",
                method->data.func_decl.name != NULL ? method->data.func_decl.name : "(anonymous)",
                p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_type_name_c_type(ctx, param_name, surface_desc);
            if (pt == NULL) {
                free(param_name);
                free(ret_name);
                free(tag);
                return;
            }
            codebuf_write(target, ", %s%s %s", pt, pointer_param ? " *" : "", p->name);
            free(param_name);
        }
        codebuf_write(target, ");\n");
        free(ret_name);
    }

    codebuf_write(target, "} %s;\n", typedef_name);
    free(tag);
}

static void
emit_role_method_impl(const char *role_name, ASTNode *method, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_method;
    const char *method_name;

    if (method == NULL || method->type != AST_FUNC_DECL)
        return;

    mir_method = transpiler_find_role_impl_mir_method(ctx, role_name, method);
    method_name = method->data.func_decl.name;
    if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing routine for role method '%s.%s'",
            role_name != NULL ? role_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)");
        return;
    }
    if (mir_method != NULL) {
        char emitted_name[256];
        snprintf(emitted_name, sizeof(emitted_name), "%s_%s",
                 role_name, method_name);
        emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
        return;
    }
    transpiler_set_mir_inventory_missing(
        ctx,
        "MIR-only C path missing routine for role method '%s.%s'",
        role_name != NULL ? role_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)");
}

static void
emit_role_vtable_instance(const char *role_name, ASTNode *impl, TranspilerCtx *ctx)
{
    const char *ability_name =
        (impl->data.impl_ability.ability_ref != NULL
         && impl->data.impl_ability.ability_ref->type == AST_TYPE)
        ? impl->data.impl_ability.ability_ref->data.type.name : NULL;
    char typedef_name[128];
    char *vtable_tag = NULL;
    if (ability_name == NULL || impl->data.impl_ability.method_count == 0)
        return;

    ensure_ability_ref_vtable_decl(impl->data.impl_ability.ability_ref, ctx);
    if (!ability_ref_vtable_typedef_name(
            impl->data.impl_ability.ability_ref, typedef_name, sizeof(typedef_name), ctx))
        return;
    vtable_tag = render_effective_ability_ref_vtable_tag(
        find_ability_decl(ctx, ability_name),
        impl->data.impl_ability.ability_ref,
        ctx);
    if (vtable_tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for role '%s' impl ability '%s'",
            role_name != NULL ? role_name : "<role>",
            ability_name != NULL ? ability_name : "<ability>");
        return;
    }
    codebuf_write(ctx->out,
        "\nstatic const %s %s_%s_vtable_instance __attribute__((unused)) = {\n",
        typedef_name, role_name, vtable_tag);

    for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
        ASTNode *method = impl->data.impl_ability.methods[j];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        if (method->data.func_decl.name == NULL)
            continue;
        codebuf_write(ctx->out, "    .%s = %s_%s,\n",
                      method->data.func_decl.name,
                      role_name, method->data.func_decl.name);
    }

    codebuf_write(ctx->out, "};\n");
    free(vtable_tag);
}

static void
emit_role_operator_aliases(ASTNode *role, TranspilerCtx *ctx)
{
    const char *role_name;
    const char *for_type;
    PgyTokenType ops[] = {
        TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
        TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
        TOKEN_GREATER, TOKEN_GREATER_EQUAL
    };

    if (role == NULL || role->type != AST_ROLE_DECL
        || role->data.role_decl.name == NULL
        || role->data.role_decl.for_type == NULL
        || role->data.role_decl.for_type->type != AST_TYPE
        || role->data.role_decl.for_type->data.type.name == NULL) {
        return;
    }

    role_name = role->data.role_decl.name;
    for_type = role->data.role_decl.for_type->data.type.name;

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        PgyTokenType op = ops[i];
        const char *suffix = operator_overload_suffix(op);
        ASTNode *method = find_role_operator_method_decl(ctx, role, op, 0);
        char fn_name[256];
        if (suffix == NULL || method == NULL
            || method->type != AST_FUNC_DECL
            || method->data.func_decl.name == NULL
            ) {
            continue;
        }
        snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, for_type);
        if (find_function_decl(ctx, fn_name) != NULL)
            continue;

        FuncParam *rhs_param = NULL;
        size_t rhs_param_count = 0;
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->name != NULL
                && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        const char *ret_type = "void";
        const char *lhs_type = transpiler_require_ast_c_type(
            ctx,
            role != NULL ? role->data.role_decl.for_type : NULL,
            "role operator lhs type");
        const char *rhs_type = NULL;
        const char *rhs_name = (rhs_param != NULL && rhs_param->name != NULL)
            ? rhs_param->name : "rhs";
        char surface_desc[256];

        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);
        snprintf(surface_desc, sizeof(surface_desc),
            "role operator parameter '%s.%s(%s)'",
            role_name != NULL ? role_name : "(anonymous)",
            method->data.func_decl.name != NULL ? method->data.func_decl.name : "(anonymous)",
            rhs_name != NULL ? rhs_name : "(anonymous)");
        rhs_type = transpiler_require_ast_c_type(
            ctx,
            rhs_param != NULL ? rhs_param->type : NULL,
            surface_desc);
        if (lhs_type == NULL || rhs_type == NULL)
            return;

        codebuf_write(ctx->out,
            "\nstatic %s\noperator_%s_%s(%s lhs, %s %s)\n{\n",
            ret_type, suffix, for_type, lhs_type, rhs_type, rhs_name);
        codebuf_write(ctx->out, "    %s lhs_copy = lhs;\n", lhs_type);
        if (strcmp(ret_type, "void") == 0) {
            codebuf_write(ctx->out, "    %s_%s(&lhs_copy, %s);\n",
                role_name, method->data.func_decl.name, rhs_name);
        } else {
            codebuf_write(ctx->out, "    return %s_%s(&lhs_copy, %s);\n",
                role_name, method->data.func_decl.name, rhs_name);
        }
        codebuf_write(ctx->out, "}\n");
    }
}

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_ABILITY_EMIT_H */
