/* =================================================================
 * Role/Ability system emitters
 * ================================================================= */

static void
emit_hosted_methods_from_mir_or_error_local(const char *host_name,
                                            const char *anonymous_host_name,
                                            const char *host_kind,
                                            ASTNode **methods,
                                            size_t method_count,
                                            TranspilerCtx *ctx)
{
    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods[i];
        const MIRRoutine *mir_method;
        char emitted_name[256];

        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        mir_method = transpiler_find_mir_method(ctx, host_name, method);
        if (mir_method == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_MIR_TOPOLOGY_INVALID,
                PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING,
                PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
                "MIR-only C path missing routine for %s method '%s.%s'",
                host_kind != NULL ? host_kind : "host",
                host_name != NULL ? host_name : anonymous_host_name,
                method->data.func_decl.name != NULL
                    ? method->data.func_decl.name
                    : "(anonymous)");
            return;
        }

        snprintf(emitted_name, sizeof(emitted_name), "%s_%s", host_name,
            method->data.func_decl.name);
        emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
        if (ctx != NULL && ctx->backend_error != NULL)
            return;
    }
}

enum {
    PGY_PROP_CAUSE_NONE = 0,
    PGY_PROP_CAUSE_REFRESH = 1,
    PGY_PROP_CAUSE_APPLY = 2,
    PGY_PROP_CAUSE_MAINTAIN = 3,
    PGY_PROP_CAUSE_DETACH = 4,
    PGY_PROP_CAUSE_LINK = 5,
    PGY_PROP_CAUSE_UNLINK = 6,
    PGY_PROP_CAUSE_WORLD_ACTIVATE = 7,
    PGY_PROP_CAUSE_WORLD_MAINTAIN = 8,
    PGY_PROP_CAUSE_WORLD_DEACTIVATE = 9,
    PGY_PROP_CAUSE_WORLD_DERIVED = 10,
    PGY_PROP_CAUSE_ACTION = 11,
};

static void
emit_hidden_provenance_fields(TranspilerCtx *ctx,
                              const char *prefix,
                              const char *name)
{
    if (ctx == NULL || ctx->out == NULL || prefix == NULL || name == NULL)
        return;

    codebuf_write(ctx->out, "    uint32_t __%s_epoch_%s;\n", prefix, name);
    codebuf_write(ctx->out, "    int __%s_cause_%s;\n", prefix, name);
}

static void
emit_hidden_provenance_stamp(TranspilerCtx *ctx,
                             const char *self_expr,
                             const char *prefix,
                             const char *name,
                             int cause)
{
    if (ctx == NULL || self_expr == NULL || prefix == NULL || name == NULL)
        return;

    write_indent(ctx);
    codebuf_write(ctx->out, "%s->__%s_epoch_%s++;\n", self_expr, prefix, name);
    write_indent(ctx);
    codebuf_write(ctx->out, "%s->__%s_cause_%s = %d;\n",
        self_expr, prefix, name, cause);
}

static void
emit_domain_projection_sync_loop(TranspilerCtx *ctx,
                                 ASTNode **slots,
                                 size_t slot_count,
                                 ASTNode **refreshes,
                                 size_t refresh_count,
                                 const char *loop_prefix,
                                 bool early_return_if_clean)
{
    bool emitted_condition = false;

    if (ctx == NULL || ctx->out == NULL || slots == NULL || refreshes == NULL
        || loop_prefix == NULL || refresh_count == 0) {
        return;
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        emitted_condition = true;
        break;
    }
    if (!emitted_condition)
        return;

    emitted_condition = false;
    write_indent(ctx);
    codebuf_write(ctx->out, "if (");
    if (early_return_if_clean)
        codebuf_write(ctx->out, "!(");
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (emitted_condition)
            codebuf_write(ctx->out, " || ");
        codebuf_write(ctx->out, "self->__projection_dirty_%s",
            slot->data.domain_slot.slot_name);
        emitted_condition = true;
    }
    if (early_return_if_clean)
        codebuf_write(ctx->out, ")) return;\n");
    else
        codebuf_write(ctx->out, ") {\n");

    if (!early_return_if_clean)
        ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_%s_pass = 0;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_%s_pass_limit = %zu;\n",
        loop_prefix, refresh_count + 1);
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_%s_continue = true;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_%s_continue && _pgy_%s_pass < _pgy_%s_pass_limit) {\n",
        loop_prefix, loop_prefix, loop_prefix);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_%s_continue = false;\n", loop_prefix);
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_%s_pass++;\n", loop_prefix);

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        const char *target_slot_name;
        char *literal;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        target_slot_name = refresh->data.zone_refresh.object_slot_name;
        if (target_slot_name == NULL
            || refresh->data.zone_refresh.source_slot_name == NULL) {
            continue;
        }

        for (size_t j = 0; j < slot_count; j++) {
            ASTNode *slot = slots[j];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                || slot->data.domain_slot.slot_name == NULL) {
                continue;
            }
            if (strcmp(slot->data.domain_slot.slot_name, target_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0) {
                source_slot = slot;
            }
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL) {
            continue;
        }

        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            const char *source_expr = transpiler_scratch_fmt(ctx, "self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(ctx, target_decl, source_decl,
                refresh, target_type_name, source_expr);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__projection_dirty_%s) {\n",
            target_slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
            target_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n", target_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            target_slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_dirty_%s = false;\n",
            target_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "projection",
            target_slot_name, PGY_PROP_CAUSE_REFRESH);

        for (size_t dep_i = 0; dep_i < refresh_count; dep_i++) {
            ASTNode *dependent = refreshes[dep_i];
            const char *dependent_target;
            const char *dependent_source;

            if (dependent == NULL || dependent->type != AST_ZONE_REFRESH)
                continue;
            dependent_target = dependent->data.zone_refresh.object_slot_name;
            dependent_source = dependent->data.zone_refresh.source_slot_name;
            if (dependent_target == NULL || dependent_source == NULL
                || strcmp(dependent_source, target_slot_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_dirty_%s = true;\n",
                dependent_target);
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                dependent_target);
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_%s_continue = true;\n", loop_prefix);
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        free(literal);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_%s_continue) {\n", loop_prefix);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PGY_PANIC(\"projection recompute exceeded bounded pass limit\");\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (!early_return_if_clean) {
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
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
    const MIRRoutine *mir_method = transpiler_find_mir_method(ctx, role_name, method);
    const char *method_name = method->data.func_decl.name;
    if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
        transpiler_set_backend_error_with_hints(ctx, PGY_CODE_MIR_TOPOLOGY_INVALID, PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING, PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING, "MIR-only C path missing routine for role method '%s.%s'",
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
    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_MIR_TOPOLOGY_INVALID, PGY_CAUSE_MIR_TOPOLOGY_ROUTINE_MISSING, PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING, "MIR-only C path missing routine for role method '%s.%s'",
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
        if (method->type != AST_FUNC_DECL)
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
            if (p != NULL && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
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

static void
emit_included_role_impls(ASTNode *role, TranspilerCtx *ctx)
{
    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *include_stmt = role->data.role_decl.includes[i];
        ASTNode *included_role = find_role_decl(ctx, include_stmt->data.include_stmt.role_name);

        if (included_role == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve included role '%s' while emitting role '%s'",
                include_stmt->data.include_stmt.role_name != NULL
                    ? include_stmt->data.include_stmt.role_name
                    : "<role>",
                role->data.role_decl.name != NULL
                    ? role->data.role_decl.name
                    : "<role>");
            return;
        }

        for (size_t j = 0; j < included_role->data.role_decl.impl_count; j++) {
            ASTNode *impl = included_role->data.role_decl.impl_abilities[j];
            if (impl->type != AST_IMPL_ABILITY)
                continue;

            if (role_has_ability(role,
                    (impl->data.impl_ability.ability_ref != NULL
                     && impl->data.impl_ability.ability_ref->type == AST_TYPE)
                        ? impl->data.impl_ability.ability_ref->data.type.name
                        : NULL))
                continue;

            for (size_t k = 0; k < impl->data.impl_ability.method_count; k++) {
                ASTNode *method = impl->data.impl_ability.methods[k];
                if (method->type != AST_FUNC_DECL)
                    continue;
                if (role_has_method(role, method->data.func_decl.name))
                    continue;
                emit_role_method_impl(role->data.role_decl.name, method, ctx);
            }

            emit_role_vtable_instance(role->data.role_decl.name, impl, ctx);
        }
    }
}

/*
 * Ability → vtable struct typedef
 *
 *   typedef struct {
 *       RetType (*MethodName)(void* self, ParamType p1, ...);
 *       ...
 *   } AbilityName_vtable;
 */
void
emit_ability_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.ability_decl.name;

    codebuf_write(ctx->out, "\n/* Ability: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct\n{\n");

    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "    %s (*%s)(void *self", ret_type, method_name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            char *param_name = NULL;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            const char *pt = NULL;
            bool pointer_param = false;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "ability method parameter '%s.%s(%s)'",
                name != NULL ? name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
            if (p != NULL && p->type != NULL)
                param_name = render_type_name(p->type);
            pointer_param = param_name != NULL
                && is_pointer_self_host_type_name(ctx, param_name);
            codebuf_write(ctx->out, ", %s%s %s", pt, pointer_param ? " *" : "", p->name);
            free(param_name);
        }
        codebuf_write(ctx->out, ");\n");
    }

    codebuf_write(ctx->out, "} %s_vtable;\n", name);
}

/*
 * Role → vtable instance + free functions
 *
 *   static RetType RoleName_MethodName(void* self, ...) { body }
 *   static const AbilityName_vtable RoleName_AbilityName_vtable_instance = {
 *       .MethodName = RoleName_MethodName,
 *   };
 */
void
emit_role_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.role_decl.name;

