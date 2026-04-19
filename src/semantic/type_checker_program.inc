bool
type_check_func_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.func_decl.name;
    bool is_action = (!node->is_async_decl && node->data.func_decl.is_action);
    ASTNode *enclosing_nominal = ctx->current_nominal_decl;
    ASTNode *prev_function_decl = ctx->current_function_decl;
    uint32_t prev_effects = ctx->current_function_effects;
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
            semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                "action '%s' references unknown zone '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.within_zone);
        }
        if (node->data.func_decl.within_zone != NULL) {
            ASTNode *zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                node->data.func_decl.within_zone);
            if (zone_decl != NULL
                && !explicit_type_reference_allowed(zone_decl, node, ctx)) {
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                    "action '%s' cannot reference non-exported zone '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
            if (zone_decl != NULL
                && !zone_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                    "action '%s' cannot reference non-exported zone '%s' in an externally visible contract",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.within_zone);
            }
        }

        if (node->data.func_decl.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect) == NULL) {
            semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                "action '%s' references unknown effect '%s'",
                name != NULL ? name : "<anonymous>",
                node->data.func_decl.causes_effect);
        }
        if (node->data.func_decl.causes_effect != NULL) {
            ASTNode *effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                node->data.func_decl.causes_effect);
            if (effect_decl != NULL
                && !explicit_type_reference_allowed(effect_decl, node, ctx)) {
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                    "action '%s' cannot reference non-exported effect '%s' from another module",
                    name != NULL ? name : "<anonymous>",
                    node->data.func_decl.causes_effect);
            }
            if (effect_decl != NULL
                && !effect_decl->is_exported
                && callable_contract_is_externally_visible(node, ctx)) {
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                    "action '%s' authorized subject '%s' must be 'self' or one of the action parameters",
                    name != NULL ? name : "<anonymous>",
                    auth_name != NULL ? auth_name : "<subject>");
                continue;
            }

            auth_type_name = find_action_binding_type_name(
                node, enclosing_nominal, ctx, auth_name);
            if (auth_type_name == NULL) {
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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
                semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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
                        semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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
                        semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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
                    slot_type = resolve_type_node(slot->data.domain_slot.type, ctx);
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
                            param_type = resolve_type_node(param->type, ctx);
                            if (param_type != NULL
                                && param_type->name != NULL
                                && strcmp(param_type->name, slot_type->name) == 0) {
                                matched = true;
                                break;
                            }
                        }
                    }

                    if (!matched) {
                        semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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
                    semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
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

    Type *return_type = TYPE_VOID;
    if (node->data.func_decl.return_type != NULL)
        return_type = resolve_type_node(node->data.func_decl.return_type, ctx);
    if (type_is_anchored_resource_handle(return_type)) {
        semantic_error(ctx, node->data.func_decl.return_type,
            "Anchored resource handle return types (Slot/SecureSlot/DeviceSlot) are not supported yet.\n"
            "Reason:\n"
            "- function return boundary would let an anchored handle escape its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at return boundaries in the closed subset\n"
            "Fix:\n"
            "- return the inner value instead\n"
            "- or keep the anchored handle local and return a boundary-safe projection/result");
    }
    if (type_is_class_object_type(return_type, ctx)) {
        semantic_error(ctx, node->data.func_decl.return_type,
            "Returning subjects by value is not supported yet.\n"
            "Reason:\n"
            "- return type '%s' is a subject, and subject values are zone/world anchored handles\n"
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
            param_types[i] = resolve_type_node(param->type, ctx);
        }
        if (param->mode != PARAM_MODE_DEFAULT
            && !type_is_anchored_resource_handle(param_types[i])) {
            if (!type_is_general_boundary_type(param_types[i], ctx)) {
                semantic_error(ctx, node,
                    "'%s' parameter mode is currently a closed subset: only ref/own subject values, ref/own movable resources, "
                    "ref/own boundary values, ref/own Slot<subject-host>, and own SecureSlot<subject-host> are supported at function boundaries.\n"
                    "Reason:\n"
                    "- value is parameter '%s'\n"
                    "- ownership mode is '%s'\n"
                    "- consumer path is function '%s'\n"
                    "- this parameter type is not part of the current move/borrow boundary subset\n"
                    "- the compiler does not yet enforce general own/ref rules for arbitrary value types at call boundaries\n"
                    "Fix:\n"
                    "- keep this value local to the function boundary\n"
                    "- or pass a subject value, movable resource, boundary value, Slot<subject-host>, or own SecureSlot<subject-host> instead",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref",
                    param->name != NULL ? param->name : "<param>",
                    param->mode == PARAM_MODE_OWN ? "own" : "ref",
                    node->data.func_decl.name != NULL ? node->data.func_decl.name : "<anonymous>");
            }
        }
        if (type_is_anchored_resource_handle(param_types[i])) {
            bool allowed_subject_slot = type_is_subject_host_slot_handle(param_types[i], ctx);
            if (!allowed_subject_slot) {
                semantic_error(ctx, node,
                    "Anchored handle parameters are currently closed to ref/own Slot<subject-host> / "
                    "own SecureSlot<subject-host>; other anchored handles remain local-only.\n"
                    "Reason:\n"
                    "- this anchored handle kind is not part of the stable boundary subset\n"
                    "- passing it across the function boundary would require a stronger ownership contract than the compiler currently guarantees\n"
                    "Fix:\n"
                    "- keep this anchored handle local to the current scope\n"
                    "- or convert it to a projection/value/boundary-safe result before the call");
            } else if (param->mode == PARAM_MODE_DEFAULT) {
                semantic_error(ctx, node,
                    "Slot<subject-host>/SecureSlot<subject-host> parameters are part of the anchored "
                    "subject-slot boundary and require explicit 'own' or 'ref'.\n"
                    "Reason:\n"
                    "- anchored subject-slot handles must declare whether the boundary borrows or transfers ownership\n"
                    "- implicit parameter passing would hide that boundary contract\n"
                    "Fix:\n"
                    "- mark the parameter as 'ref' for borrowing\n"
                    "- or mark it as 'own' for transfer");
            } else if (param_types[i]->data.slot.is_secure
                       && param->mode != PARAM_MODE_OWN) {
                semantic_error(ctx, node,
                    "SecureSlot<subject-host> parameters currently support only 'own' at function boundaries.\n"
                    "Reason:\n"
                    "- secure anchored handles carry capability-bearing ownership across the boundary\n"
                    "- borrowed secure forwarding is not part of the closed subset yet\n"
                    "Fix:\n"
                    "- change the parameter to 'own SecureSlot<subject-host>'\n"
                    "- or keep the SecureSlot local and pass a projection/value instead");
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
            "PGY_SEM_REDECLARATION",
            "semantic:function:duplicate_name",
            "rename-or-remove-duplicate",
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
    ctx->tracking_function_effects = true;
    ctx->in_async_func = prev_async || node->is_async_decl;

    /* Register parameters */
    for (size_t i = 0; i < param_count; i++) {
        Type *pt = func_type->data.function.param_types[i];
        const char *param_name = node->data.func_decl.params[i]->name;
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

    if (node->data.func_decl.body != NULL)
        type_check_block(node->data.func_decl.body, ctx);

    if (node->data.func_decl.body != NULL) {
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *param = node->data.func_decl.params[i];
            unsigned summary_mask;

            if (param == NULL || param->name == NULL || param->type == NULL)
                continue;
            if (param->mode != PARAM_MODE_REF)
                continue;
            if (!type_is_anchored_resource_handle(param_types[i])
                && !type_is_movable_resource_handle(param_types[i])
                && !type_is_subject_type(param_types[i], ctx)
                && !type_requires_boundary_borrow_tracking(param_types[i], ctx))
                continue;

            summary_mask = slot_analyze_param_summary_in_program(
                node->data.func_decl.body, param->name, ctx->program_root);
            if ((summary_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                if (type_is_movable_resource_handle(param_types[i])) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref movable resource '%s' cannot escape via return from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a return-escape path for that borrowed symbol\n"
                        "- returning it would let the borrow outlive the current call boundary\n"
                        "Fix:\n"
                        "- return a copied/projection/value result instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else if (type_is_subject_type(param_types[i], ctx)) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref subject '%s' cannot escape via return from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a return-escape path for that borrowed symbol\n"
                        "- returning it would let the borrow outlive the current call boundary\n"
                        "Fix:\n"
                        "- return a projection/object/tobject/value result instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else if (type_requires_boundary_borrow_tracking(param_types[i], ctx)) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref boundary value '%s' cannot escape via return from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a return-escape path for that borrowed symbol\n"
                        "- returning it would let the borrow outlive the current call boundary\n"
                        "Fix:\n"
                        "- return a copied/value/projection result instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref slot '%s' cannot escape via return from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' handle\n"
                        "- summary provenance root is '%s'\n"
                        "- 'ref' is a non-owning borrow tied to the caller scope\n"
                        "- returning it would let the borrow outlive the call boundary\n"
                        "Fix:\n"
                        "- return a projection/object/tobject/value instead\n"
                        "- or change the parameter to 'own' if transfer is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                }
            }
            if ((summary_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
                if (type_is_movable_resource_handle(param_types[i])) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref movable resource '%s' cannot escape through channel send from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a channel-escape path for that borrowed symbol\n"
                        "- channel send would transfer the borrow beyond the current call boundary\n"
                        "Fix:\n"
                        "- send a copied/projection/value snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else if (type_is_subject_type(param_types[i], ctx)) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref subject '%s' cannot escape through channel send from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a channel-escape path for that borrowed symbol\n"
                        "- channel send would transfer the borrow beyond the current call boundary\n"
                        "Fix:\n"
                        "- send a projection/object/tobject/value snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else if (type_requires_boundary_borrow_tracking(param_types[i], ctx)) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref boundary value '%s' cannot escape through channel send from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a channel-escape path for that borrowed symbol\n"
                        "- channel send would transfer the borrow beyond the current call boundary\n"
                        "Fix:\n"
                        "- send a copied/value/projection snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref slot '%s' cannot escape through channel send from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' handle\n"
                        "- summary provenance root is '%s'\n"
                        "- 'ref' is a non-owning borrow tied to the current call\n"
                        "- channel send would transfer the borrow beyond that boundary\n"
                        "Fix:\n"
                        "- send a projection/object/tobject/value snapshot instead\n"
                        "- or take ownership with 'own' before transfer",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                }
            }
            if ((summary_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
                if (type_is_movable_resource_handle(param_types[i])) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref movable resource '%s' cannot escape through helper/function call from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a transitive call-escape path for that borrowed symbol\n"
                        "- forwarding it to another call would create a transitive borrow the compiler cannot keep boundary-safe\n"
                        "Fix:\n"
                        "- perform the movable-resource operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else if (type_is_subject_type(param_types[i], ctx)) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref subject '%s' cannot escape through helper/function call from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a transitive call-escape path for that borrowed symbol\n"
                        "- forwarding it to another call would create a transitive borrow the compiler cannot keep boundary-safe\n"
                        "Fix:\n"
                        "- perform the subject operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else if (type_requires_boundary_borrow_tracking(param_types[i], ctx)) {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref boundary value '%s' cannot escape through helper/function call from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- summary provenance root is '%s'\n"
                        "- slot/resource summary found a transitive call-escape path for that borrowed symbol\n"
                        "- forwarding it to another call would create a transitive borrow the compiler cannot keep boundary-safe\n"
                        "Fix:\n"
                        "- perform the value operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                } else {
                    semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE", "semantic:borrow_escape", "change-ref-to-own-or-stop-escape", node,
                        "Borrowed ref slot '%s' cannot escape through helper/function call from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' handle\n"
                        "- summary provenance root is '%s'\n"
                        "- 'ref' is a non-owning borrow tied to the current call boundary\n"
                        "- forwarding it to another call would create a transitive borrow the compiler does not yet track precisely\n"
                        "Fix:\n"
                        "- perform the slot operation locally in this function\n"
                        "- or change the parameter to 'own' if transfer/forwarding is intended",
                        param->name,
                        param->name,
                        node->data.func_decl.name != NULL
                            ? node->data.func_decl.name : "<anonymous>",
                        param->name,
                        param->name);
                }
            }
        }
    }

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
            semantic_error_with_hints(ctx, "PGY_SEM_EFFECT_CONFLICT", "semantic:effect:incompatible_combo", "split-effect-families", node,
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
            semantic_warning_code(ctx, "PGY_SEM_EFFECT_CONFLICT", node,
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
            semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                "secure action '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action body derives authority-sensitive effects from capability-bearing work\n"
                "- zone '%s' makes this action part of an explicit authority boundary\n"
                "- contract source is the action header 'within %s' plus the derived authority-sensitive effect path inside the body\n"
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
            semantic_error_with_hints(ctx, "PGY_SEM_ACTION_CONTRACT_INVALID", "semantic:action_contract", "align-action-surface-with-zone", node,
                "action '%s' causing effect '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action contract declares causes '%s'\n"
                "- causing an effect inside zone '%s' is an authority-sensitive state change\n"
                "- contract source is the action header 'causes %s' together with 'within %s'\n"
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

    ctx->current_return = prev_return;
    ctx->current_function_decl = prev_function_decl;
    ctx->current_function_effects = prev_effects;
    ctx->tracking_function_effects = prev_tracking;
    ctx->in_async_func = prev_async;
    ctx->current_module_path = prev_module_path;
    free(param_types);
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_class_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.class_decl.name;
    ASTNode *saved_nominal = ctx->current_nominal_decl;

    /* If the class has generic parameters (<T, U, ...>), register them
     * as opaque types in a temporary scope so that resolve_type_node("T")
     * succeeds for field types and method signatures. */
    bool has_generics = (node->data.class_decl.generic_params != NULL
                         && node->data.class_decl.generic_params->count > 0);
    if (has_generics) {
        validate_generic_param_defaults(node->data.class_decl.generic_params,
            ctx, node, "class");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = node->data.class_decl.generic_params;
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

    Type *class_type = calloc(1, sizeof(Type));
    if (class_type == NULL) {
        if (has_generics) scope_exit(&ctx->scope);
        return false;
    }
    class_type->kind = TYPE_KIND_CLASS;
    class_type->nominal_flavor = nominal_flavor_from_decl(node);
    class_type->name = pergyra_strdup(name);

    Symbol *class_sym = symbol_create_function(name, class_type,
                                                node->line, node->column);
    class_sym->kind = SYMBOL_CLASS;

    /* Declare in the outer scope (step out of temporary generic scope).
     * Pass 1 may already have registered a nominal placeholder so that
     * forward references in earlier declarations can resolve. */
    {
        Scope *target = has_generics ? ctx->scope->parent : ctx->scope;
        Scope *saved = ctx->scope;
        ctx->scope = target;
        Symbol *existing = scope_lookup_current(ctx->scope, name);
        if (existing != NULL
            && existing->kind == SYMBOL_CLASS
            && existing->decl_line == (uint32_t)node->line
            && existing->decl_col == (uint32_t)node->column) {
            existing->type = class_type;
            symbol_destroy(class_sym);
        } else if (!scope_declare(ctx->scope, class_sym)) {
            semantic_error_with_hints(ctx,
                "PGY_SEM_REDECLARATION",
                "semantic:class:duplicate_name",
                "rename-or-remove-duplicate",
                node, "Redeclaration of class '%s'", name);
            symbol_destroy(class_sym);
            ctx->scope = saved;
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
        ctx->scope = saved;
    }

    /* Close the temporary generic-params scope before entering the real
     * class scope — the class scope will re-register generic params so
     * they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    validate_where_clause_bounds(node->data.class_decl.where_clause, ctx, node);
    validate_generic_param_default_bounds(
        node->data.class_decl.generic_params,
        node->data.class_decl.where_clause,
        ctx,
        node,
        "class",
        name);

    /* Check methods — type-check each in a temporary class scope,
     * then register the mangled name (ClassName_MethodName) in the
     * parent scope so that callers can find it. */
    scope_enter(&ctx->scope, SCOPE_CLASS);
    ctx->current_nominal_decl = node;

    /* Re-register generic type params inside the class scope */
    if (has_generics) {
        GenericParams *gp = node->data.class_decl.generic_params;
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
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *field = node->data.class_decl.fields[i];
        Type *field_type;
        Symbol *field_sym;

        if (field == NULL || field->name == NULL || field->type == NULL)
            continue;

        field_type = resolve_type_node(field->type, ctx);
        if (field->is_vessel_field) {
            ASTNode *field_decl = NULL;
            if (field_type != NULL && field_type->kind == TYPE_KIND_CLASS
                && field_type->name != NULL) {
                field_decl = find_type_decl_by_name(ctx->program_root, field_type->name);
            }
            if (field_decl == NULL
                || field_decl->data.class_decl.nominal_kind != NOMINAL_DECL_VESSEL) {
                semantic_error(ctx, field->type,
                    "subject vessel field '%s' must reference a vessel type",
                    field->name != NULL ? field->name : "<field>");
            }
        }
        field_sym = symbol_create_variable(field->name, field_type,
            node->line, node->column);
        if (field_sym != NULL)
            scope_declare(ctx->scope, field_sym);
    }
    /* struct declarations cannot have methods — use class or object */
    if (node->data.class_decl.nominal_kind == NOMINAL_DECL_STRUCT
        && node->data.class_decl.method_count > 0) {
        semantic_error_with_hints(ctx, "PGY_SEM_CLASS_CONTRACT_INVALID", "semantic:class_contract", "satisfy-generic-bound-or-widen", node,
            "struct '%s' cannot have methods; use 'class', 'subject', or 'object' instead",
            name != NULL ? name : "<struct>");
    }

    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        type_check_func_decl(node->data.class_decl.methods[i], ctx);

    /* Collect method signatures before the class scope is destroyed */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        const char *mname = method->data.func_decl.name;
        if (mname == NULL)
            continue;
        Symbol *msym = scope_lookup_current(ctx->scope, mname);
        if (msym == NULL || msym->kind != SYMBOL_FUNCTION)
            continue;

        /* Build mangled name: ClassName_MethodName */
        size_t len = strlen(name) + 1 + strlen(mname) + 1;
        char *mangled = malloc(len);
        if (mangled == NULL)
            continue;
        snprintf(mangled, len, "%s_%s", name, mname);

        /* The method's func_type already includes 'self' as the first
         * parameter (registered by type_check_func_decl), so reuse
         * the original signature directly. */
        Type *mangled_ft = msym->type;

        /* Register in parent scope (outside class) */
        Symbol *mangled_sym = symbol_create_function(
            mangled, mangled_ft, method->line, method->column);
        /* Temporarily step out to declare in parent */
        Scope *class_scope = ctx->scope;
        ctx->scope = class_scope->parent;
        if (!scope_declare(ctx->scope, mangled_sym))
            symbol_destroy(mangled_sym);
        ctx->scope = class_scope;
        free(mangled);
    }

    scope_exit(&ctx->scope);
    ctx->current_nominal_decl = saved_nominal;

    return !ctx->has_error;
}

bool
type_check_extern_block(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl != NULL && decl->type == AST_FUNC_DECL)
            type_check_func_decl(decl, ctx);
    }
    return !ctx->has_error;
}

/* -----------------------------------------------------------------
 * Program entry point
 * ----------------------------------------------------------------- */

bool
type_check_program(ASTNode *program, SemanticContext *ctx)
{
    size_t *topo_order = NULL;
    size_t topo_count = 0;

    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    ctx->program_root = program;

    /*
     * Pass 1: collect all top-level function and class names
     * so that forward references within the same file work.
     */
    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_TYPE_ALIAS) {
            const char *tname = stmt->data.type_alias.name;
            if (tname != NULL && scope_lookup_current(ctx->scope, tname) == NULL) {
                Symbol *s = symbol_create_function(tname, TYPE_UNKNOWN,
                    stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_CLASS_DECL) {
            const char *cname = stmt->data.class_decl.name;
            if (cname != NULL && scope_lookup_current(ctx->scope, cname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = nominal_flavor_from_decl(stmt);
                    t->name = pergyra_strdup(cname);
                }
                Symbol *s = symbol_create_function(cname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                if (s != NULL)
                    s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_FUNC_DECL) {
            const char *fname = stmt->data.func_decl.name;
            if (scope_lookup_current(ctx->scope, fname) == NULL) {
                /* Forward-declare with correct param count so that
                 * call-site arity checks pass before Pass 2. */
                size_t fpc = stmt->data.func_decl.param_count;
                /* Exclude implicit 'self' param from count */
                size_t real_pc = 0;
                for (size_t j = 0; j < fpc; j++) {
                    FuncParam *p = stmt->data.func_decl.params[j];
                    if (p->type == NULL && strcmp(p->name, "self") == 0)
                        continue;
                    real_pc++;
                }
                Type **ptypes = calloc(real_pc > 0 ? real_pc : 1,
                                         sizeof(Type *));
                for (size_t j = 0; j < real_pc; j++)
                    ptypes[j] = TYPE_UNKNOWN;
                Type *ret = TYPE_VOID;
                if (stmt->data.func_decl.return_type != NULL) {
                    /* Try to resolve return type; use TYPE_UNKNOWN on failure
                     * (e.g., generic return type 'T' not in scope yet). */
                    size_t saved_diag = ctx->diagnostic_count;
                    bool saved_err = ctx->has_error;
                    ret = resolve_type_node(stmt->data.func_decl.return_type, ctx);
                    if (ctx->diagnostic_count > saved_diag) {
                        /* Roll back the diagnostic — Pass 2 will re-check */
                        ctx->diagnostic_count = saved_diag;
                        ctx->has_error = saved_err;
                        ret = TYPE_UNKNOWN;
                    }
                }
                Type *placeholder = type_create_function(ptypes, real_pc, ret);
                if (placeholder != NULL)
                    placeholder->data.function.effect_mask =
                        declared_effects_from_function_node(stmt, ctx, NULL);
                free(ptypes);
                Symbol *s = symbol_create_function(fname, placeholder,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_EVENT_DECL) {
            const char *ename = stmt->data.event_decl.name;
            if (scope_lookup_current(ctx->scope, ename) == NULL) {
                size_t epc = stmt->data.event_decl.param_count;
                Type **eptypes = calloc(epc > 0 ? epc : 1, sizeof(Type *));
                for (size_t j = 0; j < epc; j++) {
                    ASTNode *p = stmt->data.event_decl.params[j];
                    if (p != NULL
                        && p->type == AST_LET_DECL
                        && p->data.let_decl.type != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_err = ctx->has_error;
                        eptypes[j] = resolve_type_node(p->data.let_decl.type, ctx);
                        if (ctx->diagnostic_count > saved_diag) {
                            /* Roll back the diagnostic — Pass 2 will re-check. */
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_err;
                            eptypes[j] = TYPE_UNKNOWN;
                        }
                    } else {
                        eptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *evt_ft = type_create_function(eptypes, epc, TYPE_VOID);
                free(eptypes);
                Symbol *s = symbol_create_function(ename, evt_ft,
                                                    stmt->line, stmt->column);
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_ENUM_DECL) {
            const char *ename = stmt->data.enum_decl.name;
            if (ename != NULL && scope_lookup_current(ctx->scope, ename) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_ENUM;
                    t->name = pergyra_strdup(ename);
                }
                Symbol *s = symbol_create_function(ename,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
            Symbol *enum_sym = scope_lookup_current(ctx->scope, ename);
            Type *etype = enum_sym != NULL ? enum_sym->type : TYPE_UNKNOWN;
            for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
                const char *vname = stmt->data.enum_decl.variants[j];
                if (vname == NULL || scope_lookup_current(ctx->scope, vname) != NULL)
                    continue;
                size_t vpc = (stmt->data.enum_decl.variant_param_counts != NULL)
                    ? stmt->data.enum_decl.variant_param_counts[j] : 0;
                if (vpc > 0) {
                    /* Tagged union variant constructor: register as function
                     * Circle(Int) → func Circle(Int) -> Shape */
                    Type **ptypes = calloc(vpc, sizeof(Type *));
                    for (size_t p = 0; p < vpc && ptypes != NULL; p++) {
                        ASTNode *pt = stmt->data.enum_decl.variant_params[j][p];
                        ptypes[p] = resolve_type_node(pt, ctx);
                    }
                    Type *ft = type_create_function(ptypes, vpc, etype);
                    free(ptypes);
                    Symbol *vs = symbol_create_function(vname, ft,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                } else {
                    /* Simple variant: register as variable */
                    Symbol *vs = symbol_create_variable(vname, etype,
                        stmt->line, stmt->column);
                    scope_declare(ctx->scope, vs);
                }
            }
        } else if (stmt->type == AST_EXTERN_BLOCK) {
            for (size_t j = 0; j < stmt->data.extern_block.count; j++) {
                ASTNode *decl = stmt->data.extern_block.declarations[j];
                if (decl == NULL || decl->type != AST_FUNC_DECL)
                    continue;
                const char *fname = decl->data.func_decl.name;
                if (scope_lookup_current(ctx->scope, fname) == NULL) {
                    Type *placeholder = type_create_function(NULL, 0, TYPE_VOID);
                    if (placeholder != NULL)
                        placeholder->data.function.effect_mask =
                            declared_effects_from_function_node(decl, ctx, NULL);
                    Symbol *s = symbol_create_function(fname, placeholder,
                                                        decl->line, decl->column);
                    scope_declare(ctx->scope, s);
                }
            }
        } else if (stmt->type == AST_INTENT_DECL) {
            const char *iname = stmt->data.intent_decl.name;
            if (iname != NULL && scope_lookup_current(ctx->scope, iname) == NULL) {
                size_t ipc = stmt->data.intent_decl.binding_count > 0
                    ? stmt->data.intent_decl.binding_count
                    : (stmt->data.intent_decl.involve_count
                        + stmt->data.intent_decl.value_count);
                Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
                for (size_t j = 0; j < ipc; j++) {
                    ASTNode *binding = stmt->data.intent_decl.binding_count > 0
                        ? stmt->data.intent_decl.bindings[j]
                        : (j < stmt->data.intent_decl.involve_count
                            ? stmt->data.intent_decl.involves[j]
                            : stmt->data.intent_decl.values[j - stmt->data.intent_decl.involve_count]);
                    if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                        && binding->data.intent_involves.subject_type != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_err = ctx->has_error;
                        ptypes[j] = resolve_type_node(
                            binding->data.intent_involves.subject_type, ctx);
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_err;
                            ptypes[j] = TYPE_UNKNOWN;
                        }
                    } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                        && binding->data.intent_value.value_type != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_err = ctx->has_error;
                        ptypes[j] = resolve_type_node(
                            binding->data.intent_value.value_type, ctx);
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_err;
                            ptypes[j] = TYPE_UNKNOWN;
                        }
                    } else {
                        ptypes[j] = TYPE_UNKNOWN;
                    }
                }
                Type *ft = type_create_function(ptypes, ipc, TYPE_BOOL);
                free(ptypes);
                Symbol *s = symbol_create_function(iname,
                    ft != NULL ? ft : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_INTENT;
                scope_declare(ctx->scope, s);
            }
        } else if (stmt->type == AST_PARTY_DECL
                   || stmt->type == AST_ROSTER_DECL
                   || stmt->type == AST_WORLD_DECL
                   || stmt->type == AST_RELATION_DECL
                   || stmt->type == AST_EFFECT_DECL
                   || stmt->type == AST_ZONE_DECL) {
            /* Register domain declarations as class-like symbols
             * so that constructor-like syntax can be introduced consistently */
            const char *dname = NULL;
            if (stmt->type == AST_PARTY_DECL)
                dname = stmt->data.party_decl.name;
            else if (stmt->type == AST_ROSTER_DECL)
                dname = stmt->data.roster_decl.name;
            else if (stmt->type == AST_WORLD_DECL)
                dname = stmt->data.world_decl.name;
            else if (stmt->type == AST_RELATION_DECL)
                dname = stmt->data.relation_decl.name;
            else if (stmt->type == AST_EFFECT_DECL)
                dname = stmt->data.effect_decl.name;
            else
                dname = stmt->data.zone_decl.name;
            if (dname != NULL && scope_lookup_current(ctx->scope, dname) == NULL) {
                Type *t = calloc(1, sizeof(Type));
                if (t != NULL) {
                    t->kind = TYPE_KIND_CLASS;
                    t->nominal_flavor = TYPE_NOMINAL_CLASS;
                    t->name = pergyra_strdup(dname);
                }
                Symbol *s = symbol_create_function(dname,
                    t != NULL ? t : TYPE_UNKNOWN, stmt->line, stmt->column);
                s->kind = SYMBOL_CLASS;
                scope_declare(ctx->scope, s);
            }
        }
    }

    semantic_type_resolution_precollect_program(program, ctx);
    if (!type_resolution_validate_graph(ctx))
        return false;
    if (!type_resolution_build_topo_order(&ctx->type_resolution_graph,
                                          &topo_order,
                                          &topo_count)) {
        semantic_error_with_hints(ctx, "PGY_SEM_TYPE_DEPENDENCY_CYCLE", "semantic:type_resolution:cycle", "break-cycle-via-indirection", program,
            "Type resolution topological ordering could not be constructed.\n"
            "Reason:\n"
            "- the semantic type dependency graph is not acyclic or not fully materialized\n"
            "- graph-backed staged resolution cannot trust the current declaration order\n"
            "Fix:\n"
            "- resolve the earlier type dependency cycle\n"
            "- or close the missing generic/alias/ability dependency edge");
        free(topo_order);
        return false;
    }

    semantic_run_type_resolution_worklist(program, ctx, topo_order, topo_count);

    /*
     * Pass 2: full type-check
     */
    for (size_t i = 0; i < program->data.program.count; i++)
        type_check_statement(program->data.program.statements[i], ctx);

    (void)type_resolution_validate_graph(ctx);

    /* Optional instrumentation for type-resolution audit (단계 1.0).
     * Enabled when PGY_TYPE_RES_STATS is set. */
    {
        const char *stats_env = getenv("PGY_TYPE_RES_STATS");
        if (stats_env != NULL && stats_env[0] != '\0' && stats_env[0] != '0') {
            TypeResolutionGraph *g = &ctx->type_resolution_graph;
            size_t kind_counts[7] = {0};
            size_t *indeg = NULL;
            size_t *outdeg = NULL;
            size_t name_dup = 0;
            size_t topo_count = 0;
            size_t *topo = NULL;
            bool topo_ok;

            for (size_t i = 0; i < g->node_count; i++) {
                int k = (int)g->nodes[i].kind;
                if (k >= 0 && k < 7) kind_counts[k]++;
            }
            indeg = calloc(g->node_count > 0 ? g->node_count : 1, sizeof(size_t));
            outdeg = calloc(g->node_count > 0 ? g->node_count : 1, sizeof(size_t));
            if (indeg != NULL && outdeg != NULL) {
                for (size_t e = 0; e < g->edge_count; e++) {
                    if (g->edges[e].from < g->node_count) outdeg[g->edges[e].from]++;
                    if (g->edges[e].to < g->node_count)   indeg[g->edges[e].to]++;
                }
            }
            /* Detect duplicate labels (re-visits of same named type) */
            for (size_t i = 0; i < g->node_count; i++) {
                const char *li = g->nodes[i].label;
                if (li == NULL) continue;
                for (size_t j = i + 1; j < g->node_count; j++) {
                    const char *lj = g->nodes[j].label;
                    if (lj != NULL && strcmp(li, lj) == 0) { name_dup++; break; }
                }
            }
            if (topo_order != NULL) {
                topo_ok = true;
                topo = NULL;
                topo_count = g->node_count;
            } else {
                topo_ok = type_resolution_build_topo_order(g, &topo, &topo_count);
            }

            fprintf(stderr, "[type-res-stats] nodes=%zu edges=%zu duplicate_labels=%zu topo_ok=%d topo_produced=%zu/%zu\n",
                    g->node_count, g->edge_count, name_dup,
                    topo_ok ? 1 : 0, topo_count, g->node_count);
            fprintf(stderr, "[type-res-stats] resolve_type_node: calls=%zu unique_nodes=%zu revisit_rate=%.1f%%\n",
                    g_resolve_type_node_calls, g_resolve_type_node_unique_nodes,
                    g_resolve_type_node_calls > 0
                        ? 100.0 * (double)(g_resolve_type_node_calls - g_resolve_type_node_unique_nodes)
                          / (double)g_resolve_type_node_calls
                        : 0.0);
            {
                size_t total = g_resolve_type_node_cache_hits + g_resolve_type_node_cache_misses;
                fprintf(stderr, "[type-res-stats] cache: hits=%zu misses=%zu hit_rate=%.1f%%\n",
                        g_resolve_type_node_cache_hits,
                        g_resolve_type_node_cache_misses,
                        total > 0
                            ? 100.0 * (double)g_resolve_type_node_cache_hits / (double)total
                            : 0.0);
            }
            fprintf(stderr, "[type-res-stats] kind: TYPE_REF=%zu BUILTIN=%zu DECL=%zu ALIAS=%zu GENERIC_PARAM=%zu LOCAL_CONTRACT=%zu PROJECTION_PATH=%zu\n",
                    kind_counts[0], kind_counts[1], kind_counts[2],
                    kind_counts[3], kind_counts[4], kind_counts[5],
                    kind_counts[6]);

            /* Top 5 in-degree nodes */
            if (indeg != NULL && g->node_count > 0) {
                for (int rank = 0; rank < 5; rank++) {
                    size_t best = 0;
                    size_t best_val = 0;
                    bool found = false;
                    for (size_t i = 0; i < g->node_count; i++) {
                        if (indeg[i] > best_val) {
                            best = i; best_val = indeg[i]; found = true;
                        }
                    }
                    if (!found || best_val == 0) break;
                    fprintf(stderr, "[type-res-stats] top-indeg[%d] %s (in=%zu)\n",
                            rank,
                            g->nodes[best].label != NULL ? g->nodes[best].label : "<?>",
                            best_val);
                    indeg[best] = 0;
                }
            }
            free(indeg);
            free(outdeg);
            free(topo);
        }
    }

    free(topo_order);
    return !ctx->has_error;
}
