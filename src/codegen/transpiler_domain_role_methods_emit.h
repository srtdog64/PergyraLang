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
    method_name = ast_declaration_name(method);
    if (transpiler_active_has_mir(ctx) && mir_method == NULL) {
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
    ASTNode *ability_ref = ast_impl_ability_ref(impl);
    const char *ability_name = ast_impl_ability_name(impl);
    char typedef_name[128];
    char *vtable_tag = NULL;
    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (ability_name == NULL || ast_impl_ability_method_count(impl) == 0)
        return;

    ensure_ability_ref_vtable_decl(ability_ref, ctx);
    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (!ability_ref_vtable_typedef_name(
            ability_ref, typedef_name, sizeof(typedef_name), ctx))
        return;
    vtable_tag = render_effective_ability_ref_vtable_tag(
        find_ability_decl(ctx, ability_name),
        ability_ref,
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

    for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
        ASTNode *method = ast_impl_ability_method(impl, j);
        const char *method_name = ast_declaration_name(method);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        if (method_name == NULL)
            continue;
        codebuf_write(ctx->out, "    .%s = %s_%s,\n",
                      method_name,
                      role_name, method_name);
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
        || ast_role_name(role) == NULL) {
        return;
    }

    role_name = ast_role_name(role);
    for_type = transpiler_role_subject_type_name_local(role);
    if (for_type == NULL)
        return;

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        PgyTokenType op = ops[i];
        const char *suffix = operator_overload_suffix(op);
        ASTNode *method = find_role_operator_method_decl(ctx, role, op, 0);
        const char *method_name = ast_declaration_name(method);
        char fn_name[256];
        if (suffix == NULL || method == NULL
            || method->type != AST_FUNC_DECL
            || method_name == NULL
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
        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            if (p != NULL && p->name != NULL
                && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        const char *ret_type = "void";
        char ret_type_storage[128];
        char lhs_type_storage[128];
        char rhs_type_storage[128];
        const char *lhs_type = NULL;
        if (transpiler_require_ast_c_type_copy(
            ctx,
            transpiler_role_subject_type_node_local(role),
            "role operator lhs type",
            lhs_type_storage,
            sizeof(lhs_type_storage))) {
            lhs_type = lhs_type_storage;
        }
        const char *rhs_type = NULL;
        const char *rhs_name = (rhs_param != NULL && rhs_param->name != NULL)
            ? rhs_param->name : "rhs";
        char surface_desc[256];

        if (ast_func_return_type(method) != NULL) {
            if (pergyra_ast_type_to_c_copy(ast_func_return_type(method),
                    ret_type_storage,
                    sizeof(ret_type_storage))) {
                ret_type = ret_type_storage;
            }
        }
        if (!transpiler_role_ability_surface_desc(surface_desc,
                sizeof(surface_desc), "role operator parameter",
                role_name, method_name, rhs_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role operator parameter diagnostic is too long");
            return;
        }
        if (transpiler_require_ast_c_type_copy(
            ctx,
            rhs_param != NULL ? rhs_param->type : NULL,
            surface_desc,
            rhs_type_storage,
            sizeof(rhs_type_storage))) {
            rhs_type = rhs_type_storage;
        }
        if (lhs_type == NULL || rhs_type == NULL)
            return;

        codebuf_write(ctx->out,
            "\nstatic %s\noperator_%s_%s(%s lhs, %s %s)\n{\n",
            ret_type, suffix, for_type, lhs_type, rhs_type, rhs_name);
        codebuf_write(ctx->out, "    %s lhs_copy = lhs;\n", lhs_type);
        if (strcmp(ret_type, "void") == 0) {
            codebuf_write(ctx->out, "    %s_%s(&lhs_copy, %s);\n",
                role_name, method_name, rhs_name);
        } else {
            codebuf_write(ctx->out, "    return %s_%s(&lhs_copy, %s);\n",
                role_name, method_name, rhs_name);
        }
        codebuf_write(ctx->out, "}\n");
    }
}

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H */
