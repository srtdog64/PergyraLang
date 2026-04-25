static Type *
program_body_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_resolve_or_fallback(ctx, type_ref);
}

static Type *
program_body_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    return program_body_resolve_type_ref(slot->data.domain_slot.type, ctx);
}

static Type *
program_body_resolve_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return program_body_resolve_type_ref(param->type, ctx);
}

static Type *
program_body_resolve_func_return_type(ASTNode *func_decl, SemanticContext *ctx)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.return_type == NULL) {
        return TYPE_VOID;
    }
    return program_body_resolve_type_ref(func_decl->data.func_decl.return_type, ctx);
}

bool
type_check_func_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.func_decl.name;
    bool is_action = (!node->is_async_decl && node->data.func_decl.is_action);
    ASTNode *enclosing_nominal = ctx->current_nominal_decl;
    ASTNode *prev_function_decl = ctx->current_function_decl;
    uint32_t prev_effects = ctx->current_function_effects;
    uint32_t prev_body_summary = ctx->current_function_body_summary;
    bool prev_tracking = ctx->tracking_function_effects;
    bool prev_async = ctx->in_async_func;
    const char *prev_module_path = ctx->current_module_path;
    bool has_effect_contract = false;
    uint32_t declared_effects =
        declared_effects_from_function_node(node, ctx, &has_effect_contract);

    if (is_action) {
        const char *subject_name = NULL;

        if (enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && enclosing_nominal->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT) {
            subject_name = enclosing_nominal->data.class_decl.name;
        }
        validate_action_required_abilities(node, enclosing_nominal, ctx);

        /* Derive 'within' from the surrounding lexical zone */
        if (node->data.func_decl.within_zone == NULL
            && ctx->current_zone != NULL
            && ctx->current_zone->type == AST_ZONE_DECL) {
            node->data.func_decl.within_zone =
                pergyra_strdup(ctx->current_zone->data.zone_decl.name);
        }

        if (node->data.func_decl.within_zone != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' references unknown zone '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.within_zone);
        }
        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            if (zone_decl != NULL
                && !explicit_type_reference_allowed(zone_decl, node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported zone '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
            if (zone_decl != NULL
                && !zone_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported zone '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
        }

        if (node->data.func_decl.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect) == NULL) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' references unknown effect '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.causes_effect);
        }
        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL
                && !explicit_type_reference_allowed(effect_decl, node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported effect '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
            if (effect_decl != NULL
                && !effect_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' cannot reference non-exported effect '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
        }

        for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
            const char *auth_name = node->data.func_decl.authorized_by[i];
            bool found = auth_name != NULL && strcmp(auth_name, "self") == 0;
            const char *auth_type_name = NULL;

            for (size_t j = 0; !found && j < node->data.func_decl.param_count; j++) {
                FuncParam *param = node->data.func_decl.params[j];
                if (param != NULL && param->name != NULL
                    && strcmp(param->name, auth_name) == 0) {
                    found = true;
                }
            }

            if (!found) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' authorized subject '%s' must be 'self' or one of the action parameters",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
                continue;
            }

            auth_type_name = find_action_binding_type_name(
                node, enclosing_nominal, ctx, auth_name);
            if (auth_type_name == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' authorized subject '%s' must be a subject host",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
            }
        }

        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            if (zone_decl != NULL && subject_name != NULL
                && !domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                    zone_decl->data.zone_decl.slot_count, ctx, subject_name)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                    "action '%s' references zone '%s', but that zone has no subject slot for '%s'",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone,
                    subject_name);
            }

            if (zone_decl != NULL) {
                for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++) {
                    const char *auth_name = node->data.func_decl.authorized_by[i];
                    const char *auth_type_name = find_action_binding_type_name(
                        node, enclosing_nominal, ctx, auth_name);
                    if (auth_type_name == NULL) {
                        continue;
                    }
                    if (!domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                            zone_decl->data.zone_decl.slot_count, ctx, auth_type_name)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                            "action '%s' authorized subject '%s' has type '%s', but zone '%s' has no matching subject slot.\n"
                            "Reason:\n"
                            "- action contract derives authority provenance from binding '%s'\n"
                            "- binding '%s' has subject type '%s'\n"
                            "- zone '%s' exposes no subject slot for that type\n"
                            "Fix:\n"
                            "- add a subject slot for '%s' to zone '%s'\n"
                            "- or authorize this action by a subject already declared in zone '%s'",
                            name != NULL ? name : "<anonymous>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            node->data.func_decl.within_zone);
                    } else if (!zone_has_authority_for_subject_type(zone_decl, ctx, auth_type_name)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                            "action '%s' authorized subject '%s' has type '%s', but zone '%s' declares no matching authority.\n"
                            "Reason:\n"
                            "- within-zone contract comes from action clause 'within %s'\n"
                            "- action contract derives authority provenance from binding '%s'\n"
                            "- binding '%s' has subject type '%s'\n"
                            "- authority check edge is action '%s' -> zone '%s' -> binding '%s'\n"
                            "- zone '%s' has a subject slot for that type but no authority contract\n"
                            "Fix:\n"
                            "- declare authority for '%s' in zone '%s'\n"
                            "- or change/remove 'authorized by %s' on action '%s'",
                            name != NULL ? name : "<anonymous>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_name != NULL ? auth_name : "<subject>",
                            auth_type_name,
                            name != NULL ? name : "<anonymous>",
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            node->data.func_decl.within_zone,
                            auth_type_name,
                            node->data.func_decl.within_zone,
                            auth_name != NULL ? auth_name : "<subject>",
                            name != NULL ? name : "<anonymous>");
                    }
                }
            }
        }

        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL) {
                for (size_t i = 0; i < effect_decl->data.effect_decl.slot_count; i++) {
                    ASTNode *slot = effect_decl->data.effect_decl.slots[i];
                    Type *slot_type;
                    bool matched = false;

                    if (slot == NULL || slot->type != AST_DOMAIN_SLOT
                        || !slot->data.domain_slot.is_binding
                        || slot->data.domain_slot.type == NULL) {
                        continue;
                    }
                    slot_type = program_body_resolve_domain_slot_type(slot, ctx);
                    if (slot_type == NULL || slot_type->name == NULL)
                        continue;

                    if (subject_name != NULL && strcmp(subject_name, slot_type->name) == 0) {
                        matched = true;
                    } else {
                        for (size_t j = 0; j < node->data.func_decl.param_count; j++) {
                            FuncParam *param = node->data.func_decl.params[j];
                            Type *param_type;
                            if (param == NULL || param->type == NULL)
                                continue;
                            param_type = program_body_resolve_param_type(param, ctx);
                            if (param_type != NULL
                                && param_type->name != NULL
                                && strcmp(param_type->name, slot_type->name) == 0) {
                                matched = true;
                                break;
                            }
                        }
                    }

                    if (!matched) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                            "action '%s' causes effect '%s', but no self/parameter matches effect target type '%s'",
                            name != NULL ? name : "<anonymous>",
                            node->data.func_decl.causes_effect,
                            slot_type->name);
                    }
                }
            }

            if (node->data.func_decl.within_zone != NULL) {
                ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                    node->data.func_decl.within_zone);
                if (zone_decl != NULL
                    && !zone_has_effect_layer_type(zone_decl, node->data.func_decl.causes_effect)) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                        "action '%s' causes effect '%s', but zone '%s' has no matching effect slot.\n"
                        "Reason:\n"
                        "- action contract declares causes '%s'\n"
                        "- zone '%s' does not materialize any matching effect slot for that contract\n"
                        "Fix:\n"
                        "- add an effect slot of type '%s' to zone '%s'\n"
                        "- or remove/change the causes clause on action '%s'",
                        name != NULL ? name : "<anonymous>",
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        node->data.func_decl.causes_effect,
                        node->data.func_decl.within_zone,
                        name != NULL ? name : "<anonymous>");
                }
            }
        }
    }
    /* subject now allows both func (private internal computation)
     * and action (public plot behavior with zone/effect/authority). */

    /* If the function has generic parameters (<T, U, ...>),
     * register them as opaque types in a temporary scope so that
     * resolve_type_node("T") succeeds for params and return type. */
    bool has_generics = (node->data.func_decl.generic_params != NULL
                         && node->data.func_decl.generic_params->count > 0);
    if (has_generics) {
        validate_generic_param_defaults(node->data.func_decl.generic_params,
            ctx, node, "function");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    /* Build parameter types for the function type */
    size_t   param_count = node->data.func_decl.param_count;
    Type   **param_types = NULL;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(Type *));
        if (param_types == NULL) {
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
    }

    Type *return_type = program_body_resolve_func_return_type(node, ctx);
    if (type_is_class_object_type(return_type, ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
            PGY_CAUSE_ANCHORED_HANDLE_RETURN_BOUNDARY,
            PGY_FIX_RETURN_PROJECTION_OR_KEEP_LOCAL,
            node->data.func_decl.return_type,
            "Returning subjects by value is not supported yet.\n"
            "Reason:\n"
            "- return type '%s' is a subject, and subject values are zone/world slot handles (anchored)\n"
            "Fix:\n"
            "- return a struct/class/object/tobject projection instead\n"
            "- keep the subject local to its owning zone/world\n"
            "- or use Box<T>/another explicit handle layer",
            type_name_or_unknown(return_type));
    }

    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = node->data.func_decl.params[i];
        /* Implicit 'self' type: if a parameter named "self" has no
         * type annotation and we're inside a class scope, infer the
         * enclosing class type. */
        if (param->type == NULL && param->name != NULL
            && strcmp(param->name, "self") == 0
            && ctx->scope != NULL && ctx->scope->kind == SCOPE_CLASS) {
            Scope *parent = ctx->scope->parent;
            const char *nominal_name = NULL;

            if (ctx->current_nominal_decl != NULL
                && ctx->current_nominal_decl->type == AST_CLASS_DECL) {
                nominal_name = ctx->current_nominal_decl->data.class_decl.name;
            }

            if (parent != NULL && nominal_name != NULL) {
                Symbol *self_sym = scope_lookup(parent, nominal_name);
                if (self_sym != NULL && self_sym->kind == SYMBOL_CLASS)
                    param_types[i] = self_sym->type;
            }

            if (param_types[i] == NULL && parent != NULL) {
                /* Fallback for older paths that do not set current_nominal_decl. */
                for (size_t s = parent->symbol_count; s > 0; s--) {
                    Symbol *cs = parent->symbols[s - 1];
                    if (cs != NULL && cs->kind == SYMBOL_CLASS) {
                        param_types[i] = cs->type;
                        break;
                    }
                }
            }

            if (param_types[i] == NULL)
                param_types[i] = TYPE_UNKNOWN;
        } else {
            param_types[i] = program_body_resolve_param_type(param, ctx);
        }
        if (param->mode != PARAM_MODE_DEFAULT
            && semantic_classify_ownership_type(param_types[i], ctx)
                == OWNERSHIP_TYPE_COPY_ONLY) {
            /* copy-only values keep trivial own/ref semantics */
        } else if (param->mode != PARAM_MODE_DEFAULT
                   && !type_is_anchored_resource_handle(param_types[i])) {
            if (!type_is_general_boundary_type(param_types[i], ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_PARAM_MODE_UNSUPPORTED_BOUNDARY_TYPE,
                    PGY_FIX_USE_BOUNDARY_VISIBLE_TYPE_OR_DROP_QUALIFIER,
                    node,
                    "'%s' parameter mode requires a boundary-visible type at function boundaries.\n"
                    "Reason:\n"
                    "- value is parameter '%s'\n"
                    "- ownership mode is '%s'\n"
                    "- consumer path is function '%s'\n"
                    "- type '%s' is not a copy-visible value, boundary-tracked aggregate, subject identity, or slot handle (movable)\n"
                    "- own/ref only changes boundary semantics when the parameter carries ownership-relevant state across the call\n"
                    "Fix:\n"
                    "- remove '%s' and pass it as an ordinary value\n"
                    "- or change the parameter type to a boundary-visible value / subject / slot handle",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref",
                    param->name != NULL ? param->name : "<param>",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref",
                    node->data.func_decl.name != NULL ? node->data.func_decl.name : "<anonymous>",
                    param_types[i] != NULL && param_types[i]->name != NULL
                        ? param_types[i]->name : "<type>",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref");
            }
        }
        if (type_is_anchored_resource_handle(param_types[i])) {
            if (param->mode == PARAM_MODE_DEFAULT) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_SLOT_PARAM_QUALIFIER_MISSING,
                    PGY_FIX_ANNOTATE_SLOT_PARAM_QUALIFIER,
                    node,
                    "Slot handle (anchored) parameters require explicit 'own' or 'ref'.\n"
                    "Reason:\n"
                    "- slot handles (anchored) must declare whether the boundary borrows or transfers ownership\n"
                    "- implicit parameter passing would hide that boundary contract\n"
                    "Fix:\n"
                    "- mark the parameter as 'ref' for borrowing\n"
                    "- or mark it as 'own' for transfer");
            }
        }
        /* Subject parameters are passed by reference (pointer) internally.
         * The language hides pointer semantics from the user — subjects
         * are identity-bearing types, so reference passing is automatic. */
    }

    Type *func_type = type_create_function(param_types, param_count,
                                            return_type);

    Symbol *func_sym = symbol_create_function(name, func_type,
                                               node->line, node->column);
    /* If a forward-declaration placeholder exists from Pass 1,
       update its type instead of re-declaring. */
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_FUNCTION) {
        existing->type = func_type;
        symbol_destroy(func_sym);
    } else if (!scope_declare(ctx->scope, func_sym)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_FUNCTION_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of function '%s'", name);
        symbol_destroy(func_sym);
        return false;
    }

    /* Close the temporary generic-params scope (if opened) before
     * entering the real function scope — the function scope will
     * re-register the generic params so they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    validate_where_clause_bounds(node->data.func_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.func_decl.generic_params,
        node->data.func_decl.where_clause,
        ctx,
        node,
        "function",
        name);

    /* Check body in new function scope */
    scope_enter(&ctx->scope, SCOPE_FUNCTION);
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    /* Re-register generic type params inside the function scope */
    if (has_generics) {
        GenericParams *gp = node->data.func_decl.generic_params;
        for (size_t gi = 0; gi < gp->count; gi++) {
            if (gp->params[gi] == NULL || gp->params[gi]->name == NULL)
                continue;
            Type *tp = calloc(1, sizeof(Type));
            if (tp != NULL) {
                tp->kind = TYPE_KIND_CLASS;
                tp->name = pergyra_strdup(gp->params[gi]->name);
            }
            Symbol *s = symbol_create_variable(
                gp->params[gi]->name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_CLASS;
            scope_declare(ctx->scope, s);
        }
    }

    Type *prev_return  = ctx->current_return;
    ctx->current_function_decl = node;
    ctx->current_return = return_type;
    ctx->current_function_effects = EFFECT_NONE;
    ctx->current_function_body_summary = BODY_SUMMARY_NONE;
    ctx->tracking_function_effects = true;
    ctx->in_async_func = prev_async || node->is_async_decl;

    if (declared_effects != EFFECT_NONE)
        semantic_record_body_summary(ctx, BODY_SUMMARY_EFFECTS);
    if (is_action && node->data.func_decl.within_zone != NULL)
        semantic_record_body_summary(ctx, BODY_SUMMARY_REQUIRES_ZONE);

    /* Register parameters */
    for (size_t i = 0; i < param_count; i++) {
        Type *pt = func_type->data.function.param_types[i];
        const char *param_name = node->data.func_decl.params[i]->name;
        if (node->data.func_decl.params[i]->mode == PARAM_MODE_OWN)
            semantic_record_body_summary(ctx, BODY_SUMMARY_MOVES_PARAM);
        else if (node->data.func_decl.params[i]->mode == PARAM_MODE_REF)
            semantic_record_body_summary(ctx, BODY_SUMMARY_BORROWS_PARAM);
        if (pt != NULL && pt->kind == TYPE_KIND_SLOT && pt->data.slot.is_secure)
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (type_is_constructed_named(pt, "Token"))
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (type_is_subject_host_slot_handle(pt, ctx) && param_name != NULL) {
            char token_name[256];
            const char *paired_token = NULL;
            Symbol *slot_sym;

            if (pt->data.slot.is_secure) {
                snprintf(token_name, sizeof(token_name), "%s_token", param_name);
                paired_token = token_name;
            }

            slot_sym = symbol_create_slot(param_name, pt,
                pt->data.slot.is_secure, paired_token,
                node->line, node->column);
            scope_declare(ctx->scope, slot_sym);
            scope_register_slot(ctx->scope, slot_sym);

            if (pt->data.slot.is_secure) {
                Symbol *tok = symbol_create_token(token_name, param_name,
                    node->line, node->column);
                if (tok != NULL) {
                    Type *token_args[1] = { pt->data.slot.inner_type };
                    tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
                }
                scope_declare(ctx->scope, tok);
            }
            continue;
        }

        Symbol *p = symbol_create_variable(
            param_name, pt,
            node->line, node->column);
        scope_declare(ctx->scope, p);
    }

    if (node->data.func_decl.body != NULL) {
        bool body_must_return = false;
        semantic_check_body_flow(node->data.func_decl.body, ctx, &body_must_return);
        if (!type_equals(return_type, TYPE_VOID)
            && return_type != TYPE_UNKNOWN
            && !body_must_return) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_MISSING_RETURN,
                PGY_CAUSE_CFG_MISSING_RETURN,
                PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                node,
                "Function '%s' with return type '%s' may fall through without returning a value.\n"
                "Reason:\n"
                "- the CFG body summary has at least one reachable normal path without a return terminator\n"
                "- beta-stable body safety requires non-Void functions to return on every normal path\n"
                "Fix:\n"
                "- add a return on the missing branch/path\n"
                "- or change the function return type to Void if falling through is intended",
                name != NULL ? name : "<anonymous>",
                return_type->name != NULL ? return_type->name : "<unknown>");
        }
    }

    semantic_check_param_summary_escapes(node, param_count, param_types, ctx);

    {
        uint32_t derived_effects = type_effect_mask_closure(ctx->current_function_effects);
        uint32_t missing_effects =
            type_effect_mask_closure(derived_effects) & ~type_effect_mask_closure(declared_effects);
        char derived_buf[128];
        char missing_buf[128];
        char declared_buf[128];

        if (has_effect_contract && missing_effects != EFFECT_NONE) {
            effect_mask_to_string(derived_effects, derived_buf, sizeof(derived_buf));
            effect_mask_to_string(missing_effects, missing_buf, sizeof(missing_buf));
            effect_mask_to_string(declared_effects, declared_buf, sizeof(declared_buf));
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EFFECT_CONFLICT, PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO, PGY_FIX_SPLIT_EFFECT_FAMILIES, node,
                "Function '%s' is missing declared effects: %s (declared: %s, derived from body: %s)",
                name != NULL ? name : "<anonymous>",
                missing_buf, declared_buf, derived_buf);
        }

        func_type->data.function.effect_mask =
            type_effect_mask_join(declared_effects, derived_effects);

        if (type_effect_mask_conflicts(func_type->data.function.effect_mask,
                                       func_type->data.function.effect_mask)) {
            effect_mask_to_string(func_type->data.function.effect_mask,
                                  derived_buf, sizeof(derived_buf));
            semantic_warning_code(ctx, PGY_CODE_SEM_EFFECT_CONFLICT, node,
                "Function '%s' combines effect classes that are currently treated as conflicting (%s).\n"
                "Reason:\n"
                "- derived body effects joined into '%s'\n"
                "- current partial order still treats part of that join as conflicting in one routine\n"
                "- this usually means authority-sensitive work and boundary/resource work were merged in one flow\n"
                "Fix:\n"
                "- split the routine into smaller helpers so each helper owns one effect family\n"
                "- or isolate the conflicting branch/handoff path behind an explicit boundary helper",
                name != NULL ? name : "<anonymous>",
                derived_buf,
                derived_buf);
        }

        if (is_action
            && node->data.func_decl.within_zone != NULL
            && type_effect_mask_requires_authority(func_type->data.function.effect_mask)
            && node->data.func_decl.authorized_by_count == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "secure action '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action body derives authority-sensitive effects from authority-bearing work\n"
                "- zone '%s' makes this action part of an explicit authority boundary\n"
                "Contract source:\n"
                "- action header 'within %s' plus the derived authority-sensitive effect path inside the body\n"
                "- without 'authorized by', the approval provenance for that boundary is missing\n"
                "Fix:\n"
                "- add 'authorized by <subject-slot>' to the action contract\n"
                "- or move the authority-sensitive work behind a helper that is called from an already-authorized action",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.within_zone,
                node->data.func_decl.within_zone,
                node->data.func_decl.within_zone);
        }
        if (is_action
            && node->data.func_decl.within_zone != NULL
            && node->data.func_decl.causes_effect != NULL
            && node->data.func_decl.authorized_by_count == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' causing effect '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action contract declares causes '%s'\n"
                "- causing an effect inside zone '%s' is an authority-sensitive state change\n"
                "Contract source:\n"
                "- action header 'causes %s' together with 'within %s'\n"
                "- without 'authorized by', the approval provenance for that state change is missing\n"
                "Fix:\n"
                "- add 'authorized by <subject-slot>' to the action contract\n"
                "- or remove/change the causes clause if this action should stay authority-free",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.causes_effect,
                node->data.func_decl.within_zone,
                node->data.func_decl.causes_effect,
                node->data.func_decl.within_zone,
                node->data.func_decl.causes_effect,
                node->data.func_decl.within_zone);
        }
    }

    func_type->data.function.body_summary_mask =
        ctx->current_function_body_summary;

    ctx->current_return = prev_return;
    ctx->current_function_decl = prev_function_decl;
    ctx->current_function_effects = prev_effects;
    ctx->current_function_body_summary = prev_body_summary;
    ctx->tracking_function_effects = prev_tracking;
    ctx->in_async_func = prev_async;
    ctx->current_module_path = prev_module_path;
    free(param_types);
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}
