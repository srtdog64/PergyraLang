static const char *
ability_ref_name(ASTNode *ability_ref)
{
    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return NULL;
    return ability_ref->data.type.name;
}

static char *
ability_ref_display(ASTNode *ability_ref)
{
    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return pergyra_strdup("<ability>");
    if (ability_ref->data.type.name == NULL)
        return pergyra_strdup("<ability>");
    if (ability_ref->data.type.generic_args == NULL
        || ability_ref->data.type.generic_args->count == 0) {
        return pergyra_strdup(ability_ref->data.type.name);
    }

    char *result = tc_strdup_fmt("%s<", ability_ref->data.type.name);
    if (result == NULL)
        return pergyra_strdup(ability_ref->data.type.name);

    for (size_t i = 0; i < ability_ref->data.type.generic_args->count; i++) {
        GenericParam *gp = ability_ref->data.type.generic_args->params[i];
        ASTNode *arg = gp != NULL ? gp->constraint : NULL;
        char *arg_text = ability_ref_display(arg);
        char *next;
        if (i + 1 < ability_ref->data.type.generic_args->count) {
            next = tc_strdup_fmt("%s%s, ", result, arg_text != NULL ? arg_text : "<type>");
        } else {
            next = tc_strdup_fmt("%s%s>", result, arg_text != NULL ? arg_text : "<type>");
        }
        free(arg_text);
        free(result);
        result = next;
        if (result == NULL)
            return pergyra_strdup(ability_ref->data.type.name);
    }

    return result;
}

static ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name);

static bool
ability_ref_type_arg_equal(ASTNode *lhs, ASTNode *rhs)
{
    if (lhs == rhs)
        return true;
    if (lhs == NULL || rhs == NULL)
        return false;
    if (lhs->type != AST_TYPE || rhs->type != AST_TYPE)
        return false;
    if ((lhs->data.type.name == NULL) != (rhs->data.type.name == NULL))
        return false;
    if (lhs->data.type.name != NULL
        && strcmp(lhs->data.type.name, rhs->data.type.name) != 0) {
        return false;
    }

    {
        size_t lhs_count = lhs->data.type.generic_args != NULL
            ? lhs->data.type.generic_args->count : 0;
        size_t rhs_count = rhs->data.type.generic_args != NULL
            ? rhs->data.type.generic_args->count : 0;
        if (lhs_count != rhs_count)
            return false;
        for (size_t i = 0; i < lhs_count; i++) {
            GenericParam *lhs_gp = lhs->data.type.generic_args->params[i];
            GenericParam *rhs_gp = rhs->data.type.generic_args->params[i];
            ASTNode *lhs_arg = lhs_gp != NULL ? lhs_gp->constraint : NULL;
            ASTNode *rhs_arg = rhs_gp != NULL ? rhs_gp->constraint : NULL;
            if (!ability_ref_type_arg_equal(lhs_arg, rhs_arg))
                return false;
        }
    }

    return true;
}

static bool
ability_ref_matches(ASTNode *impl_ref, ASTNode *required_ref)
{
    if (impl_ref == NULL || required_ref == NULL)
        return false;
    if (impl_ref->type != AST_TYPE || required_ref->type != AST_TYPE)
        return false;
    return ability_ref_type_arg_equal(impl_ref, required_ref);
}

static ASTNode *
resolve_required_ability_decl(ASTNode *ability_ref,
                              const ASTNode *site,
                              SemanticContext *ctx,
                              const char *owner_label,
                              const char *owner_name)
{
    const char *ability = ability_ref_name(ability_ref);
    ASTNode *ability_decl;

    resolve_type_node(ability_ref, ctx);
    if (ability == NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires a valid ability type reference",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>");
        return NULL;
    }

    ability_decl = find_ability_decl_by_name(ctx->program_root, ability);
    if (ability_decl == NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires unknown ability '%s'",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            ability);
        return NULL;
    }

    if (ability_ref->type == AST_TYPE
        && ability_ref->data.type.generic_args != NULL) {
        size_t arg_count = ability_ref->data.type.generic_args->count;
        size_t param_count = ability_decl->data.ability_decl.generic_params != NULL
            ? ability_decl->data.ability_decl.generic_params->count : 0;

        if (arg_count > 0 && param_count == 0) {
            semantic_error(ctx, site,
                "Ability '%s' does not accept generic type arguments in requires clauses",
                ability);
            return NULL;
        }

        if (arg_count != param_count) {
            semantic_error(ctx, site,
                "Ability '%s' requires %zu generic argument(s) in requires clauses, got %zu",
                ability, param_count, arg_count);
            return NULL;
        }

        for (size_t i = 0; i < arg_count; i++) {
            GenericParam *gp = ability_ref->data.type.generic_args->params[i];
            ASTNode *arg = gp != NULL ? gp->constraint : NULL;
            if (arg == NULL) {
                semantic_error(ctx, site,
                    "Ability '%s' has an invalid generic argument in requires clause",
                    ability);
                return NULL;
            }
            resolve_type_node(arg, ctx);
        }
    }

    return ability_decl;
}

static void
validate_ability_require_fields(ASTNode *node, SemanticContext *ctx)
{
    const char *name;

    if (node == NULL || node->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    name = node->data.ability_decl.name;

    for (size_t i = 0; i < node->data.ability_decl.require_count; i++) {
        ASTNode *req = node->data.ability_decl.require_fields[i];
        if (req == NULL || req->data.require_field.name == NULL
            || req->data.require_field.type == NULL) {
            semantic_error(ctx, node,
                "Ability '%s' has an invalid require field declaration",
                name != NULL ? name : "<ability>");
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            ASTNode *prev = node->data.ability_decl.require_fields[j];
            if (prev != NULL && prev->data.require_field.name != NULL
                && strcmp(prev->data.require_field.name,
                          req->data.require_field.name) == 0) {
                semantic_error(ctx, req,
                    "Ability '%s' declares duplicate require field '%s'",
                    name != NULL ? name : "<ability>",
                    req->data.require_field.name);
                break;
            }
        }
        resolve_type_node(req->data.require_field.type, ctx);
        if (req->data.require_field.type != NULL
            && req->data.require_field.type->type == AST_TYPE
            && req->data.require_field.type->data.type.name != NULL) {
            ASTNode *type_decl = find_type_decl_by_name(
                ctx->program_root, req->data.require_field.type->data.type.name);
            if (type_decl != NULL
                && !explicit_type_reference_allowed(type_decl, node, ctx)) {
                semantic_error(ctx, req,
                    "Ability '%s' cannot require field '%s' with non-exported type '%s' from another module",
                    name != NULL ? name : "<ability>",
                    req->data.require_field.name,
                    req->data.require_field.type->data.type.name);
            }
        }
    }
}

static void
validate_action_required_abilities(ASTNode *node,
                                   ASTNode *enclosing_nominal,
                                   SemanticContext *ctx)
{
    const char *name;
    const char *subject_name = NULL;

    if (node == NULL || node->type != AST_FUNC_DECL || ctx == NULL)
        return;

    name = node->data.func_decl.name;

    if (enclosing_nominal == NULL
        || enclosing_nominal->type != AST_CLASS_DECL
        || enclosing_nominal->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        semantic_error(ctx, node,
            "action '%s' is only supported inside subject declarations",
            name != NULL ? name : "<anonymous>");
    } else {
        subject_name = enclosing_nominal->data.class_decl.name;
    }

    for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++) {
        ASTNode *ability_ref = node->data.func_decl.required_abilities[i];
        const char *ability = ability_ref_name(ability_ref);
        char *required_text = ability_ref_display(ability_ref);
        ASTNode *ability_decl = resolve_required_ability_decl(
            ability_ref, node, ctx, "action", name);
        if (ability_decl == NULL)
        {
            free(required_text);
            continue;
        }
        if (ability_decl->type == AST_ABILITY_DECL
            && !explicit_type_reference_allowed(ability_decl, node, ctx)) {
            semantic_error(ctx, node,
                "action '%s' cannot require non-exported ability '%s' from another module",
                name != NULL ? name : "<anonymous>",
                required_text != NULL ? required_text
                                      : (ability != NULL ? ability : "<ability>"));
            free(required_text);
            continue;
        }
        if (subject_name != NULL
            && !subject_type_has_ability(ctx->program_root, subject_name, ability_ref)) {
            ASTNode *actual_impl = subject_type_find_base_ability_impl(
                ctx->program_root, subject_name, ability);
            char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
            semantic_error(ctx, node,
                actual_impl != NULL
                    ? "action '%s' requires ability '%s' but subject '%s' implements '%s' instead"
                    : "action '%s' requires ability '%s' but subject '%s' does not implement it",
                name != NULL ? name : "<anonymous>",
                required_text != NULL ? required_text
                                      : (ability != NULL ? ability : "<ability>"),
                subject_name,
                actual_text != NULL ? actual_text : NULL);
            free(actual_text);
        }
        free(required_text);
    }
}

static void
validate_stdlib_use_decl(ASTNode *stmt, SemanticContext *ctx)
{
    if (stmt == NULL || stmt->type != AST_USE_DECL || ctx == NULL
        || stmt->data.use_decl.module_name == NULL) {
        return;
    }

    if (!semantic_is_known_stdlib_use_module(stmt->data.use_decl.module_name)) {
        semantic_error(ctx, stmt,
            "Unknown stdlib use '%s'; expected one of datetime, http, page, spray, storage",
            stmt->data.use_decl.module_name);
        return;
    }

    if (ctx->program_root == NULL || ctx->program_root->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < ctx->program_root->data.program.count; i++) {
        ASTNode *prev = ctx->program_root->data.program.statements[i];
        if (prev == stmt)
            break;
        if (prev != NULL && prev->type == AST_USE_DECL
            && prev->data.use_decl.module_name != NULL
            && strcmp(prev->data.use_decl.module_name,
                      stmt->data.use_decl.module_name) == 0) {
            semantic_warning(ctx, stmt,
                "Duplicate stdlib use '%s'; resolver will merge it once",
                stmt->data.use_decl.module_name);
            break;
        }
    }
}
