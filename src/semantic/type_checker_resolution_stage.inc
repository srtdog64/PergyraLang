const char *
semantic_type_resolution_decl_label(ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        return stmt->data.type_alias.name;
    case AST_CLASS_DECL:
        return stmt->data.class_decl.name;
    case AST_FUNC_DECL:
        return stmt->data.func_decl.name;
    case AST_EVENT_DECL:
        return stmt->data.event_decl.name;
    case AST_ENUM_DECL:
        return stmt->data.enum_decl.name;
    case AST_ABILITY_DECL:
        return stmt->data.ability_decl.name;
    case AST_ROLE_DECL:
        return stmt->data.role_decl.name;
    case AST_PARTY_DECL:
        return stmt->data.party_decl.name;
    case AST_ROSTER_DECL:
        return stmt->data.roster_decl.name;
    case AST_WORLD_DECL:
        return stmt->data.world_decl.name;
    case AST_INTENT_DECL:
        return stmt->data.intent_decl.name;
    case AST_RELATION_DECL:
        return stmt->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return stmt->data.effect_decl.name;
    case AST_ZONE_DECL:
        return stmt->data.zone_decl.name;
    default:
        return NULL;
    }
}

static TypeResolutionNodeKind
semantic_type_resolution_decl_kind(ASTNode *stmt)
{
    if (stmt != NULL && stmt->type == AST_TYPE_ALIAS)
        return TYPE_RES_NODE_ALIAS;
    return TYPE_RES_NODE_DECL;
}

static Type *
semantic_stage_resolve_type_quiet(ASTNode *type_node,
                                  SemanticContext *ctx,
                                  const ASTNode *consumer_site,
                                  const char *consumer_name,
                                  const char *reason)
{
    size_t saved_diag;
    bool saved_error;
    Type *resolved;

    if (type_node == NULL || ctx == NULL)
        return TYPE_UNKNOWN;

    (void)consumer_site;
    (void)consumer_name;
    (void)reason;

    saved_diag = ctx->diagnostic_count;
    saved_error = ctx->has_error;
    resolved = resolve_type_node(type_node, ctx);
    if (ctx->diagnostic_count > saved_diag) {
        for (size_t i = saved_diag; i < ctx->diagnostic_count; i++) {
            if (ctx->diagnostics[i] != NULL) {
                free(ctx->diagnostics[i]->message);
                free(ctx->diagnostics[i]);
                ctx->diagnostics[i] = NULL;
            }
        }
        ctx->diagnostic_count = saved_diag;
        ctx->has_error = saved_error;
        return TYPE_UNKNOWN;
    }

    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static ASTNode *
semantic_stage_named_decl_quiet(SemanticContext *ctx,
                                ASTNodeType decl_type,
                                const char *provider_name)
{
    if (ctx == NULL || ctx->program_root == NULL
        || provider_name == NULL || provider_name[0] == '\0') {
        return NULL;
    }

    switch (decl_type) {
    case AST_CLASS_DECL:
        return find_type_decl_by_name(ctx->program_root, provider_name);
    case AST_ABILITY_DECL:
        return find_ability_decl_by_name(ctx->program_root, provider_name);
    case AST_ROLE_DECL:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_ZONE_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
        return find_domain_decl_by_name(ctx->program_root, decl_type, provider_name);
    default:
        return semantic_find_top_level_decl_by_label(ctx->program_root,
                                                     provider_name,
                                                     TYPE_RES_NODE_DECL);
    }
}

static void
semantic_stage_required_abilities(ASTNode **ability_refs,
                                  size_t ability_count,
                                  SemanticContext *ctx,
                                  const ASTNode *owner,
                                  const char *consumer_name,
                                  const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *ability_ref = ability_refs[i];

        if (ability_ref != NULL
            && ability_ref->type == AST_TYPE
            && ability_ref->data.type.name != NULL) {
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ABILITY_DECL,
                ability_ref->data.type.name);
        }

        (void)semantic_stage_resolve_type_quiet(
            ability_ref,
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_stage_generic_contract_nodes(GenericParams *gp,
                                      WhereClause *wc,
                                      SemanticContext *ctx,
                                      ASTNode *owner,
                                      const char *kind_name,
                                      const char *owner_name)
{
    if (ctx == NULL)
        return;

    if (gp != NULL) {
        for (size_t i = 0; i < gp->count; i++) {
            GenericParam *param = gp->params[i];
            char *consumer_name;

            if (param == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          kind_name != NULL ? kind_name : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            if (param->default_type != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->default_type,
                    ctx,
                    owner,
                    consumer_name,
                    "default-type lookup");
            }

            if (param->constraint != NULL) {
                (void)semantic_stage_resolve_type_quiet(
                    param->constraint,
                    ctx,
                    owner,
                    consumer_name,
                    "generic constraint lookup");
            }
            free(consumer_name);
        }
    }

    if (wc != NULL) {
        for (size_t i = 0; i < wc->count; i++) {
            TypeConstraint *tc = wc->constraints[i];
            char *consumer_name;

            if (tc == NULL)
                continue;

            consumer_name = tc_strdup_fmt("%s %s.%s",
                                          kind_name != NULL ? kind_name : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                (void)semantic_stage_resolve_type_quiet(
                    tc->bounds[b],
                    ctx,
                    owner,
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }

    validate_generic_param_defaults(gp, ctx, owner, kind_name);
    validate_where_clause_bounds(wc, ctx, owner);
}

static void
semantic_stage_function_signature(ASTNode *func_decl,
                                  SemanticContext *ctx,
                                  const char *fallback_name)
{
    const char *consumer_name;

    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = func_decl->data.func_decl.name != NULL
        ? func_decl->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<func>");

    semantic_stage_generic_contract_nodes(
        func_decl->data.func_decl.generic_params,
        func_decl->data.func_decl.where_clause,
        ctx,
        func_decl,
        "func",
        consumer_name);

    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *param = func_decl->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_strdup_fmt("func %s.%s",
                                            consumer_name,
                                            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->type,
            ctx,
            func_decl,
            param_consumer_name,
            "function parameter type lookup");
        free(param_consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        func_decl->data.func_decl.return_type,
        ctx,
        func_decl,
        consumer_name,
        "function return type lookup");

    semantic_stage_required_abilities(
        func_decl->data.func_decl.required_abilities,
        func_decl->data.func_decl.required_ability_count,
        ctx,
        func_decl,
        consumer_name,
        "action ability consumer lookup");
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_ZONE_DECL,
        func_decl->data.func_decl.within_zone);
    (void)semantic_stage_named_decl_quiet(
        ctx,
        AST_EFFECT_DECL,
        func_decl->data.func_decl.causes_effect);
}

void
semantic_stage_method_array(ASTNode **methods,
                            size_t method_count,
                            SemanticContext *ctx,
                            const char *fallback_name)
{
    if (methods == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < method_count; i++)
        semantic_stage_function_signature(methods[i], ctx, fallback_name);
}

static void
semantic_stage_event_signature(ASTNode *event_decl,
                               SemanticContext *ctx)
{
    if (event_decl == NULL || event_decl->type != AST_EVENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < event_decl->data.event_decl.param_count; i++) {
        ASTNode *param = event_decl->data.event_decl.params[i];
        char *consumer_name;

        if (param == NULL || param->type != AST_LET_DECL)
            continue;

        consumer_name = tc_strdup_fmt("event %s.%s",
                                      event_decl->data.event_decl.name != NULL
                                          ? event_decl->data.event_decl.name : "<event>",
                                      param->data.let_decl.name != NULL
                                          ? param->data.let_decl.name : "<param>");
        if (consumer_name == NULL)
            continue;

        (void)semantic_stage_resolve_type_quiet(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    (void)semantic_stage_resolve_type_quiet(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name,
        "event return type lookup");
}

static ASTNode *
semantic_find_top_level_decl_by_label(ASTNode *program,
                                      const char *label,
                                      TypeResolutionNodeKind kind)
{
    if (program == NULL || program->type != AST_PROGRAM || label == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *stmt_label;

        if (stmt == NULL)
            continue;
        if (semantic_type_resolution_decl_kind(stmt) != kind)
            continue;

        stmt_label = semantic_type_resolution_decl_label(stmt);
        if (stmt_label != NULL && strcmp(stmt_label, label) == 0)
            return stmt;
    }

    return NULL;
}

static ASTNode *
semantic_find_graph_host_decl(ASTNode *program,
                              const char *label)
{
    const char *space;
    const char *dot;
    size_t name_len;
    char *host_name;
    ASTNode *decl = NULL;

    if (program == NULL || label == NULL)
        return NULL;

    if (strncmp(label, "world ", 6) == 0) {
        space = label + 6;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_WORLD_DECL, host_name);
        free(host_name);
        return decl;
    }

    if (strncmp(label, "zone ", 5) == 0) {
        space = label + 5;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_ZONE_DECL, host_name);
        free(host_name);
        return decl;
    }

    return NULL;
}

static void
semantic_stage_top_level_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode *saved_nominal;
    ASTNode *saved_relation;
    ASTNode *saved_effect;
    ASTNode *saved_zone;
    ASTNode *saved_world;

    if (decl == NULL || ctx == NULL)
        return;

    saved_nominal = ctx->current_nominal_decl;
    saved_relation = ctx->current_relation;
    saved_effect = ctx->current_effect;
    saved_zone = ctx->current_zone;
    saved_world = ctx->current_world;

    switch (decl->type) {
    case AST_TYPE_ALIAS: {
        Symbol *sym;
        if (decl->data.type_alias.name == NULL)
            break;
        sym = scope_lookup_current(ctx->scope, decl->data.type_alias.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.type_alias.target_type,
            ctx,
            decl,
            decl->data.type_alias.name,
            "type-alias target lookup");
        if (sym != NULL)
            sym->type = resolve_type_node(decl->data.type_alias.target_type, ctx);
        break;
    }

    case AST_CLASS_DECL:
        ctx->current_nominal_decl = decl;
        semantic_stage_generic_contract_nodes(
            decl->data.class_decl.generic_params,
            decl->data.class_decl.where_clause,
            ctx,
            decl,
            "class",
            decl->data.class_decl.name);
        for (size_t i = 0; i < decl->data.class_decl.field_count; i++) {
            ClassField *field = decl->data.class_decl.fields[i];
            char *consumer_name;
            if (field == NULL)
                continue;
            consumer_name = tc_strdup_fmt("class %s.%s",
                                          decl->data.class_decl.name != NULL
                                              ? decl->data.class_decl.name : "<class>",
                                          field->name != NULL ? field->name : "<field>");
            if (consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->type,
                ctx,
                decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.class_decl.methods,
            decl->data.class_decl.method_count,
            ctx,
            decl->data.class_decl.name);
        break;

    case AST_FUNC_DECL:
        semantic_stage_function_signature(decl, ctx, decl->data.func_decl.name);
        break;

    case AST_EVENT_DECL:
        semantic_stage_event_signature(decl, ctx);
        break;

    case AST_ENUM_DECL:
        for (size_t i = 0; i < decl->data.enum_decl.variant_count; i++) {
            ASTNode **params = decl->data.enum_decl.variant_params != NULL
                ? decl->data.enum_decl.variant_params[i] : NULL;
            size_t param_count = decl->data.enum_decl.variant_param_counts != NULL
                ? decl->data.enum_decl.variant_param_counts[i] : 0;
            const char *variant_name = decl->data.enum_decl.variants != NULL
                ? decl->data.enum_decl.variants[i] : NULL;
            char *consumer_name;

            if (params == NULL || param_count == 0)
                continue;

            consumer_name = tc_strdup_fmt("enum %s.%s",
                                          decl->data.enum_decl.name != NULL
                                              ? decl->data.enum_decl.name : "<enum>",
                                          variant_name != NULL ? variant_name : "<variant>");
            if (consumer_name == NULL)
                continue;

            for (size_t j = 0; j < param_count; j++) {
                (void)semantic_stage_resolve_type_quiet(
                    params[j],
                    ctx,
                    decl,
                    consumer_name,
                    "enum variant payload type lookup");
            }
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.enum_decl.methods,
            decl->data.enum_decl.method_count,
            ctx,
            decl->data.enum_decl.name);
        break;

    case AST_ABILITY_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.ability_decl.generic_params,
            decl->data.ability_decl.where_clause,
            ctx,
            decl,
            "ability",
            decl->data.ability_decl.name);
        for (size_t i = 0; i < decl->data.ability_decl.require_count; i++) {
            ASTNode *req = decl->data.ability_decl.require_fields[i];
            char *consumer_name;
            if (req == NULL || req->type != AST_REQUIRE_FIELD)
                continue;
            consumer_name = tc_strdup_fmt("ability %s.%s",
                                          decl->data.ability_decl.name != NULL
                                              ? decl->data.ability_decl.name : "<ability>",
                                          req->data.require_field.name != NULL
                                              ? req->data.require_field.name : "<require-field>");
            if (consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.ability_decl.methods,
            decl->data.ability_decl.method_count,
            ctx,
            decl->data.ability_decl.name);
        break;

    case AST_ROLE_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.role_decl.generic_params,
            decl->data.role_decl.where_clause,
            ctx,
            decl,
            "role",
            decl->data.role_decl.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.role_decl.for_type,
            ctx,
            decl,
            decl->data.role_decl.name,
            "role host-type lookup");
        for (size_t i = 0; i < decl->data.role_decl.include_count; i++) {
            ASTNode *inc = decl->data.role_decl.includes[i];
            ASTNode *included_role_decl;
            ASTNode **effective = NULL;
            size_t effective_count = 0;

            if (inc == NULL || inc->type != AST_INCLUDE_STMT)
                continue;

            included_role_decl = semantic_stage_named_decl_quiet(
                ctx,
                AST_ROLE_DECL,
                inc->data.include_stmt.role_name);
            effective = collect_effective_generic_arg_nodes(
                (included_role_decl != NULL && included_role_decl->type == AST_ROLE_DECL)
                    ? included_role_decl->data.role_decl.generic_params
                    : NULL,
                inc->data.include_stmt.type_args,
                inc,
                ctx,
                "role include",
                inc->data.include_stmt.role_name,
                &effective_count);
            free(effective);
            (void)effective_count;
        }
        for (size_t i = 0; i < decl->data.role_decl.impl_count; i++) {
            ASTNode *impl = decl->data.role_decl.impl_abilities[i];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;
            if (impl->data.impl_ability.ability_ref != NULL
                && impl->data.impl_ability.ability_ref->type == AST_TYPE
                && impl->data.impl_ability.ability_ref->data.type.name != NULL) {
                (void)semantic_stage_named_decl_quiet(
                    ctx,
                    AST_ABILITY_DECL,
                    impl->data.impl_ability.ability_ref->data.type.name);
            }
            (void)semantic_stage_resolve_type_quiet(
                impl->data.impl_ability.ability_ref,
                ctx,
                impl,
                decl->data.role_decl.name,
                "role impl ability lookup");
        }
        break;

    case AST_PARTY_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.party_decl.generic_params,
            NULL,
            ctx,
            decl,
            "party",
            decl->data.party_decl.name);
        (void)semantic_stage_resolve_type_quiet(
            decl->data.party_decl.extends,
            ctx,
            decl,
            decl->data.party_decl.name,
            "party extends lookup");
        for (size_t i = 0; i < decl->data.party_decl.shared_count; i++) {
            ASTNode *field = decl->data.party_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "party shared field type lookup");
        }
        for (size_t i = 0; i < decl->data.party_decl.role_count; i++) {
            ASTNode *role_slot = decl->data.party_decl.role_slots[i];
            char *consumer_name;
            if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
                continue;
            consumer_name = tc_strdup_fmt("party %s.%s",
                                          decl->data.party_decl.name != NULL
                                              ? decl->data.party_decl.name : "<party>",
                                          role_slot->data.role_slot.slot_name != NULL
                                              ? role_slot->data.role_slot.slot_name : "<role-slot>");
            if (consumer_name == NULL)
                continue;
            semantic_stage_required_abilities(
                role_slot->data.role_slot.required_abilities,
                role_slot->data.role_slot.ability_count,
                ctx,
                role_slot,
                consumer_name,
                "party role slot ability consumer lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.party_decl.methods,
            decl->data.party_decl.method_count,
            ctx,
            decl->data.party_decl.name);
        break;

    case AST_ROSTER_DECL:
        semantic_stage_generic_contract_nodes(
            decl->data.roster_decl.generic_params,
            NULL,
            ctx,
            decl,
            "roster",
            decl->data.roster_decl.name);
        for (size_t i = 0; i < decl->data.roster_decl.party_count; i++) {
            ASTNode *slot = decl->data.roster_decl.party_slots[i];
            if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_PARTY_DECL,
                slot->data.roster_slot.party_type);
        }
        for (size_t i = 0; i < decl->data.roster_decl.shared_count; i++) {
            ASTNode *field = decl->data.roster_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "roster shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.roster_decl.methods,
            decl->data.roster_decl.method_count,
            ctx,
            decl->data.roster_decl.name);
        break;

    case AST_WORLD_DECL:
        ctx->current_world = decl;
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *roster = decl->data.world_decl.rosters[i];
            if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ROSTER_DECL,
                roster->data.world_roster.roster_type);
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *zone = decl->data.world_decl.zones[i];
            if (zone == NULL || zone->type != AST_WORLD_ZONE)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_ZONE_DECL,
                zone->data.world_zone.zone_type);
        }
        semantic_stage_world_local_contracts(decl, ctx);
        for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
            ASTNode *field = decl->data.world_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "world shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.world_decl.methods,
            decl->data.world_decl.method_count,
            ctx,
            decl->data.world_decl.name);
        break;

    case AST_INTENT_DECL:
        for (size_t i = 0; i < decl->data.intent_decl.involve_count; i++) {
            ASTNode *binding = decl->data.intent_decl.involves[i];
            if (binding == NULL || binding->type != AST_INTENT_INVOLVES)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                binding->data.intent_involves.subject_type,
                ctx,
                binding,
                binding->data.intent_involves.alias,
                "intent involves type lookup");
        }
        for (size_t i = 0; i < decl->data.intent_decl.value_count; i++) {
            ASTNode *binding = decl->data.intent_decl.values[i];
            if (binding == NULL || binding->type != AST_INTENT_VALUE)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                binding->data.intent_value.value_type,
                ctx,
                binding,
                binding->data.intent_value.alias,
                "intent value type lookup");
        }
        (void)semantic_stage_resolve_type_quiet(
            decl->data.intent_decl.default_where_type,
            ctx,
            decl,
            decl->data.intent_decl.name,
            "intent default where-type lookup");
        for (size_t i = 0; i < decl->data.intent_decl.step_count; i++) {
            ASTNode *step = decl->data.intent_decl.steps[i];
            char *step_consumer_name;
            if (step == NULL || step->type != AST_INTENT_STEP)
                continue;
            step_consumer_name = tc_strdup_fmt(
                "intent %s.%s",
                decl->data.intent_decl.name != NULL
                    ? decl->data.intent_decl.name : "<intent>",
                step->data.intent_step.name != NULL
                    ? step->data.intent_step.name : "<step>");
            if (step_consumer_name == NULL)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                step->data.intent_step.where_type,
                ctx,
                step,
                step_consumer_name,
                "intent step where-type lookup");
            semantic_stage_required_abilities(
                step->data.intent_step.required_abilities,
                step->data.intent_step.required_ability_count,
                ctx,
                step,
                step_consumer_name,
                "intent step ability consumer lookup");
            (void)semantic_stage_named_decl_quiet(
                ctx,
                AST_EFFECT_DECL,
                step->data.intent_step.causes_effect);
            free(step_consumer_name);
        }
        break;

    case AST_RELATION_DECL:
        ctx->current_relation = decl;
        (void)semantic_stage_resolve_type_quiet(
            decl->data.relation_decl.between_left_type,
            ctx,
            decl,
            decl->data.relation_decl.name,
            "relation between-left type lookup");
        (void)semantic_stage_resolve_type_quiet(
            decl->data.relation_decl.between_right_type,
            ctx,
            decl,
            decl->data.relation_decl.name,
            "relation between-right type lookup");
        for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
            ASTNode *slot = decl->data.relation_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "relation slot type lookup");
        }
        for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
            ASTNode *field = decl->data.relation_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "relation shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.relation_decl.methods,
            decl->data.relation_decl.method_count,
            ctx,
            decl->data.relation_decl.name);
        break;

    case AST_EFFECT_DECL:
        ctx->current_effect = decl;
        for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
            ASTNode *slot = decl->data.effect_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "effect slot type lookup");
        }
        for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
            ASTNode *field = decl->data.effect_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "effect shared field type lookup");
        }
        semantic_stage_method_array(
            decl->data.effect_decl.methods,
            decl->data.effect_decl.method_count,
            ctx,
            decl->data.effect_decl.name);
        break;

    case AST_ZONE_DECL:
        ctx->current_zone = decl;
        for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
            ASTNode *slot = decl->data.zone_decl.slots[i];
            if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                slot->data.domain_slot.type,
                ctx,
                slot,
                slot->data.domain_slot.slot_name,
                "zone slot type lookup");
        }
        for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
            ASTNode *layer = decl->data.zone_decl.layer_slots[i];
            if (layer == NULL || layer->type != AST_ZONE_LAYER_SLOT)
                continue;
            (void)semantic_stage_named_decl_quiet(
                ctx,
                layer->data.zone_layer_slot.is_relation
                    ? AST_RELATION_DECL
                    : AST_EFFECT_DECL,
                layer->data.zone_layer_slot.layer_type);
        }
        semantic_stage_zone_local_contracts(decl);
        for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
            ASTNode *field = decl->data.zone_decl.shared_fields[i];
            if (field == NULL || field->type != AST_PARTY_SHARED)
                continue;
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "zone shared field type lookup");
        }
        for (size_t i = 0; i < decl->data.zone_decl.authority_count; i++) {
            ASTNode *authority = decl->data.zone_decl.authorities[i];
            char *consumer_name;
            if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
                continue;
            consumer_name = tc_strdup_fmt("zone %s.%s",
                                          decl->data.zone_decl.name != NULL
                                              ? decl->data.zone_decl.name : "<zone>",
                                          authority->data.zone_authority.subject_slot_name != NULL
                                              ? authority->data.zone_authority.subject_slot_name
                                              : "<authority>");
            if (consumer_name == NULL)
                continue;
            semantic_stage_required_abilities(
                authority->data.zone_authority.required_abilities,
                authority->data.zone_authority.ability_count,
                ctx,
                authority,
                consumer_name,
                "zone authority ability consumer lookup");
            free(consumer_name);
        }
        semantic_stage_method_array(
            decl->data.zone_decl.methods,
            decl->data.zone_decl.method_count,
            ctx,
            decl->data.zone_decl.name);
        break;

    default:
        break;
    }

    ctx->current_nominal_decl = saved_nominal;
    ctx->current_relation = saved_relation;
    ctx->current_effect = saved_effect;
    ctx->current_zone = saved_zone;
    ctx->current_world = saved_world;
}
