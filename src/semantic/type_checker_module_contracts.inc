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

static char *
ability_decl_signature_display(const char *ability_name, GenericParams *params)
{
    char *result;

    if (ability_name == NULL)
        return pergyra_strdup("<ability>");
    if (params == NULL || params->count == 0)
        return pergyra_strdup(ability_name);

    result = tc_strdup_fmt("%s<", ability_name);
    if (result == NULL)
        return pergyra_strdup(ability_name);

    for (size_t i = 0; i < params->count; i++) {
        GenericParam *gp = params->params[i];
        const char *param_name =
            (gp != NULL && gp->name != NULL) ? gp->name : "<type>";
        char *next;

        if (i + 1 < params->count)
            next = tc_strdup_fmt("%s%s, ", result, param_name);
        else
            next = tc_strdup_fmt("%s%s>", result, param_name);
        free(result);
        result = next;
        if (result == NULL)
            return pergyra_strdup(ability_name);
    }

    return result;
}

static ASTNode *
subject_type_find_base_ability_impl(ASTNode *program, const char *type_name,
                                    const char *ability_name);
static bool
subject_type_has_ability(ASTNode *program, const char *type_name,
                         ASTNode *ability_ref);

static void
report_subject_ability_requirement_mismatch(SemanticContext *ctx,
                                            const ASTNode *site,
                                            const char *owner_label,
                                            const char *owner_name,
                                            const char *subject_label,
                                            const char *subject_name,
                                            const char *required_text,
                                            const char *actual_text,
                                            const char *fix_tail)
{
    if (ctx == NULL || site == NULL)
        return;

    if (actual_text != NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires ability '%s', but %s '%s' implements '%s' instead.\n"
            "Reason:\n"
            "- consumer path is %s '%s'\n"
            "- %s '%s' does not satisfy required ability '%s'\n"
            "- expected type args are '%s'\n"
            "- actual implementation is '%s'\n"
            "- actual type args are '%s'\n"
            "Fix:\n"
            "- implement '%s' for %s '%s'\n"
            "- %s",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            required_text != NULL ? required_text : "<ability>",
            subject_label != NULL ? subject_label : "subject",
            subject_name != NULL ? subject_name : "<subject>",
            actual_text,
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            subject_label != NULL ? subject_label : "subject",
            subject_name != NULL ? subject_name : "<subject>",
            required_text != NULL ? required_text : "<ability>",
            required_text != NULL ? required_text : "<ability>",
            actual_text,
            actual_text,
            required_text != NULL ? required_text : "<ability>",
            subject_label != NULL ? subject_label : "subject",
            subject_name != NULL ? subject_name : "<subject>",
            fix_tail != NULL ? fix_tail : "change/remove the ability requirement");
        return;
    }

    semantic_error(ctx, site,
        "%s '%s' requires ability '%s', but %s '%s' has no matching role impl.\n"
        "Reason:\n"
        "- consumer path is %s '%s'\n"
        "- %s '%s' does not satisfy required ability '%s'\n"
        "- expected type args are '%s'\n"
        "- no matching implementation was found for '%s'\n"
        "Fix:\n"
        "- implement '%s' for %s '%s'\n"
        "- %s",
        owner_label != NULL ? owner_label : "construct",
        owner_name != NULL ? owner_name : "<anonymous>",
        required_text != NULL ? required_text : "<ability>",
        subject_label != NULL ? subject_label : "subject",
        subject_name != NULL ? subject_name : "<subject>",
        owner_label != NULL ? owner_label : "construct",
        owner_name != NULL ? owner_name : "<anonymous>",
        subject_label != NULL ? subject_label : "subject",
        subject_name != NULL ? subject_name : "<subject>",
        required_text != NULL ? required_text : "<ability>",
        required_text != NULL ? required_text : "<ability>",
        required_text != NULL ? required_text : "<ability>",
        required_text != NULL ? required_text : "<ability>",
        subject_label != NULL ? subject_label : "subject",
        subject_name != NULL ? subject_name : "<subject>",
        fix_tail != NULL ? fix_tail : "change/remove the ability requirement");
}

static int
ability_decl_generic_param_index(GenericParams *gp, const char *param_name)
{
    if (gp == NULL || param_name == NULL)
        return -1;

    for (size_t i = 0; i < gp->count; i++) {
        if (gp->params[i] != NULL
            && gp->params[i]->name != NULL
            && strcmp(gp->params[i]->name, param_name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static bool
ability_generic_arg_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                                    SemanticContext *ctx)
{
    Type *bound_type;
    Symbol *bound_sym;

    if (concrete_type == NULL || bound_node == NULL || ctx == NULL)
        return false;

    bound_type = resolve_type_node(bound_node, ctx);
    if (bound_type == NULL || bound_type == TYPE_UNKNOWN)
        return false;

    if (type_satisfies_constraint(concrete_type, bound_type))
        return true;

    if (ctx->program_root == NULL
        || bound_node->type != AST_TYPE
        || bound_node->data.type.name == NULL
        || concrete_type->name == NULL) {
        return false;
    }

    bound_sym = scope_lookup(ctx->scope, bound_node->data.type.name);
    if (bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY) {
        return subject_type_has_ability(ctx->program_root,
                                        concrete_type->name,
                                        bound_node);
    }

    return false;
}

static bool
validate_ability_decl_where_clause_reference(ASTNode *ability_decl,
                                             ASTNode *ability_ref,
                                             const ASTNode *site,
                                             SemanticContext *ctx,
                                             const char *owner_label,
                                             const char *owner_name)
{
    GenericParams *decl_params;
    WhereClause *wc;
    char *required_text;
    ASTNode **effective_args;
    size_t effective_count = 0;

    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL
        || ability_ref == NULL || ability_ref->type != AST_TYPE
        || ctx == NULL) {
        return true;
    }

    decl_params = ability_decl->data.ability_decl.generic_params;
    wc = ability_decl->data.ability_decl.where_clause;
    if (decl_params == NULL || decl_params->count == 0
        || wc == NULL || wc->count == 0) {
        return true;
    }

    effective_args = collect_effective_generic_arg_nodes(
        decl_params,
        ability_ref->data.type.generic_args,
        site,
        ctx,
        "Ability",
        ability_decl->data.ability_decl.name != NULL
            ? ability_decl->data.ability_decl.name
            : "<ability>",
        &effective_count);
    if (effective_args == NULL)
        return ctx != NULL ? !ctx->has_error : false;

    required_text = ability_ref_display(ability_ref);

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = ability_decl_generic_param_index(decl_params, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= effective_count) {
            semantic_error(ctx, site,
                "%s '%s' could not validate generic bound '%s' on ability '%s'.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- ability where-clause references type parameter '%s'\n"
                "- effective generic arguments were not materialized for that parameter\n"
                "Fix:\n"
                "- pass/supply generic arguments for '%s'\n"
                "- or fix the ability declaration so where-clause parameters match its generic parameter list",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>");
            free(required_text);
            free(effective_args);
            return false;
        }

        concrete_type = resolve_type_node(effective_args[param_index], ctx);
        if (concrete_type == NULL) {
            semantic_error(ctx, site,
                "%s '%s' could not resolve generic argument for bound '%s' on ability '%s'.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- effective generic argument for '%s' did not resolve to a concrete type\n"
                "Fix:\n"
                "- provide a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so the generic argument resolves",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                ability_decl->data.ability_decl.name != NULL
                    ? ability_decl->data.ability_decl.name : "<ability>",
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                tc->type_param);
            free(required_text);
            free(effective_args);
            return false;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                (bound_node != NULL
                 && bound_node->type == AST_TYPE
                 && bound_node->data.type.name != NULL)
                    ? bound_node->data.type.name
                    : "<constraint>";

            if (!ability_generic_arg_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_error(ctx, site,
                    "%s '%s' uses ability '%s' with generic argument '%s' that does not satisfy bound '%s'.\n"
                    "Reason:\n"
                    "- consumer path is %s '%s'\n"
                    "- ability declaration '%s' requires '%s: %s'\n"
                    "- full bound set is '%s: %s'\n"
                    "- generic subject is ability '%s'\n"
                    "- %s uses '%s'\n"
                    "- actual type argument is '%s'\n"
                    "- expected type args are constrained by '%s: %s'\n"
                    "Fix:\n"
                    "- pass a type argument that satisfies '%s'\n"
                    "- or relax the bound on ability '%s'",
                    owner_label != NULL ? owner_label : "construct",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    ability_decl->data.ability_decl.name != NULL
                        ? ability_decl->data.ability_decl.name : "<ability>",
                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                    bound_name,
                    owner_label != NULL ? owner_label : "construct",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    ability_decl->data.ability_decl.name != NULL
                        ? ability_decl->data.ability_decl.name : "<ability>",
                    ability_decl->data.ability_decl.name != NULL
                        ? ability_decl->data.ability_decl.name : "<ability>",
                    tc->type_param,
                    bound_name,
                    tc->type_param,
                    bounds_text != NULL ? bounds_text : "<constraint>",
                    owner_label != NULL ? owner_label : "construct",
                    required_text != NULL ? required_text : "<ability>",
                    concrete_type->name != NULL ? concrete_type->name : "<type>",
                    tc->type_param,
                    bound_name,
                    bound_name,
                    ability_decl->data.ability_decl.name != NULL
                        ? ability_decl->data.ability_decl.name : "<ability>");
                free(bounds_text);
                free(required_text);
                free(effective_args);
                return false;
            }
            free(bounds_text);
        }
    }

    free(required_text);
    free(effective_args);
    return true;
}

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
ability_ref_matches(ASTNode *program, ASTNode *impl_ref, ASTNode *required_ref)
{
    ASTNode *ability_decl;
    GenericParams *decl_params;
    ASTNode **impl_effective = NULL;
    ASTNode **required_effective = NULL;
    size_t impl_count = 0;
    size_t required_count = 0;
    bool matches = false;

    if (impl_ref == NULL || required_ref == NULL)
        return false;
    if (impl_ref->type != AST_TYPE || required_ref->type != AST_TYPE)
        return false;
    if (impl_ref->data.type.name == NULL || required_ref->data.type.name == NULL)
        return false;
    if (strcmp(impl_ref->data.type.name, required_ref->data.type.name) != 0)
        return false;

    ability_decl = find_ability_decl_by_name(program, impl_ref->data.type.name);
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL)
        return ability_ref_type_arg_equal(impl_ref, required_ref);

    decl_params = ability_decl->data.ability_decl.generic_params;
    if (decl_params == NULL || decl_params->count == 0)
        return true;

    impl_effective = collect_effective_generic_arg_nodes(
        decl_params, impl_ref->data.type.generic_args, NULL, NULL,
        "Ability",
        ability_decl->data.ability_decl.name != NULL
            ? ability_decl->data.ability_decl.name : "<ability>",
        &impl_count);
    required_effective = collect_effective_generic_arg_nodes(
        decl_params, required_ref->data.type.generic_args, NULL, NULL,
        "Ability",
        ability_decl->data.ability_decl.name != NULL
            ? ability_decl->data.ability_decl.name : "<ability>",
        &required_count);

    if (impl_effective == NULL || required_effective == NULL || impl_count != required_count)
        goto cleanup;

    matches = true;
    for (size_t i = 0; i < impl_count; i++) {
        if (!ability_ref_type_arg_equal(impl_effective[i], required_effective[i])) {
            matches = false;
            break;
        }
    }

cleanup:
    free(impl_effective);
    free(required_effective);
    return matches;
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
    char *required_text = ability_ref_display(ability_ref);
    const char *site_module = NULL;

    resolve_type_node(ability_ref, ctx);
    if (ability == NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires a valid ability type reference.\n"
            "Reason:\n"
            "- this contract path expects an ability type\n"
            "- the current reference is missing, malformed, or not an ability surface\n"
            "Fix:\n"
            "- replace it with a visible ability type such as 'AbilityName' or 'AbilityName<T>'\n"
            "- or remove the invalid requirement from %s '%s'",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>");
        free(required_text);
        return NULL;
    }

    ability_decl = find_ability_decl_by_name(ctx->program_root, ability);
    if (ability_decl == NULL) {
        semantic_error(ctx, site,
            "%s '%s' requires unknown ability '%s'.\n"
            "Reason:\n"
            "- no visible ability declaration named '%s' was found\n"
            "Fix:\n"
            "- declare ability '%s'\n"
            "- or import/export the module that defines it",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            ability,
            ability);
        free(required_text);
        return NULL;
    }

    site_module = (site != NULL && site->origin_path != NULL)
        ? site->origin_path
        : (ctx != NULL ? ctx->current_module_path : NULL);
    if (ability_decl->type == AST_ABILITY_DECL
        && ((site_module != NULL
             && ability_decl->origin_path != NULL
             && !same_module_origin(site_module, ability_decl->origin_path)
             && !ability_decl->is_exported)
            || ((site_module == NULL
                 || ability_decl->origin_path == NULL
                || !same_module_origin(site_module, ability_decl->origin_path))
                && ability_decl->data.ability_decl.has_explicit_access
                && ability_decl->data.ability_decl.access != ACCESS_PUBLIC))) {
        semantic_error(ctx, site,
            "%s '%s' cannot require non-exported ability '%s' from another module.\n"
            "Reason:\n"
            "- consumer path is %s '%s'\n"
            "- ability reference is '%s'\n"
            "- the declaration exists but is not visible across the current module boundary\n"
            "Fix:\n"
            "- export ability '%s' from its defining module\n"
            "- or move this contract to the same module\n"
            "- or require a public ability instead",
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            ability,
            owner_label != NULL ? owner_label : "construct",
            owner_name != NULL ? owner_name : "<anonymous>",
            required_text != NULL ? required_text : ability,
            ability);
        free(required_text);
        return NULL;
    }

    if (ability_ref->type == AST_TYPE
        && ability_ref->data.type.generic_args != NULL) {
        size_t arg_count = ability_ref->data.type.generic_args->count;
        size_t param_count = ability_decl->data.ability_decl.generic_params != NULL
            ? ability_decl->data.ability_decl.generic_params->count : 0;
        char *expected_text = ability_decl_signature_display(
            ability, ability_decl->data.ability_decl.generic_params);

        if (arg_count > 0 && param_count == 0) {
            semantic_error(ctx, site,
                "Ability '%s' does not accept generic type arguments in requires clauses.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- generic subject is ability '%s'\n"
                "- requires uses '%s'\n"
                "- ability declaration '%s' is non-generic\n"
                "- expected type args are '%s'\n"
                "- actual type args are '%s'\n"
                "Fix:\n"
                "- remove the generic arguments from '%s'\n"
                "- or declare '%s' with matching generic parameters",
                ability,
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                ability,
                required_text != NULL ? required_text : ability,
                ability,
                expected_text != NULL ? expected_text : ability,
                required_text != NULL ? required_text : ability,
                ability,
                ability);
            free(expected_text);
            free(required_text);
            return NULL;
        }

        if (arg_count > param_count
            || arg_count < generic_params_required_count(
                ability_decl->data.ability_decl.generic_params)) {
            size_t required_count = generic_params_required_count(
                ability_decl->data.ability_decl.generic_params);
            semantic_error(ctx, site,
                "Ability '%s' requires between %zu and %zu generic argument(s) in requires clauses, got %zu.\n"
                "Reason:\n"
                "- consumer path is %s '%s'\n"
                "- requires uses '%s'\n"
                "- actual type args are '%s'\n"
                "Fix:\n"
                "- pass enough generic argument(s) to cover non-default parameters of '%s'\n"
                "- or rely on declaration defaults for trailing parameters only\n"
                "- or change the ability declaration to match the intended arity",
                ability,
                required_count, param_count, arg_count,
                owner_label != NULL ? owner_label : "construct",
                owner_name != NULL ? owner_name : "<anonymous>",
                required_text != NULL ? required_text : ability,
                required_text != NULL ? required_text : ability,
                ability);
            free(expected_text);
            free(required_text);
            return NULL;
        }

        for (size_t i = 0; i < arg_count; i++) {
            GenericParam *gp = ability_ref->data.type.generic_args->params[i];
            ASTNode *arg = gp != NULL ? gp->constraint : NULL;
            if (arg == NULL) {
                semantic_error(ctx, site,
                    "Ability '%s' has an invalid generic argument in requires clause.\n"
                    "Reason:\n"
                    "- consumer path is %s '%s'\n"
                    "- generic subject is ability '%s'\n"
                    "- generic argument %zu in '%s' is missing or malformed\n"
                    "- expected type args are '%s'\n"
                    "- actual type args are '%s'\n"
                    "Fix:\n"
                    "- provide a concrete type argument for each generic parameter",
                    ability,
                    owner_label != NULL ? owner_label : "construct",
                    owner_name != NULL ? owner_name : "<anonymous>",
                    ability, i + 1,
                    required_text != NULL ? required_text : ability,
                    expected_text != NULL ? expected_text : ability,
                    required_text != NULL ? required_text : ability);
                free(expected_text);
                free(required_text);
                return NULL;
            }
            resolve_type_node(arg, ctx);
        }

        free(expected_text);
    }

    if (!validate_ability_decl_where_clause_reference(
            ability_decl, ability_ref, site, ctx, owner_label, owner_name)) {
        free(required_text);
        return NULL;
    }

    free(required_text);
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
                "Ability '%s' has an invalid fields declaration",
                name != NULL ? name : "<ability>");
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            ASTNode *prev = node->data.ability_decl.require_fields[j];
            if (prev != NULL && prev->data.require_field.name != NULL
                && strcmp(prev->data.require_field.name,
                          req->data.require_field.name) == 0) {
                semantic_error(ctx, req,
                    "Ability '%s' declares duplicate field '%s' in fields",
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
                    "Ability '%s' cannot declare field '%s' in fields with non-exported type '%s' from another module",
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
                ability != NULL ? ability : "<ability>");
            free(required_text);
            continue;
        }
        if (subject_name != NULL
            && !subject_type_has_ability(ctx->program_root, subject_name, ability_ref)) {
            ASTNode *actual_impl = subject_type_find_base_ability_impl(
                ctx->program_root, subject_name, ability);
            char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
            report_subject_ability_requirement_mismatch(
                ctx, node, "action", name != NULL ? name : "<anonymous>",
                "subject", subject_name,
                required_text != NULL ? required_text
                                      : (ability != NULL ? ability : "<ability>"),
                actual_text,
                "change/remove the action requirement");
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
            "Unknown stdlib use '%s'; expected one of datetime, device_adapter, http, ledger, money, obligation, page, spray, storage, timer, versioning",
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
