static bool
any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref);
static ASTNode *
any_subject_role_find_base_ability_impl(ASTNode *program, const char *ability_name);
static bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx);

static void
intent_step_format_contract_source_summary(const ASTNode *step,
                                           char *buffer,
                                           size_t buffer_size)
{
    size_t used = 0;
    bool has_any = false;

    if (buffer == NULL || buffer_size == 0 || step == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }

    buffer[0] = '\0';

    if (step->data.intent_step.who_count > 0
        && !step->data.intent_step.inferred_who_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared who on step",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.where_type != NULL
        && !step->data.intent_step.inherited_where_from_action
        && !step->data.intent_step.inferred_where_from_transfer
        && step->data.intent_step.where_type->type == AST_TYPE
        && step->data.intent_step.where_type->data.type.name != NULL) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared zone on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.where_type->data.type.name);
        has_any = true;
    }
    if (step->data.intent_step.required_ability_count > 0
        && !step->data.intent_step.inherited_requires_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared requires on step",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.causes_effect != NULL
        && !step->data.intent_step.inherited_causes_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared causes on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.causes_effect);
        has_any = true;
    }
    if (step->data.intent_step.authorized_by_count > 0
        && !step->data.intent_step.inherited_authorized_by_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared authorized by on step",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.using_expr != NULL
        && !step->data.intent_step.inferred_using_from_transfer
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER
        && step->data.intent_step.using_expr->data.identifier.name != NULL) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared using on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.using_expr->data.identifier.name);
        has_any = true;
    }

    if (step->data.intent_step.inferred_who_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inferred who from matching action",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.inherited_where_from_action) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inherited zone from matching action: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.inherited_requires_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inherited requires from matching action",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.inherited_causes_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inherited causes from matching action: %s",
            has_any ? "\n" : "",
            step->data.intent_step.causes_effect != NULL
                ? step->data.intent_step.causes_effect
                : "<effect>");
        has_any = true;
    }
    if (step->data.intent_step.inherited_authorized_by_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inherited authorized by from matching action",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.inferred_where_from_transfer) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inferred zone from transfer target: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.inferred_using_from_transfer) {
        const char *using_name =
            (step->data.intent_step.using_expr != NULL
             && step->data.intent_step.using_expr->type == AST_IDENTIFIER
             && step->data.intent_step.using_expr->data.identifier.name != NULL)
                ? step->data.intent_step.using_expr->data.identifier.name
                : "<binding>";
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- inferred using from transfer target: %s",
            has_any ? "\n" : "", using_name);
    }
}

static ClassField *
find_nominal_field_by_name(ASTNode *decl, const char *field_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || field_name == NULL)
        return NULL;

    for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
        ClassField *field = decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return field;
        }
    }

    return NULL;
}

static ClassField *
find_subject_surface_field_by_name(ASTNode *program_root,
                                   ASTNode *subject_decl,
                                   const char *field_name,
                                   const char **container_field_name_out)
{
    if (container_field_name_out != NULL)
        *container_field_name_out = NULL;

    if (program_root == NULL || subject_decl == NULL
        || subject_decl->type != AST_CLASS_DECL || field_name == NULL) {
        return NULL;
    }

    {
        ClassField *direct = find_nominal_field_by_name(subject_decl, field_name);
        if (direct != NULL)
            return direct;
    }

    for (size_t i = 0; i < subject_decl->data.class_decl.field_count; i++) {
        ClassField *field = subject_decl->data.class_decl.fields[i];
        ASTNode *vessel_decl;
        ClassField *nested;

        if (field == NULL || !field->is_vessel_field || field->type == NULL
            || field->type->type != AST_TYPE
            || field->type->data.type.name == NULL) {
            continue;
        }

        vessel_decl = find_type_decl_by_name(program_root, field->type->data.type.name);
        if (vessel_decl == NULL || vessel_decl->type != AST_CLASS_DECL
            || vessel_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
            continue;
        }

        nested = find_nominal_field_by_name(vessel_decl, field_name);
        if (nested != NULL) {
            if (container_field_name_out != NULL)
                *container_field_name_out = field->name;
            return nested;
        }
    }

    return NULL;
}

static void
validate_ability_require_fields_for_role(ASTNode *role_decl,
                                         ASTNode *ability_decl,
                                         SemanticContext *ctx)
{
    ASTNode *bound_decl;

    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL
        || ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ctx == NULL
        || role_decl->data.role_decl.for_type == NULL
        || role_decl->data.role_decl.for_type->type != AST_TYPE
        || role_decl->data.role_decl.for_type->data.type.name == NULL) {
        return;
    }

    bound_decl = find_type_decl_by_name(
        ctx->program_root, role_decl->data.role_decl.for_type->data.type.name);
    if (bound_decl == NULL || !decl_is_subject_host(bound_decl))
        return;

    for (size_t i = 0; i < ability_decl->data.ability_decl.require_count; i++) {
        ASTNode *req = ability_decl->data.ability_decl.require_fields[i];
        ClassField *field;
        const char *container_field_name = NULL;
        Type *required_type;
        Type *field_type;

        if (req == NULL || req->type != AST_REQUIRE_FIELD
            || req->data.require_field.name == NULL
            || req->data.require_field.type == NULL) {
            continue;
        }

        field = find_subject_surface_field_by_name(
            ctx->program_root, bound_decl, req->data.require_field.name,
            &container_field_name);
        if (field == NULL) {
            semantic_error(ctx, role_decl,
                "Role '%s' implements ability '%s', but subject '%s' is missing required field '%s'",
                role_decl->data.role_decl.name != NULL ? role_decl->data.role_decl.name : "<role>",
                ability_decl->data.ability_decl.name != NULL ? ability_decl->data.ability_decl.name : "<ability>",
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                req->data.require_field.name);
            continue;
        }

        required_type = resolve_type_node(req->data.require_field.type, ctx);
        field_type = resolve_type_node(field->type, ctx);
        if (required_type != NULL && field_type != NULL
            && !type_is_assignable(field_type, required_type)) {
            semantic_error(ctx, role_decl,
                "Role '%s' implements ability '%s', but subject '%s.%s%s%s' has type '%s' and ability requires '%s'",
                role_decl->data.role_decl.name != NULL ? role_decl->data.role_decl.name : "<role>",
                ability_decl->data.ability_decl.name != NULL ? ability_decl->data.ability_decl.name : "<ability>",
                bound_decl->data.class_decl.name != NULL ? bound_decl->data.class_decl.name : "<subject>",
                container_field_name != NULL ? container_field_name : "",
                container_field_name != NULL ? "." : "",
                req->data.require_field.name,
                field_type->name != NULL ? field_type->name : "<type>",
                required_type->name != NULL ? required_type->name : "<type>");
        }
    }
}

bool
type_check_role_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.role_decl.name;

    /* Register role as a symbol */
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ROLE;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of role '%s'", name);
        symbol_destroy(sym);
        return false;
    }
    scope_declare(ctx->scope, sym);

    if (node->data.role_decl.generic_params != NULL
        && node->data.role_decl.generic_params->count > 0) {
        validate_generic_param_defaults(node->data.role_decl.generic_params,
            ctx, node, "role");
    }

    /* Check for_type exists */
    if (node->data.role_decl.for_type != NULL) {
        Type *bound_type = resolve_type_node(node->data.role_decl.for_type, ctx);
        if (bound_type != NULL
            && node->data.role_decl.for_type->type == AST_TYPE
            && node->data.role_decl.for_type->data.type.name != NULL) {
            ASTNode *type_decl = find_type_decl_by_name(
                ctx->program_root, node->data.role_decl.for_type->data.type.name);
            if (type_decl != NULL && !decl_is_subject_host(type_decl)) {
                semantic_error(ctx, node->data.role_decl.for_type,
                    "Role '%s' must be bound to a subject or primitive domain; '%s' is not a subject",
                    name,
                    node->data.role_decl.for_type->data.type.name);
            }
        }
    }

    validate_where_clause_bounds(node->data.role_decl.where_clause, ctx, node);

    /* Check includes reference existing roles */
    for (size_t i = 0; i < node->data.role_decl.include_count; i++) {
        ASTNode *inc = node->data.role_decl.includes[i];
        const char *role_name = inc->data.include_stmt.role_name;
        Symbol *role_sym = scope_lookup(ctx->scope, role_name);
        if (role_sym == NULL) {
            semantic_warning(ctx, inc,
                "Included role '%s' not found in current scope", role_name);
        }
    }

    /* Check impl ability blocks */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            /* Check that the ability exists */
            if (impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL) {
                const char *ability_name = impl->data.impl_ability.ability_ref->data.type.name;
                ASTNode *ability_decl = find_ability_decl_by_name(
                    ctx->program_root, ability_name);
                Symbol *ab = scope_lookup(ctx->scope,
                    ability_name);
                if (ability_decl != NULL
                           && ability_decl->type == AST_ABILITY_DECL
                           && !explicit_type_reference_allowed(ability_decl, node, ctx)) {
                    semantic_error(ctx, impl,
                        "Role '%s' cannot implement non-exported ability '%s' from another module",
                        name != NULL ? name : "<role>",
                        ability_name);
                } else if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_warning(ctx, impl,
                        "Ability '%s' not found in current scope",
                        ability_name);
                } else if (ability_decl != NULL
                           && ability_decl->type == AST_ABILITY_DECL
                           && ability_decl->data.ability_decl.is_innate
                           && ability_decl->origin_path != NULL
                           && node->origin_path != NULL
                           && strcmp(ability_decl->origin_path,
                                     node->origin_path) != 0) {
                    semantic_error(ctx, impl,
                        "innate ability '%s' cannot be implemented outside its declaring module",
                        ability_name);
                } else if (ability_decl != NULL
                           && ability_decl->type == AST_ABILITY_DECL) {
                    size_t arg_count = impl->data.impl_ability.ability_ref->data.type.generic_args != NULL
                        ? impl->data.impl_ability.ability_ref->data.type.generic_args->count : 0;
                    size_t param_count = ability_decl->data.ability_decl.generic_params != NULL
                        ? ability_decl->data.ability_decl.generic_params->count : 0;

                    if (arg_count > 0 && param_count == 0) {
                        char *impl_text =
                            ability_ref_display(impl->data.impl_ability.ability_ref);
                        semantic_error(ctx, impl,
                            "Ability '%s' does not accept generic type arguments in impl clauses.\n"
                            "Reason:\n"
                            "- impl ability uses '%s'\n"
                            "- ability declaration '%s' is non-generic\n"
                            "Fix:\n"
                            "- remove the generic arguments from '%s'\n"
                            "- or declare '%s' with matching generic parameters",
                            ability_name,
                            impl_text != NULL ? impl_text : ability_name,
                            ability_name,
                            ability_name,
                            ability_name);
                        free(impl_text);
                    } else if (arg_count != param_count) {
                        char *impl_text =
                            ability_ref_display(impl->data.impl_ability.ability_ref);
                        semantic_error(ctx, impl,
                            "Ability '%s' requires %zu generic argument(s) in impl clauses, got %zu.\n"
                            "Reason:\n"
                            "- impl ability uses '%s'\n"
                            "- ability declaration '%s' expects %zu type parameter(s)\n"
                            "Fix:\n"
                            "- pass exactly %zu generic argument(s) to '%s'\n"
                            "- or change the ability declaration to match the intended arity",
                            ability_name, param_count, arg_count,
                            impl_text != NULL ? impl_text : ability_name,
                            ability_name, param_count,
                            param_count, ability_name);
                        free(impl_text);
                    } else {
                        for (size_t k = 0; k < arg_count; k++) {
                            GenericParam *gp =
                                impl->data.impl_ability.ability_ref->data.type.generic_args->params[k];
                            ASTNode *arg = gp != NULL ? gp->constraint : NULL;
                            if (arg != NULL)
                                resolve_type_node(arg, ctx);
                        }
                    }
                    validate_ability_decl_where_clause_reference(
                        ability_decl,
                        impl->data.impl_ability.ability_ref,
                        impl,
                        ctx,
                        "role",
                        name != NULL ? name : "<role>");
                    validate_ability_require_fields_for_role(node, ability_decl, ctx);
                }
            }

            /* Type-check each method implementation */
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                type_check_func_decl(impl->data.impl_ability.methods[j], ctx);
            }
        } else if (impl->type == AST_OVERRIDE_FUNC) {
            /* Type-check the overridden function */
            if (impl->data.override_func.func_decl != NULL)
                type_check_func_decl(impl->data.override_func.func_decl, ctx);
        }
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_party_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.party_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_PARTY;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        /* Forward-declared in Pass 1 — update kind */
        existing->kind = SYMBOL_PARTY;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of party '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    if (node->data.party_decl.generic_params != NULL
        && node->data.party_decl.generic_params->count > 0) {
        validate_generic_param_defaults(node->data.party_decl.generic_params,
            ctx, node, "party");
    }

    /* Check role slot ability references */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];

        /* dyn slots require at least one ability for vtable dispatch */
        if (rs->data.role_slot.is_dynamic &&
            rs->data.role_slot.ability_count == 0) {
            semantic_error(ctx, rs,
                "Dynamic role slot '%s' requires at least one ability type",
                rs->data.role_slot.slot_name);
        }

        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab_type = rs->data.role_slot.required_abilities[j];
            if (ab_type != NULL && ab_type->data.type.name != NULL) {
                const char *ability_name = ab_type->data.type.name;
                char *required_text = ability_ref_display(ab_type);
                Symbol *ab = scope_lookup(ctx->scope, ab_type->data.type.name);
                if (ab == NULL || ab->kind != SYMBOL_ABILITY) {
                    semantic_warning(ctx, rs,
                        "Ability '%s' not found for role slot '%s'",
                        ab_type->data.type.name,
                        rs->data.role_slot.slot_name);
                } else if (ctx->program_root != NULL) {
                    ASTNode *ability_decl = resolve_required_ability_decl(
                        ab_type, rs, ctx, "party role slot",
                        rs->data.role_slot.slot_name);
                    if (ability_decl == NULL) {
                        free(required_text);
                        continue;
                    }
                }
                if (ctx->program_root != NULL
                           && !any_subject_role_has_ability(ctx->program_root,
                               ab_type)) {
                    ASTNode *actual_impl = any_subject_role_find_base_ability_impl(
                        ctx->program_root, ability_name);
                    char *actual_text = actual_impl != NULL
                        ? ability_ref_display(actual_impl) : NULL;
                    semantic_error(ctx, rs,
                        actual_impl != NULL
                            ? "Party role slot '%s' requires ability '%s', but subject-bound roles implement '%s' instead.\n"
                              "Reason:\n"
                              "- no subject-bound role satisfies required ability '%s'\n"
                              "Fix:\n"
                              "- implement '%s' on a subject-bound role\n"
                              "- or change the role slot contract"
                            : "Party role slot '%s' requires ability '%s', but no subject-bound role implements it.\n"
                              "Reason:\n"
                              "- no subject-bound role satisfies required ability '%s'\n"
                              "Fix:\n"
                              "- implement '%s' on a subject-bound role\n"
                              "- or change the role slot contract",
                        rs->data.role_slot.slot_name,
                        required_text != NULL ? required_text : ability_name,
                        actual_text != NULL ? actual_text : NULL,
                        required_text != NULL ? required_text : ability_name,
                        required_text != NULL ? required_text : ability_name);
                    free(actual_text);
                }
                free(required_text);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        type_check_func_decl(node->data.party_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_roster_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.roster_decl.name;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_ROSTER;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_ROSTER;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of roster '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    if (node->data.roster_decl.generic_params != NULL
        && node->data.roster_decl.generic_params->count > 0) {
        validate_generic_param_defaults(node->data.roster_decl.generic_params,
            ctx, node, "roster");
    }

    /* Check party slot references */
    for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
        ASTNode *ps = node->data.roster_decl.party_slots[i];
        if (ps->data.roster_slot.party_type != NULL) {
            Symbol *party = scope_lookup(ctx->scope,
                ps->data.roster_slot.party_type);
            if (party == NULL || party->kind != SYMBOL_PARTY) {
                semantic_warning(ctx, ps,
                    "Party type '%s' not found for slot '%s'",
                    ps->data.roster_slot.party_type,
                    ps->data.roster_slot.slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.roster_decl.shared_count; i++) {
        ASTNode *shared = node->data.roster_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.roster_decl.method_count; i++) {
        type_check_func_decl(node->data.roster_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

static ASTNode *
find_intent_involves_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static const char *
intent_involves_type_name(ASTNode *involves);

static ASTNode *
find_unique_intent_involves_by_type_name(ASTNode *intent,
                                         const char *type_name,
                                         const char **alias_out)
{
    ASTNode *matched = NULL;

    if (alias_out != NULL)
        *alias_out = NULL;
    if (intent == NULL || intent->type != AST_INTENT_DECL || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        const char *participant_type_name = intent_involves_type_name(involves);

        if (participant_type_name == NULL
            || strcmp(participant_type_name, type_name) != 0) {
            continue;
        }

        if (matched != NULL)
            return NULL;
        matched = involves;
    }

    if (matched != NULL && alias_out != NULL)
        *alias_out = matched->data.intent_involves.alias;
    return matched;
}

static ASTNode *
intent_step_resolve_transfer_target_involves(ASTNode *intent_decl,
                                             ASTNode *step,
                                             const char **resolved_alias_out)
{
    const char *to_name;
    ASTNode *to_involves;
    const char *resolved_alias = NULL;

    if (resolved_alias_out != NULL)
        *resolved_alias_out = NULL;
    if (intent_decl == NULL || step == NULL || step->type != AST_INTENT_STEP)
        return NULL;

    to_name = step->data.intent_step.transfer_to_alias;
    if (to_name == NULL)
        return NULL;

    to_involves = find_intent_involves_local(intent_decl, to_name);
    if (to_involves == NULL) {
        to_involves = find_unique_intent_involves_by_type_name(
            intent_decl, to_name, &resolved_alias);
        if (to_involves != NULL && resolved_alias != NULL) {
            free(step->data.intent_step.transfer_to_alias);
            step->data.intent_step.transfer_to_alias = pergyra_strdup(resolved_alias);
        }
    } else {
        resolved_alias = to_name;
    }

    if (resolved_alias_out != NULL)
        *resolved_alias_out = resolved_alias;
    return to_involves;
}

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref);

static bool
intent_condition_is_bool(ASTNode *expr, SemanticContext *ctx, const char *label)
{
    Type *ty;
    if (expr == NULL)
        return true;
    ty = type_check_expression(expr, ctx);
    if (ty != NULL && !type_equals(ty, TYPE_BOOL)) {
        semantic_error(ctx, expr,
            "Intent %s expects a Bool value, got '%s'. Use a Bool predicate or cast/normalize the expression to Bool before this clause.",
            label != NULL ? label : "condition",
            ty->name != NULL ? ty->name : "<type>");
        return false;
    }
    return true;
}

static const char *
intent_forbidden_control_construct(ASTNode *node)
{
    const char *nested = NULL;

    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_AWAIT_EXPR:
            return "await";
        case AST_SPAWN_EXPR:
            return "spawn";
        case AST_ASYNC_BLOCK:
            return "async";
        case AST_PARALLEL_BLOCK:
            return "parallel";
        case AST_SELECT_STMT:
            return "select";
        case AST_CHANNEL_SEND:
            return "channel send";
        case AST_CHANNEL_RECV:
            return "channel recv";
        case AST_TASK_GROUP:
            return "task-group";
        case AST_LAMBDA_EXPR:
            if (node->data.lambda_expr.is_async)
                return "async lambda";
            return intent_forbidden_control_construct(node->data.lambda_expr.body);
        case AST_BINARY:
            nested = intent_forbidden_control_construct(node->data.binary.left);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.binary.right);
        case AST_UNARY:
            return intent_forbidden_control_construct(node->data.unary.operand);
        case AST_CALL:
            nested = intent_forbidden_control_construct(node->data.call.callee);
            if (nested != NULL)
                return nested;
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.call.arguments[i]);
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_MEMBER_ACCESS:
            return intent_forbidden_control_construct(node->data.member.object);
        case AST_ARRAY_ACCESS:
            nested = intent_forbidden_control_construct(
                node->data.array_access.array);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(
                node->data.array_access.index);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < node->data.array_literal.count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.array_literal.elements[i]);
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_ASSIGNMENT:
            nested = intent_forbidden_control_construct(node->data.assignment.target);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.assignment.value);
        case AST_MATCH_STMT:
            nested = intent_forbidden_control_construct(node->data.match_stmt.subject);
            if (nested != NULL)
                return nested;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.match_stmt.cases[i]);
                if (nested != NULL)
                    return nested;
            }
            return intent_forbidden_control_construct(
                node->data.match_stmt.default_body);
        case AST_MATCH_CASE:
            nested = intent_forbidden_control_construct(node->data.match_case.pattern);
            if (nested != NULL)
                return nested;
            nested = intent_forbidden_control_construct(node->data.match_case.guard);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.match_case.body);
        case AST_IF_STMT:
            nested = intent_forbidden_control_construct(node->data.if_stmt.condition);
            if (nested != NULL)
                return nested;
            nested = intent_forbidden_control_construct(node->data.if_stmt.then_branch);
            if (nested != NULL)
                return nested;
            return intent_forbidden_control_construct(node->data.if_stmt.else_branch);
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                nested = intent_forbidden_control_construct(
                    node->data.block.statements[i]);
                if (nested != NULL)
                    return nested;
            }
            return NULL;
        case AST_RETURN:
            return intent_forbidden_control_construct(node->data.return_stmt.value);
        case AST_DEFER_STMT:
            return intent_forbidden_control_construct(node->data.defer_stmt.body);
        default:
            return NULL;
    }
}

static bool
intent_clause_rejects_control_transfer(ASTNode *expr,
                                       SemanticContext *ctx,
                                       const char *step_name,
                                       const char *label)
{
    const char *construct = intent_forbidden_control_construct(expr);

    if (construct == NULL)
        return true;

    semantic_error(ctx, expr,
        "Intent step '%s' %s clause cannot contain '%s'. "
        "Keep control-transfer constructs out of intent clauses and run them in surrounding intent/action code.",
        step_name != NULL ? step_name : "<step>",
        label != NULL ? label : "expression",
        construct);
    return false;
}

static const char *
intent_involves_type_name(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
intent_involves_is_subject_host(ASTNode *program, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name(involves);
    ASTNode *decl = NULL;

    if (type_name == NULL)
        return false;
    decl = find_subject_host_decl_by_name(program, type_name);
    return decl_is_subject_host(decl);
}

static bool
subject_decl_has_action_named(ASTNode *decl, const char *action_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || action_name == NULL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return false;
    }

    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return true;
        }
    }
    return false;
}

static ASTNode *
subject_decl_find_action_named(ASTNode *decl, const char *action_name)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL || action_name == NULL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }

    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return method;
        }
    }
    return NULL;
}

static bool
intent_semantic_append_name(char ***items, size_t *count, const char *name)
{
    char **grown;

    if (items == NULL || count == NULL || name == NULL)
        return false;
    grown = realloc(*items, (*count + 1) * sizeof(char *));
    if (grown == NULL)
        return false;
    grown[*count] = pergyra_strdup(name);
    *items = grown;
    (*count)++;
    return true;
}

static const char *
intent_action_binding_type_name(ASTNode *action_decl, ASTNode *subject_decl,
                                SemanticContext *ctx, const char *binding_name)
{
    if (binding_name == NULL || action_decl == NULL || ctx == NULL)
        return NULL;
    if (strcmp(binding_name, "self") == 0) {
        return (subject_decl != NULL && subject_decl->type == AST_CLASS_DECL)
            ? subject_decl->data.class_decl.name
            : NULL;
    }

    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *param = action_decl->data.func_decl.params[i];
        Type *param_type;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, binding_name) != 0
            || param->type == NULL) {
            continue;
        }
        param_type = resolve_type_node(param->type, ctx);
        return (param_type != NULL) ? param_type->name : NULL;
    }

    return NULL;
}

static ASTNode *
intent_step_find_inheritable_action(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx,
                                    ASTNode **subject_decl_out)
{
    ASTNode *matched_action = NULL;
    ASTNode *matched_subject_decl = NULL;

    if (subject_decl_out != NULL)
        *subject_decl_out = NULL;
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return NULL;
    }

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        ASTNode *involves = find_intent_involves_local(intent_decl, alias);
        const char *type_name = intent_involves_type_name(involves);
        ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root, type_name);
        ASTNode *action_decl = subject_decl_find_action_named(
            subject_decl, step->data.intent_step.name);

        if (action_decl == NULL)
            continue;
        if (matched_action != NULL && matched_action != action_decl)
            return NULL;
        matched_action = action_decl;
        matched_subject_decl = subject_decl;
    }

    if (subject_decl_out != NULL)
        *subject_decl_out = matched_subject_decl;
    return matched_action;
}

static void
intent_step_infer_who_from_action(ASTNode *intent_decl, ASTNode *step,
                                  SemanticContext *ctx)
{
    const char *matched_alias = NULL;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.who_count != 0) {
        return;
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        ASTNode *subject_decl;
        ASTNode *action_decl;

        if (!intent_involves_is_subject_host(ctx->program_root, involves))
            continue;
        subject_decl = find_subject_host_decl_by_name(
            ctx->program_root, intent_involves_type_name(involves));
        action_decl = subject_decl_find_action_named(
            subject_decl, step->data.intent_step.name);
        if (action_decl == NULL)
            continue;

        if (matched_alias != NULL)
            return;
        matched_alias = involves->data.intent_involves.alias;
    }

    if (matched_alias != NULL
        && intent_semantic_append_name(&step->data.intent_step.who_names,
               &step->data.intent_step.who_count, matched_alias)) {
        step->data.intent_step.inferred_who_from_action = true;
    }
}

static void
intent_step_inherit_action_contract(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx)
{
    ASTNode *action_subject_decl = NULL;
    ASTNode *action_decl = intent_step_find_inheritable_action(
        intent_decl, step, ctx, &action_subject_decl);

    if (action_decl == NULL)
        return;

    if (step->data.intent_step.where_type == NULL
        && action_decl->data.func_decl.within_zone != NULL) {
        step->data.intent_step.where_type =
            ast_create_type(action_decl->data.func_decl.within_zone);
        step->data.intent_step.inherited_where_from_action = true;
    }

    if (step->data.intent_step.required_ability_count == 0
        && action_decl->data.func_decl.required_ability_count > 0) {
        for (size_t i = 0; i < action_decl->data.func_decl.required_ability_count; i++) {
            ASTNode *ability_ref = action_decl->data.func_decl.required_abilities[i];
            ASTNode *ability_copy = ast_clone(ability_ref);
            size_t next = step->data.intent_step.required_ability_count + 1;
            step->data.intent_step.required_abilities = realloc(
                step->data.intent_step.required_abilities,
                next * sizeof(ASTNode *));
            step->data.intent_step.required_abilities[next - 1] = ability_copy;
            step->data.intent_step.required_ability_count = next;
        }
        step->data.intent_step.inherited_requires_from_action = true;
    }

    if (step->data.intent_step.causes_effect == NULL
        && action_decl->data.func_decl.causes_effect != NULL) {
        step->data.intent_step.causes_effect =
            pergyra_strdup(action_decl->data.func_decl.causes_effect);
        step->data.intent_step.inherited_causes_from_action = true;
    }

    if (step->data.intent_step.authorized_by_count == 0
        && action_decl->data.func_decl.authorized_by_count > 0) {
        bool mapped_all = true;

        for (size_t i = 0; i < action_decl->data.func_decl.authorized_by_count; i++) {
            const char *binding_name = action_decl->data.func_decl.authorized_by[i];
            const char *binding_type_name = intent_action_binding_type_name(
                action_decl, action_subject_decl, ctx, binding_name);
            const char *mapped_alias = NULL;

            if (strcmp(binding_name, "self") == 0) {
                if (step->data.intent_step.who_count == 1) {
                    mapped_alias = step->data.intent_step.who_names[0];
                }
            } else if (binding_type_name != NULL) {
                for (size_t j = 0; j < intent_decl->data.intent_decl.involve_count; j++) {
                    ASTNode *involves = intent_decl->data.intent_decl.involves[j];
                    const char *participant_type_name = intent_involves_type_name(involves);
                    if (participant_type_name != NULL
                        && strcmp(participant_type_name, binding_type_name) == 0) {
                        if (mapped_alias != NULL) {
                            mapped_alias = NULL;
                            break;
                        }
                        mapped_alias = involves->data.intent_involves.alias;
                    }
                }
            }

            if (mapped_alias == NULL) {
                mapped_all = false;
                break;
            }
            if (!intent_semantic_append_name(&step->data.intent_step.authorized_by,
                    &step->data.intent_step.authorized_by_count, mapped_alias)) {
                mapped_all = false;
                break;
            }
        }

        if (mapped_all) {
            step->data.intent_step.inherited_authorized_by_from_action = true;
        } else {
            for (size_t i = 0; i < step->data.intent_step.authorized_by_count; i++)
                free(step->data.intent_step.authorized_by[i]);
            free(step->data.intent_step.authorized_by);
            step->data.intent_step.authorized_by = NULL;
            step->data.intent_step.authorized_by_count = 0;
        }
    }
}

static void
intent_step_infer_transfer_context(ASTNode *intent_decl, ASTNode *step,
                                   SemanticContext *ctx)
{
    const char *to_alias = NULL;
    ASTNode *to_involves;
    Type *to_type;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP)
        return;

    to_involves = intent_step_resolve_transfer_target_involves(
        intent_decl, step, &to_alias);
    if (to_involves == NULL || to_involves->type != AST_INTENT_INVOLVES)
        return;

    to_type = resolve_type_node(to_involves->data.intent_involves.subject_type, ctx);

    if (step->data.intent_step.using_expr == NULL) {
        step->data.intent_step.using_expr = ast_create_identifier(to_alias);
        step->data.intent_step.inferred_using_from_transfer = true;
    }

    if (step->data.intent_step.where_type == NULL
        && to_type != NULL
        && to_type->name != NULL) {
        step->data.intent_step.where_type = ast_create_type(to_type->name);
        step->data.intent_step.inferred_where_from_transfer = true;
    }
}

static void
intent_step_infer_zone_binding_context(ASTNode *intent_decl, ASTNode *step,
                                       SemanticContext *ctx)
{
    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }

    if (step->data.intent_step.where_type == NULL
        && step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        ASTNode *using_involves = find_intent_involves_local(intent_decl,
            step->data.intent_step.using_expr->data.identifier.name);
        Type *using_type = using_involves != NULL
            ? resolve_type_node(using_involves->data.intent_involves.subject_type, ctx)
            : NULL;
        ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root,
            AST_ZONE_DECL, using_type != NULL ? using_type->name : NULL);
        if (zone_decl != NULL && using_type != NULL && using_type->name != NULL) {
            step->data.intent_step.where_type = ast_create_type(using_type->name);
        }
    }

    if (step->data.intent_step.using_expr == NULL
        && step->data.intent_step.where_type != NULL) {
        Type *zone_type = resolve_type_node(step->data.intent_step.where_type, ctx);
        const char *matched_alias = NULL;

        if (zone_type == NULL || zone_type->name == NULL)
            return;

        for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
            ASTNode *involves = intent_decl->data.intent_decl.involves[i];
            Type *participant_type;

            if (involves == NULL || involves->type != AST_INTENT_INVOLVES
                || involves->data.intent_involves.subject_type == NULL) {
                continue;
            }

            participant_type = resolve_type_node(
                involves->data.intent_involves.subject_type, ctx);
            if (participant_type == NULL || participant_type->name == NULL
                || strcmp(participant_type->name, zone_type->name) != 0) {
                continue;
            }

            if (matched_alias != NULL)
                return;
            matched_alias = involves->data.intent_involves.alias;
        }

        if (matched_alias != NULL)
            step->data.intent_step.using_expr = ast_create_identifier(matched_alias);
    }
}

static const char *
projection_refresh_source_field_name(ASTNode *refresh,
                                     const char *target_field_name)
{
    if (target_field_name == NULL)
        return NULL;
    if (refresh != NULL && refresh->type == AST_ZONE_REFRESH) {
        for (size_t i = 0; i < refresh->data.zone_refresh.field_map_count; i++) {
            const char *mapped_target =
                refresh->data.zone_refresh.mapped_target_fields[i];
            const char *mapped_source =
                refresh->data.zone_refresh.mapped_source_fields[i];
            if (mapped_target != NULL && mapped_source != NULL
                && strcmp(mapped_target, target_field_name) == 0) {
                return mapped_source;
            }
        }
    }
    return target_field_name;
}

static bool
projection_target_decl_has_field(ASTNode *target_decl, const char *field_name)
{
    if (target_decl == NULL || target_decl->type != AST_CLASS_DECL
        || field_name == NULL) {
        return false;
    }
    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *field = target_decl->data.class_decl.fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0) {
            return true;
        }
    }
    return false;
}

bool
type_check_intent_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.intent_decl.name;
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind != SYMBOL_INTENT) {
        semantic_error(ctx, node, "Redeclaration of intent '%s'", name);
        return false;
    }
    if (existing != NULL && existing->kind == SYMBOL_INTENT) {
        size_t ipc = node->data.intent_decl.involve_count
            + node->data.intent_decl.value_count;
        Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
        Type *ft;
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            ASTNode *involves = node->data.intent_decl.involves[i];
            if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                && involves->data.intent_involves.subject_type != NULL) {
                ptypes[i] = resolve_type_node(
                    involves->data.intent_involves.subject_type, ctx);
            } else {
                ptypes[i] = TYPE_UNKNOWN;
            }
        }
        for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
            ASTNode *value = node->data.intent_decl.values[i];
            size_t param_index = node->data.intent_decl.involve_count + i;
            if (value != NULL && value->type == AST_INTENT_VALUE
                && value->data.intent_value.value_type != NULL) {
                ptypes[param_index] = resolve_type_node(
                    value->data.intent_value.value_type, ctx);
            } else {
                ptypes[param_index] = TYPE_UNKNOWN;
            }
        }
        ft = type_create_function(ptypes, ipc, TYPE_BOOL);
        free(ptypes);
        if (ft != NULL) {
            existing->type = ft;
        }
    }

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        (void)resolve_type_node(involves->data.intent_involves.subject_type, ctx);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        (void)resolve_type_node(value->data.intent_value.value_type, ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        Type *subject_type;
        Symbol *participant_sym;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;

        subject_type = resolve_type_node(involves->data.intent_involves.subject_type, ctx);
        participant_sym = symbol_create_variable(involves->data.intent_involves.alias,
            subject_type, involves->line, involves->column);
        scope_declare(ctx->scope, participant_sym);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        Type *value_type;
        Symbol *value_sym;

        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;

        value_type = resolve_type_node(value->data.intent_value.value_type, ctx);
        value_sym = symbol_create_variable(value->data.intent_value.alias,
            value_type, value->line, value->column);
        scope_declare(ctx->scope, value_sym);
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        bool matched_action = false;
        ASTNode *zone_decl = NULL;
        bool has_subintent = false;
        bool step_requires_authority_flow = false;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        has_subintent = (step->data.intent_step.intent_expr != NULL);
        intent_step_infer_who_from_action(node, step, ctx);
        intent_step_inherit_action_contract(node, step, ctx);
        intent_step_infer_transfer_context(node, step, ctx);
        intent_step_infer_zone_binding_context(node, step, ctx);

        if (step->data.intent_step.where_type == NULL && !has_subintent) {
            semantic_error(ctx, step, "Intent step '%s' requires a where zone type",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
        } else {
            Type *zone_type = resolve_type_node(step->data.intent_step.where_type, ctx);
            zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                zone_type != NULL ? zone_type->name : NULL);
            if (step->data.intent_step.where_type != NULL && zone_decl == NULL) {
                semantic_error(ctx, step->data.intent_step.where_type,
                    "Intent step '%s' refers to unknown zone '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_type != NULL ? zone_type->name : "<zone>");
            }
        }

        if (step->data.intent_step.using_expr != NULL) {
            Type *using_type = type_check_expression(step->data.intent_step.using_expr, ctx);
            Type *zone_type = step->data.intent_step.where_type != NULL
                ? resolve_type_node(step->data.intent_step.where_type, ctx) : NULL;
            intent_clause_rejects_control_transfer(step->data.intent_step.using_expr, ctx,
                step->data.intent_step.name, "using");
            if (step->data.intent_step.using_expr->type != AST_IDENTIFIER) {
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using clause must reference an intent parameter alias",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            } else if (find_intent_involves_local(node,
                    step->data.intent_step.using_expr->data.identifier.name) == NULL) {
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using clause refers to unknown binding '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>");
            }
            if (using_type != NULL && zone_type != NULL && !type_equals(using_type, zone_type)) {
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using binding must match zone type '%s', got '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_type->name != NULL ? zone_type->name : "<zone>",
                    using_type->name != NULL ? using_type->name : "<type>");
            }
        }

        if (step->data.intent_step.transfer_from_alias != NULL
            || step->data.intent_step.transfer_to_alias != NULL) {
            const char *from_alias = step->data.intent_step.transfer_from_alias;
            const char *to_alias = NULL;
            ASTNode *from_involves = find_intent_involves_local(node, from_alias);
            ASTNode *to_involves = intent_step_resolve_transfer_target_involves(
                node, step, &to_alias);
            Type *from_type = from_involves != NULL
                ? resolve_type_node(from_involves->data.intent_involves.subject_type, ctx) : NULL;
            Type *to_type = to_involves != NULL
                ? resolve_type_node(to_involves->data.intent_involves.subject_type, ctx) : NULL;
            ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                from_type != NULL ? from_type->name : NULL);
            ASTNode *to_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                to_type != NULL ? to_type->name : NULL);
            Type *where_zone_type = step->data.intent_step.where_type != NULL
                ? resolve_type_node(step->data.intent_step.where_type, ctx) : NULL;

            if (from_alias == NULL || to_alias == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer clause requires both source and target zone aliases",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            }
            if (from_alias != NULL && from_involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer source '%s' is not a declared intent binding",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias);
            }
            if (to_alias != NULL && to_involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer target '%s' is not a declared intent binding or a unique zone type in this intent",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias);
            }
            if (from_involves != NULL && from_zone_decl == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer source '%s' must be a zone binding",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias != NULL ? from_alias : "<source>");
            }
            if (to_involves != NULL && to_zone_decl == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer target '%s' must be a zone binding",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias != NULL ? to_alias : "<target>");
            }
            if (to_type != NULL && where_zone_type != NULL && !type_equals(to_type, where_zone_type)) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    step, contract_summary, sizeof(contract_summary));
                semantic_error(ctx, step,
                    "Intent step '%s' transfer target '%s' does not match the current zone contract.\n"
                    "Reason:\n"
                    "- transfer target '%s' has zone type '%s'\n"
                    "- current step zone is '%s'\n"
                    "- transfer target inference would choose zone '%s'\n"
                    "%s%s"
                    "Fix:\n"
                    "- change the transfer target to a '%s' binding\n"
                    "- or override the step zone to '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_type->name != NULL ? to_type->name : "<zone>",
                    where_zone_type->name != NULL ? where_zone_type->name : "<zone>",
                    to_type->name != NULL ? to_type->name : "<zone>",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    where_zone_type->name != NULL ? where_zone_type->name : "<zone>",
                    to_type->name != NULL ? to_type->name : "<zone>");
            }
            if (step->data.intent_step.using_expr != NULL
                && step->data.intent_step.using_expr->type == AST_IDENTIFIER
                && to_alias != NULL
                && strcmp(step->data.intent_step.using_expr->data.identifier.name, to_alias) != 0) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    step, contract_summary, sizeof(contract_summary));
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using binding does not match the transfer target.\n"
                    "Reason:\n"
                    "- current using binding is '%s'\n"
                    "- transfer target binding is '%s'\n"
                    "- transfer target inference would choose using '%s'\n"
                    "%s%s"
                    "Fix:\n"
                    "- change 'using' to '%s'\n"
                    "- or change the transfer target to '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>",
                    to_alias,
                    to_alias,
                    to_alias,
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>");
            }
        }

        if (step->data.intent_step.who_count == 0 && !has_subintent) {
            semantic_error(ctx, step, "Intent step '%s' requires at least one who participant",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
        }

        if (step->data.intent_step.intent_expr != NULL) {
            Type *intent_type = type_check_expression(step->data.intent_step.intent_expr, ctx);
            const char *callee_name = "<callee>";
            intent_clause_rejects_control_transfer(step->data.intent_step.intent_expr, ctx,
                step->data.intent_step.name, "intent");
            if (step->data.intent_step.intent_expr->type != AST_CALL
                || step->data.intent_step.intent_expr->data.call.callee == NULL
                || step->data.intent_step.intent_expr->data.call.callee->type != AST_IDENTIFIER) {
                semantic_error(ctx, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause must call a named intent before lowering to Bool-gated orchestration",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            } else {
                const char *resolved_name =
                    step->data.intent_step.intent_expr->data.call.callee->data.identifier.name;
                callee_name = resolved_name != NULL ? resolved_name : callee_name;
                Symbol *intent_sym = callee_name != NULL
                    ? scope_lookup(ctx->scope, callee_name)
                    : NULL;
                if (intent_sym == NULL || intent_sym->kind != SYMBOL_INTENT) {
                    semantic_error(ctx, step->data.intent_step.intent_expr,
                        "Intent step '%s' intent clause target '%s' is not a declared intent (resolved kind: %s). "
                        "Boolean-gated orchestration requires a declared intent that returns Bool. "
                        "Declare '%s' as an intent or replace this clause with a Bool-compatible predicate.",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        callee_name != NULL ? callee_name : "<callee>",
                        intent_sym != NULL
                            ? semantic_symbol_kind_label(intent_sym->kind)
                            : "unresolved",
                        callee_name != NULL ? callee_name : "<callee>");
                }
            }
            if (intent_type != NULL && !type_equals(intent_type, TYPE_BOOL)) {
                semantic_error(ctx, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause must return Bool for boolean-orchestration, got '%s'. "
                    "Expected `Bool`; adjust the callee '%s' return type to Bool or wrap with a Bool predicate.",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    intent_type->name != NULL ? intent_type->name : "<type>",
                    callee_name != NULL ? callee_name : "<callee>");
            } else if (intent_type == NULL) {
                semantic_error(ctx, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause type could not be resolved as Bool. "
                    "Boolean-gated orchestration requires the called clause to evaluate to Bool; "
                    "make '%s' return Bool or wrap it in a Bool predicate.",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    callee_name != NULL ? callee_name : "<callee>");
            }
        }

        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (step->data.intent_step.on_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(step->data.intent_step.on_exprs[j], ctx,
                    step->data.intent_step.name, "on");
                type_check_expression(step->data.intent_step.on_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        step->data.intent_step.on_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            if (step->data.intent_step.compensate_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(
                    step->data.intent_step.compensate_exprs[j], ctx,
                    step->data.intent_step.name, "compensate");
                type_check_expression(step->data.intent_step.compensate_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        step->data.intent_step.compensate_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        if (step->data.intent_step.pre_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.pre_expr, ctx,
                step->data.intent_step.name, "pre");
            intent_condition_is_bool(step->data.intent_step.pre_expr, ctx, "pre");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.pre_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.guard_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.guard_expr, ctx,
                step->data.intent_step.name, "guard");
            intent_condition_is_bool(step->data.intent_step.guard_expr, ctx, "guard");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.guard_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.post_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.post_expr, ctx,
                step->data.intent_step.name, "post");
            intent_condition_is_bool(step->data.intent_step.post_expr, ctx, "post");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.post_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.invariant_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.invariant_expr, ctx,
                step->data.intent_step.name, "invariant");
            intent_condition_is_bool(step->data.intent_step.invariant_expr, ctx, "invariant");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.invariant_expr, ctx))
                step_requires_authority_flow = true;
        }

        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            const char *alias = step->data.intent_step.who_names[j];
            ASTNode *involves = find_intent_involves_local(node, alias);
            const char *participant_type_name = NULL;
            if (involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' refers to unknown participant '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>");
                continue;
            }

            participant_type_name = intent_involves_type_name(involves);
            if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
                semantic_error(ctx, step,
                    "Intent step '%s' who participant '%s' must bind to a subject type",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>");
                continue;
            }
            if (zone_decl != NULL && participant_type_name != NULL
                && !domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                    zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
                semantic_error(ctx, step,
                    "Intent step '%s' binds participant '%s' of type '%s', but zone '%s' has no matching subject slot",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
            }
            if (step->data.intent_step.transfer_from_alias != NULL && participant_type_name != NULL) {
                ASTNode *from_involves = find_intent_involves_local(node,
                    step->data.intent_step.transfer_from_alias);
                Type *from_type = from_involves != NULL
                    ? resolve_type_node(from_involves->data.intent_involves.subject_type, ctx) : NULL;
                ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                    from_type != NULL ? from_type->name : NULL);
                if (from_zone_decl != NULL
                    && !domain_has_subject_slot_type(from_zone_decl->data.zone_decl.slots,
                        from_zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' transfer source zone '%s' has no matching subject slot for participant '%s' of type '%s'",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        from_zone_decl->data.zone_decl.name != NULL
                            ? from_zone_decl->data.zone_decl.name : "<zone>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name);
                }
            }

            if (involves->data.intent_involves.subject_type != NULL
                && involves->data.intent_involves.subject_type->type == AST_TYPE
                && involves->data.intent_involves.subject_type->data.type.name != NULL) {
                ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root,
                    involves->data.intent_involves.subject_type->data.type.name);
                if (subject_decl_has_action_named(subject_decl, step->data.intent_step.name))
                    matched_action = true;
            }
        }

        for (size_t j = 0; j < step->data.intent_step.required_ability_count; j++) {
            ASTNode *ability_ref = step->data.intent_step.required_abilities[j];
            const char *ability = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            if (resolve_required_ability_decl(
                    ability_ref, step, ctx, "Intent step",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>") == NULL) {
                free(required_text);
                continue;
            }

            for (size_t k = 0; k < step->data.intent_step.who_count; k++) {
                const char *alias = step->data.intent_step.who_names[k];
                ASTNode *involves = find_intent_involves_local(node, alias);
                const char *participant_type_name = intent_involves_type_name(involves);
                if (!intent_involves_is_subject_host(ctx->program_root, involves))
                    continue;
                if (participant_type_name != NULL
                    && !subject_type_has_ability(ctx->program_root, participant_type_name, ability_ref)) {
                    ASTNode *actual_impl = subject_type_find_base_ability_impl(
                        ctx->program_root, participant_type_name, ability);
                    char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                    char contract_summary[512];
                    intent_step_format_contract_source_summary(
                        step, contract_summary, sizeof(contract_summary));
                    semantic_error(ctx, step,
                        actual_impl != NULL
                            ? "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' implements '%s' instead.\n"
                              "Reason:\n"
                              "- participant type '%s' does not satisfy required ability '%s'\n"
                              "%s%s"
                              "Fix:\n"
                              "- implement '%s' for subject type '%s'\n"
                              "- or choose a participant whose subject type satisfies the contract\n"
                              "- or override/remove the inherited step requirement"
                            : "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' does not implement it.\n"
                              "Reason:\n"
                              "- participant type '%s' does not satisfy required ability '%s'\n"
                              "%s%s"
                              "Fix:\n"
                              "- implement '%s' for subject type '%s'\n"
                              "- or choose a participant whose subject type satisfies the contract\n"
                              "- or override/remove the inherited step requirement",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        required_text != NULL ? required_text
                                              : (ability != NULL ? ability : "<ability>"),
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        actual_text != NULL ? actual_text : NULL,
                        participant_type_name,
                        required_text != NULL ? required_text
                                              : (ability != NULL ? ability : "<ability>"),
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        required_text != NULL ? required_text
                                              : (ability != NULL ? ability : "<ability>"),
                        participant_type_name,
                        participant_type_name,
                        required_text != NULL ? required_text
                                              : (ability != NULL ? ability : "<ability>"),
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        required_text != NULL ? required_text
                                              : (ability != NULL ? ability : "<ability>"),
                        participant_type_name);
                    free(actual_text);
                }
            }
            free(required_text);
        }

        if (step->data.intent_step.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                step->data.intent_step.causes_effect) == NULL) {
            semantic_error(ctx, step,
                "Intent step '%s' causes unknown effect '%s'",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                step->data.intent_step.causes_effect);
        } else if (step->data.intent_step.causes_effect != NULL
            && zone_decl != NULL
            && !zone_has_effect_layer_type(zone_decl, step->data.intent_step.causes_effect)) {
            semantic_warning(ctx, step,
                "Intent step '%s' causes effect '%s', but zone '%s' has no matching effect slot",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                step->data.intent_step.causes_effect,
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
        }

        if (zone_decl != NULL
            && zone_decl->data.zone_decl.authority_count > 0
            && step->data.intent_step.authorized_by_count == 0
            && (step->data.intent_step.causes_effect != NULL
                || step->data.intent_step.transfer_from_alias != NULL
                || step->data.intent_step.transfer_to_alias != NULL
                || step_requires_authority_flow)) {
            char inherited_summary[512];
            intent_step_format_contract_source_summary(
                step, inherited_summary, sizeof(inherited_summary));
            semantic_error(ctx, step,
                step->data.intent_step.inherited_where_from_action
                    ? "Intent step '%s' cannot run in authority-bearing zone '%s' without 'authorized by'.\n"
                      "Reason:\n"
                      "- the step causes effects, transfers zone state, or invokes authority-sensitive helpers\n"
                      "- zone '%s' was inherited from the matching action contract\n"
                      "%s%s"
                      "Fix:\n"
                      "- add 'authorized by <participant>' to the step\n"
                      "- or move the step to a non-authority zone\n"
                      "- or change the action contract that this step inherits"
                    : "Intent step '%s' cannot run in authority-bearing zone '%s' without 'authorized by'.\n"
                      "Reason:\n"
                      "- the step causes effects, transfers zone state, or invokes authority-sensitive helpers\n"
                      "- zone '%s' declares authority, so explicit approval is required\n"
                      "%s%s"
                      "Fix:\n"
                      "- add 'authorized by <participant>' to the step\n"
                      "- or move the step to a non-authority zone\n"
                      "- or remove the authority-sensitive operation from this step",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                inherited_summary[0] != '\0' ? inherited_summary : "",
                inherited_summary[0] != '\0' ? "\n" : "",
                inherited_summary[0] != '\0' ? inherited_summary : "",
                inherited_summary[0] != '\0' ? "\n" : "");
        }

        for (size_t j = 0; j < step->data.intent_step.authorized_by_count; j++) {
            const char *alias = step->data.intent_step.authorized_by[j];
            ASTNode *involves = find_intent_involves_local(node, alias);
            const char *participant_type_name = intent_involves_type_name(involves);
            char contract_summary[512];
            intent_step_format_contract_source_summary(
                step, contract_summary, sizeof(contract_summary));
            if (involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' authorizes unknown participant '%s'.\n"
                    "Reason:\n"
                    "- the authorized-by clause references a participant alias that is not declared on the intent\n"
                    "%s%s"
                    "Fix:\n"
                    "- add 'involves %s: <Subject>' to the intent\n"
                    "- or change 'authorized by' to one of the declared participants",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    alias != NULL ? alias : "<participant>");
                continue;
            }

            if (zone_decl != NULL && participant_type_name != NULL) {
                if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' authorized participant '%s' must bind to a subject type.\n"
                        "Reason:\n"
                        "- only subject participants can satisfy zone authority contracts\n"
                        "- participant '%s' is not a subject host\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize a subject participant instead\n"
                        "- or change the intent binding so '%s' is a subject type",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        alias != NULL ? alias : "<participant>",
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        alias != NULL ? alias : "<participant>");
                    continue;
                }
                if (!domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                        zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' authorized participant '%s' has type '%s', but zone '%s' has no matching subject slot.\n"
                        "Reason:\n"
                        "- authorized participants must map to a subject slot inside the current zone\n"
                        "- zone '%s' does not declare a subject slot of type '%s'\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize a participant whose subject type exists in zone '%s'\n"
                        "- or add a matching subject slot to zone '%s'",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        participant_type_name,
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
                } else if (zone_decl->data.zone_decl.authority_count > 0
                    && !zone_has_authority_for_subject_type(zone_decl, ctx, participant_type_name)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' authorized participant '%s' has type '%s', but zone '%s' declares no matching authority.\n"
                        "Reason:\n"
                        "- zone '%s' has authority rules, but none match subject type '%s'\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize a participant whose subject type is listed in zone authority\n"
                        "- or add an authority declaration for subject type '%s' in zone '%s'",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        participant_type_name,
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        participant_type_name,
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
                }
            }
        }

        if (step->data.intent_step.expect_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.expect_expr, ctx,
                step->data.intent_step.name, "expect");
            intent_condition_is_bool(step->data.intent_step.expect_expr, ctx, "expect");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.expect_expr, ctx))
                step_requires_authority_flow = true;
        }

        if (!matched_action && step->data.intent_step.on_expr_count == 0
            && step->data.intent_step.intent_expr == NULL) {
            semantic_warning(ctx, step,
                "Intent step '%s' does not currently match a subject action of the same name; intent is declarative only",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
        }
    }

    if (node->data.intent_decl.priority_expr != NULL) {
        Type *priority_type = type_check_expression(node->data.intent_decl.priority_expr, ctx);
        intent_clause_rejects_control_transfer(node->data.intent_decl.priority_expr, ctx,
            name, "priority");
        if (priority_type != NULL && !type_equals(priority_type, TYPE_INT)) {
            semantic_error(ctx, node->data.intent_decl.priority_expr,
                "Intent priority expression must be Int, got '%s'",
                priority_type->name != NULL ? priority_type->name : "<type>");
        }
    }

    if (node->data.intent_decl.success_expr != NULL) {
        intent_clause_rejects_control_transfer(node->data.intent_decl.success_expr, ctx,
            name, "success");
        intent_condition_is_bool(node->data.intent_decl.success_expr, ctx, "success");
    }
    if (node->data.intent_decl.failure_expr != NULL) {
        intent_clause_rejects_control_transfer(node->data.intent_decl.failure_expr, ctx,
            name, "failure");
        intent_condition_is_bool(node->data.intent_decl.failure_expr, ctx, "failure");
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx);

static ASTNode *
find_world_zone_slot_local(ASTNode *world, const char *slot_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.zone_count; i++) {
        ASTNode *zone = world->data.world_decl.zones[i];
        if (zone != NULL && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
find_world_state_local(ASTNode *world, const char *state_name)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < world->data.world_decl.state_count; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
find_world_state_before_local(ASTNode *world, const char *state_name, size_t limit)
{
    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    if (limit > world->data.world_decl.state_count)
        limit = world->data.world_decl.state_count;

    for (size_t i = 0; i < limit; i++) {
        ASTNode *state = world->data.world_decl.states[i];
        if (state != NULL && state->type == AST_WORLD_STATE
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
resolve_world_zone_decl_local(ASTNode *world, SemanticContext *ctx, const char *slot_name)
{
    ASTNode *slot;
    if (world == NULL || world->type != AST_WORLD_DECL
        || ctx == NULL || slot_name == NULL) {
        return NULL;
    }

    slot = find_world_zone_slot_local(world, slot_name);
    if (slot == NULL || slot->data.world_zone.zone_type == NULL)
        return NULL;

    return find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
        slot->data.world_zone.zone_type);
}

static const char *
resolve_world_plain_zone_input_name(ASTNode *world, const char *input_name)
{
    ASTNode *state;

    if (world == NULL || world->type != AST_WORLD_DECL || input_name == NULL)
        return NULL;

    if (find_world_zone_slot_local(world, input_name) != NULL)
        return input_name;

    state = find_world_state_local(world, input_name);
    if (state != NULL && state->type == AST_WORLD_STATE
        && state->data.world_state.source_kind == WORLD_STATE_SOURCE_ZONE) {
        return state->data.world_state.zone_slot_name;
    }

    return NULL;
}

static ASTNode *
find_zone_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_state_decl_local(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static bool
resolve_world_zone_state(ASTNode *world, ASTNode *site, const char *state_name,
                         SemanticContext *ctx, const char *action_name,
                         const char **zone_slot_name_out)
{
    ASTNode *state = find_world_state_local(world, state_name);
    if (state == NULL) {
        ASTNode *zone = find_world_zone_slot_local(world, state_name);
        if (zone != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = zone->data.world_zone.slot_name;
            return true;
        }
    }
    if (state == NULL) {
        semantic_error(ctx, site,
            "World %s references unknown zone/state '%s'",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (state->data.world_state.source_kind != WORLD_STATE_SOURCE_ZONE) {
        semantic_error(ctx, site,
            "World %s cannot target derived state '%s'; use the underlying zone slot or a plain 'state name: zone slot' alias",
            action_name, state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = state->data.world_state.zone_slot_name;
    return true;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.world_decl.name;
    ASTNode *saved_world = ctx->current_world;

    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = SYMBOL_WORLD;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = SYMBOL_WORLD;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of world '%s'", name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    ctx->current_world = node;

    /* Check roster references */
    for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
        ASTNode *ws = node->data.world_decl.rosters[i];
        if (ws->data.world_roster.roster_type != NULL) {
            Symbol *sys = scope_lookup(ctx->scope,
                ws->data.world_roster.roster_type);
            if (sys == NULL || sys->kind != SYMBOL_ROSTER) {
                semantic_warning(ctx, ws,
                    "Roster type '%s' not found for slot '%s'",
                    ws->data.world_roster.roster_type,
                    ws->data.world_roster.slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        if (wz->data.world_zone.zone_type != NULL) {
            Symbol *zone = scope_lookup(ctx->scope,
                wz->data.world_zone.zone_type);
            if (zone == NULL || zone->kind != SYMBOL_ZONE) {
                semantic_warning(ctx, wz,
                    "Zone type '%s' not found for slot '%s'",
                    wz->data.world_zone.zone_type,
                    wz->data.world_zone.slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        const char *zone_slot_name = state->data.world_state.zone_slot_name;
        ASTNode *zone_decl = NULL;
        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            bool saw_direct_zone_input = false;
            bool saw_state_input = false;
            if (state->data.world_state.input_count == 0) {
                semantic_error(ctx, state,
                    "Composed world state '%s' must reference at least one zone/state input",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                if (input_name == NULL)
                    continue;
                if (state->data.world_state.state_name != NULL
                    && strcmp(state->data.world_state.state_name, input_name) == 0) {
                    semantic_error(ctx, state,
                        "Composed world state '%s' cannot reference itself",
                        input_name);
                    continue;
                }
                if (find_world_zone_slot_local(node, input_name) != NULL)
                {
                    saw_direct_zone_input = true;
                    continue;
                }
                if (find_world_state_before_local(node, input_name, i) != NULL)
                {
                    saw_state_input = true;
                    continue;
                }
                semantic_error(ctx, state,
                    "Composed world state '%s' references unknown or later world zone/state '%s'",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>",
                    input_name);
            }
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                const char *input_plain_zone;
                if (input_name == NULL)
                    continue;
                input_plain_zone = resolve_world_plain_zone_input_name(node, input_name);
                for (size_t prev_i = 0; prev_i < input_i; prev_i++) {
                    const char *prev_name = state->data.world_state.input_names[prev_i];
                    const char *prev_plain_zone;
                    if (prev_name == NULL)
                        continue;
                    if (strcmp(prev_name, input_name) == 0) {
                        semantic_warning(ctx, state,
                            "Composed world state '%s' repeats input '%s'",
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>",
                            input_name);
                        break;
                    }
                    prev_plain_zone = resolve_world_plain_zone_input_name(node, prev_name);
                    if (input_plain_zone != NULL && prev_plain_zone != NULL
                        && strcmp(input_plain_zone, prev_plain_zone) == 0) {
                        semantic_warning(ctx, state,
                            "Composed world state '%s' redundantly mixes zone slot '%s' with plain alias/input '%s'",
                            state->data.world_state.state_name != NULL
                                ? state->data.world_state.state_name : "<unknown>",
                            prev_name,
                            input_name);
                        break;
                    }
                }
                if (find_world_zone_slot_local(node, input_name) != NULL) {
                    semantic_warning(ctx, state,
                        "Composed world state '%s' directly references zone slot '%s'; prefer a plain 'state name: zone %s' alias so command and derived layers stay separate",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        input_name,
                        input_name);
                }
            }
            if (saw_direct_zone_input && saw_state_input) {
                semantic_warning(ctx, state,
                    "Composed world state '%s' mixes direct zone-slot inputs with world-state inputs; prefer composing from world-state aliases only",
                    state->data.world_state.state_name != NULL
                        ? state->data.world_state.state_name : "<unknown>");
            }
        } else if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, state,
                "World state '%s' references unknown zone slot '%s'",
                state->data.world_state.state_name != NULL
                    ? state->data.world_state.state_name : "<unknown>",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        } else {
            zone_decl = resolve_world_zone_decl_local(node, ctx, zone_slot_name);
        }

        if (zone_decl != NULL && state->data.world_state.detail_name != NULL) {
            const char *detail_name = state->data.world_state.detail_name;
            ASTNode *detail_decl = NULL;

            if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                detail_decl = find_zone_domain_slot(zone_decl, detail_name);
                if (detail_decl == NULL
                    || detail_decl->data.domain_slot.is_subject) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown object/tobject projection slot '%s' in zone '%s'",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name);
                }
            } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                detail_decl = find_zone_layer_slot_local(zone_decl, detail_name);
                if (detail_decl == NULL) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown layer slot '%s' in zone '%s'",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name);
                }
            } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                detail_decl = find_zone_state_decl_local(zone_decl, detail_name);
                if (detail_decl == NULL) {
                    semantic_error(ctx, state,
                        "World state '%s' references unknown zone state '%s' in zone '%s'",
                        state->data.world_state.state_name != NULL
                            ? state->data.world_state.state_name : "<unknown>",
                        detail_name,
                        zone_slot_name);
                }
            }
        }
        for (size_t j = i + 1; j < node->data.world_decl.state_count; j++) {
            ASTNode *other = node->data.world_decl.states[j];
            if (other != NULL
                && other->data.world_state.state_name != NULL
                && state->data.world_state.state_name != NULL
                && strcmp(other->data.world_state.state_name,
                          state->data.world_state.state_name) == 0) {
                semantic_error(ctx, other,
                    "Redeclaration of world state '%s'",
                    state->data.world_state.state_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *activate = node->data.world_decl.activations[i];
        const char *zone_slot_name = activate->data.world_activate.zone_slot_name;
        if (activate->data.world_activate.state_name != NULL) {
            if (!resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, activate,
                "World activate references unknown zone slot '%s'",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.activate_count; j++) {
            ASTNode *other = node->data.world_decl.activations[j];
            const char *other_zone = other->data.world_activate.zone_slot_name;
            if (other->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' activates zone '%s' more than once",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and deactivates zone '%s'; choose one lifecycle direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *maintain = node->data.world_decl.maintained_zones[j];
            const char *other_zone = maintain->data.world_maintain.zone_slot_name;
            if (maintain->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, maintain,
                    maintain->data.world_maintain.state_name, ctx, "maintain",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, activate,
                    "World '%s' both activates and maintains zone '%s'; maintain already implies the continuing direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = node->data.world_decl.deactivations[i];
        const char *zone_slot_name = deactivate->data.world_deactivate.zone_slot_name;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            if (!resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, deactivate,
                "World deactivate references unknown zone slot '%s'",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *other = node->data.world_decl.deactivations[j];
            const char *other_zone = other->data.world_deactivate.zone_slot_name;
            if (other->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' deactivates zone '%s' more than once",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = node->data.world_decl.maintained_zones[i];
        const char *zone_slot_name = maintain->data.world_maintain.zone_slot_name;
        const char *state_name = maintain->data.world_maintain.state_name;
        if (state_name != NULL) {
            if (!resolve_world_zone_state(node, maintain, state_name, ctx, "maintain",
                    &zone_slot_name)) {
                continue;
            }
        }
        if (find_world_zone_slot_local(node, zone_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "World maintain references unknown zone slot '%s'",
                zone_slot_name != NULL ? zone_slot_name : "<unknown>");
        }
        for (size_t j = i + 1; j < node->data.world_decl.maintained_zone_count; j++) {
            ASTNode *other = node->data.world_decl.maintained_zones[j];
            const char *other_zone = other->data.world_maintain.zone_slot_name;
            if (other->data.world_maintain.state_name != NULL) {
                resolve_world_zone_state(node, other,
                    other->data.world_maintain.state_name, ctx, "maintain", &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, other,
                    "World '%s' maintains zone '%s' more than once",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.deactivate_count; j++) {
            ASTNode *deactivate = node->data.world_decl.deactivations[j];
            const char *other_zone = deactivate->data.world_deactivate.zone_slot_name;
            if (deactivate->data.world_deactivate.state_name != NULL) {
                resolve_world_zone_state(node, deactivate,
                    deactivate->data.world_deactivate.state_name, ctx, "deactivate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and deactivates zone '%s'; choose one lifecycle direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.world_decl.activate_count; j++) {
            ASTNode *activate = node->data.world_decl.activations[j];
            const char *other_zone = activate->data.world_activate.zone_slot_name;
            if (activate->data.world_activate.state_name != NULL) {
                resolve_world_zone_state(node, activate,
                    activate->data.world_activate.state_name, ctx, "activate",
                    &other_zone);
            }
            if (zone_slot_name != NULL && other_zone != NULL
                && strcmp(zone_slot_name, other_zone) == 0) {
                semantic_warning(ctx, maintain,
                    "World '%s' both maintains and activates zone '%s'; maintain already implies the continuing direction",
                    node->data.world_decl.name, zone_slot_name);
            }
        }
    }

    /* Check shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    /* Check methods */
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        type_check_func_decl(node->data.world_decl.methods[i], ctx);
    }
    scope_exit(&ctx->scope);

    ctx->current_world = saved_world;
    return !ctx->has_error;
}

static bool
type_check_domain_slots(ASTNode **slots,
                        size_t slot_count,
                        SemanticContext *ctx,
                        const char *kind_name)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot->data.domain_slot.is_subject
            && !type_is_subject_type(slot_type, ctx)) {
            semantic_error(ctx, slot,
                "%s subject slot '%s' requires a subject type",
                kind_name,
                slot->data.domain_slot.slot_name);
        } else if (slot->data.domain_slot.is_vessel) {
            ASTNode *slot_decl = NULL;
            if (slot_type != NULL && slot_type->kind == TYPE_KIND_CLASS
                && slot_type->name != NULL) {
                slot_decl = find_type_decl_by_name(ctx->program_root, slot_type->name);
            }
            if (slot_decl == NULL || slot_decl->type != AST_CLASS_DECL
                || slot_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
                semantic_error(ctx, slot,
                    "%s vessel slot '%s' requires a vessel type",
                    kind_name,
                    slot->data.domain_slot.slot_name);
            }
        }
    }

    return !ctx->has_error;
}

static bool
type_check_domain_slot_initializers(ASTNode **slots,
                                    size_t slot_count,
                                    SemanticContext *ctx,
                                    const char *kind_name)
{
    if (slots == NULL || ctx == NULL)
        return true;

    scope_enter(&ctx->scope, SCOPE_BLOCK);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Symbol *sym;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }

        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        if (slot_type == NULL || slot_type == TYPE_UNKNOWN)
            continue;

        sym = symbol_create_variable(slot->data.domain_slot.slot_name, slot_type,
            slot->line, slot->column);
        if (sym == NULL)
            continue;

        if (!scope_declare(ctx->scope, sym)) {
            semantic_error(ctx, slot,
                "Redeclaration of %s slot '%s'",
                kind_name,
                slot->data.domain_slot.slot_name);
            symbol_destroy(sym);
        }
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        Type *init_type;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || slot->data.domain_slot.initializer == NULL) {
            continue;
        }

        slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
        init_type = type_check_expression(slot->data.domain_slot.initializer, ctx);
        if (slot_type == NULL || init_type == NULL
            || slot_type == TYPE_UNKNOWN || init_type == TYPE_UNKNOWN) {
            continue;
        }

        require_assignable(init_type, slot_type,
            slot->data.domain_slot.initializer, ctx);
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

static size_t
count_subject_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t subject_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.is_subject) {
            subject_count++;
        }
    }
    return subject_count;
}

static size_t
count_object_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t object_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && !slot->data.domain_slot.is_subject) {
            object_count++;
        }
    }
    return object_count;
}

static size_t
count_bindable_domain_slots(ASTNode **slots, size_t slot_count,
                            ASTNode **refreshes, size_t refresh_count)
{
    size_t bindable_count = 0;
    (void)refreshes;
    (void)refresh_count;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }
        bindable_count++;
    }

    return bindable_count;
}

static ASTNode *
find_nth_bindable_domain_slot(ASTNode **slots, size_t slot_count,
                              ASTNode **refreshes, size_t refresh_count,
                              size_t ordinal)
{
    size_t seen = 0;
    (void)refreshes;
    (void)refresh_count;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }

        if (seen == ordinal)
            return slot;
        seen++;
    }

    return NULL;
}

static bool
role_decl_has_ability(ASTNode *role, ASTNode *program,
                      ASTNode *ability_ref, int depth)
{
    if (role == NULL || role->type != AST_ROLE_DECL
        || ability_ref == NULL || depth > 16) {
        return false;
    }

    for (size_t i = 0; i < role->data.role_decl.impl_count; i++) {
        ASTNode *impl = role->data.role_decl.impl_abilities[i];
        if (impl != NULL
            && impl->type == AST_IMPL_ABILITY
            && impl->data.impl_ability.ability_ref != NULL
            && ability_ref_matches(impl->data.impl_ability.ability_ref, ability_ref)) {
            return true;
        }
    }

    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *inc = role->data.role_decl.includes[i];
        ASTNode *included = find_role_decl_in_program(program,
            inc != NULL ? inc->data.include_stmt.role_name : NULL);
        if (role_decl_has_ability(included, program, ability_ref, depth + 1))
            return true;
    }

    return false;
}

static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, type_name) != 0) {
            continue;
        }
        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

static ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || type_name == NULL || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL
            || strcmp(stmt->data.role_decl.for_type->data.type.name, type_name) != 0) {
            continue;
        }
        for (size_t j = 0; j < stmt->data.role_decl.impl_count; j++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[j];
            if (impl != NULL
                && impl->type == AST_IMPL_ABILITY
                && impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL
                && strcmp(impl->data.impl_ability.ability_ref->data.type.name, ability_name) == 0) {
                return impl->data.impl_ability.ability_ref;
            }
        }
    }

    return NULL;
}

static bool
any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_ref == NULL) {
        return false;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        if (role_decl_has_ability(stmt, program, ability_ref, 0))
            return true;
    }

    return false;
}

static ASTNode *
any_subject_role_find_base_ability_impl(ASTNode *program, const char *ability_name)
{
    if (program == NULL || program->type != AST_PROGRAM
        || ability_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        ASTNode *type_decl;
        const char *type_name;

        if (stmt == NULL || stmt->type != AST_ROLE_DECL
            || stmt->data.role_decl.for_type == NULL
            || stmt->data.role_decl.for_type->type != AST_TYPE
            || stmt->data.role_decl.for_type->data.type.name == NULL) {
            continue;
        }

        type_name = stmt->data.role_decl.for_type->data.type.name;
        type_decl = find_subject_host_decl_by_name(program, type_name);
        if (!decl_is_subject_host(type_decl))
            continue;

        for (size_t j = 0; j < stmt->data.role_decl.impl_count; j++) {
            ASTNode *impl = stmt->data.role_decl.impl_abilities[j];
            ASTNode *ability_ref;

            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            ability_ref = impl->data.impl_ability.ability_ref;
            if (ability_ref == NULL || ability_ref->type != AST_TYPE
                || ability_ref->data.type.name == NULL) {
                continue;
            }
            if (strcmp(ability_ref->data.type.name, ability_name) == 0)
                return ability_ref;
        }
    }

    return NULL;
}

static ASTNode *
find_domain_slot_local(ASTNode **slots, size_t slot_count, const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    return find_domain_slot_local(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, slot_name);
}

static ASTNode *
find_domain_decl_by_name(ASTNode *program,
                         ASTNodeType decl_type,
                         const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != decl_type)
            continue;

        const char *decl_name = NULL;
        if (decl_type == AST_RELATION_DECL)
            decl_name = stmt->data.relation_decl.name;
        else if (decl_type == AST_EFFECT_DECL)
            decl_name = stmt->data.effect_decl.name;
        else if (decl_type == AST_ZONE_DECL)
            decl_name = stmt->data.zone_decl.name;
        else if (decl_type == AST_WORLD_DECL)
            decl_name = stmt->data.world_decl.name;
        else if (decl_type == AST_ROSTER_DECL)
            decl_name = stmt->data.roster_decl.name;

        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
find_zone_effect_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && !slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_relation_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_authority(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        if (authority != NULL
            && authority->type == AST_ZONE_AUTHORITY
            && authority->data.zone_authority.subject_slot_name != NULL
            && strcmp(authority->data.zone_authority.subject_slot_name, slot_name) == 0) {
            return authority;
        }
    }

    return NULL;
}

static ASTNode *
find_zone_state(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static bool
resolve_zone_effect_state(ASTNode *zone,
                          ASTNode *site,
                          const char *state_name,
                          SemanticContext *ctx,
                          const char *action_name,
                          const char **effect_slot_name,
                          const char **target_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (effect_slot_name != NULL)
        *effect_slot_name = NULL;
    if (target_slot_name != NULL)
        *target_slot_name = NULL;

    if (state == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (state->data.zone_state.is_relation) {
        semantic_error(ctx, site,
            "Zone %s state '%s' is a relation state and cannot be used as an effect state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (effect_slot_name != NULL)
        *effect_slot_name = state->data.zone_state.layer_slot_name;
    if (target_slot_name != NULL)
        *target_slot_name = state->data.zone_state.left_or_target_slot_name;
    return true;
}

static bool
resolve_zone_relation_state(ASTNode *zone,
                            ASTNode *site,
                            const char *state_name,
                            SemanticContext *ctx,
                            const char *action_name,
                            const char **relation_slot_name,
                            const char **left_slot_name,
                            const char **right_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (relation_slot_name != NULL)
        *relation_slot_name = NULL;
    if (left_slot_name != NULL)
        *left_slot_name = NULL;
    if (right_slot_name != NULL)
        *right_slot_name = NULL;

    if (state == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (!state->data.zone_state.is_relation) {
        semantic_error(ctx, site,
            "Zone %s state '%s' is an effect state and cannot be used as a relation state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (relation_slot_name != NULL)
        *relation_slot_name = state->data.zone_state.layer_slot_name;
    if (left_slot_name != NULL)
        *left_slot_name = state->data.zone_state.left_or_target_slot_name;
    if (right_slot_name != NULL)
        *right_slot_name = state->data.zone_state.right_slot_name;
    return true;
}

static bool
type_check_zone_participant_authority(ASTNode *zone,
                                ASTNode *site,
                                const char *participant_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *participant_slot;
    ASTNode *authority;

    if (zone == NULL || ctx == NULL || participant_slot_name == NULL)
        return false;

    participant_slot = find_zone_domain_slot(zone, participant_slot_name);
    if (participant_slot == NULL) {
        semantic_error(ctx, site,
            "Zone %s references unknown authority subject slot '%s'",
            action_name, participant_slot_name);
        return true;
    }

    if (!participant_slot->data.domain_slot.is_subject) {
        semantic_error(ctx, site,
            "Zone %s authority '%s' must be a subject slot",
            action_name, participant_slot_name);
        return true;
    }

    authority = find_zone_authority(zone, participant_slot_name);
    if (zone->data.zone_decl.authority_count > 0 && authority == NULL) {
        semantic_error(ctx, site,
            "Zone %s participant '%s' is not declared in zone authority set",
            action_name, participant_slot_name);
        return true;
    }

    return true;
}

static ASTNode *
find_named_class_decl_local(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL
            || stmt->data.class_decl.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.class_decl.name, name) == 0)
            return stmt;
    }

    return NULL;
}

static bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee;

    if (expr == NULL || ctx == NULL || expr->type != AST_CALL)
        return false;

    callee = expr->data.call.callee;
    if (callee == NULL)
        return false;

    if (callee->type == AST_IDENTIFIER && callee->data.identifier.name != NULL) {
        Symbol *sym = scope_lookup(ctx->scope, callee->data.identifier.name);
        return sym != NULL
            && sym->type != NULL
            && sym->type->kind == TYPE_KIND_FUNCTION
            && type_effect_mask_requires_authority(type_function_effects(sym->type));
    }

    if (callee->type == AST_MEMBER_ACCESS
        && callee->data.member.object != NULL
        && callee->data.member.name != NULL) {
        Type *object_type = type_check_expression(callee->data.member.object, ctx);
        ASTNode *class_decl;

        if (object_type == NULL || object_type->name == NULL)
            return false;

        class_decl = find_type_decl_by_name(ctx->program_root, object_type->name);
        if (class_decl == NULL || class_decl->type != AST_CLASS_DECL)
            return false;

        for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
            ASTNode *method = class_decl->data.class_decl.methods[i];
            if (method == NULL || method->type != AST_FUNC_DECL
                || method->data.func_decl.name == NULL) {
                continue;
            }
            if (strcmp(method->data.func_decl.name, callee->data.member.name) == 0) {
                uint32_t method_effects =
                    declared_effects_from_function_node(method, ctx, NULL);
                if (type_effect_mask_requires_authority(method_effects))
                    return true;
                if (method->data.func_decl.is_action
                    && (method->data.func_decl.within_zone != NULL
                        || method->data.func_decl.causes_effect != NULL)) {
                    return true;
                }
                return false;
            }
        }
    }

    return false;
}

static bool
type_check_projection_contract(ASTNode **slots,
                               size_t slot_count,
                               const char *owner_label,
                               ASTNode *site,
                               const char *object_slot_name,
                               const char *source_slot_name,
                               SemanticContext *ctx,
                               const char *action_name)
{
    ASTNode *object_slot;
    ASTNode *source_slot;
    bool requires_dto;
    Type *target_type;
    Type *source_type;
    ASTNode *target_decl;
    ASTNode *source_decl;

    object_slot = find_domain_slot_local(slots, slot_count, object_slot_name);
    source_slot = find_domain_slot_local(slots, slot_count, source_slot_name);
    if (object_slot == NULL || source_slot == NULL || ctx == NULL)
        return false;

    if (object_slot->data.domain_slot.is_subject
        || object_slot->data.domain_slot.is_vessel) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' is not a projection slot.\n"
            "Reason:\n"
            "- %s only writes into object/tobject projection targets\n"
            "- subject/vessel slots are ownership anchors, not projection sinks\n"
            "Fix:\n"
            "- choose an object slot for local projection sync\n"
            "- or choose a tobject slot for boundary publication",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            action_name);
        return true;
    }
    requires_dto =
        site != NULL && site->type == AST_ZONE_REFRESH
        && (site->data.zone_refresh.infer_target_kind
            ? object_slot->data.domain_slot.is_tobject
            : site->data.zone_refresh.requires_dto);
    if (requires_dto && !object_slot->data.domain_slot.is_tobject) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' must be a tobject slot.\n"
            "Reason:\n"
            "- publish writes a boundary transfer snapshot\n"
            "- object slots are local projections and cannot be published as transfer targets\n"
            "Fix:\n"
            "- change '%s' to a tobject slot\n"
            "- or use refresh/bind into an object slot instead",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            object_slot_name != NULL ? object_slot_name : "<slot>");
        return true;
    }
    target_type = resolve_type_node(object_slot->data.domain_slot.type, ctx);
    source_type = resolve_type_node(source_slot->data.domain_slot.type, ctx);
    if (target_type == NULL || source_type == NULL
        || target_type == TYPE_UNKNOWN || source_type == TYPE_UNKNOWN) {
        return true;
    }

    target_decl = find_named_class_decl_local(ctx->program_root, target_type->name);
    if (target_decl == NULL || !target_decl->data.class_decl.is_struct) {
        semantic_error(ctx, site,
            "%s %s target slot '%s' does not use a projection declaration.\n"
            "Reason:\n"
            "- projection sync requires an object/tobject nominal target\n"
            "- '%s' is not an object/tobject declaration\n"
            "Fix:\n"
            "- change the target slot type to an object or tobject declaration\n"
            "- or move this data through a plain assignment path instead",
            owner_label, action_name,
            object_slot_name != NULL ? object_slot_name : "<unknown>",
            target_type->name != NULL ? target_type->name : "<unknown>");
        return true;
    }
    if (site != NULL && site->type == AST_ZONE_REFRESH) {
        NominalDeclKind expected_kind =
            requires_dto ? NOMINAL_DECL_TOBJECT : NOMINAL_DECL_OBJECT;
        const char *expected_label =
            requires_dto ? "tobject" : "object";

        if (target_decl->data.class_decl.nominal_kind != expected_kind) {
            semantic_error(ctx, site,
                "%s %s target slot '%s' uses the wrong projection kind.\n"
                "Reason:\n"
                "- %s expects a %s declaration\n"
                "- '%s' is declared as a different projection kind\n"
                "Fix:\n"
                "- change '%s' to a %s declaration\n"
                "- or switch to the matching projection operation",
                owner_label, action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                expected_label,
                target_type->name != NULL ? target_type->name : "<unknown>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                expected_label);
            return true;
        }
    }

    if (source_slot->data.domain_slot.is_tobject) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' cannot be a tobject slot.\n"
            "Reason:\n"
            "- tobject is already a boundary snapshot\n"
            "- projection sync must read from a subject/object source, not from a transfer snapshot\n"
            "Fix:\n"
            "- use the original subject/object slot as the source\n"
            "- or materialize a new object/tobject from the original source instead",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>",
            source_type != NULL && source_type->name != NULL
                ? source_type->name
                : "<unknown>");
        return true;
    }

    source_decl = find_subject_host_decl_by_name(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl))
        source_decl = find_named_class_decl_local(ctx->program_root, source_type->name);
    if (!decl_is_projection_source(source_decl)) {
        semantic_error(ctx, site,
            "%s %s source slot '%s' is not a valid projection source.\n"
            "Reason:\n"
            "- projection sync reads fields from a subject/object declaration\n"
            "- this slot does not point at a subject/object source host\n"
            "Fix:\n"
            "- use a subject slot as the source\n"
            "- or use an object slot that mirrors the desired fields",
            owner_label, action_name,
            source_slot_name != NULL ? source_slot_name : "<unknown>");
        return true;
    }

    if (site != NULL && site->type == AST_ZONE_REFRESH
        && site->data.zone_refresh.field_map_count > 0) {
        for (size_t i = 0; i < site->data.zone_refresh.field_map_count; i++) {
            const char *mapped_target =
                site->data.zone_refresh.mapped_target_fields[i];

            if (!projection_target_decl_has_field(target_decl, mapped_target)) {
                semantic_error(ctx, site,
                    "%s %s projection map refers to unknown target field '%s' in slot '%s'",
                    owner_label, action_name,
                    mapped_target != NULL ? mapped_target : "<field>",
                    object_slot_name != NULL ? object_slot_name : "<unknown>");
            }

            for (size_t j = i + 1; j < site->data.zone_refresh.field_map_count; j++) {
                const char *other_target =
                    site->data.zone_refresh.mapped_target_fields[j];
                if (mapped_target != NULL && other_target != NULL
                    && strcmp(mapped_target, other_target) == 0) {
                    semantic_error(ctx, site,
                        "%s %s projection map duplicates target field '%s'",
                        owner_label, action_name, mapped_target);
                }
            }
        }
    }

    for (size_t i = 0; i < target_decl->data.class_decl.field_count; i++) {
        ClassField *target_field = target_decl->data.class_decl.fields[i];
        Type *target_field_type;
        Type *source_field_type;
        const char *source_field_name;
        char *source_path = NULL;
        int source_status;

        if (target_field == NULL || target_field->name == NULL
            || target_field->type == NULL) {
            continue;
        }

        source_field_name = projection_refresh_source_field_name(site,
            target_field->name);
        source_status = resolve_projection_source_field_path(
            ctx->program_root, source_decl, source_field_name, ctx,
            &source_path, &source_field_type);
        if (source_status == 0 || source_field_type == NULL) {
            if (source_field_name != NULL
                && strcmp(source_field_name, target_field->name) != 0) {
                semantic_error(ctx, site,
                    "%s %s target field '%s' maps from missing source field '%s' in slot '%s'",
                    owner_label, action_name,
                    target_field->name,
                    source_field_name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>");
            } else {
                semantic_error(ctx, site,
                    "%s %s target field '%s' is missing from source slot '%s'",
                    owner_label, action_name,
                    target_field->name,
                    source_slot_name != NULL ? source_slot_name : "<unknown>");
            }
            continue;
        }
        if (source_status == 2) {
            semantic_error(ctx, site,
                "%s %s target field '%s' is ambiguous in source slot '%s'; rename the field or expose it through a dedicated object/tobject field",
                owner_label, action_name,
                target_field->name,
                source_slot_name != NULL ? source_slot_name : "<unknown>");
            continue;
        }

        target_field_type = resolve_type_node(target_field->type, ctx);
        require_assignable(source_field_type, target_field_type, site, ctx);
        free(source_path);
    }

    return true;
}

static bool
type_check_zone_projection_contract(ASTNode *zone,
                                    ASTNode *site,
                                    const char *object_slot_name,
                                    const char *source_slot_name,
                                    SemanticContext *ctx,
                                    const char *action_name)
{
    if (zone == NULL)
        return false;
    return type_check_projection_contract(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, "Zone", site,
        object_slot_name, source_slot_name, ctx, action_name);
}

static bool
type_check_zone_effect_contract(ASTNode *zone,
                                ASTNode *apply_like,
                                const char *effect_slot_name,
                                const char *target_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *effect_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    ASTNode *decl_target;
    Type *target_type;
    Type *decl_target_type;
    size_t target_count;

    effect_slot = find_zone_effect_slot(zone, effect_slot_name);
    target_slot = find_zone_domain_slot(zone, target_slot_name);
    if (effect_slot == NULL || target_slot == NULL || ctx == NULL)
        return false;

    effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
        effect_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return false;

    target_count = count_bindable_domain_slots(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count);
    if (target_count != 1) {
        semantic_error(ctx, apply_like,
            "Zone %s requires effect '%s' to declare exactly one target slot, found %zu",
            action_name,
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            target_count);
        return true;
    }

    decl_target = find_nth_bindable_domain_slot(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count, 0);
    if (decl_target == NULL)
        return false;

    target_type = resolve_type_node(target_slot->data.domain_slot.type, ctx);
    decl_target_type = resolve_type_node(decl_target->data.domain_slot.type, ctx);
    if (!type_is_assignable(target_type, decl_target_type)) {
        semantic_error(ctx, apply_like,
            "Zone %s target slot '%s' has type '%s' but effect '%s' expects subject type '%s'",
            action_name,
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>");
    }

    return true;
}

static bool
relation_slot_matches_between_kind(ASTNode *slot,
                                   Type *slot_type,
                                   RelationEndpointKind kind,
                                   ASTNode *named_type_ref,
                                   SemanticContext *ctx)
{
    ASTNode *decl;
    Type *named_type;

    if (slot == NULL)
        return false;

    if (kind == RELATION_ENDPOINT_SUBJECT)
        return slot->data.domain_slot.is_subject;

    if (kind == RELATION_ENDPOINT_OBJECT) {
        if (slot->data.domain_slot.is_subject)
            return false;
        if (slot->data.domain_slot.is_tobject)
            return false;  /* tobject slot uses "tobject" kind, not "object" */
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
    }

    if (kind == RELATION_ENDPOINT_TOBJECT) {
        if (slot->data.domain_slot.is_subject)
            return false;
        if (!slot->data.domain_slot.is_tobject)
            return false;
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_TOBJECT;
    }

    if (kind == RELATION_ENDPOINT_CLASS) {
        if (slot->data.domain_slot.is_subject || slot->data.domain_slot.is_tobject)
            return false;
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        if (decl == NULL)
            return true;
        return decl->data.class_decl.nominal_kind != NOMINAL_DECL_OBJECT
            && decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT;
    }

    named_type = named_type_ref != NULL ? resolve_type_node(named_type_ref, ctx) : NULL;
    return slot_type != NULL
        && named_type != NULL
        && type_equals(slot_type, named_type);
}

static bool
type_check_zone_relation_contract(ASTNode *zone,
                                  ASTNode *link_like,
                                  const char *relation_slot_name,
                                  const char *left_slot_name,
                                  const char *right_slot_name,
                                  SemanticContext *ctx,
                                  const char *action_name)
{
    ASTNode *relation_slot;
    ASTNode *relation_decl;
    ASTNode *left_slot;
    ASTNode *right_slot;
    ASTNode *decl_left;
    ASTNode *decl_right;
    Type *left_type;
    Type *right_type;
    Type *decl_left_type;
    Type *decl_right_type;
    size_t endpoint_count;
    RelationEndpointKind between_left_kind;
    RelationEndpointKind between_right_kind;
    ASTNode *between_left_type;
    ASTNode *between_right_type;

    relation_slot = find_zone_relation_slot(zone, relation_slot_name);
    left_slot = find_zone_domain_slot(zone, left_slot_name);
    right_slot = find_zone_domain_slot(zone, right_slot_name);
    if (relation_slot == NULL || left_slot == NULL || right_slot == NULL || ctx == NULL)
        return false;

    relation_decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
        relation_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return false;

    between_left_kind = relation_decl->data.relation_decl.between_left_kind;
    between_right_kind = relation_decl->data.relation_decl.between_right_kind;
    between_left_type = relation_decl->data.relation_decl.between_left_type;
    between_right_type = relation_decl->data.relation_decl.between_right_type;
    left_type = resolve_type_node(left_slot->data.domain_slot.type, ctx);
    right_type = resolve_type_node(right_slot->data.domain_slot.type, ctx);

    if (between_left_kind != RELATION_ENDPOINT_NAMED || between_right_kind != RELATION_ENDPOINT_NAMED
        || between_left_type != NULL || between_right_type != NULL) {
        if (!relation_slot_matches_between_kind(left_slot, left_type, between_left_kind, between_left_type, ctx)) {
            semantic_error(ctx, link_like,
                "Zone %s left slot '%s' does not satisfy relation '%s' between-left contract",
                action_name,
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>");
        }

        if (!relation_slot_matches_between_kind(right_slot, right_type, between_right_kind, between_right_type, ctx)) {
            semantic_error(ctx, link_like,
                "Zone %s right slot '%s' does not satisfy relation '%s' between-right contract",
                action_name,
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>");
        }
        return true;
    }

    endpoint_count = count_bindable_domain_slots(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count);
    if (endpoint_count != 2) {
        semantic_error(ctx, link_like,
            "Zone %s requires relation '%s' to declare exactly two endpoint slots, found %zu",
            action_name,
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            endpoint_count);
        return true;
    }

    decl_left = find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 0);
    decl_right = find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 1);
    if (decl_left == NULL || decl_right == NULL)
        return false;

    decl_left_type = resolve_type_node(decl_left->data.domain_slot.type, ctx);
    decl_right_type = resolve_type_node(decl_right->data.domain_slot.type, ctx);

    if (!type_is_assignable(left_type, decl_left_type)) {
        semantic_error(ctx, link_like,
            "Zone %s left slot '%s' has type '%s' but relation '%s' expects left endpoint type '%s'",
            action_name,
            left_slot_name != NULL ? left_slot_name : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>");
    }

    if (!type_is_assignable(right_type, decl_right_type)) {
        semantic_error(ctx, link_like,
            "Zone %s right slot '%s' has type '%s' but relation '%s' expects right endpoint type '%s'",
            action_name,
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>");
    }

    return true;
}

static bool
type_check_overlay_decl_common(ASTNode *node,
                               SemanticContext *ctx,
                               const char *name,
                               SymbolKind kind,
                               ASTNode **shared_fields,
                               size_t shared_count,
                               ASTNode **methods,
                               size_t method_count,
                               const char *kind_name)
{
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->name = pergyra_strdup(name);
    sym->kind = kind;
    sym->type = create_overlay_nominal_type(name);
    sym->decl_line = node->line;
    sym->decl_col = node->column;

    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_CLASS) {
        existing->kind = kind;
        if (existing->type == NULL || existing->type == TYPE_VOID)
            existing->type = create_overlay_nominal_type(name);
        symbol_destroy(sym);
    } else if (existing != NULL) {
        semantic_error(ctx, node, "Redeclaration of %s '%s'", kind_name, name);
        symbol_destroy(sym);
        return false;
    } else {
        scope_declare(ctx->scope, sym);
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (shared->data.party_shared.type != NULL)
            resolve_type_node(shared->data.party_shared.type, ctx);
        if (shared->data.party_shared.initializer != NULL)
            type_check_expression(shared->data.party_shared.initializer, ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    /* Register domain slots so bare slot access works in hosted funcs */
    if (node->type == AST_ZONE_DECL) {
        for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
            ASTNode *slot = node->data.zone_decl.slots[i];
            if (slot != NULL && slot->type == AST_DOMAIN_SLOT
                && slot->data.domain_slot.slot_name != NULL
                && slot->data.domain_slot.type != NULL) {
                Type *slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
                Symbol *slot_sym = calloc(1, sizeof(Symbol));
                slot_sym->name = pergyra_strdup(slot->data.domain_slot.slot_name);
                slot_sym->kind = SYMBOL_VARIABLE;
                slot_sym->type = slot_type != NULL ? slot_type : TYPE_INT;
                slot_sym->decl_line = slot->line;
                slot_sym->decl_col = slot->column;
                scope_declare(ctx->scope, slot_sym);
            }
        }
    }
    if (node->type == AST_WORLD_DECL) {
        for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
            ASTNode *wz = node->data.world_decl.zones[i];
            if (wz != NULL && wz->type == AST_WORLD_ZONE
                && wz->data.world_zone.slot_name != NULL
                && wz->data.world_zone.zone_type != NULL) {
                Type *zone_type = resolve_named_type(
                    wz->data.world_zone.zone_type, ctx, wz);
                Symbol *zone_sym = calloc(1, sizeof(Symbol));
                zone_sym->name = pergyra_strdup(wz->data.world_zone.slot_name);
                zone_sym->kind = SYMBOL_VARIABLE;
                zone_sym->type = zone_type != NULL ? zone_type : TYPE_INT;
                zone_sym->decl_line = wz->line;
                zone_sym->decl_col = wz->column;
                scope_declare(ctx->scope, zone_sym);
            }
        }
    }
    /* Register shared fields so bare field access works in hosted funcs */
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (shared != NULL && shared->data.party_shared.name != NULL) {
            Type *field_type = TYPE_INT;
            if (shared->data.party_shared.type != NULL)
                field_type = resolve_type_node(shared->data.party_shared.type, ctx);
            Symbol *field_sym = calloc(1, sizeof(Symbol));
            field_sym->name = pergyra_strdup(shared->data.party_shared.name);
            field_sym->kind = SYMBOL_VARIABLE;
            field_sym->type = field_type;
            field_sym->decl_line = shared->line;
            field_sym->decl_col = shared->column;
            scope_declare(ctx->scope, field_sym);
        }
    }
    for (size_t i = 0; i < method_count; i++)
        type_check_func_decl(methods[i], ctx);
    scope_exit(&ctx->scope);

    return !ctx->has_error;
}

bool
type_check_relation_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_relation = ctx->current_relation;
    ctx->current_relation = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.relation_decl.name,
        SYMBOL_RELATION,
        node->data.relation_decl.shared_fields,
        node->data.relation_decl.shared_count,
        node->data.relation_decl.methods,
        node->data.relation_decl.method_count,
        "relation");

    ok = type_check_domain_slots(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "Relation") && ok;
    ok = type_check_domain_slot_initializers(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "relation") && ok;
    for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.relation_decl.refreshes[i];
        const char *action_name =
            refresh->data.zone_refresh.infer_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.relation_decl.slots,
            node->data.relation_decl.slot_count, "Relation", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            action_name) && ok;
    }
    size_t endpoint_count = count_bindable_domain_slots(
        node->data.relation_decl.slots,
        node->data.relation_decl.slot_count,
        node->data.relation_decl.refreshes,
        node->data.relation_decl.refresh_count);
    if (endpoint_count == 0
        && !(node->data.relation_decl.between_left_kind != RELATION_ENDPOINT_NAMED
            || node->data.relation_decl.between_right_kind != RELATION_ENDPOINT_NAMED
            || node->data.relation_decl.between_left_type != NULL
            || node->data.relation_decl.between_right_type != NULL)) {
        semantic_warning(ctx, node,
            "Relation '%s' should declare at least one endpoint slot; use 'for name: Type' or a bindable subject/object slot",
            node->data.relation_decl.name);
    } else if (endpoint_count > 2) {
        semantic_warning(ctx, node,
            "Relation '%s' currently models a small edge; more than two endpoint slots may be better expressed as party or zone",
            node->data.relation_decl.name);
    }
    /* Validate 'between' clause if present */
    RelationEndpointKind blk = node->data.relation_decl.between_left_kind;
    RelationEndpointKind brk = node->data.relation_decl.between_right_kind;
    ASTNode *blt = node->data.relation_decl.between_left_type;
    ASTNode *brt = node->data.relation_decl.between_right_type;
    if (blk != RELATION_ENDPOINT_NAMED || brk != RELATION_ENDPOINT_NAMED
        || blt != NULL || brt != NULL) {
        /* At least one side must be 'subject' */
        bool left_is_subject = (blk == RELATION_ENDPOINT_SUBJECT);
        bool right_is_subject = (brk == RELATION_ENDPOINT_SUBJECT);
        if (!left_is_subject && !right_is_subject) {
            semantic_error(ctx, node,
                "Relation '%s': at least one side of 'between' must be 'subject' "
                "(got endpoint kinds %d and %d)",
                node->data.relation_decl.name, (int)blk, (int)brk);
            ok = false;
        }
    }

    ctx->current_relation = saved_relation;
    return ok && !ctx->has_error;
}

bool
type_check_effect_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_effect = ctx->current_effect;
    ctx->current_effect = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.effect_decl.name,
        SYMBOL_EFFECT,
        node->data.effect_decl.shared_fields,
        node->data.effect_decl.shared_count,
        node->data.effect_decl.methods,
        node->data.effect_decl.method_count,
        "effect");

    ok = type_check_domain_slots(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "Effect") && ok;
    ok = type_check_domain_slot_initializers(node->data.effect_decl.slots,
        node->data.effect_decl.slot_count, ctx, "effect") && ok;
    for (size_t i = 0; i < node->data.effect_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.effect_decl.refreshes[i];
        const char *action_name =
            refresh->data.zone_refresh.infer_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.effect_decl.slots,
            node->data.effect_decl.slot_count, "Effect", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            action_name) && ok;
    }
    size_t target_count = count_bindable_domain_slots(
        node->data.effect_decl.slots,
        node->data.effect_decl.slot_count,
        node->data.effect_decl.refreshes,
        node->data.effect_decl.refresh_count);
    if (target_count == 0) {
        semantic_warning(ctx, node,
            "Effect '%s' should declare at least one target slot; use 'for name: Type' or a bindable subject/object slot",
            node->data.effect_decl.name);
    } else if (target_count > 1) {
        semantic_warning(ctx, node,
            "Effect '%s' currently expects a small target surface; multiple target slots may be better expressed via relation or zone",
            node->data.effect_decl.name);
    }
    ctx->current_effect = saved_effect;
    return ok && !ctx->has_error;
}

bool
type_check_zone_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_zone = ctx->current_zone;
    size_t subject_count;
    size_t object_count;
    size_t mutation_rule_count;
    ctx->current_zone = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.zone_decl.name,
        SYMBOL_ZONE,
        node->data.zone_decl.shared_fields,
        node->data.zone_decl.shared_count,
        node->data.zone_decl.methods,
        node->data.zone_decl.method_count,
        "zone");

    ok = type_check_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "Zone") && ok;
    ok = type_check_domain_slot_initializers(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count, ctx, "zone") && ok;
    subject_count = count_subject_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);
    object_count = count_object_domain_slots(node->data.zone_decl.slots,
        node->data.zone_decl.slot_count);

    if (subject_count > 4) {
        semantic_warning(ctx, node,
            "Zone '%s' declares %zu subject slots; prefer keeping active subjects to 4 or fewer and model supporting state as objects",
            node->data.zone_decl.name,
            subject_count);
    }

    if (subject_count > 1 && object_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has multiple subject slots but no object slots; consider modeling passive support state as objects",
            node->data.zone_decl.name);
    }

    mutation_rule_count = node->data.zone_decl.apply_count
        + node->data.zone_decl.link_count
        + node->data.zone_decl.detach_count
        + node->data.zone_decl.unlink_count
        + node->data.zone_decl.refresh_count
        + node->data.zone_decl.maintained_effect_count
        + node->data.zone_decl.maintained_relation_count
        + node->data.zone_decl.maintained_state_count;

    if (subject_count == 0
        && (mutation_rule_count > 0 || node->data.zone_decl.authority_count > 0)) {
        semantic_warning(ctx, node,
            "Zone '%s' mutates state or declares authority but has no subject slot",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
        ASTNode *authority = node->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(node, authority->data.zone_authority.subject_slot_name);
        if (slot == NULL) {
            semantic_error(ctx, authority,
                "Zone authority references unknown subject slot '%s'",
                authority->data.zone_authority.subject_slot_name);
            continue;
        }
        if (!slot->data.domain_slot.is_subject) {
            semantic_error(ctx, authority,
                "Zone authority '%s' must reference a subject slot",
                authority->data.zone_authority.subject_slot_name);
        }
        slot_type = slot != NULL ? resolve_type_node(slot->data.domain_slot.type, ctx) : NULL;
        for (size_t j = 0; j < authority->data.zone_authority.ability_count; j++) {
            ASTNode *ability_ref = authority->data.zone_authority.required_abilities[j];
            const char *ability_name = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            if (resolve_required_ability_decl(
                    ability_ref, authority, ctx, "Zone authority",
                    authority->data.zone_authority.subject_slot_name) == NULL) {
                free(required_text);
                continue;
            }
            if (slot_type == NULL || slot_type == TYPE_UNKNOWN
                || slot_type->name == NULL) {
                free(required_text);
                continue;
            }
            if (!subject_type_has_ability(ctx->program_root, slot_type->name, ability_ref)) {
                ASTNode *actual_impl = subject_type_find_base_ability_impl(
                    ctx->program_root, slot_type->name, ability_name);
                char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                semantic_error(ctx, authority,
                    actual_impl != NULL
                        ? "Zone authority '%s' requires ability '%s', but subject type '%s' implements '%s' instead.\n"
                          "Reason:\n"
                          "- subject type '%s' does not satisfy required ability '%s'\n"
                          "Fix:\n"
                          "- implement '%s' for subject type '%s'\n"
                          "- or change/remove the zone authority requirement"
                        : "Zone authority '%s' requires ability '%s', but subject type '%s' has no matching role impl.\n"
                          "Reason:\n"
                          "- subject type '%s' does not satisfy required ability '%s'\n"
                          "Fix:\n"
                          "- implement '%s' for subject type '%s'\n"
                          "- or change/remove the zone authority requirement",
                    authority->data.zone_authority.subject_slot_name,
                    required_text != NULL ? required_text : ability_name,
                    slot_type->name,
                    actual_text != NULL ? actual_text : NULL,
                    slot_type->name,
                    required_text != NULL ? required_text : ability_name,
                    required_text != NULL ? required_text : ability_name,
                    slot_type->name,
                    authority->data.zone_authority.subject_slot_name,
                    required_text != NULL ? required_text : ability_name,
                    slot_type->name,
                    slot_type->name,
                    required_text != NULL ? required_text : ability_name,
                    required_text != NULL ? required_text : ability_name,
                    slot_type->name,
                    actual_text != NULL ? actual_text : NULL);
                free(actual_text);
            }
            free(required_text);
        }
        for (size_t j = i + 1; j < node->data.zone_decl.authority_count; j++) {
            ASTNode *other = node->data.zone_decl.authorities[j];
            if (other != NULL
                && other->data.zone_authority.subject_slot_name != NULL
                && strcmp(authority->data.zone_authority.subject_slot_name,
                          other->data.zone_authority.subject_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' declares authority '%s' more than once",
                    node->data.zone_decl.name,
                    authority->data.zone_authority.subject_slot_name);
            }
        }
    }

    if (mutation_rule_count > 0 && node->data.zone_decl.authority_count == 0) {
        semantic_warning(ctx, node,
            "Zone '%s' has lifecycle-changing rules but no explicit authority set",
            node->data.zone_decl.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = node->data.zone_decl.layer_slots[i];
        const char *type_name = layer_slot->data.zone_layer_slot.layer_type;
        Symbol *sym = type_name != NULL ? scope_lookup(ctx->scope, type_name) : NULL;
        SymbolKind expected = layer_slot->data.zone_layer_slot.is_relation
            ? SYMBOL_RELATION
            : SYMBOL_EFFECT;
        const char *kind_name = layer_slot->data.zone_layer_slot.is_relation
            ? "relation"
            : "effect";
        if (sym == NULL || sym->kind != expected) {
            semantic_warning(ctx, layer_slot,
                "%s type '%s' not found for slot '%s'",
                kind_name,
                type_name != NULL ? type_name : "<unknown>",
                layer_slot->data.zone_layer_slot.slot_name);
        }
        if (layer_slot->data.zone_layer_slot.is_relation
            && layer_slot->data.zone_layer_slot.is_pool) {
            semantic_error(ctx, layer_slot,
                "Zone relation pool '%s' is not supported yet",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<unknown>");
        }
        if (!layer_slot->data.zone_layer_slot.is_relation
            && layer_slot->data.zone_layer_slot.is_pool
            && layer_slot->data.zone_layer_slot.pool_capacity <= 0) {
            semantic_error(ctx, layer_slot,
                "Zone effect pool '%s' must declare a positive capacity",
                layer_slot->data.zone_layer_slot.slot_name != NULL
                    ? layer_slot->data.zone_layer_slot.slot_name
                    : "<unknown>");
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        const char *effect_slot_name = apply->data.zone_apply.effect_slot_name;
        const char *target_slot_name = apply->data.zone_apply.target_slot_name;
        const char *state_name = apply->data.zone_apply.state_name;
        const char *participant_slot_name = apply->data.zone_apply.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, apply, state_name, ctx, "apply",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, apply,
                "Zone apply references unknown effect slot '%s'",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, apply,
                "Zone apply references unknown target slot '%s'",
                target_slot_name != NULL ? target_slot_name : "<unknown>");
        }
        type_check_zone_effect_contract(node, apply,
            effect_slot_name, target_slot_name, ctx, "apply");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, apply,
                "Zone apply should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, apply, participant_slot_name, ctx, "apply");
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        const char *relation_slot_name = link->data.zone_link.relation_slot_name;
        const char *left_slot_name = link->data.zone_link.left_slot_name;
        const char *right_slot_name = link->data.zone_link.right_slot_name;
        const char *state_name = link->data.zone_link.state_name;
        const char *participant_slot_name = link->data.zone_link.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, link, state_name, ctx, "link",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown relation slot '%s'",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown left slot '%s'",
                left_slot_name != NULL ? left_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, link,
                "Zone link references unknown right slot '%s'",
                right_slot_name != NULL ? right_slot_name : "<unknown>");
        }
        type_check_zone_relation_contract(node, link,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "link");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, link,
                "Zone link should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, link, participant_slot_name, ctx, "link");
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        const char *effect_slot_name = detach->data.zone_detach.effect_slot_name;
        const char *target_slot_name = detach->data.zone_detach.target_slot_name;
        const char *state_name = detach->data.zone_detach.state_name;
        const char *participant_slot_name = detach->data.zone_detach.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_effect_state(node, detach, state_name, ctx, "detach",
                &effect_slot_name, &target_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, detach,
                "Zone detach references unknown effect slot '%s'",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, detach,
                "Zone detach references unknown target slot '%s'",
                target_slot_name != NULL ? target_slot_name : "<unknown>");
        }
        type_check_zone_effect_contract(node, detach,
            effect_slot_name, target_slot_name, ctx, "detach");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, detach,
                "Zone detach should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, detach, participant_slot_name, ctx, "detach");
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        const char *relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
        const char *left_slot_name = unlink->data.zone_unlink.left_slot_name;
        const char *right_slot_name = unlink->data.zone_unlink.right_slot_name;
        const char *state_name = unlink->data.zone_unlink.state_name;
        const char *participant_slot_name = unlink->data.zone_unlink.participant_slot_name;
        bool state_ok = true;
        if (state_name != NULL) {
            state_ok = resolve_zone_relation_state(node, unlink, state_name, ctx, "unlink",
                &relation_slot_name, &left_slot_name, &right_slot_name);
        }
        if (!state_ok)
            continue;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown relation slot '%s'",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown left slot '%s'",
                left_slot_name != NULL ? left_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, unlink,
                "Zone unlink references unknown right slot '%s'",
                right_slot_name != NULL ? right_slot_name : "<unknown>");
        }
        type_check_zone_relation_contract(node, unlink,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "unlink");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, unlink,
                "Zone unlink should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, unlink, participant_slot_name, ctx, "unlink");
    }

    for (size_t i = 0; i < node->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.zone_decl.refreshes[i];
        const char *object_slot_name = refresh->data.zone_refresh.object_slot_name;
        const char *source_slot_name = refresh->data.zone_refresh.source_slot_name;
        const char *participant_slot_name = refresh->data.zone_refresh.participant_slot_name;
        ASTNode *target_slot = NULL;
        bool boundary_projection = false;
        const char *action_name =
            refresh->data.zone_refresh.infer_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (find_zone_domain_slot(node, object_slot_name) == NULL) {
            semantic_error(ctx, refresh,
                "Zone %s references unknown target slot '%s'.\n"
                "Reason:\n"
                "- the projection target must already be declared in the zone\n"
                "- '%s' is not a known object/tobject slot in zone '%s'\n"
                "Fix:\n"
                "- declare object/tobject slot '%s' in the zone first\n"
                "- or change %s to an existing projection target slot",
                action_name,
                object_slot_name != NULL ? object_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                object_slot_name != NULL ? object_slot_name : "<slot>",
                action_name);
        }
        if (find_zone_domain_slot(node, source_slot_name) == NULL) {
            semantic_error(ctx, refresh,
                "Zone %s references unknown source slot '%s'.\n"
                "Reason:\n"
                "- projection sync must read from a declared zone slot\n"
                "- '%s' is not a known source slot in zone '%s'\n"
                "Fix:\n"
                "- declare source slot '%s' in the zone first\n"
                "- or change %s to an existing source slot",
                action_name,
                source_slot_name != NULL ? source_slot_name : "<unknown>",
                node->data.zone_decl.name != NULL ? node->data.zone_decl.name : "<zone>",
                source_slot_name != NULL ? source_slot_name : "<slot>",
                action_name);
        }
        target_slot = find_zone_domain_slot(node, object_slot_name);
        boundary_projection = target_slot != NULL
            && target_slot->type == AST_DOMAIN_SLOT
            && target_slot->data.domain_slot.is_tobject;
        type_check_zone_projection_contract(node, refresh,
            object_slot_name, source_slot_name, ctx, action_name);
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            if (boundary_projection) {
                semantic_error(ctx, refresh,
                    "Zone %s to boundary target '%s' must specify 'by <subjectSlot>' when authority is declared",
                    action_name,
                    object_slot_name != NULL ? object_slot_name : "<unknown>");
            } else {
                semantic_warning(ctx, refresh,
                    "Zone %s should specify 'by <subjectSlot>' when authority is declared",
                    action_name);
            }
        }
        type_check_zone_participant_authority(node, refresh, participant_slot_name, ctx, action_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        const char *effect_slot_name = maintain->data.zone_maintain_effect.effect_slot_name;
        const char *target_slot_name = maintain->data.zone_maintain_effect.target_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_effect.participant_slot_name;
        if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown effect slot '%s'",
                effect_slot_name != NULL ? effect_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, target_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown target slot '%s'",
                target_slot_name != NULL ? target_slot_name : "<unknown>");
        }
        type_check_zone_effect_contract(node, maintain,
            effect_slot_name, target_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, maintain,
                "Zone maintain should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_effect_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_effects[j];
            if (strcmp(effect_slot_name, other->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(target_slot_name, other->data.zone_maintain_effect.target_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains effect '%s' on '%s' more than once",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
            ASTNode *detach = node->data.zone_decl.detaches[j];
            const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
            const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
            if (detach->data.zone_detach.state_name != NULL) {
                resolve_zone_effect_state(node, detach,
                    detach->data.zone_detach.state_name, ctx, "detach",
                    &detach_effect_slot_name, &detach_target_slot_name);
            }
            if (detach_effect_slot_name != NULL
                && detach_target_slot_name != NULL
                && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and detaches effect '%s' on '%s'; choose one lifecycle direction",
                    node->data.zone_decl.name,
                    effect_slot_name,
                    target_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
        const char *relation_slot_name = maintain->data.zone_maintain_relation.relation_slot_name;
        const char *left_slot_name = maintain->data.zone_maintain_relation.left_slot_name;
        const char *right_slot_name = maintain->data.zone_maintain_relation.right_slot_name;
        const char *participant_slot_name = maintain->data.zone_maintain_relation.participant_slot_name;
        if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown relation slot '%s'",
                relation_slot_name != NULL ? relation_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, left_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown left slot '%s'",
                left_slot_name != NULL ? left_slot_name : "<unknown>");
        }
        if (find_zone_domain_slot(node, right_slot_name) == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown right slot '%s'",
                right_slot_name != NULL ? right_slot_name : "<unknown>");
        }
        type_check_zone_relation_contract(node, maintain,
            relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, maintain,
                "Zone maintain should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");

        for (size_t j = i + 1; j < node->data.zone_decl.maintained_relation_count; j++) {
            ASTNode *other = node->data.zone_decl.maintained_relations[j];
            if (strcmp(relation_slot_name, other->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(left_slot_name, other->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(right_slot_name, other->data.zone_maintain_relation.right_slot_name) == 0) {
                semantic_warning(ctx, other,
                    "Zone '%s' maintains relation '%s' between '%s' and '%s' more than once",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
        for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
            ASTNode *unlink = node->data.zone_decl.unlinks[j];
            const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
            const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
            const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
            if (unlink->data.zone_unlink.state_name != NULL) {
                resolve_zone_relation_state(node, unlink,
                    unlink->data.zone_unlink.state_name, ctx, "unlink",
                    &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
            }
            if (unlink_relation_slot_name != NULL
                && unlink_left_slot_name != NULL
                && unlink_right_slot_name != NULL
                && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                && strcmp(left_slot_name, unlink_left_slot_name) == 0
                && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                semantic_warning(ctx, maintain,
                    "Zone '%s' both maintains and unlinks relation '%s' between '%s' and '%s'; choose one lifecycle direction",
                    node->data.zone_decl.name,
                    relation_slot_name,
                    left_slot_name,
                    right_slot_name);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        ASTNode *state;
        const char *state_name = maintain->data.zone_maintain_state.state_name;
        const char *participant_slot_name = maintain->data.zone_maintain_state.participant_slot_name;
        state = find_zone_state(node, state_name);
        if (state == NULL) {
            semantic_error(ctx, maintain,
                "Zone maintain references unknown state '%s'",
                state_name != NULL ? state_name : "<unknown>");
        } else if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            type_check_zone_relation_contract(node, maintain,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.unlink_count; j++) {
                ASTNode *unlink = node->data.zone_decl.unlinks[j];
                const char *unlink_relation_slot_name = unlink->data.zone_unlink.relation_slot_name;
                const char *unlink_left_slot_name = unlink->data.zone_unlink.left_slot_name;
                const char *unlink_right_slot_name = unlink->data.zone_unlink.right_slot_name;
                if (unlink->data.zone_unlink.state_name != NULL) {
                    resolve_zone_relation_state(node, unlink,
                        unlink->data.zone_unlink.state_name, ctx, "unlink",
                        &unlink_relation_slot_name, &unlink_left_slot_name, &unlink_right_slot_name);
                }
                if (unlink_relation_slot_name != NULL
                    && unlink_left_slot_name != NULL
                    && unlink_right_slot_name != NULL
                    && strcmp(relation_slot_name, unlink_relation_slot_name) == 0
                    && strcmp(left_slot_name, unlink_left_slot_name) == 0
                    && strcmp(right_slot_name, unlink_right_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and unlinks state '%s'; choose one lifecycle direction",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            type_check_zone_effect_contract(node, maintain,
                effect_slot_name, target_slot_name, ctx, "maintain");
            for (size_t j = i + 1; j < node->data.zone_decl.maintained_state_count; j++) {
                ASTNode *other = node->data.zone_decl.maintained_states[j];
                if (other != NULL
                    && other->data.zone_maintain_state.state_name != NULL
                    && strcmp(state_name, other->data.zone_maintain_state.state_name) == 0) {
                    semantic_warning(ctx, other,
                        "Zone '%s' maintains state '%s' more than once",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
            for (size_t j = 0; j < node->data.zone_decl.detach_count; j++) {
                ASTNode *detach = node->data.zone_decl.detaches[j];
                const char *detach_effect_slot_name = detach->data.zone_detach.effect_slot_name;
                const char *detach_target_slot_name = detach->data.zone_detach.target_slot_name;
                if (detach->data.zone_detach.state_name != NULL) {
                    resolve_zone_effect_state(node, detach,
                        detach->data.zone_detach.state_name, ctx, "detach",
                        &detach_effect_slot_name, &detach_target_slot_name);
                }
                if (detach_effect_slot_name != NULL
                    && detach_target_slot_name != NULL
                    && strcmp(effect_slot_name, detach_effect_slot_name) == 0
                    && strcmp(target_slot_name, detach_target_slot_name) == 0) {
                    semantic_warning(ctx, maintain,
                        "Zone '%s' both maintains and detaches state '%s'; choose one lifecycle direction",
                        node->data.zone_decl.name,
                        state_name);
                }
            }
        }
        if (node->data.zone_decl.authority_count > 0 && participant_slot_name == NULL) {
            semantic_warning(ctx, maintain,
                "Zone maintain should specify 'by <subjectSlot>' when authority is declared");
        }
        type_check_zone_participant_authority(node, maintain, participant_slot_name, ctx, "maintain");
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        const char *state_name = state->data.zone_state.state_name;
        if (state->data.zone_state.is_relation) {
            const char *relation_slot_name = state->data.zone_state.layer_slot_name;
            const char *left_slot_name = state->data.zone_state.left_or_target_slot_name;
            const char *right_slot_name = state->data.zone_state.right_slot_name;
            if (find_zone_relation_slot(node, relation_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown relation slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    relation_slot_name != NULL ? relation_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, left_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown left slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    left_slot_name != NULL ? left_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, right_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown right slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    right_slot_name != NULL ? right_slot_name : "<unknown>");
            }
            type_check_zone_relation_contract(node, state,
                relation_slot_name, left_slot_name, right_slot_name, ctx, "state");
        } else {
            const char *effect_slot_name = state->data.zone_state.layer_slot_name;
            const char *target_slot_name = state->data.zone_state.left_or_target_slot_name;
            if (find_zone_effect_slot(node, effect_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown effect slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    effect_slot_name != NULL ? effect_slot_name : "<unknown>");
            }
            if (find_zone_domain_slot(node, target_slot_name) == NULL) {
                semantic_error(ctx, state,
                    "Zone state '%s' references unknown target slot '%s'",
                    state_name != NULL ? state_name : "<unknown>",
                    target_slot_name != NULL ? target_slot_name : "<unknown>");
            }
            type_check_zone_effect_contract(node, state,
                effect_slot_name, target_slot_name, ctx, "state");
        }

        for (size_t j = i + 1; j < node->data.zone_decl.state_count; j++) {
            ASTNode *other = node->data.zone_decl.states[j];
            if (state_name != NULL
                && other != NULL
                && other->data.zone_state.state_name != NULL
                && strcmp(state_name, other->data.zone_state.state_name) == 0) {
                semantic_error(ctx, other,
                    "Redeclaration of zone state '%s'",
                    state_name);
            }
        }
    }

    ctx->current_zone = saved_zone;
    return ok && !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Async system type checkers
 * ----------------------------------------------------------------- */

bool
type_check_async_block(ASTNode *node, SemanticContext *ctx)
{
    bool saved_async = ctx->in_async_func;
    ctx->in_async_func = true;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
        type_check_statement(node->data.async_block.statements[i], ctx);
    }

    ctx->in_async_func = saved_async;
    return !ctx->has_error;
}

bool
type_check_select_stmt(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        if (c != NULL) {
            bool valid_case = false;
            if (c->type == AST_BLOCK && c->data.block.count > 0) {
                ASTNode *first = c->data.block.statements[0];
                ASTNode *recv_expr = NULL;
                const char *bind_name = NULL;

                if (first != NULL && first->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    recv_expr = first;
                } else if (first != NULL && first->type == AST_ASSIGNMENT
                           && first->data.assignment.target != NULL
                           && first->data.assignment.target->type == AST_IDENTIFIER
                           && first->data.assignment.value != NULL
                           && first->data.assignment.value->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    bind_name = first->data.assignment.target->data.identifier.name;
                    recv_expr = first->data.assignment.value;
                }

                if (!valid_case) {
                    semantic_error(ctx, c,
                        "select case must begin with a channel receive pattern");
                    type_check_statement(c, ctx);
                    continue;
                }

                scope_enter(&ctx->scope, SCOPE_BLOCK);
                if (recv_expr != NULL) {
                    Type *recv_type = type_check_expression(recv_expr, ctx);
                    if (bind_name != NULL && recv_type != NULL) {
                        Symbol *binding = symbol_create_variable(
                            bind_name, recv_type, first->line, first->column);
                        scope_declare(ctx->scope, binding);
                    }
                }
                for (size_t j = 1; j < c->data.block.count; j++)
                    type_check_statement(c->data.block.statements[j], ctx);
                scope_exit(&ctx->scope);
            } else {
                semantic_error(ctx, c,
                    "select case must begin with a channel receive pattern");
                type_check_statement(c, ctx);
            }
        }
    }

    if (node->data.select_stmt.default_case)
        type_check_statement(node->data.select_stmt.default_case, ctx);

    return !ctx->has_error;
}
