#include "type_checker_internal.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

ASTNode *
semantic_stage_domain_find_zone_decl(SemanticContext *ctx,
                                     const char *zone_name)
{
    if (ctx == NULL || ctx->program_root == NULL || zone_name == NULL)
        return NULL;
    return find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL, zone_name);
}

ASTNode *
semantic_world_find_zone_slot_local(ASTNode *world, const char *slot_name)
{
    ASTNode **zones;
    size_t zone_count;

    if (world == NULL || world->type != AST_WORLD_DECL || slot_name == NULL)
        return NULL;

    zones = ast_world_zones(world, &zone_count);
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        const char *zone_slot_name = ast_world_zone_slot_name(zone);
        if (zone != NULL
            && zone->type == AST_WORLD_ZONE
            && zone_slot_name != NULL
            && strcmp(zone_slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

ASTNode *
semantic_world_find_state_local(ASTNode *world, const char *state_name)
{
    ASTNode **states;
    size_t state_count;

    if (world == NULL || world->type != AST_WORLD_DECL || state_name == NULL)
        return NULL;

    states = ast_world_states(world, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL
            && state->type == AST_WORLD_STATE
            && ast_world_state_name(state) != NULL
            && strcmp(ast_world_state_name(state), state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

ASTNode *
semantic_zone_find_layer_slot_local(ASTNode *zone, const char *slot_name)
{
    ASTNode **layer_slots;
    size_t layer_slot_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || slot_name == NULL)
        return NULL;

    layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && ast_zone_layer_slot_name(slot) != NULL
            && strcmp(ast_zone_layer_slot_name(slot), slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

ASTNode *
semantic_zone_find_state_local(ASTNode *zone, const char *state_name)
{
    ASTNode **states;
    size_t state_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || state_name == NULL)
        return NULL;

    states = ast_zone_states(zone, &state_count);
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && ast_zone_state_name(state) != NULL
            && strcmp(ast_zone_state_name(state), state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

void
semantic_stage_world_local_contracts(ASTNode *world_decl,
                                     SemanticContext *ctx)
{
    ASTNode **states;
    ASTNode **activations;
    ASTNode **deactivations;
    ASTNode **maintained_zones;
    size_t state_count;
    size_t activate_count;
    size_t deactivate_count;
    size_t maintained_zone_count;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    states = ast_world_states(world_decl, &state_count);
    activations = ast_world_activations(world_decl, &activate_count);
    deactivations = ast_world_deactivations(world_decl, &deactivate_count);
    maintained_zones = ast_world_maintained_zones(world_decl,
                                                  &maintained_zone_count);

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        ASTNode *zone_slot_decl = NULL;
        ASTNode *zone_decl = NULL;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            ast_world_state_zone_slot_name(state));
        if (zone_slot_decl != NULL) {
            zone_decl = semantic_stage_domain_find_zone_decl(
                ctx,
                ast_world_zone_type_name(zone_slot_decl));
        }

        switch (ast_world_state_source_kind(state)) {
        case WORLD_STATE_SOURCE_ZONE:
            (void)zone_slot_decl;
            break;

        case WORLD_STATE_SOURCE_PROJECTION:
            if (zone_decl != NULL && ast_world_state_detail_name(state) != NULL)
                (void)find_zone_domain_slot(zone_decl, ast_world_state_detail_name(state));
            break;

        case WORLD_STATE_SOURCE_LAYER:
            if (zone_decl != NULL && ast_world_state_detail_name(state) != NULL)
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_world_state_detail_name(state));
            break;

        case WORLD_STATE_SOURCE_STATE:
            if (zone_decl != NULL && ast_world_state_detail_name(state) != NULL)
                (void)semantic_zone_find_state_local(zone_decl,
                    ast_world_state_detail_name(state));
            break;

        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY:
            for (size_t j = 0; j < ast_world_state_input_count(state); j++) {
                const char *input_name = ast_world_state_input_name(state, j);
                if (semantic_world_find_state_local(world_decl, input_name) == NULL)
                    (void)semantic_world_find_zone_slot_local(world_decl, input_name);
            }
            break;
        }
    }

    for (size_t i = 0; i < activate_count; i++) {
        ASTNode *activate = activations[i];
        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;
        if (ast_world_directive_state_name(activate) != NULL)
            (void)semantic_world_find_state_local(world_decl,
                ast_world_directive_state_name(activate));
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                ast_world_directive_zone_slot_name(activate));
    }

    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *deactivate = deactivations[i];
        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;
        if (ast_world_directive_state_name(deactivate) != NULL)
            (void)semantic_world_find_state_local(world_decl,
                ast_world_directive_state_name(deactivate));
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                ast_world_directive_zone_slot_name(deactivate));
    }

    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *maintain = maintained_zones[i];
        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;
        if (ast_world_directive_state_name(maintain) != NULL)
            (void)semantic_world_find_state_local(world_decl,
                ast_world_directive_state_name(maintain));
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                ast_world_directive_zone_slot_name(maintain));
    }
}

void
semantic_stage_zone_local_contracts(ASTNode *zone_decl)
{
    ASTNode **refreshes;
    ASTNode **states;
    ASTNode **maintained_states;
    size_t refresh_count;
    size_t state_count;
    size_t maintained_state_count;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;

    refreshes = ast_zone_refreshes(zone_decl, &refresh_count);
    states = ast_zone_states(zone_decl, &state_count);
    maintained_states = ast_zone_maintained_states(zone_decl,
                                                   &maintained_state_count);

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;
        (void)find_zone_domain_slot(zone_decl, ast_zone_refresh_object_slot_name(refresh));
        (void)find_zone_domain_slot(zone_decl, ast_zone_refresh_source_slot_name(refresh));
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            ast_zone_state_layer_slot_name(state));
        (void)find_zone_domain_slot(zone_decl,
            ast_zone_state_left_or_target_slot_name(state));
        if (ast_zone_state_is_relation(state))
            (void)find_zone_domain_slot(zone_decl,
                ast_zone_state_right_slot_name(state));
    }

    for (size_t i = 0; i < maintained_state_count; i++) {
        ASTNode *maintain = maintained_states[i];
        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;
        (void)semantic_zone_find_state_local(zone_decl,
            ast_zone_directive_state_name(maintain));
    }
}
