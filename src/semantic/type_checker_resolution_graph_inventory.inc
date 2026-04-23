semantic_type_resolution_collect_type_refs(ASTNode *type_node,
                                           SemanticContext *ctx,
                                           const ASTNode *consumer_site,
                                           const char *consumer_name,
                                           const char *reason)
{
    if (type_node == NULL || ctx == NULL || consumer_name == NULL)
        return;

    switch (type_node->type) {
    case AST_CHANNEL_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.channel_type.element_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_FUTURE_TYPE:
        semantic_type_resolution_collect_type_refs(
            type_node->data.future_type.value_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_EVENT_HANDLER_TYPE:
        for (size_t i = 0; i < type_node->data.event_handler_type.param_count; i++) {
            semantic_type_resolution_collect_type_refs(
                type_node->data.event_handler_type.param_types[i],
                ctx,
                consumer_site,
                consumer_name,
                reason);
        }
        semantic_type_resolution_collect_type_refs(
            type_node->data.event_handler_type.return_type,
            ctx,
            consumer_site,
            consumer_name,
            reason);
        return;

    case AST_TYPE:
        if (type_node->data.type.name != NULL) {
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                consumer_site != NULL ? consumer_site : type_node,
                consumer_name,
                type_node,
                reason != NULL ? reason : "type dependency");
        }
        if (type_node->data.type.generic_args != NULL) {
            for (size_t i = 0; i < type_node->data.type.generic_args->count; i++) {
                GenericParam *gp = type_node->data.type.generic_args->params[i];
                if (gp != NULL && gp->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        gp->constraint,
                        ctx,
                        consumer_site,
                        consumer_name,
                        reason);
                }
            }
        }
        return;

    default:
        return;
    }
}

static void
semantic_type_resolution_collect_generic_contract_inventory(GenericParams *gp,
                                                            WhereClause *wc,
                                                            SemanticContext *ctx,
                                                            const ASTNode *owner,
                                                            const char *owner_kind,
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
                                          owner_kind != NULL ? owner_kind : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          param->name != NULL ? param->name : "<type-param>");
            if (consumer_name == NULL)
                continue;

            semantic_type_resolution_collect_type_refs(
                param->default_type,
                ctx,
                owner,
                consumer_name,
                "default-type lookup");
            semantic_type_resolution_collect_type_refs(
                param->constraint,
                ctx,
                owner,
                consumer_name,
                "generic constraint lookup");
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
                                          owner_kind != NULL ? owner_kind : "decl",
                                          owner_name != NULL ? owner_name : "<anon>",
                                          tc->type_param != NULL ? tc->type_param : "<type-param>");
            if (consumer_name == NULL)
                continue;

            for (size_t b = 0; b < tc->bound_count; b++) {
                semantic_type_resolution_collect_type_refs(
                    tc->bounds[b],
                    ctx,
                    owner != NULL ? owner : tc->bounds[b],
                    consumer_name,
                    "where-bound lookup");
            }
            free(consumer_name);
        }
    }
}

static void
semantic_type_resolution_record_string_dependency(SemanticContext *ctx,
                                                  const ASTNode *consumer_site,
                                                  const char *consumer_name,
                                                  const char *provider_name,
                                                  const char *reason)
{
    if (provider_name == NULL || provider_name[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_name,
        TYPE_RES_NODE_DECL,
        NULL,
        provider_name,
        reason);
}

static void
semantic_type_resolution_precollect_required_abilities(ASTNode **ability_refs,
                                                       size_t ability_count,
                                                       SemanticContext *ctx,
                                                       const ASTNode *owner,
                                                       const char *consumer_name,
                                                       const char *reason)
{
    if (ability_refs == NULL || ctx == NULL)
        return;

    for (size_t i = 0; i < ability_count; i++) {
        semantic_type_resolution_collect_type_refs(
            ability_refs[i],
            ctx,
            owner,
            consumer_name,
            reason);
    }
}

static void
semantic_type_resolution_precollect_ability_inventory(ASTNode *ability_decl,
                                                      SemanticContext *ctx)
{
    if (ability_decl == NULL || ability_decl->type != AST_ABILITY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        ability_decl->data.ability_decl.generic_params,
        ability_decl->data.ability_decl.where_clause,
        ctx,
        ability_decl,
        "ability",
        ability_decl->data.ability_decl.name);

    for (size_t i = 0; i < ability_decl->data.ability_decl.require_count; i++) {
        ASTNode *req = ability_decl->data.ability_decl.require_fields[i];
        char *consumer_name;

        if (req == NULL || req->type != AST_REQUIRE_FIELD)
            continue;

        consumer_name = tc_strdup_fmt("ability %s.%s",
                                      ability_decl->data.ability_decl.name != NULL
                                          ? ability_decl->data.ability_decl.name : "<ability>",
                                      req->data.require_field.name != NULL
                                          ? req->data.require_field.name : "<require-field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                req->data.require_field.type,
                ctx,
                req,
                consumer_name,
                "ability require-field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < ability_decl->data.ability_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            ability_decl->data.ability_decl.methods[i],
            ctx,
            ability_decl->data.ability_decl.name);
    }
}

static void
semantic_type_resolution_precollect_event_inventory(ASTNode *event_decl,
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

        semantic_type_resolution_collect_type_refs(
            param->data.let_decl.type,
            ctx,
            event_decl,
            consumer_name,
            "event parameter type lookup");
        free(consumer_name);
    }

    semantic_type_resolution_collect_type_refs(
        event_decl->data.event_decl.return_type,
        ctx,
        event_decl,
        event_decl->data.event_decl.name != NULL
            ? event_decl->data.event_decl.name : "<event>",
        "event return type lookup");
}

static void
semantic_type_resolution_precollect_enum_inventory(ASTNode *enum_decl,
                                                   SemanticContext *ctx)
{
    if (enum_decl == NULL || enum_decl->type != AST_ENUM_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < enum_decl->data.enum_decl.variant_count; i++) {
        ASTNode **params = enum_decl->data.enum_decl.variant_params != NULL
            ? enum_decl->data.enum_decl.variant_params[i] : NULL;
        size_t param_count = enum_decl->data.enum_decl.variant_param_counts != NULL
            ? enum_decl->data.enum_decl.variant_param_counts[i] : 0;
        const char *variant_name = enum_decl->data.enum_decl.variants != NULL
            ? enum_decl->data.enum_decl.variants[i] : NULL;
        char *consumer_name;

        if (params == NULL || param_count == 0)
            continue;

        consumer_name = tc_strdup_fmt("enum %s.%s",
                                      enum_decl->data.enum_decl.name != NULL
                                          ? enum_decl->data.enum_decl.name : "<enum>",
                                      variant_name != NULL ? variant_name : "<variant>");
        if (consumer_name == NULL)
            continue;

        for (size_t j = 0; j < param_count; j++) {
            semantic_type_resolution_collect_type_refs(
                params[j],
                ctx,
                enum_decl,
                consumer_name,
                "enum variant payload type lookup");
        }
        free(consumer_name);
    }

    semantic_stage_method_array(
        enum_decl->data.enum_decl.methods,
        enum_decl->data.enum_decl.method_count,
        ctx,
        enum_decl->data.enum_decl.name);
}

static void
semantic_type_resolution_precollect_role_inventory(ASTNode *role_decl,
                                                   SemanticContext *ctx)
{
    if (role_decl == NULL || role_decl->type != AST_ROLE_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        role_decl->data.role_decl.generic_params,
        role_decl->data.role_decl.where_clause,
        ctx,
        role_decl,
        "role",
        role_decl->data.role_decl.name);

    semantic_type_resolution_collect_type_refs(
        role_decl->data.role_decl.for_type,
        ctx,
        role_decl,
        role_decl->data.role_decl.name != NULL
            ? role_decl->data.role_decl.name : "<role>",
        "role host-type lookup");

    for (size_t i = 0; i < role_decl->data.role_decl.include_count; i++) {
        ASTNode *inc = role_decl->data.role_decl.includes[i];
        char *consumer_name;

        if (inc == NULL || inc->type != AST_INCLUDE_STMT)
            continue;

        consumer_name = tc_strdup_fmt("role %s.include",
                                      role_decl->data.role_decl.name != NULL
                                          ? role_decl->data.role_decl.name : "<role>");
        if (consumer_name == NULL)
            continue;

        semantic_type_resolution_record_string_dependency(
            ctx,
            inc,
            consumer_name,
            inc->data.include_stmt.role_name,
            "role include lookup");

        if (inc->data.include_stmt.type_args != NULL) {
            for (size_t j = 0; j < inc->data.include_stmt.type_args->count; j++) {
                GenericParam *arg = inc->data.include_stmt.type_args->params[j];
                if (arg != NULL && arg->constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
                        ctx,
                        inc,
                        consumer_name,
                        "role include type-argument lookup");
                }
            }
        }
        free(consumer_name);
    }

    for (size_t i = 0; i < role_decl->data.role_decl.impl_count; i++) {
        ASTNode *impl = role_decl->data.role_decl.impl_abilities[i];
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        semantic_type_resolution_collect_type_refs(
            impl->data.impl_ability.ability_ref,
            ctx,
            impl,
            role_decl->data.role_decl.name != NULL
                ? role_decl->data.role_decl.name : "<role>",
            "role impl ability lookup");
    }
}

static void
semantic_type_resolution_precollect_action_contract(ASTNode *method,
                                                    SemanticContext *ctx,
                                                    const char *fallback_name)
{
    const char *consumer_name;

    if (method == NULL || method->type != AST_FUNC_DECL || ctx == NULL)
        return;

    consumer_name = method->data.func_decl.name != NULL
        ? method->data.func_decl.name
        : (fallback_name != NULL ? fallback_name : "<action>");

    semantic_type_resolution_collect_generic_contract_inventory(
        method->data.func_decl.generic_params,
        method->data.func_decl.where_clause,
        ctx,
        method,
        "func",
        consumer_name);

    for (size_t i = 0; i < method->data.func_decl.param_count; i++) {
        FuncParam *param = method->data.func_decl.params[i];
        char *param_consumer_name;

        if (param == NULL)
            continue;

        param_consumer_name = tc_strdup_fmt("func %s.%s",
                                            consumer_name,
                                            param->name != NULL ? param->name : "<param>");
        if (param_consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                param->type,
                ctx,
                method,
                param_consumer_name,
                "function parameter type lookup");
            free(param_consumer_name);
        }
    }

    semantic_type_resolution_collect_type_refs(
        method->data.func_decl.return_type,
        ctx,
        method,
        consumer_name,
        "function return type lookup");

    semantic_type_resolution_precollect_required_abilities(
        method->data.func_decl.required_abilities,
        method->data.func_decl.required_ability_count,
        ctx,
        method,
        consumer_name,
        "action ability consumer lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.within_zone,
        "action within-zone lookup");
    semantic_type_resolution_record_string_dependency(
        ctx,
        method,
        consumer_name,
        method->data.func_decl.causes_effect,
        "action causes-effect lookup");
}

static void
semantic_type_resolution_precollect_class_inventory(ASTNode *class_decl,
                                                    SemanticContext *ctx)
{
    if (class_decl == NULL || class_decl->type != AST_CLASS_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        class_decl->data.class_decl.generic_params,
        class_decl->data.class_decl.where_clause,
        ctx,
        class_decl,
        "class",
        class_decl->data.class_decl.name);

    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *field = class_decl->data.class_decl.fields[i];
        char *consumer_name;

        if (field == NULL)
            continue;

        consumer_name = tc_strdup_fmt("class %s.%s",
                                      class_decl->data.class_decl.name != NULL
                                          ? class_decl->data.class_decl.name : "<class>",
                                      field->name != NULL ? field->name : "<field>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                field->type,
                ctx,
                class_decl,
                consumer_name,
                "class field type lookup");
            free(consumer_name);
        }
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        semantic_type_resolution_precollect_action_contract(
            method,
            ctx,
            class_decl->data.class_decl.name);
    }
}

static void
semantic_type_resolution_precollect_party_inventory(ASTNode *party_decl,
                                                    SemanticContext *ctx)
{
    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        party_decl->data.party_decl.generic_params,
        NULL,
        ctx,
        party_decl,
        "party",
        party_decl->data.party_decl.name);

    semantic_type_resolution_collect_type_refs(
        party_decl->data.party_decl.extends,
        ctx,
        party_decl,
        party_decl->data.party_decl.name != NULL
            ? party_decl->data.party_decl.name : "<party>",
        "party extends lookup");

    for (size_t i = 0; i < party_decl->data.party_decl.shared_count; i++) {
        ASTNode *field = party_decl->data.party_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<party-shared>",
            "party shared field type lookup");
    }

    for (size_t i = 0; i < party_decl->data.party_decl.role_count; i++) {
        ASTNode *role_slot = party_decl->data.party_decl.role_slots[i];
        char *consumer_name;

        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;

        consumer_name = tc_strdup_fmt("party %s.%s",
                                      party_decl->data.party_decl.name != NULL
                                          ? party_decl->data.party_decl.name : "<party>",
                                      role_slot->data.role_slot.slot_name != NULL
                                          ? role_slot->data.role_slot.slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            role_slot->data.role_slot.required_abilities,
            role_slot->data.role_slot.ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < party_decl->data.party_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            party_decl->data.party_decl.methods[i],
            ctx,
            party_decl->data.party_decl.name);
    }
}

static void
semantic_type_resolution_precollect_roster_inventory(ASTNode *roster_decl,
                                                     SemanticContext *ctx)
{
    if (roster_decl == NULL || roster_decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_generic_contract_inventory(
        roster_decl->data.roster_decl.generic_params,
        NULL,
        ctx,
        roster_decl,
        "roster",
        roster_decl->data.roster_decl.name);

    for (size_t i = 0; i < roster_decl->data.roster_decl.shared_count; i++) {
        ASTNode *field = roster_decl->data.roster_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<roster-shared>",
            "roster shared field type lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.party_count; i++) {
        ASTNode *slot = roster_decl->data.roster_decl.party_slots[i];
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.roster_slot.slot_name != NULL
                ? slot->data.roster_slot.slot_name : "<roster-slot>",
            slot->data.roster_slot.party_type,
            "roster party lookup");
    }

    for (size_t i = 0; i < roster_decl->data.roster_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            roster_decl->data.roster_decl.methods[i],
            ctx,
            roster_decl->data.roster_decl.name);
    }
}

static void
semantic_type_resolution_precollect_world_inventory(ASTNode *world_decl,
                                                    SemanticContext *ctx)
{
    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        char *zone_slot_label;

        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;

        zone_slot_label = semantic_type_resolution_world_zone_slot_label(
            world_decl,
            zone->data.world_zone.slot_name);
        if (zone_slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, zone, zone_slot_label);
            free(zone_slot_label);
        }
    }

    for (size_t i = 0; i < world_decl->data.world_decl.shared_count; i++) {
        ASTNode *field = world_decl->data.world_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<world-shared>",
            "world shared field type lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.roster_count; i++) {
        ASTNode *roster = world_decl->data.world_decl.rosters[i];
        if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            roster,
            roster->data.world_roster.slot_name != NULL
                ? roster->data.world_roster.slot_name : "<world-roster>",
            roster->data.world_roster.roster_type,
            "world roster lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = world_decl->data.world_decl.zones[i];
        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            zone,
            zone->data.world_zone.slot_name != NULL
                ? zone->data.world_zone.slot_name : "<world-zone>",
            zone->data.world_zone.zone_type,
            "world zone lookup");
    }

    for (size_t i = 0; i < world_decl->data.world_decl.state_count; i++) {
        ASTNode *state = world_decl->data.world_decl.states[i];
        ASTNode *zone_slot_decl = NULL;
        char *state_label;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        state_label = semantic_type_resolution_world_state_label(
            world_decl,
            state->data.world_state.state_name);
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);

        if (state->data.world_state.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                state->data.world_state.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    zone_slot_label,
                    "world state zone-slot lookup");
                free(zone_slot_label);
            }
        }

        if (zone_slot_decl != NULL
            && zone_slot_decl->type == AST_WORLD_ZONE
            && zone_slot_decl->data.world_zone.zone_type != NULL
            && state->data.world_state.detail_name != NULL) {
            ASTNode *zone_type_decl = find_domain_decl_by_name(
                ctx->program_root,
                AST_ZONE_DECL,
                zone_slot_decl->data.world_zone.zone_type);
            if (zone_type_decl != NULL) {
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                    char *projection_label = semantic_type_resolution_zone_slot_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (projection_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            projection_label,
                            "world state projection lookup");
                        free(projection_label);
                    }
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                    char *layer_label = semantic_type_resolution_zone_layer_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (layer_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            layer_label,
                            "world state layer lookup");
                        free(layer_label);
                    }
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                    char *nested_state_label = semantic_type_resolution_zone_state_label(
                        zone_type_decl,
                        state->data.world_state.detail_name);
                    if (nested_state_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            zone_type_decl,
                            nested_state_label,
                            "world state nested-state lookup");
                        free(nested_state_label);
                    }
                }
            }
        }

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                char *input_state_label = semantic_type_resolution_world_state_label(
                    world_decl,
                    input_name);
                if (input_state_label != NULL) {
                    semantic_type_resolution_record_local_contract_dependency(
                        ctx,
                        state,
                        state_label,
                        NULL,
                        input_state_label,
                        "world state composition input lookup");
                    free(input_state_label);
                }

                if (input_name != NULL) {
                    char *input_zone_label = semantic_type_resolution_world_zone_slot_label(
                        world_decl,
                        input_name);
                    if (input_zone_label != NULL) {
                        semantic_type_resolution_record_local_contract_dependency(
                            ctx,
                            state,
                            state_label,
                            NULL,
                            input_zone_label,
                            "world state composition zone-input lookup");
                        free(input_zone_label);
                    }
                }
            }
        }

        free(state_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.activate_count; i++) {
        ASTNode *activate = world_decl->data.world_decl.activations[i];
        char *consumer_label;

        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;

        consumer_label = tc_strdup_fmt("world %s.activate.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       activate->data.world_activate.state_name != NULL
                                           ? activate->data.world_activate.state_name
                                           : (activate->data.world_activate.zone_slot_name != NULL
                                               ? activate->data.world_activate.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (activate->data.world_activate.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                activate->data.world_activate.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    activate,
                    consumer_label,
                    NULL,
                    state_label,
                    "world activate state lookup");
                free(state_label);
            }
        } else if (activate->data.world_activate.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                activate->data.world_activate.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    activate,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world activate zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.deactivate_count; i++) {
        ASTNode *deactivate = world_decl->data.world_decl.deactivations[i];
        char *consumer_label;

        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;

        consumer_label = tc_strdup_fmt("world %s.deactivate.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       deactivate->data.world_deactivate.state_name != NULL
                                           ? deactivate->data.world_deactivate.state_name
                                           : (deactivate->data.world_deactivate.zone_slot_name != NULL
                                               ? deactivate->data.world_deactivate.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (deactivate->data.world_deactivate.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                deactivate->data.world_deactivate.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    deactivate,
                    consumer_label,
                    NULL,
                    state_label,
                    "world deactivate state lookup");
                free(state_label);
            }
        } else if (deactivate->data.world_deactivate.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                deactivate->data.world_deactivate.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    deactivate,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world deactivate zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.maintained_zone_count; i++) {
        ASTNode *maintain = world_decl->data.world_decl.maintained_zones[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;

        consumer_label = tc_strdup_fmt("world %s.maintain.%s",
                                       world_decl->data.world_decl.name != NULL
                                           ? world_decl->data.world_decl.name : "<world>",
                                       maintain->data.world_maintain.state_name != NULL
                                           ? maintain->data.world_maintain.state_name
                                           : (maintain->data.world_maintain.zone_slot_name != NULL
                                               ? maintain->data.world_maintain.zone_slot_name
                                               : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (maintain->data.world_maintain.state_name != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                maintain->data.world_maintain.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "world maintain state lookup");
                free(state_label);
            }
        } else if (maintain->data.world_maintain.zone_slot_name != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                maintain->data.world_maintain.zone_slot_name);
            if (zone_slot_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    zone_slot_label,
                    "world maintain zone lookup");
                free(zone_slot_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < world_decl->data.world_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            world_decl->data.world_decl.methods[i],
            ctx,
            world_decl->data.world_decl.name);
    }
}

static void
semantic_type_resolution_precollect_domain_inventory(ASTNode *domain_decl,
                                                     ASTNode **slots,
                                                     size_t slot_count,
                                                     ASTNode **shared_fields,
                                                     size_t shared_count,
                                                     ASTNode **methods,
                                                     size_t method_count,
                                                     SemanticContext *ctx,
                                                     const char *kind_name,
                                                     const char *decl_name,
                                                     const char *slot_reason,
                                                     const char *shared_reason)
{
    (void)domain_decl;

    if (ctx == NULL)
        return;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        semantic_type_resolution_collect_type_refs(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name : "<domain-slot>",
            slot_reason != NULL ? slot_reason : "domain slot type lookup");
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<domain-shared>",
            shared_reason != NULL ? shared_reason : "domain shared field type lookup");
    }

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            decl_name != NULL ? decl_name : kind_name);
    }
}

static void
semantic_type_resolution_precollect_relation_inventory(ASTNode *relation_decl,
                                                       SemanticContext *ctx)
{
    if (relation_decl == NULL || relation_decl->type != AST_RELATION_DECL || ctx == NULL)
        return;

    semantic_type_resolution_collect_type_refs(
        relation_decl->data.relation_decl.between_left_type,
        ctx,
        relation_decl,
        relation_decl->data.relation_decl.name != NULL
            ? relation_decl->data.relation_decl.name : "<relation>",
        "relation between-left type lookup");
    semantic_type_resolution_collect_type_refs(
        relation_decl->data.relation_decl.between_right_type,
        ctx,
        relation_decl,
        relation_decl->data.relation_decl.name != NULL
            ? relation_decl->data.relation_decl.name : "<relation>",
        "relation between-right type lookup");

    semantic_type_resolution_precollect_domain_inventory(
        relation_decl,
        relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.shared_fields,
        relation_decl->data.relation_decl.shared_count,
        relation_decl->data.relation_decl.methods,
        relation_decl->data.relation_decl.method_count,
        ctx,
        "relation",
        relation_decl->data.relation_decl.name,
        "relation slot type lookup",
        "relation shared field type lookup");
}

static void
semantic_type_resolution_precollect_effect_inventory(ASTNode *effect_decl,
                                                     SemanticContext *ctx)
{
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL || ctx == NULL)
        return;

    semantic_type_resolution_precollect_domain_inventory(
        effect_decl,
        effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.shared_fields,
        effect_decl->data.effect_decl.shared_count,
        effect_decl->data.effect_decl.methods,
        effect_decl->data.effect_decl.method_count,
        ctx,
        "effect",
        effect_decl->data.effect_decl.name,
        "effect slot type lookup",
        "effect shared field type lookup");
}

static void
semantic_type_resolution_precollect_intent_inventory(ASTNode *intent_decl,
                                                     SemanticContext *ctx)
{
    if (intent_decl == NULL || intent_decl->type != AST_INTENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < intent_decl->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent_decl->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        semantic_type_resolution_collect_type_refs(
            involves->data.intent_involves.subject_type,
            ctx,
            involves,
            involves->data.intent_involves.alias != NULL
                ? involves->data.intent_involves.alias : "<intent-binding>",
            "intent involves type lookup");
    }

    for (size_t i = 0; i < intent_decl->data.intent_decl.value_count; i++) {
        ASTNode *value = intent_decl->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        semantic_type_resolution_collect_type_refs(
            value->data.intent_value.value_type,
            ctx,
            value,
            value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "<intent-value>",
            "intent value type lookup");
    }

    semantic_type_resolution_collect_type_refs(
        intent_decl->data.intent_decl.default_where_type,
        ctx,
        intent_decl,
        intent_decl->data.intent_decl.name != NULL
            ? intent_decl->data.intent_decl.name : "<intent>",
        "intent default where-type lookup");

    for (size_t i = 0; i < intent_decl->data.intent_decl.step_count; i++) {
        ASTNode *step = intent_decl->data.intent_decl.steps[i];
        char *step_consumer_name;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        step_consumer_name = tc_strdup_fmt(
            "intent %s.%s",
            intent_decl->data.intent_decl.name != NULL
                ? intent_decl->data.intent_decl.name : "<intent>",
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>");
        if (step_consumer_name == NULL)
            continue;

        semantic_type_resolution_collect_type_refs(
            step->data.intent_step.where_type,
            ctx,
            step,
            step_consumer_name,
            "intent step where-type lookup");
        semantic_type_resolution_precollect_required_abilities(
            step->data.intent_step.required_abilities,
            step->data.intent_step.required_ability_count,
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        semantic_type_resolution_record_string_dependency(
            ctx,
            step,
            step_consumer_name,
            step->data.intent_step.causes_effect,
            "intent step causes-effect lookup");
        free(step_consumer_name);
    }
}

static void
semantic_type_resolution_precollect_zone_inventory(ASTNode *zone_decl,
                                                   SemanticContext *ctx)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        char *slot_label;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;

        slot_label = semantic_type_resolution_zone_slot_label(
            zone_decl,
            slot->data.domain_slot.slot_name);
        if (slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, slot_label);
            free(slot_label);
        }

        semantic_type_resolution_collect_type_refs(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name : "<zone-slot>",
            "zone slot type lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.shared_count; i++) {
        ASTNode *field = zone_decl->data.zone_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name != NULL
                ? field->data.party_shared.name : "<zone-shared>",
            "zone shared field type lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.layer_slots[i];
        char *layer_label;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT)
            continue;
        layer_label = semantic_type_resolution_zone_layer_label(
            zone_decl,
            slot->data.zone_layer_slot.slot_name);
        if (layer_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, layer_label);
            free(layer_label);
        }
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            slot->data.zone_layer_slot.slot_name != NULL
                ? slot->data.zone_layer_slot.slot_name : "<zone-layer>",
            slot->data.zone_layer_slot.layer_type,
            "zone layer lookup");
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.refresh_count; i++) {
        ASTNode *refresh = zone_decl->data.zone_decl.refreshes[i];
        char *consumer_label;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.refresh.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       refresh->data.zone_refresh.object_slot_name != NULL
                                           ? refresh->data.zone_refresh.object_slot_name : "<refresh>");
        if (consumer_label == NULL)
            continue;

        if (refresh->data.zone_refresh.object_slot_name != NULL) {
            char *object_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                refresh->data.zone_refresh.object_slot_name);
            if (object_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    NULL,
                    object_label,
                    "zone refresh target-slot lookup");
                free(object_label);
            }
        }

        if (refresh->data.zone_refresh.source_slot_name != NULL) {
            char *source_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                refresh->data.zone_refresh.source_slot_name);
            if (source_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    NULL,
                    source_label,
                    "zone refresh source-slot lookup");
                free(source_label);
            }
        }

        for (size_t map_i = 0; map_i < refresh->data.zone_refresh.field_map_count; map_i++) {
            const char *target_field = refresh->data.zone_refresh.mapped_target_fields != NULL
                ? refresh->data.zone_refresh.mapped_target_fields[map_i] : NULL;
            const char *source_field = refresh->data.zone_refresh.mapped_source_fields != NULL
                ? refresh->data.zone_refresh.mapped_source_fields[map_i] : NULL;
            char *projection_label = semantic_type_resolution_projection_path_label(
                zone_decl,
                refresh->data.zone_refresh.object_slot_name,
                refresh->data.zone_refresh.source_slot_name,
                target_field,
                source_field);
            if (projection_label != NULL) {
                (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                  TYPE_RES_NODE_PROJECTION_PATH,
                                                  refresh,
                                                  projection_label);
                semantic_type_resolution_record_named_dependency(
                    ctx,
                    refresh,
                    consumer_label,
                    TYPE_RES_NODE_PROJECTION_PATH,
                    refresh,
                    projection_label,
                    "zone refresh projection-path lookup");

                if (refresh->data.zone_refresh.object_slot_name != NULL) {
                    char *target_slot_label = semantic_type_resolution_zone_slot_label(
                        zone_decl,
                        refresh->data.zone_refresh.object_slot_name);
                    if (target_slot_label != NULL) {
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_LOCAL_CONTRACT,
                            refresh,
                            target_slot_label,
                            "projection target-slot carrier");
                        free(target_slot_label);
                    }
                }

                if (refresh->data.zone_refresh.source_slot_name != NULL) {
                    char *source_slot_label = semantic_type_resolution_zone_slot_label(
                        zone_decl,
                        refresh->data.zone_refresh.source_slot_name);
                    if (source_slot_label != NULL) {
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_LOCAL_CONTRACT,
                            refresh,
                            source_slot_label,
                            "projection source-slot carrier");
                        free(source_slot_label);
                    }
                }

                if (target_field != NULL) {
                    char *target_field_label = semantic_type_resolution_projection_slot_field_label(
                        zone_decl,
                        refresh->data.zone_refresh.object_slot_name,
                        target_field);
                    if (target_field_label != NULL) {
                        (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                          TYPE_RES_NODE_PROJECTION_PATH,
                                                          refresh,
                                                          target_field_label);
                        semantic_type_resolution_record_named_dependency(
                            ctx,
                            refresh,
                            projection_label,
                            TYPE_RES_NODE_PROJECTION_PATH,
                            refresh,
                            target_field_label,
                            "projection target field-path lookup");
                        free(target_field_label);
                    }
                }

                if (source_field != NULL) {
                    ASTNode *source_decl = semantic_type_resolution_projection_source_decl(
                        zone_decl,
                        refresh->data.zone_refresh.source_slot_name,
                        ctx);
                    char *resolved_source_path = NULL;
                    const char *source_path_text = source_field;

                    if (source_decl != NULL) {
                        size_t saved_diag = ctx->diagnostic_count;
                        bool saved_error = ctx->has_error;
                        Type *field_type = NULL;
                        int path_status = resolve_projection_source_field_path(
                            ctx->program_root,
                            source_decl,
                            source_field,
                            ctx,
                            &resolved_source_path,
                            &field_type);
                        (void)field_type;
                        if (ctx->diagnostic_count > saved_diag) {
                            ctx->diagnostic_count = saved_diag;
                            ctx->has_error = saved_error;
                        }
                        if (path_status == 1 && resolved_source_path != NULL)
                            source_path_text = resolved_source_path;
                    }

                    if (source_path_text != NULL) {
                        char *source_field_label = semantic_type_resolution_projection_slot_field_label(
                            zone_decl,
                            refresh->data.zone_refresh.source_slot_name,
                            source_path_text);
                        if (source_field_label != NULL) {
                            (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                              TYPE_RES_NODE_PROJECTION_PATH,
                                                              refresh,
                                                              source_field_label);
                            semantic_type_resolution_record_named_dependency(
                                ctx,
                                refresh,
                                projection_label,
                                TYPE_RES_NODE_PROJECTION_PATH,
                                refresh,
                                source_field_label,
                                "projection source field-path lookup");
                            free(source_field_label);
                        }
                    }

                    if (resolved_source_path != NULL)
                        free(resolved_source_path);
                }

                free(projection_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.apply_count; i++) {
        ASTNode *apply = zone_decl->data.zone_decl.applies[i];
        char *consumer_label;

        if (apply == NULL || apply->type != AST_ZONE_APPLY)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.apply.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       apply->data.zone_apply.effect_slot_name != NULL
                                           ? apply->data.zone_apply.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (apply->data.zone_apply.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                apply->data.zone_apply.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone apply effect-slot lookup");
                free(layer_label);
            }
        }
        if (apply->data.zone_apply.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                apply->data.zone_apply.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone apply target-slot lookup");
                free(target_label);
            }
        }
        if (apply->data.zone_apply.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                apply->data.zone_apply.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    apply,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone apply state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.link_count; i++) {
        ASTNode *link = zone_decl->data.zone_decl.links[i];
        char *consumer_label;

        if (link == NULL || link->type != AST_ZONE_LINK)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.link.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       link->data.zone_link.relation_slot_name != NULL
                                           ? link->data.zone_link.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (link->data.zone_link.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                link->data.zone_link.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone link relation-slot lookup");
                free(layer_label);
            }
        }
        if (link->data.zone_link.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                link->data.zone_link.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone link left-slot lookup");
                free(left_label);
            }
        }
        if (link->data.zone_link.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                link->data.zone_link.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone link right-slot lookup");
                free(right_label);
            }
        }
        if (link->data.zone_link.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                link->data.zone_link.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    link,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone link state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.detach_count; i++) {
        ASTNode *detach = zone_decl->data.zone_decl.detaches[i];
        char *consumer_label;

        if (detach == NULL || detach->type != AST_ZONE_DETACH)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.detach.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       detach->data.zone_detach.effect_slot_name != NULL
                                           ? detach->data.zone_detach.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (detach->data.zone_detach.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                detach->data.zone_detach.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone detach effect-slot lookup");
                free(layer_label);
            }
        }
        if (detach->data.zone_detach.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                detach->data.zone_detach.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone detach target-slot lookup");
                free(target_label);
            }
        }
        if (detach->data.zone_detach.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                detach->data.zone_detach.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    detach,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone detach state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = zone_decl->data.zone_decl.unlinks[i];
        char *consumer_label;

        if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.unlink.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       unlink->data.zone_unlink.relation_slot_name != NULL
                                           ? unlink->data.zone_unlink.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (unlink->data.zone_unlink.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                unlink->data.zone_unlink.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone unlink relation-slot lookup");
                free(layer_label);
            }
        }
        if (unlink->data.zone_unlink.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                unlink->data.zone_unlink.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone unlink left-slot lookup");
                free(left_label);
            }
        }
        if (unlink->data.zone_unlink.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                unlink->data.zone_unlink.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone unlink right-slot lookup");
                free(right_label);
            }
        }
        if (unlink->data.zone_unlink.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                unlink->data.zone_unlink.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    unlink,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone unlink state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_effects[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-effect.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_effect.effect_slot_name != NULL
                                           ? maintain->data.zone_maintain_effect.effect_slot_name : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_effect.effect_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                maintain->data.zone_maintain_effect.effect_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone maintain-effect slot lookup");
                free(layer_label);
            }
        }
        if (maintain->data.zone_maintain_effect.target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_effect.target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    target_label,
                    "zone maintain-effect target-slot lookup");
                free(target_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_relations[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-relation.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_relation.relation_slot_name != NULL
                                           ? maintain->data.zone_maintain_relation.relation_slot_name : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_relation.relation_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                maintain->data.zone_maintain_relation.relation_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    layer_label,
                    "zone maintain-relation slot lookup");
                free(layer_label);
            }
        }
        if (maintain->data.zone_maintain_relation.left_slot_name != NULL) {
            char *left_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_relation.left_slot_name);
            if (left_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    left_label,
                    "zone maintain-relation left-slot lookup");
                free(left_label);
            }
        }
        if (maintain->data.zone_maintain_relation.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                maintain->data.zone_maintain_relation.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    right_label,
                    "zone maintain-relation right-slot lookup");
                free(right_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.state_count; i++) {
        ASTNode *state = zone_decl->data.zone_decl.states[i];
        char *state_label;

        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;

        state_label = semantic_type_resolution_zone_state_label(
            zone_decl,
            state->data.zone_state.state_name);
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        if (state->data.zone_state.layer_slot_name != NULL) {
            char *layer_label = semantic_type_resolution_zone_layer_label(
                zone_decl,
                state->data.zone_state.layer_slot_name);
            if (layer_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    layer_label,
                    "zone state layer lookup");
                free(layer_label);
            }
        }

        if (state->data.zone_state.left_or_target_slot_name != NULL) {
            char *target_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                state->data.zone_state.left_or_target_slot_name);
            if (target_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    target_label,
                    "zone state target-slot lookup");
                free(target_label);
            }
        }

        if (state->data.zone_state.is_relation
            && state->data.zone_state.right_slot_name != NULL) {
            char *right_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                state->data.zone_state.right_slot_name);
            if (right_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    state,
                    state_label,
                    NULL,
                    right_label,
                    "zone state right-slot lookup");
                free(right_label);
            }
        }
        free(state_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = zone_decl->data.zone_decl.maintained_states[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;

        consumer_label = tc_strdup_fmt("zone %s.maintain-state.%s",
                                       zone_decl->data.zone_decl.name != NULL
                                           ? zone_decl->data.zone_decl.name : "<zone>",
                                       maintain->data.zone_maintain_state.state_name != NULL
                                           ? maintain->data.zone_maintain_state.state_name
                                           : "<state>");
        if (consumer_label == NULL)
            continue;

        if (maintain->data.zone_maintain_state.state_name != NULL) {
            char *state_label = semantic_type_resolution_zone_state_label(
                zone_decl,
                maintain->data.zone_maintain_state.state_name);
            if (state_label != NULL) {
                semantic_type_resolution_record_local_contract_dependency(
                    ctx,
                    maintain,
                    consumer_label,
                    NULL,
                    state_label,
                    "zone maintain-state lookup");
                free(state_label);
            }
        }
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone_decl->data.zone_decl.authorities[i];
        char *consumer_name;

        if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
            continue;

        consumer_name = tc_strdup_fmt("zone %s.%s",
                                      zone_decl->data.zone_decl.name != NULL
                                          ? zone_decl->data.zone_decl.name : "<zone>",
                                      authority->data.zone_authority.subject_slot_name != NULL
                                          ? authority->data.zone_authority.subject_slot_name
                                          : "<authority>");
        if (consumer_name == NULL)
            continue;
        semantic_type_resolution_precollect_required_abilities(
            authority->data.zone_authority.required_abilities,
            authority->data.zone_authority.ability_count,
            ctx,
            authority,
            consumer_name,
            "zone authority ability consumer lookup");
        free(consumer_name);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            zone_decl->data.zone_decl.methods[i],
            ctx,
            zone_decl->data.zone_decl.name);
    }
}

static void
semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                      const ASTNode *site,
                                                      const char *label)
{
    if (ctx == NULL || label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      TYPE_RES_NODE_LOCAL_CONTRACT,
                                      site,
                                      label);
}

static void
semantic_type_resolution_record_local_contract_dependency(SemanticContext *ctx,
                                                          const ASTNode *consumer_site,
                                                          const char *consumer_label,
                                                          const ASTNode *provider_site,
                                                          const char *provider_label,
                                                          const char *reason)
{
    if (ctx == NULL || provider_label == NULL || provider_label[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_label,
        TYPE_RES_NODE_LOCAL_CONTRACT,
        provider_site,
        provider_label,
        reason);
}

static char *
semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                               const char *slot_name)
{
    return tc_strdup_fmt("world %s.zone.%s",
                         world_decl != NULL
                             && world_decl->type == AST_WORLD_DECL
                             && world_decl->data.world_decl.name != NULL
                                 ? world_decl->data.world_decl.name : "<world>",
                         slot_name != NULL ? slot_name : "<zone-slot>");
}

static char *
semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                           const char *state_name)
{
    return tc_strdup_fmt("world %s.state.%s",
                         world_decl != NULL
                             && world_decl->type == AST_WORLD_DECL
                             && world_decl->data.world_decl.name != NULL
                                 ? world_decl->data.world_decl.name : "<world>",
                         state_name != NULL ? state_name : "<state>");
}

static char *
semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                         const char *slot_name)
{
    return tc_strdup_fmt("zone %s.slot.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<slot>");
}

static char *
semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                          const char *slot_name)
{
    return tc_strdup_fmt("zone %s.layer.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<layer>");
}

static char *
semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                          const char *state_name)
{
    return tc_strdup_fmt("zone %s.state.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         state_name != NULL ? state_name : "<state>");
}

static char *
semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                               const char *target_slot_name,
                                               const char *source_slot_name,
                                               const char *target_field_name,
                                               const char *source_field_name)
{
    return tc_strdup_fmt("zone %s.projection.%s.%s<-%s.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         target_slot_name != NULL ? target_slot_name : "<target-slot>",
                         target_field_name != NULL ? target_field_name : "<target-field>",
                         source_slot_name != NULL ? source_slot_name : "<source-slot>",
                         source_field_name != NULL ? source_field_name : "<source-field>");
}

static char *
semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                     const char *slot_name,
                                                     const char *field_path)
{
    return tc_strdup_fmt("zone %s.slot.%s.field.%s",
                         zone_decl != NULL
                             && zone_decl->type == AST_ZONE_DECL
                             && zone_decl->data.zone_decl.name != NULL
                                 ? zone_decl->data.zone_decl.name : "<zone>",
                         slot_name != NULL ? slot_name : "<slot>",
                         field_path != NULL ? field_path : "<field-path>");
}

static ASTNode *
semantic_type_resolution_projection_source_decl(ASTNode *zone_decl,
                                                const char *slot_name,
                                                SemanticContext *ctx)
{
    ASTNode *slot;
    ASTNode *type_node;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || slot_name == NULL || ctx == NULL) {
        return NULL;
    }

    slot = find_zone_domain_slot(zone_decl, slot_name);
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return NULL;

    type_node = slot->data.domain_slot.type;
    if (type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return NULL;
    }

    return find_type_decl_by_name(ctx->program_root, type_node->data.type.name);
}

static void
semantic_type_resolution_register_top_level_decl(ASTNode *stmt,
                                                 SemanticContext *ctx)
{
    const char *label = NULL;
    TypeResolutionNodeKind kind = TYPE_RES_NODE_DECL;

    if (stmt == NULL || ctx == NULL)
        return;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        label = stmt->data.type_alias.name;
        kind = TYPE_RES_NODE_ALIAS;
        break;
    case AST_CLASS_DECL:
        label = stmt->data.class_decl.name;
        break;
    case AST_FUNC_DECL:
        label = stmt->data.func_decl.name;
        break;
    case AST_EVENT_DECL:
        label = stmt->data.event_decl.name;
        break;
    case AST_ENUM_DECL:
        label = stmt->data.enum_decl.name;
        break;
    case AST_ABILITY_DECL:
        label = stmt->data.ability_decl.name;
        break;
    case AST_ROLE_DECL:
        label = stmt->data.role_decl.name;
        break;
    case AST_PARTY_DECL:
        label = stmt->data.party_decl.name;
        break;
    case AST_ROSTER_DECL:
        label = stmt->data.roster_decl.name;
        break;
    case AST_WORLD_DECL:
        label = stmt->data.world_decl.name;
        break;
    case AST_INTENT_DECL:
        label = stmt->data.intent_decl.name;
        break;
    case AST_RELATION_DECL:
        label = stmt->data.relation_decl.name;
        break;
    case AST_EFFECT_DECL:
        label = stmt->data.effect_decl.name;
        break;
    case AST_ZONE_DECL:
        label = stmt->data.zone_decl.name;
        break;
    default:
        return;
    }

    if (label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      kind,
                                      stmt,
                                      label);
}

static void
semantic_type_resolution_precollect_program(ASTNode *program,
                                            SemanticContext *ctx)
{
    if (program == NULL || ctx == NULL || program->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL)
            continue;

        semantic_type_resolution_register_top_level_decl(stmt, ctx);

        switch (stmt->type) {
        case AST_TYPE_ALIAS:
            if (stmt->data.type_alias.name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    stmt->data.type_alias.target_type,
                    ctx,
                    stmt,
                    stmt->data.type_alias.name,
                    "type-alias target lookup");
            }
            break;

        case AST_CLASS_DECL:
            semantic_type_resolution_precollect_class_inventory(stmt, ctx);
            break;

        case AST_FUNC_DECL:
            semantic_type_resolution_precollect_action_contract(
                stmt,
                ctx,
                stmt->data.func_decl.name);
            break;

        case AST_EVENT_DECL:
            semantic_type_resolution_precollect_event_inventory(stmt, ctx);
            break;

        case AST_ENUM_DECL:
            semantic_type_resolution_precollect_enum_inventory(stmt, ctx);
            break;

        case AST_ABILITY_DECL:
            semantic_type_resolution_precollect_ability_inventory(stmt, ctx);
            break;

        case AST_ROLE_DECL:
            semantic_type_resolution_precollect_role_inventory(stmt, ctx);
            break;

        case AST_PARTY_DECL:
            semantic_type_resolution_precollect_party_inventory(stmt, ctx);
            break;

        case AST_ROSTER_DECL:
            semantic_type_resolution_precollect_roster_inventory(stmt, ctx);
            break;

        case AST_WORLD_DECL:
            semantic_type_resolution_precollect_world_inventory(stmt, ctx);
            break;

        case AST_INTENT_DECL:
            semantic_type_resolution_precollect_intent_inventory(stmt, ctx);
            break;

        case AST_ZONE_DECL:
            semantic_type_resolution_precollect_zone_inventory(stmt, ctx);
            break;

        case AST_RELATION_DECL:
            semantic_type_resolution_precollect_relation_inventory(stmt, ctx);
            break;

        case AST_EFFECT_DECL:
            semantic_type_resolution_precollect_effect_inventory(stmt, ctx);
            break;

        default:
            break;
        }
    }
}

static const char *
