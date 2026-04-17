static bool
any_subject_role_has_ability(ASTNode *program, ASTNode *ability_ref);
static ASTNode *
any_subject_role_find_base_ability_impl(ASTNode *program, const char *ability_name);
static const char *
intent_involves_type_name(ASTNode *involves);
static bool
intent_clause_invokes_authority_sensitive_call(ASTNode *expr, SemanticContext *ctx);
static ASTNode *
intent_step_find_inheritable_action(ASTNode *intent_decl, ASTNode *step,
                                    SemanticContext *ctx,
                                    ASTNode **subject_decl_out);
static const char *
intent_action_binding_type_name(ASTNode *action_decl, ASTNode *action_subject_decl,
                                SemanticContext *ctx, const char *binding_name);
static const char *
nominal_decl_name(ASTNode *decl);

static const char *
nominal_decl_name(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL)
        return decl->data.class_decl.name;
    return NULL;
}

static bool
intent_step_same_ability_list(ASTNode *step, ASTNode *action_decl)
{
    if (step == NULL || action_decl == NULL
        || step->type != AST_INTENT_STEP
        || action_decl->type != AST_FUNC_DECL) {
        return false;
    }
    if (step->data.intent_step.required_ability_count
        != action_decl->data.func_decl.required_ability_count) {
        return false;
    }

    for (size_t i = 0; i < step->data.intent_step.required_ability_count; i++) {
        char *step_text = ability_ref_display(
            step->data.intent_step.required_abilities[i]);
        char *action_text = ability_ref_display(
            action_decl->data.func_decl.required_abilities[i]);
        bool same = step_text != NULL && action_text != NULL
            && strcmp(step_text, action_text) == 0;
        free(step_text);
        free(action_text);
        if (!same)
            return false;
    }

    return true;
}

static bool
intent_step_same_authorized_by_list(ASTNode *intent_decl,
                                    ASTNode *step,
                                    ASTNode *action_decl,
                                    ASTNode *action_subject_decl,
                                    SemanticContext *ctx)
{
    if (intent_decl == NULL || step == NULL || action_decl == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP
        || action_decl->type != AST_FUNC_DECL) {
        return false;
    }

    if (step->data.intent_step.authorized_by_count
        != action_decl->data.func_decl.authorized_by_count) {
        return false;
    }

    for (size_t i = 0; i < action_decl->data.func_decl.authorized_by_count; i++) {
        const char *binding_name = action_decl->data.func_decl.authorized_by[i];
        const char *binding_type_name = intent_action_binding_type_name(
            action_decl, action_subject_decl, ctx, binding_name);
        const char *mapped_alias = NULL;

        if (binding_name == NULL)
            return false;

        if (strcmp(binding_name, "self") == 0) {
            if (step->data.intent_step.who_count == 1)
                mapped_alias = step->data.intent_step.who_names[0];
        } else if (binding_type_name != NULL) {
            for (size_t j = 0; j < intent_decl->data.intent_decl.involve_count; j++) {
                ASTNode *involves = intent_decl->data.intent_decl.involves[j];
                const char *participant_type_name = intent_involves_type_name(involves);
                if (participant_type_name != NULL
                    && strcmp(participant_type_name, binding_type_name) == 0) {
                    if (mapped_alias != NULL)
                        return false;
                    mapped_alias = involves->data.intent_involves.alias;
                }
            }
        }

        if (mapped_alias == NULL
            || step->data.intent_step.authorized_by[i] == NULL
            || strcmp(step->data.intent_step.authorized_by[i], mapped_alias) != 0) {
            return false;
        }
    }

    return true;
}

static bool
intent_step_same_who_binding(ASTNode *intent_decl,
                             ASTNode *step,
                             ASTNode *action_subject_decl)
{
    const char *subject_name;
    const char *step_alias;
    const char *matched_alias = NULL;

    if (intent_decl == NULL || step == NULL || action_subject_decl == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.who_count != 1) {
        return false;
    }

    subject_name = nominal_decl_name(action_subject_decl);
    step_alias = step->data.intent_step.who_names[0];
    if (subject_name == NULL || step_alias == NULL)
        return false;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        const char *participant_type_name = intent_involves_type_name(involves);
        if (participant_type_name == NULL
            || strcmp(participant_type_name, subject_name) != 0) {
            continue;
        }
        if (matched_alias != NULL)
            return false;
        matched_alias = involves->data.intent_involves.alias;
    }

    return matched_alias != NULL && strcmp(matched_alias, step_alias) == 0;
}

static void
intent_step_warn_redundant_action_contract(ASTNode *intent_decl,
                                           ASTNode *step,
                                           SemanticContext *ctx)
{
    ASTNode *action_subject_decl = NULL;
    ASTNode *action_decl = intent_step_find_inheritable_action(
        intent_decl, step, ctx, &action_subject_decl);
    char redundant[256];
    size_t used = 0;
    bool has_any = false;
    const char *step_name;
    const char *action_name;
    const char *subject_name;

    (void)action_subject_decl;

    if (intent_decl == NULL || step == NULL || ctx == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return;

    redundant[0] = '\0';
    step_name = step->data.intent_step.name != NULL
        ? step->data.intent_step.name : "<step>";
    action_name = action_decl->data.func_decl.name != NULL
        ? action_decl->data.func_decl.name : "<action>";
    subject_name = nominal_decl_name(action_subject_decl);

    if (step->data.intent_step.where_type != NULL
        && !step->data.intent_step.inherited_where_from_action
        && !step->data.intent_step.derived_where_from_using
        && !step->data.intent_step.derived_where_from_transfer
        && action_decl->data.func_decl.within_zone != NULL
        && step->data.intent_step.where_type->type == AST_TYPE
        && step->data.intent_step.where_type->data.type.name != NULL
        && strcmp(step->data.intent_step.where_type->data.type.name,
                  action_decl->data.func_decl.within_zone) == 0) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- where", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.who_count > 0
        && !step->data.intent_step.inherited_who_from_action
        && intent_step_same_who_binding(intent_decl, step, action_subject_decl)) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- who", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.required_ability_count > 0
        && !step->data.intent_step.inherited_requires_from_action
        && action_decl->data.func_decl.required_ability_count > 0
        && intent_step_same_ability_list(step, action_decl)) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- requires", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.causes_effect != NULL
        && !step->data.intent_step.inherited_causes_from_action
        && action_decl->data.func_decl.causes_effect != NULL
        && strcmp(step->data.intent_step.causes_effect,
                  action_decl->data.func_decl.causes_effect) == 0) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- causes", has_any ? "\n" : "");
        has_any = true;
    }

    if (step->data.intent_step.authorized_by_count > 0
        && !step->data.intent_step.inherited_authorized_by_from_action
        && action_decl->data.func_decl.authorized_by_count > 0
        && intent_step_same_authorized_by_list(intent_decl, step, action_decl,
                                               action_subject_decl, ctx)) {
        used += (size_t)snprintf(redundant + used, sizeof(redundant) - used,
            "%s- authorized by", has_any ? "\n" : "");
        has_any = true;
    }

    if (!has_any)
        return;

    semantic_warning(ctx, step,
        "Intent step '%s' restates contract clauses already provided by the matching action contract.\n"
        "Matching action contract: %s.%s\n"
        "Redundant clauses:\n"
        "%s\n"
        "Fix:\n"
        "- remove the duplicated step clauses and let the matching action contract supply them\n"
        "- keep the local clause only when you intentionally override the matching action contract",
        step_name,
        subject_name != NULL ? subject_name : "<subject>",
        action_name,
        redundant);
}

static void
intent_step_format_contract_source_summary(const ASTNode *intent_decl,
                                           const ASTNode *step,
                                           SemanticContext *ctx,
                                           char *buffer,
                                           size_t buffer_size)
{
    size_t used = 0;
    bool has_any = false;
    ASTNode *matched_subject_decl = NULL;
    ASTNode *matched_action = NULL;

    if (buffer == NULL || buffer_size == 0 || step == NULL
        || step->type != AST_INTENT_STEP) {
        return;
    }

    buffer[0] = '\0';

    if (intent_decl != NULL && ctx != NULL) {
        matched_action = intent_step_find_inheritable_action(
            (ASTNode *)intent_decl, (ASTNode *)step, ctx, &matched_subject_decl);
        if (matched_action != NULL) {
            const char *subject_name =
                matched_subject_decl != NULL
                    ? nominal_decl_name(matched_subject_decl)
                    : NULL;
            const char *action_name =
                matched_action->type == AST_FUNC_DECL
                    ? matched_action->data.func_decl.name
                    : NULL;
            used += (size_t)snprintf(buffer + used, buffer_size - used,
                "%s- matching action contract: %s.%s",
                has_any ? "\n" : "",
                subject_name != NULL ? subject_name : "<subject>",
                action_name != NULL ? action_name : "<action>");
            has_any = true;
        } else if (step->data.intent_step.who_count > 0) {
            used += (size_t)snprintf(buffer + used, buffer_size - used,
                "%s- no unique matching action contract was found for this step name across current who participants",
                has_any ? "\n" : "");
            has_any = true;
        }
    }

    if (step->data.intent_step.who_count > 0
        && !step->data.intent_step.inherited_who_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared who on step",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.where_type != NULL
        && !step->data.intent_step.inherited_where_from_action
        && !step->data.intent_step.derived_where_from_using
        && !step->data.intent_step.derived_where_from_transfer
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
        && !step->data.intent_step.derived_using_from_transfer
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER
        && step->data.intent_step.using_expr->data.identifier.name != NULL) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- locally declared using on step: %s",
            has_any ? "\n" : "",
            step->data.intent_step.using_expr->data.identifier.name);
        has_any = true;
    }

    if (step->data.intent_step.inherited_who_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- reused from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- reused who from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
    } else if (step->data.intent_step.inherited_where_from_action
               || step->data.intent_step.inherited_requires_from_action
               || step->data.intent_step.inherited_causes_from_action
               || step->data.intent_step.inherited_authorized_by_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- reused from matching action contract",
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
            "%s- reused zone from matching action contract: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.inherited_requires_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- reused requires from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.inherited_causes_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- reused causes from matching action contract: %s",
            has_any ? "\n" : "",
            step->data.intent_step.causes_effect != NULL
                ? step->data.intent_step.causes_effect
                : "<effect>");
        has_any = true;
    }
    if (step->data.intent_step.inherited_authorized_by_from_action) {
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- reused authorized by from matching action contract",
            has_any ? "\n" : "");
        has_any = true;
    }
    if (step->data.intent_step.derived_where_from_transfer) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- derived zone from transfer target: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.derived_where_from_using) {
        const char *zone_name =
            (step->data.intent_step.where_type != NULL
             && step->data.intent_step.where_type->type == AST_TYPE
             && step->data.intent_step.where_type->data.type.name != NULL)
                ? step->data.intent_step.where_type->data.type.name
                : "<zone>";
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- derived zone from using binding: %s",
            has_any ? "\n" : "", zone_name);
        has_any = true;
    }
    if (step->data.intent_step.derived_using_from_transfer) {
        const char *using_name =
            (step->data.intent_step.using_expr != NULL
             && step->data.intent_step.using_expr->type == AST_IDENTIFIER
             && step->data.intent_step.using_expr->data.identifier.name != NULL)
                ? step->data.intent_step.using_expr->data.identifier.name
                : "<binding>";
        used += (size_t)snprintf(buffer + used, buffer_size - used,
            "%s- derived using from transfer target: %s",
            has_any ? "\n" : "", using_name);
    }
}

static const char *
intent_step_single_who_alias(const ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.who_count != 1
        || step->data.intent_step.who_names == NULL) {
        return NULL;
    }
    return step->data.intent_step.who_names[0];
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
resolve_ability_require_field_type_scope(ASTNode *ability_decl,
                                         ASTNode *ability_ref,
                                         ASTNode *type_node,
                                         ASTNode *site,
                                         SemanticContext *ctx,
                                         Type **out_type)
{
    GenericParams *decl_params;
    ASTNode **effective_args = NULL;
    size_t effective_count = 0;

    if (out_type != NULL)
        *out_type = TYPE_UNKNOWN;
    if (ctx == NULL || out_type == NULL || type_node == NULL) {
        return;
    }
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ability_ref == NULL || ability_ref->type != AST_TYPE) {
        *out_type = resolve_type_node(type_node, ctx);
        return;
    }

    decl_params = ability_decl->data.ability_decl.generic_params;
    if (decl_params == NULL || decl_params->count == 0) {
        *out_type = resolve_type_node(type_node, ctx);
        return;
    }

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        ability_ref->data.type.generic_args,
        site != NULL ? site : type_node,
        ctx,
        "ability",
        ability_decl->data.ability_decl.name != NULL
            ? ability_decl->data.ability_decl.name : "<ability>",
        &effective_count);
    if (effective_args == NULL) {
        *out_type = TYPE_UNKNOWN;
        return;
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < decl_params->count && i < effective_count; i++) {
        GenericParam *param = decl_params->params[i];
        Type *arg_type = NULL;
        Symbol *s = NULL;

        if (param == NULL || param->name == NULL || effective_args[i] == NULL)
            continue;

        arg_type = resolve_type_node(effective_args[i], ctx);
        s = symbol_create_variable(param->name,
                                   arg_type != NULL ? arg_type : TYPE_UNKNOWN,
                                   site != NULL ? site->line : ability_decl->line,
                                   site != NULL ? site->column : ability_decl->column);
        if (s == NULL)
            continue;
        s->kind = SYMBOL_TYPE_PARAM;
        scope_declare(ctx->scope, s);
    }

    *out_type = resolve_type_node(type_node, ctx);
    scope_exit(&ctx->scope);
    free(effective_args);
}

static void
validate_ability_require_fields_for_role(ASTNode *role_decl,
                                         ASTNode *ability_decl,
                                         ASTNode *ability_ref,
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

        resolve_ability_require_field_type_scope(ability_decl,
                                                 ability_ref,
                                                 req->data.require_field.type,
                                                 role_decl,
                                                 ctx,
                                                 &required_type);
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
    validate_generic_param_default_bounds(
        node->data.role_decl.generic_params,
        node->data.role_decl.where_clause,
        ctx,
        node,
        "role",
        name);

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
                    } else if (arg_count > param_count
                               || arg_count < generic_params_required_count(
                                      ability_decl->data.ability_decl.generic_params)) {
                        size_t required_count = generic_params_required_count(
                            ability_decl->data.ability_decl.generic_params);
                        char *impl_text =
                            ability_ref_display(impl->data.impl_ability.ability_ref);
                        semantic_error(ctx, impl,
                            "Ability '%s' requires between %zu and %zu generic argument(s) in impl clauses, got %zu.\n"
                            "Reason:\n"
                            "- impl ability uses '%s'\n"
                            "- ability declaration '%s' expects %zu type parameter(s)\n"
                            "Fix:\n"
                            "- pass enough generic argument(s) to cover non-default parameters of '%s'\n"
                            "- or rely on declaration defaults for trailing parameters only\n"
                            "- or change the ability declaration to match the intended arity",
                            ability_name, required_count, param_count, arg_count,
                            impl_text != NULL ? impl_text : ability_name,
                            ability_name, param_count,
                            ability_name);
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
                    validate_ability_require_fields_for_role(
                        node,
                        ability_decl,
                        impl->data.impl_ability.ability_ref,
                        ctx);
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
    const char *prev_module_path = ctx->current_module_path;
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

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
        ctx->current_module_path = prev_module_path;
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
                    if (actual_text != NULL) {
                        semantic_error(ctx, rs,
                            "Party role slot '%s' requires ability '%s', but subject-bound roles implement '%s' instead.\n"
                            "Reason:\n"
                            "- consumer path is party role slot '%s'\n"
                            "- no subject-bound role satisfies required ability '%s'\n"
                            "- expected type args are encoded in '%s'\n"
                            "- actual implementation is '%s'\n"
                            "- actual type args are encoded in '%s'\n"
                            "Fix:\n"
                            "- implement '%s' on a subject-bound role\n"
                            "- or change the role slot contract",
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            actual_text,
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name,
                            actual_text,
                            actual_text,
                            required_text != NULL ? required_text : ability_name);
                    } else {
                        semantic_error(ctx, rs,
                            "Party role slot '%s' requires ability '%s', but no subject-bound role implements it.\n"
                            "Reason:\n"
                            "- consumer path is party role slot '%s'\n"
                            "- no subject-bound role satisfies required ability '%s'\n"
                            "- expected type args are encoded in '%s'\n"
                            "- no matching implementation was found for '%s'\n"
                            "Fix:\n"
                            "- implement '%s' on a subject-bound role\n"
                            "- or change the role slot contract",
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            rs->data.role_slot.slot_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name,
                            required_text != NULL ? required_text : ability_name);
                    }
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

    ctx->current_module_path = prev_module_path;
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
intent_step_derive_who_from_action(ASTNode *intent_decl, ASTNode *step,
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
        step->data.intent_step.inherited_who_from_action = true;
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
intent_step_derive_transfer_context(ASTNode *intent_decl, ASTNode *step,
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
        step->data.intent_step.derived_using_from_transfer = true;
    }

    if (step->data.intent_step.where_type == NULL
        && to_type != NULL
        && to_type->name != NULL) {
        step->data.intent_step.where_type = ast_create_type(to_type->name);
        step->data.intent_step.derived_where_from_transfer = true;
    }
}

static void
intent_step_derive_zone_binding_context(ASTNode *intent_decl, ASTNode *step,
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
            step->data.intent_step.derived_where_from_using = true;
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
