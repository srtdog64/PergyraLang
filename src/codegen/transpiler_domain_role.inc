/* =================================================================
 * Role/Ability system emitters
 * ================================================================= */

static void
emit_role_method_impl(const char *role_name, ASTNode *method, TranspilerCtx *ctx)
{
    const char *method_name = method->data.func_decl.name;
    const char *ret_type = "void";
    if (method->data.func_decl.return_type != NULL)
        ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

    /* Role methods use void *self for vtable compatibility, cast in body */
    codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *_raw_self",
                  ret_type, role_name, method_name);

    for (size_t k = 0; k < method->data.func_decl.param_count; k++) {
        FuncParam *p = method->data.func_decl.params[k];
        if (strcmp(p->name, "self") == 0 && p->type == NULL)
            continue;
        const char *pt = "int32_t";
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        codebuf_write(ctx->out, ", %s %s", pt, p->name);
    }
    codebuf_write(ctx->out, ")\n{\n");

    {
        const char *saved_class = ctx->current_class_name;
        int saved_typed = ctx->typed_var_count;
        const char *subject_name = NULL;
        /* Role methods operate on subject (pointer-self);
         * find the role's target subject so bare field access uses -> */
        ASTNode *role_node = find_role_decl(ctx, role_name);
        if (role_node != NULL
            && role_node->data.role_decl.for_type != NULL
            && role_node->data.role_decl.for_type->type == AST_TYPE
            && role_node->data.role_decl.for_type->data.type.name != NULL) {
            subject_name = role_node->data.role_decl.for_type->data.type.name;
            ctx->current_class_name = subject_name;
        }
        /* Cast void *_raw_self to concrete subject type */
        if (subject_name != NULL && is_pointer_self_host_type_name(ctx, subject_name)) {
            codebuf_write(ctx->out, "    %s *self = (%s *)_raw_self;\n", subject_name, subject_name);
            codebuf_write(ctx->out, "    (void)self;\n");
        }
        else if (subject_name != NULL) {
            codebuf_write(ctx->out, "    %s self = *(%s *)_raw_self;\n", subject_name, subject_name);
            codebuf_write(ctx->out, "    (void)self;\n");
        }
        else
            codebuf_write(ctx->out, "    (void)_raw_self;\n");

        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;

        ctx->current_class_name = saved_class;
        ctx->typed_var_count = saved_typed;
    }

    codebuf_write(ctx->out, "}\n");
}

static void
emit_role_vtable_instance(const char *role_name, ASTNode *impl, TranspilerCtx *ctx)
{
    const char *ability_name = impl->data.impl_ability.ability_name;
    if (ability_name == NULL || impl->data.impl_ability.method_count == 0)
        return;

    codebuf_write(ctx->out,
        "\nstatic const %s_vtable %s_%s_vtable_instance __attribute__((unused)) = {\n",
        ability_name, role_name, ability_name);

    for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
        ASTNode *method = impl->data.impl_ability.methods[j];
        if (method->type != AST_FUNC_DECL)
            continue;
        codebuf_write(ctx->out, "    .%s = %s_%s,\n",
                      method->data.func_decl.name,
                      role_name, method->data.func_decl.name);
    }

    codebuf_write(ctx->out, "};\n");
}

static void
emit_role_operator_aliases(ASTNode *role, TranspilerCtx *ctx)
{
    const char *role_name;
    const char *for_type;
    TokenType ops[] = {
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
        TokenType op = ops[i];
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
        const char *lhs_type = pergyra_ast_type_to_c(role->data.role_decl.for_type);
        const char *rhs_type = "int32_t";
        const char *rhs_name = (rhs_param != NULL && rhs_param->name != NULL)
            ? rhs_param->name : "rhs";

        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);
        if (rhs_param != NULL && rhs_param->type != NULL)
            rhs_type = pergyra_ast_type_to_c(rhs_param->type);

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
            codebuf_write(ctx->out, "/* unresolved include role: %s */\n",
                          include_stmt->data.include_stmt.role_name);
            continue;
        }

        for (size_t j = 0; j < included_role->data.role_decl.impl_count; j++) {
            ASTNode *impl = included_role->data.role_decl.impl_abilities[j];
            if (impl->type != AST_IMPL_ABILITY)
                continue;

            if (role_has_ability(role, impl->data.impl_ability.ability_name))
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
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
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

    codebuf_write(ctx->out, "\n/* Role: %s */\n", name);
    emit_included_role_impls(node, ctx);

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method->type != AST_FUNC_DECL)
                    continue;
                emit_role_method_impl(name, method, ctx);
            }

            emit_role_vtable_instance(name, impl, ctx);

        } else if (impl->type == AST_OVERRIDE_FUNC) {
            ASTNode *func = impl->data.override_func.func_decl;
            if (func == NULL || func->type != AST_FUNC_DECL)
                continue;

            const char *method_name = func->data.func_decl.name;
            const char *ret_type = "void";
            if (func->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(func->data.func_decl.return_type);

            codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                          ret_type, name, method_name);

            for (size_t k = 0; k < func->data.func_decl.param_count; k++) {
                FuncParam *p = func->data.func_decl.params[k];
                if (strcmp(p->name, "self") == 0 && p->type == NULL)
                    continue;
                const char *pt = "int32_t";
                if (p->type != NULL)
                    pt = pergyra_ast_type_to_c(p->type);
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");

            ctx->indent++;
            if (func->data.func_decl.body != NULL)
                emit_block(func->data.func_decl.body, ctx);
            ctx->indent--;

            codebuf_write(ctx->out, "}\n");
        }
    }

    emit_role_operator_aliases(node, ctx);
}

/* =================================================================
 * Party system emitters
 * ================================================================= */

/*
 * Party → C struct with role slot pointers + shared fields
 * Party methods → free functions
 */
void
emit_party_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.party_decl.name;

    codebuf_write(ctx->out, "\n/* Party: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Role slots as void* + vtable pointer */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        const char *slot_name = rs->data.role_slot.slot_name;
        bool is_dyn = rs->data.role_slot.is_dynamic;
        codebuf_write(ctx->out, "    void *%s;\n", slot_name);
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab != NULL && ab->data.type.name != NULL) {
                if (is_dyn) {
                    /* dyn: mutable vtable pointer — swappable at runtime */
                    codebuf_write(ctx->out,
                        "    const %s_vtable *%s_%s_vt; /* dyn */\n",
                        ab->data.type.name, slot_name, ab->data.type.name);
                } else {
                    codebuf_write(ctx->out,
                        "    const %s_vtable *%s_%s_vt;\n",
                        ab->data.type.name, slot_name, ab->data.type.name);
                }
            }
        }
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods as free functions */
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        ASTNode *method = node->data.party_decl.methods[i];
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");

        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;

        codebuf_write(ctx->out, "}\n");
    }

    /* Emit bind helpers for dyn role slots */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        if (!rs->data.role_slot.is_dynamic)
            continue;
        const char *slot_name = rs->data.role_slot.slot_name;
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab == NULL || ab->data.type.name == NULL)
                continue;
            const char *ab_name = ab->data.type.name;
            codebuf_write(ctx->out,
                "\nstatic inline void\n"
                "%s_bind_%s(%s *self, void *impl, const %s_vtable *vt)\n"
                "{\n"
                "    self->%s = impl;\n"
                "    self->%s_%s_vt = vt;\n"
                "}\n",
                name, slot_name, name, ab_name,
                slot_name,
                slot_name, ab_name);
        }
    }
}

/* =================================================================
 * Systemic/World system emitters
 * ================================================================= */

void
emit_systemic_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.systemic_decl.name;

    codebuf_write(ctx->out, "\n/* Systemic: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Party slots */
    for (size_t i = 0; i < node->data.systemic_decl.party_count; i++) {
        ASTNode *ps = node->data.systemic_decl.party_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ps->data.systemic_slot.party_type,
            ps->data.systemic_slot.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.systemic_decl.shared_count; i++) {
        ASTNode *shared = node->data.systemic_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods */
    for (size_t i = 0; i < node->data.systemic_decl.method_count; i++) {
        ASTNode *method = node->data.systemic_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.func_decl.body != NULL)
            emit_block(method->data.func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_relation_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.relation_decl.name;

    codebuf_write(ctx->out, "\n/* Relation: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
        ASTNode *slot = node->data.relation_decl.slots[i];
        const char *ft = "int32_t";
        if (slot->data.domain_slot.type != NULL)
            ft = pergyra_ast_type_to_c(slot->data.domain_slot.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (slot->data.domain_slot.is_dto
            || domain_slot_is_projection_target_local(slot,
                node->data.relation_decl.refreshes,
                node->data.relation_decl.refresh_count)) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
        ASTNode *shared = node->data.relation_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
        ASTNode *slot = node->data.relation_decl.slots[i];
        if (slot->data.domain_slot.is_dto
            || domain_slot_is_projection_target_local(slot,
                node->data.relation_decl.refreshes,
                node->data.relation_decl.refresh_count)) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                slot->data.domain_slot.slot_name);
        }
    }
    for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.relation_decl.refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;
        for (size_t j = 0; j < node->data.relation_decl.slot_count; j++) {
            ASTNode *slot = node->data.relation_decl.slots[j];
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.object_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0)
                source_slot = slot;
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL)
            continue;
        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            char *source_expr = strdup_fmt("self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(ctx, target_decl, source_decl,
                target_type_name, source_expr);
            free(source_expr);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            refresh->data.zone_refresh.object_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            refresh->data.zone_refresh.object_slot_name);
        free(literal);
    }
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
        ASTNode *method = node->data.relation_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
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
        ctx->indent++;
        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_relation_name = ctx->current_relation_name;
            ctx->current_relation_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    if (is_pointer_self_host_type_name(ctx, type_name)) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                        if (entry != NULL)
                            entry->is_subject_ref = true;
                    }
                    free(type_name);
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_relation_name = saved_relation_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_effect_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.effect_decl.name;

    codebuf_write(ctx->out, "\n/* Effect: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
        ASTNode *slot = node->data.effect_decl.slots[i];
        const char *ft = "int32_t";
        if (slot->data.domain_slot.type != NULL)
            ft = pergyra_ast_type_to_c(slot->data.domain_slot.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (slot->data.domain_slot.is_dto
            || domain_slot_is_projection_target_local(slot,
                node->data.effect_decl.refreshes,
                node->data.effect_decl.refresh_count)) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
        ASTNode *shared = node->data.effect_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
        ASTNode *slot = node->data.effect_decl.slots[i];
        if (slot->data.domain_slot.is_dto
            || domain_slot_is_projection_target_local(slot,
                node->data.effect_decl.refreshes,
                node->data.effect_decl.refresh_count)) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                slot->data.domain_slot.slot_name);
        }
    }
    for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.effect_decl.refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;
        for (size_t j = 0; j < node->data.effect_decl.slot_count; j++) {
            ASTNode *slot = node->data.effect_decl.slots[j];
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.object_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0)
                source_slot = slot;
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL)
            continue;
        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            char *source_expr = strdup_fmt("self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(ctx, target_decl, source_decl,
                target_type_name, source_expr);
            free(source_expr);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            refresh->data.zone_refresh.object_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            refresh->data.zone_refresh.object_slot_name);
        free(literal);
    }
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
        ASTNode *method = node->data.effect_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
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
        ctx->indent++;
        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_effect_name = ctx->current_effect_name;
            ctx->current_effect_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    if (is_pointer_self_host_type_name(ctx, type_name)) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                        if (entry != NULL)
                            entry->is_subject_ref = true;
                    }
                    free(type_name);
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_effect_name = saved_effect_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_zone_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.zone_decl.name;

    codebuf_write(ctx->out, "\n/* Zone: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        const char *ft = "int32_t";
        if (slot->data.domain_slot.type != NULL)
            ft = pergyra_ast_type_to_c(slot->data.domain_slot.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (slot->data.domain_slot.is_dto
            || domain_slot_is_projection_target_local(slot,
                node->data.zone_decl.refreshes,
                node->data.zone_decl.refresh_count)) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            slot->data.zone_layer_slot.layer_type,
            slot->data.zone_layer_slot.slot_name);
        codebuf_write(ctx->out, "    bool __layer_active_%s;\n",
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
        ASTNode *shared = node->data.zone_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        codebuf_write(ctx->out, "    bool __state_%s;\n",
            state->data.zone_state.state_name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = false;\n",
            state->data.zone_state.state_name);
    }
    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        if (slot->data.domain_slot.is_dto
            || domain_slot_is_projection_target_local(slot,
                node->data.zone_decl.refreshes,
                node->data.zone_decl.refresh_count)) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__projection_ready_%s = false;\n",
                slot->data.domain_slot.slot_name);
        }
    }
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.zone_decl.refreshes[i];
        ASTNode *target_slot = NULL;
        ASTNode *source_slot = NULL;
        ASTNode *target_decl = NULL;
        ASTNode *source_decl = NULL;
        const char *target_type_name = NULL;
        const char *source_type_name = NULL;
        char *literal;
        for (size_t j = 0; j < node->data.zone_decl.slot_count; j++) {
            ASTNode *slot = node->data.zone_decl.slots[j];
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.object_slot_name) == 0)
                target_slot = slot;
            if (strcmp(slot->data.domain_slot.slot_name,
                       refresh->data.zone_refresh.source_slot_name) == 0)
                source_slot = slot;
        }
        if (target_slot == NULL || source_slot == NULL
            || target_slot->data.domain_slot.type == NULL
            || source_slot->data.domain_slot.type == NULL)
            continue;
        target_type_name = target_slot->data.domain_slot.type->data.type.name;
        source_type_name = source_slot->data.domain_slot.type->data.type.name;
        target_decl = find_class_decl(ctx, target_type_name);
        source_decl = find_class_decl(ctx, source_type_name);
        {
            char *source_expr = strdup_fmt("self->%s",
                refresh->data.zone_refresh.source_slot_name);
            literal = emit_projection_literal(ctx, target_decl, source_decl,
                target_type_name, source_expr);
            free(source_expr);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->%s = %s;\n",
            refresh->data.zone_refresh.object_slot_name, literal);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__projection_ready_%s = true;\n",
            refresh->data.zone_refresh.object_slot_name);
        free(literal);
    }

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        if (apply->data.zone_apply.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                apply->data.zone_apply.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (!state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              apply->data.zone_apply.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_zone_bind_effect_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name, ctx);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            apply->data.zone_apply.effect_slot_name);
        emit_zone_bind_effect_layer(ctx->out, node,
            apply->data.zone_apply.effect_slot_name,
            apply->data.zone_apply.target_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          apply->data.zone_apply.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          apply->data.zone_apply.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            maintain->data.zone_maintain_effect.effect_slot_name);
        emit_zone_bind_effect_layer(ctx->out, node,
            maintain->data.zone_maintain_effect.effect_slot_name,
            maintain->data.zone_maintain_effect.target_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_effect.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = true;\n",
            maintain->data.zone_maintain_state.state_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (strcmp(state->data.zone_state.state_name,
                       maintain->data.zone_maintain_state.state_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                    state->data.zone_state.layer_slot_name);
                if (!state->data.zone_state.is_relation) {
                    emit_zone_bind_effect_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name, ctx);
                } else {
                    emit_zone_bind_relation_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name,
                        state->data.zone_state.right_slot_name, ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        if (detach->data.zone_detach.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                detach->data.zone_detach.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (!state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              detach->data.zone_detach.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        state->data.zone_state.layer_slot_name);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            detach->data.zone_detach.effect_slot_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          detach->data.zone_detach.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          detach->data.zone_detach.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        if (link->data.zone_link.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                link->data.zone_link.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              link->data.zone_link.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_zone_bind_relation_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name,
                        state->data.zone_state.right_slot_name, ctx);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            link->data.zone_link.relation_slot_name);
        emit_zone_bind_relation_layer(ctx->out, node,
            link->data.zone_link.relation_slot_name,
            link->data.zone_link.left_slot_name,
            link->data.zone_link.right_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          link->data.zone_link.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          link->data.zone_link.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          link->data.zone_link.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            maintain->data.zone_maintain_relation.relation_slot_name);
        emit_zone_bind_relation_layer(ctx->out, node,
            maintain->data.zone_maintain_relation.relation_slot_name,
            maintain->data.zone_maintain_relation.left_slot_name,
            maintain->data.zone_maintain_relation.right_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          maintain->data.zone_maintain_relation.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        if (unlink->data.zone_unlink.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                unlink->data.zone_unlink.state_name);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              unlink->data.zone_unlink.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        state->data.zone_state.layer_slot_name);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            unlink->data.zone_unlink.relation_slot_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          unlink->data.zone_unlink.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          unlink->data.zone_unlink.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          unlink->data.zone_unlink.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    state->data.zone_state.state_name);
            }
        }
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.zone_decl.method_count; i++) {
        ASTNode *method = node->data.zone_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
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
        ctx->indent++;
        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_zone_name = ctx->current_zone_name;
            ctx->current_zone_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    if (is_pointer_self_host_type_name(ctx, type_name)) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                        if (entry != NULL)
                            entry->is_subject_ref = true;
                    }
                    free(type_name);
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_zone_name = saved_zone_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_world_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.world_decl.name;

    codebuf_write(ctx->out, "\n/* World: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Systemic instances */
    for (size_t i = 0; i < node->data.world_decl.systemic_count; i++) {
        ASTNode *ws = node->data.world_decl.systemics[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ws->data.world_systemic.systemic_type,
            ws->data.world_systemic.slot_name);
    }

    /* Zone instances */
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            wz->data.world_zone.zone_type,
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    bool __zone_active_%s;\n",
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    bool __zone_dirty_%s;\n",
            wz->data.world_zone.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        const char *ft = "int32_t";
        if (shared->data.party_shared.type != NULL)
            ft = pergyra_ast_type_to_c(shared->data.party_shared.type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        codebuf_write(ctx->out, "    bool __zone_state_%s;\n",
            state->data.world_state.state_name);
    }
    codebuf_write(ctx->out, "    bool __world_derived_dirty;\n");

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: reset */\n");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_active_%s = self->__zone_active_%s;\n",
            wz->data.world_zone.slot_name, wz->data.world_zone.slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_active_%s = false;\n",
            wz->data.world_zone.slot_name);
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_needs_derived = self->__world_derived_dirty;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: directives */\n");
    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *act = node->data.world_decl.activations[i];
        const char *slot_name = act->data.world_activate.zone_slot_name;
        if (slot_name == NULL && act->data.world_activate.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, act->data.world_activate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (world_has_zone_slot(node, act->data.world_activate.state_name))
                slot_name = act->data.world_activate.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n", slot_name);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *mnt = node->data.world_decl.maintained_zones[i];
        const char *slot_name = mnt->data.world_maintain.zone_slot_name;
        if (slot_name == NULL && mnt->data.world_maintain.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, mnt->data.world_maintain.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (world_has_zone_slot(node, mnt->data.world_maintain.state_name))
                slot_name = mnt->data.world_maintain.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n", slot_name);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *act = node->data.world_decl.deactivations[i];
        const char *slot_name = act->data.world_deactivate.zone_slot_name;
        if (slot_name == NULL && act->data.world_deactivate.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, act->data.world_deactivate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (world_has_zone_slot(node, act->data.world_deactivate.state_name))
                slot_name = act->data.world_deactivate.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = false;\n", slot_name);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        const char *slot_name = wz->data.world_zone.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__zone_active_%s != _pgy_prev_active_%s) {\n",
            slot_name, slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = true;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_needs_derived = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world zone sync pass */\n");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        const char *slot_name = wz->data.world_zone.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__zone_dirty_%s) {\n", slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s_sync(&self->%s);\n",
            wz->data.world_zone.zone_type, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = false;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_needs_derived = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world derived pass */\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_world_needs_derived) {\n");
    ctx->indent++;
    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        const char *expr_fmt = "self->__zone_active_%s";
        const char *detail_name = state->data.world_state.detail_name;
        write_indent(ctx);
        switch (state->data.world_state.source_kind) {
        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY: {
            bool first = true;
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state->data.world_state.state_name);
            codebuf_write(ctx->out, "(");
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                if (input_name == NULL)
                    continue;
                if (!first) {
                    codebuf_write(ctx->out,
                        state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                            ? " && " : " || ");
                }
                if (world_has_zone_slot(node, input_name))
                    codebuf_write(ctx->out, "self->__zone_active_%s", input_name);
                else
                    codebuf_write(ctx->out, "self->__zone_state_%s", input_name);
                first = false;
            }
            if (first)
                codebuf_write(ctx->out,
                    state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                        ? "true" : "false");
            codebuf_write(ctx->out, ");\n");
            break;
        }
        case WORLD_STATE_SOURCE_PROJECTION:
            expr_fmt = "(self->__zone_active_%s && self->%s.__projection_ready_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_LAYER:
            expr_fmt = "(self->__zone_active_%s && self->%s.__layer_active_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_STATE:
            expr_fmt = "(self->__zone_active_%s && self->%s.__state_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_ZONE:
        default:
            codebuf_write(ctx->out, "self->__zone_state_%s = self->__zone_active_%s;\n",
                state->data.world_state.state_name,
                state->data.world_state.zone_slot_name);
            break;
        }
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "self->__world_derived_dirty = false;\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    /* Methods */
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        ASTNode *method = node->data.world_decl.methods[i];
        if (method->type != AST_FUNC_DECL) continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
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
        ctx->indent++;
        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_world_name = ctx->current_world_name;
            ctx->current_world_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    if (is_pointer_self_host_type_name(ctx, type_name)) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                        if (entry != NULL)
                            entry->is_subject_ref = true;
                    }
                    free(type_name);
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            for (size_t j = 0; j < node->data.world_decl.zone_count; j++) {
                ASTNode *zone = node->data.world_decl.zones[j];
                const char *slot_name = zone != NULL
                    ? zone->data.world_zone.slot_name
                    : NULL;
                if (slot_name == NULL)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__zone_dirty_%s = true;\n", slot_name);
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__world_derived_dirty = true;\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", name);
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_world_name = saved_world_name;
        }
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Async system emitters
 * ================================================================= */

void
emit_actor_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.actor_decl.name;

    codebuf_write(ctx->out, "\n/* Actor: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.actor_decl.field_count; i++) {
        ClassField *f = node->data.actor_decl.fields[i];
        const char *ft = "int32_t";
        if (f->type != NULL)
            ft = pergyra_ast_type_to_c(f->type);
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for actor host %s */\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods (actor methods are emitted as free functions) */
    for (size_t i = 0; i < node->data.actor_decl.method_count; i++) {
        ASTNode *method = node->data.actor_decl.methods[i];
        if (method == NULL) continue;

        const char *method_name = method->data.async_func_decl.name;
        const char *ret_type = "void";
        if (method->data.async_func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.async_func_decl.return_type);

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                      ret_type, name, method_name, name);
        for (size_t j = 0; j < method->data.async_func_decl.param_count; j++) {
            FuncParam *p = method->data.async_func_decl.params[j];
            if (p != NULL && p->type == NULL && p->name != NULL
                && strcmp(p->name, "self") == 0)
                continue;
            const char *pt = "int32_t";
            if (p->type != NULL)
                pt = pergyra_ast_type_to_c(p->type);
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        ctx->indent++;
        if (method->data.async_func_decl.body != NULL)
            emit_block(method->data.async_func_decl.body, ctx);
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }
}

void
emit_select_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    size_t case_count = node->data.select_stmt.case_count;

    codebuf_write(ctx->out, "\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* select */\n");

    if (case_count == 0) {
        if (node->data.select_stmt.default_case != NULL)
            emit_statement(node->data.select_stmt.default_case, ctx);
        return;
    }

    for (size_t i = 0; i < case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        ASTNode *channel = NULL;
        ASTNode *body = NULL;
        const char *bind_name = NULL;
        bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
        const char *inner = "Int";

        if (!valid_case || bind_name == NULL || channel == NULL
            || channel->type != AST_IDENTIFIER)
            continue;

        {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                inner = slot_inner_type_name(type_name);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "%s _sel_recv_%zu;\n",
                      pergyra_type_to_c(inner), i);
    }

    {
        int select_id = ctx->tmp_counter++;
        write_indent(ctx);
        codebuf_write(ctx->out, "static unsigned int _sel_rr_%d = 0;\n", select_id);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "unsigned int _sel_start_%d = _sel_rr_%d++ %% %zu;\n",
            select_id, select_id, case_count);
        write_indent(ctx);
        codebuf_write(ctx->out, "switch (_sel_start_%d) {\n", select_id);
        ctx->indent++;

        for (size_t start = 0; start < case_count; start++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "case %zu:\n", start);
            ctx->indent++;

            for (size_t offset = 0; offset < case_count; offset++) {
                size_t i = (start + offset) % case_count;
                ASTNode *c = node->data.select_stmt.cases[i];
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
                const char *inner = "Int";

                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                    const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
                    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                        inner = slot_inner_type_name(type_name);
                }

                write_indent(ctx);
                if (offset == 0) {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "if (1) { /* select case %zu */\n", i);
                    }
                } else {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "} else if (1) { /* select case %zu */\n", i);
                    }
                }
                ctx->indent++;
                if (valid_case) {
                    if (bind_name != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s %s = _sel_recv_%zu;\n",
                                      pergyra_type_to_c(inner), bind_name, i);
                    } else if (channel != NULL) {
                        char *recv = emit_channel_recv(ast_create_channel_recv(channel), ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "(void)%s;\n", recv);
                        free(recv);
                    }
                    if (body != NULL)
                        emit_statement(body, ctx);
                } else if (c != NULL) {
                    emit_statement(c, ctx);
                }
                ctx->indent--;
            }

            if (node->data.select_stmt.default_case != NULL) {
                write_indent(ctx);
                codebuf_write(ctx->out, "} else { /* default */\n");
                ctx->indent++;
                emit_statement(node->data.select_stmt.default_case, ctx);
                ctx->indent--;
            }

            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "break;\n");
            ctx->indent--;
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Event system emitters
 * ================================================================= */

void
emit_event_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.event_decl.name;
    char *event_type = strdup_fmt("%s_Event", name);
    
    /* Generate event handler type typedef */
    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);
    
    /* Build parameter types string */
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);
    
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt, param->data.let_decl.name);
    }
    
    codebuf_write(ctx->out, ");\n");
    
    /* Generate event struct with handler array */
    codebuf_write(ctx->out, "typedef struct {\n");
    codebuf_write(ctx->out, "    %s_Handler handlers[PGY_EVENT_MAX_HANDLERS];\n", name);
    codebuf_write(ctx->out, "    void* contexts[PGY_EVENT_MAX_HANDLERS];\n");
    codebuf_write(ctx->out, "    size_t count;\n");
    codebuf_write(ctx->out, "    bool is_invoking;\n");
    codebuf_write(ctx->out, "    bool pending_changes;\n");
    codebuf_write(ctx->out, "} %s;\n", event_type);
    codebuf_write(ctx->out, "static %s %s;\n", event_type, name);
    
    /* Generate inline init function */
    codebuf_write(ctx->out, "static inline void %s_INIT(%s* e) {\n", name, event_type);
    codebuf_write(ctx->out, "    memset(e, 0, sizeof(*e));\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate subscribe function */
    codebuf_write(ctx->out, "static inline void %s_SUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    if (e->count < PGY_EVENT_MAX_HANDLERS) {\n");
    codebuf_write(ctx->out, "        e->handlers[e->count++] = h;\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate unsubscribe function */
    codebuf_write(ctx->out, "static inline void %s_UNSUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        if (e->handlers[i] == h) {\n");
    codebuf_write(ctx->out, "            for (size_t j = i; j < e->count - 1; j++) {\n");
    codebuf_write(ctx->out, "                e->handlers[j] = e->handlers[j + 1];\n");
    codebuf_write(ctx->out, "            }\n");
    codebuf_write(ctx->out, "            e->count--;\n");
    codebuf_write(ctx->out, "            break;\n");
    codebuf_write(ctx->out, "        }\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");
    
    /* Generate invoke function */
    codebuf_write(ctx->out, "static inline void %s_INVOKE(%s* e", name, event_type);
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        codebuf_write(ctx->out, ", %s %s", pt, param->data.let_decl.name);
    }
    codebuf_write(ctx->out, ") {\n");
    codebuf_write(ctx->out, "    e->is_invoking = true;\n");
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        e->handlers[i](");
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s", node->data.event_decl.params[i]->data.let_decl.name);
    }
    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "    e->is_invoking = false;\n");
    codebuf_write(ctx->out, "}\n");
    free(event_type);
}

void
emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);
    
    write_indent(ctx);
    codebuf_write(ctx->out, "%s_SUBSCRIBE(&%s, %s);\n", 
                  event_expr, event_expr, handler_expr);
    
    free(event_expr);
    free(handler_expr);
}

void
emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);
    
    write_indent(ctx);
    codebuf_write(ctx->out, "%s_UNSUBSCRIBE(&%s, %s);\n", 
                  event_expr, event_expr, handler_expr);
    
    free(event_expr);
    free(handler_expr);
}
