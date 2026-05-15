#ifndef PGY_TYPE_CHECKER_GENERIC_CONTRACTS_H
#define PGY_TYPE_CHECKER_GENERIC_CONTRACTS_H

int
find_generic_param_index(GenericParams *gp, const char *param_name)
{
    size_t param_count;

    if (gp == NULL || param_name == NULL)
        return -1;

    param_count = ast_generic_param_count(gp);
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *param = ast_generic_param_at(gp, i);
        const char *name = ast_generic_param_name(param);
        if (name != NULL && strcmp(name, param_name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static Type *
generic_contract_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
}

bool
concrete_type_satisfies_bound(Type *concrete_type, ASTNode *bound_node,
                              SemanticContext *ctx)
{
    Type *bound_type;
    const char *bound_name;
    Symbol *bound_sym;

    if (concrete_type == NULL || bound_node == NULL || ctx == NULL)
        return false;

    bound_type = generic_contract_resolve_type_ref(bound_node, ctx);
    if (bound_type != NULL
        && bound_type != TYPE_UNKNOWN
        && type_satisfies_constraint(concrete_type, bound_type)) {
        return true;
    }

    if (ctx->program_root == NULL
        || ast_type_name(bound_node) == NULL
        || concrete_type->name == NULL) {
        return false;
    }

    bound_name = ast_type_name(bound_node);
    bound_sym = scope_lookup(ctx->scope, bound_name);
    if ((bound_sym != NULL && bound_sym->kind == SYMBOL_ABILITY)
        || (ctx->program_root != NULL
            && find_ability_decl_by_name(ctx->program_root, bound_name) != NULL)) {
        return subject_type_has_ability(ctx->program_root,
                                        concrete_type->name,
                                        bound_node);
    }

    return false;
}

void
validate_generic_param_default_bounds(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *owner_kind,
                                      const char *owner_name)
{
    if (gp == NULL || wc == NULL || ctx == NULL)
        return;

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        GenericParam *param;
        Type *default_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(gp, tc->type_param);
        if (param_index < 0
            || (size_t)param_index >= ast_generic_param_count(gp)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                owner != NULL ? owner : (ASTNode *)wc,
                "%s '%s' could not validate default generic bound for unknown parameter '%s'.\n"
                "Reason:\n"
                "- where clause references '%s'\n"
                "- that parameter does not exist in the generic parameter list\n"
                "Fix:\n"
                "- change the where-clause to reference an existing generic parameter\n"
                "- or add generic parameter '%s' to %s '%s'",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                tc->type_param,
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
            continue;
        }

        param = ast_generic_param_at(gp, (size_t)param_index);
        ASTNode *default_type_node = ast_generic_param_default_type(param);
        if (default_type_node == NULL)
            continue;

        semantic_type_resolution_record_type_ref_dependency(
            ctx,
            owner != NULL ? owner : default_type_node,
            tc->type_param != NULL ? tc->type_param : "<type-param>",
            default_type_node,
            "default-bound subject lookup");

        default_type = generic_contract_resolve_type_ref(default_type_node, ctx);
        if (default_type == NULL || default_type == TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                owner != NULL ? owner : default_type_node,
                "Default generic type argument for parameter '%s' in %s '%s' could not be resolved.\n"
                "Reason:\n"
                "- default type argument participates in effective generic argument derivation\n"
                "- where-clause validation cannot proceed until that default materializes\n"
                "Fix:\n"
                "- provide a resolvable default type argument for '%s'\n"
                "- or remove the default and require the caller to pass the type argument explicitly",
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                ast_type_name(bound_node) != NULL
                    ? ast_type_name(bound_node)
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    owner != NULL ? owner : bound_node,
                    tc->type_param != NULL ? tc->type_param : "<type-param>",
                    bound_node,
                    "default-bound constraint lookup");
            }

            if (concrete_type_satisfies_bound(default_type, bound_node, ctx)) {
                free(bounds_text);
                continue;
            }

            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
                owner != NULL ? owner : default_type_node,
                "Default generic type argument '%s' does not satisfy constraint '%s' for parameter '%s' in %s '%s'.\n"
                "Reason:\n"
                "- %s '%s' declares '%s = %s'\n"
                "- where clause requires '%s: %s'\n"
                "- full bound set is '%s: %s'\n"
                "Fix:\n"
                "- change the default type argument to satisfy '%s'\n"
                "- or relax the where-clause on %s '%s'",
                default_type->name != NULL ? default_type->name : "<type>",
                bound_name,
                tc->type_param,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>",
                tc->type_param,
                default_type->name != NULL ? default_type->name : "<type>",
                tc->type_param,
                bound_name,
                tc->type_param,
                bounds_text != NULL ? bounds_text : "<constraint>",
                bound_name,
                owner_kind != NULL ? owner_kind : "declaration",
                owner_name != NULL ? owner_name : "<anonymous>");
            free(bounds_text);
        }
    }
}

__attribute__((unused))
static void
validate_class_where_clause_instantiation(ASTNode *class_decl,
                                          Type *constructed_type,
                                          ASTNode *site,
                                          SemanticContext *ctx)
{
    WhereClause *wc;
    GenericParams *gp;
    const char *expected_text = NULL;
    const char *class_name;

    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL
        || constructed_type == NULL
        || constructed_type->kind != TYPE_KIND_CONSTRUCTED
        || site == NULL || ctx == NULL) {
        return;
    }

    gp = ast_class_generic_params(class_decl);
    wc = ast_class_where_clause(class_decl);
    if (gp == NULL || ast_generic_param_count(gp) == 0
        || wc == NULL || wc->count == 0)
        return;
    class_name = ast_class_name(class_decl) != NULL
        ? ast_class_name(class_decl)
        : "<class>";
    expected_text = format_generic_subject_signature_scratch(
        ctx,
        class_name,
        gp);

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(gp, tc->type_param);
        if (param_index < 0
            || (size_t)param_index >= type_constructed_arg_count(constructed_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not validate where-clause parameter '%s' during instantiation.\n"
                "Reason:\n"
                "- instantiated type '%s' does not provide an effective type argument for '%s'\n"
                "- class where-clause validation cannot continue until every effective type argument resolves\n"
                "Fix:\n"
                "- pass/supply a type argument for '%s'\n"
                "- or fix the class generic parameter list/default arguments so '%s' is materialized",
                class_name,
                tc->type_param,
                constructed_type->name != NULL ? constructed_type->name : "<constructed>",
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        concrete_type = type_constructed_arg(constructed_type, (size_t)param_index);
        if (concrete_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not resolve instantiated type argument for '%s'.\n"
                "Reason:\n"
                "- where-clause validation reached instantiation with no concrete type for '%s'\n"
                "- class specialization cannot be validated until every effective type argument resolves\n"
                "Fix:\n"
                "- pass a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so it resolves",
                class_name,
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                ast_type_name(bound_node) != NULL
                    ? ast_type_name(bound_node)
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    class_name,
                    bound_node,
                    "class instantiation where-bound lookup");
            }

            if (!concrete_type_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_report_class_generic_bound_failure(
                    ctx,
                    site,
                    class_name,
                    tc->type_param,
                    bound_name,
                    bounds_text,
                    expected_text,
                    constructed_type->name != NULL
                        ? constructed_type->name : "<constructed>",
                    concrete_type->name != NULL
                        ? concrete_type->name : "<type>",
                    "instantiated");
            }
            free(bounds_text);
        }
    }
}

void
validate_class_where_clause_specialization_ast(ASTNode *class_decl,
                                               ASTNode *specialized_type,
                                               ASTNode *site,
                                               SemanticContext *ctx)
{
    GenericParams *decl_params;
    WhereClause *wc;
    Type **effective_types = NULL;
    size_t effective_count = 0;
    const char *expected_text = NULL;
    const char *actual_text = NULL;
    const char *site_label = "specialized";
    const char *class_name;

    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL
        || specialized_type == NULL || specialized_type->type != AST_TYPE
        || site == NULL || ctx == NULL) {
        return;
    }

    decl_params = ast_class_generic_params(class_decl);
    wc = ast_class_where_clause(class_decl);
    if (decl_params == NULL || ast_generic_param_count(decl_params) == 0
        || wc == NULL || wc->count == 0) {
        return;
    }
    class_name = ast_class_name(class_decl) != NULL
        ? ast_class_name(class_decl)
        : "<class>";
    expected_text = format_generic_subject_signature_scratch(
        ctx,
        class_name,
        decl_params);

    effective_types = collect_effective_generic_arg_types(
        decl_params,
        ast_type_generic_args(specialized_type),
        specialized_type,
        ctx,
        "class",
        class_name,
        &effective_count);
    if (effective_types == NULL)
        return;
    if ((ast_type_generic_args(specialized_type) == NULL
         || ast_generic_param_count(ast_type_generic_args(specialized_type)) < effective_count)
        && effective_count > 0) {
        site_label = "instantiated";
    }

    if (effective_count > 0) {
        bool all_effective_types_resolved = true;

        for (size_t i = 0; i < effective_count; i++) {
            if (effective_types[i] == NULL || effective_types[i] == TYPE_UNKNOWN)
                all_effective_types_resolved = false;
        }
        if (all_effective_types_resolved) {
            actual_text = format_effective_generic_type_list_scratch(
                ctx,
                ast_type_name(specialized_type) != NULL
                    ? ast_type_name(specialized_type) : "<specialized>",
                effective_types,
                effective_count);
        }
    }
    if (actual_text == NULL) {
        actual_text = ast_type_name(specialized_type) != NULL
            ? ast_type_name(specialized_type) : "<specialized>";
    }

    for (size_t ci = 0; ci < wc->count; ci++) {
        TypeConstraint *tc = wc->constraints[ci];
        int param_index;
        Type *concrete_type;

        if (tc == NULL || tc->type_param == NULL)
            continue;

        param_index = find_generic_param_index(decl_params, tc->type_param);
        if (param_index < 0 || (size_t)param_index >= effective_count) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not validate where-clause parameter '%s' during specialization.\n"
                "Reason:\n"
                "- specialized type syntax did not materialize an effective type argument for '%s'\n"
                "- class where-clause validation cannot continue until every effective type argument resolves\n"
                "Fix:\n"
                "- provide/supply a type argument for '%s'\n"
                "- or fix the class generic parameter list/default arguments so '%s' is materialized",
                class_name,
                tc->type_param,
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        concrete_type = effective_types[param_index];
        if (concrete_type == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
                "Class '%s' could not resolve specialized type argument for '%s'.\n"
                "Reason:\n"
                "- where-clause validation reached specialization with no concrete type for '%s'\n"
                "- specialization cannot be validated until every effective type argument resolves\n"
                "Fix:\n"
                "- pass a concrete type argument for '%s'\n"
                "- or fix the default type argument / imported type so it resolves",
                class_name,
                tc->type_param,
                tc->type_param,
                tc->type_param);
            continue;
        }

        for (size_t bi = 0; bi < tc->bound_count; bi++) {
            ASTNode *bound_node = tc->bounds[bi];
            char *bounds_text = format_type_constraint_bounds(tc);
            const char *bound_name =
                ast_type_name(bound_node) != NULL
                    ? ast_type_name(bound_node)
                    : "<constraint>";

            if (bound_node != NULL) {
                semantic_type_resolution_record_type_ref_dependency(
                    ctx,
                    site,
                    class_name,
                    bound_node,
                    "class specialization where-bound lookup");
            }

            if (!concrete_type_satisfies_bound(concrete_type, bound_node, ctx)) {
                semantic_report_class_generic_bound_failure(
                    ctx,
                    site,
                    class_name,
                    tc->type_param,
                    bound_name,
                    bounds_text,
                    expected_text,
                    actual_text,
                    concrete_type->name != NULL
                        ? concrete_type->name : "<type>",
                    site_label);
            }
            free(bounds_text);
        }
    }
    free(effective_types);
}

#endif /* PGY_TYPE_CHECKER_GENERIC_CONTRACTS_H */
