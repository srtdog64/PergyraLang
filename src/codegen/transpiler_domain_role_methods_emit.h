#ifndef PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H

static void
emit_role_method_impl(const char *role_name, ASTNode *method, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_method;
    const char *method_name;

    if (ctx != NULL && ctx->backend_error != NULL)
        return;
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
        if (!transpiler_role_ability_host_method_name(
                emitted_name, sizeof(emitted_name), role_name, method_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role method symbol name is too long for '%s.%s'",
                role_name != NULL ? role_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }
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
    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (ability_name == NULL || impl->data.impl_ability.method_count == 0)
        return;

    ensure_ability_ref_vtable_decl(impl->data.impl_ability.ability_ref, ctx);
    if (ctx != NULL && ctx->backend_error != NULL)
        return;
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

    if (ctx != NULL && ctx->backend_error != NULL)
        return;
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
        if (!transpiler_role_operator_alias_name(fn_name, sizeof(fn_name),
                suffix, for_type)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role operator alias name is too long for role '%s'",
                role_name != NULL ? role_name : "(anonymous)");
            return;
        }
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
        if (!transpiler_role_ability_surface_desc(surface_desc,
                sizeof(surface_desc), "role operator parameter",
                role_name, method->data.func_decl.name, rhs_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role operator parameter diagnostic is too long");
            return;
        }
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

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H */
