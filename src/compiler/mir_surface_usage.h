#ifndef PERGYRA_MIR_SURFACE_USAGE_H
#define PERGYRA_MIR_SURFACE_USAGE_H

static bool
mir_ast_array_uses_thread_pool_surface(ASTNode *const *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_uses_thread_pool_surface(nodes[i]))
            return true;
    }
    return false;
}

static bool
mir_ast_array_uses_intent_observability_surface(ASTNode *const *nodes,
                                                size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_uses_intent_observability_surface(nodes[i]))
            return true;
    }
    return false;
}

static bool
mir_inventory_uses_thread_pool_surface(const MIRProgram *mir)
{
    if (mir == NULL)
        return false;

    return mir_ast_array_uses_thread_pool_surface(mir->types, mir->type_count)
        || mir_ast_array_uses_thread_pool_surface(mir->abilities, mir->ability_count)
        || mir_ast_array_uses_thread_pool_surface(mir->roles, mir->role_count)
        || mir_ast_array_uses_thread_pool_surface(mir->parties, mir->party_count)
        || mir_ast_array_uses_thread_pool_surface(mir->rosters, mir->roster_count)
        || mir_ast_array_uses_thread_pool_surface(mir->worlds, mir->world_count)
        || mir_ast_array_uses_thread_pool_surface(mir->relations, mir->relation_count)
        || mir_ast_array_uses_thread_pool_surface(mir->effects, mir->effect_count)
        || mir_ast_array_uses_thread_pool_surface(mir->zones, mir->zone_count)
        || mir_ast_array_uses_thread_pool_surface(mir->events, mir->event_count)
        || mir_ast_array_uses_thread_pool_surface(mir->intents, mir->intent_count)
        || mir_ast_array_uses_thread_pool_surface(mir->functions, mir->function_count)
        || mir_ast_array_uses_thread_pool_surface(mir->externs, mir->extern_count);
}

static bool
mir_inventory_uses_intent_observability_surface(const MIRProgram *mir)
{
    if (mir == NULL)
        return false;

    return mir_ast_array_uses_intent_observability_surface(mir->types, mir->type_count)
        || mir_ast_array_uses_intent_observability_surface(mir->abilities, mir->ability_count)
        || mir_ast_array_uses_intent_observability_surface(mir->roles, mir->role_count)
        || mir_ast_array_uses_intent_observability_surface(mir->parties, mir->party_count)
        || mir_ast_array_uses_intent_observability_surface(mir->rosters, mir->roster_count)
        || mir_ast_array_uses_intent_observability_surface(mir->worlds, mir->world_count)
        || mir_ast_array_uses_intent_observability_surface(mir->relations, mir->relation_count)
        || mir_ast_array_uses_intent_observability_surface(mir->effects, mir->effect_count)
        || mir_ast_array_uses_intent_observability_surface(mir->zones, mir->zone_count)
        || mir_ast_array_uses_intent_observability_surface(mir->events, mir->event_count)
        || mir_ast_array_uses_intent_observability_surface(mir->intents, mir->intent_count)
        || mir_ast_array_uses_intent_observability_surface(mir->functions, mir->function_count)
        || mir_ast_array_uses_intent_observability_surface(mir->externs, mir->extern_count);
}

static void
mir_program_record_inventory_surface_usage(MIRProgram *mir)
{
    if (mir == NULL)
        return;

    mir->has_inventory_surface_usage_facts = true;
    mir->inventory_uses_thread_pool_surface =
        mir_inventory_uses_thread_pool_surface(mir);
    mir->inventory_uses_intent_observability_surface =
        mir_inventory_uses_intent_observability_surface(mir);
}

#endif /* PERGYRA_MIR_SURFACE_USAGE_H */
