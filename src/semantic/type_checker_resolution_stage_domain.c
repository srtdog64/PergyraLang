#include "type_checker_internal.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static ASTNode *
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
        if (zone != NULL
            && zone->type == AST_WORLD_ZONE
            && zone->data.world_zone.slot_name != NULL
            && strcmp(zone->data.world_zone.slot_name, slot_name) == 0) {
            return zone;
        }
    }

    return NULL;
}

static ASTNode *
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
            && state->data.world_state.state_name != NULL
            && strcmp(state->data.world_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

static ASTNode *
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
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static ASTNode *
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
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
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
            state->data.world_state.zone_slot_name);
        if (zone_slot_decl != NULL) {
            zone_decl = semantic_stage_domain_find_zone_decl(
                ctx,
                zone_slot_decl->data.world_zone.zone_type);
        }

        switch (state->data.world_state.source_kind) {
        case WORLD_STATE_SOURCE_ZONE:
            (void)zone_slot_decl;
            break;

        case WORLD_STATE_SOURCE_PROJECTION:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)find_zone_domain_slot(zone_decl, state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_LAYER:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_STATE:
            if (zone_decl != NULL && state->data.world_state.detail_name != NULL)
                (void)semantic_zone_find_state_local(zone_decl,
                    state->data.world_state.detail_name);
            break;

        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY:
            for (size_t j = 0; j < state->data.world_state.input_count; j++) {
                const char *input_name = state->data.world_state.input_names[j];
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
        if (activate->data.world_activate.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                activate->data.world_activate.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                activate->data.world_activate.zone_slot_name);
    }

    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *deactivate = deactivations[i];
        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;
        if (deactivate->data.world_deactivate.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                deactivate->data.world_deactivate.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                deactivate->data.world_deactivate.zone_slot_name);
    }

    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *maintain = maintained_zones[i];
        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;
        if (maintain->data.world_maintain.state_name != NULL)
            (void)semantic_world_find_state_local(world_decl,
                maintain->data.world_maintain.state_name);
        else
            (void)semantic_world_find_zone_slot_local(world_decl,
                maintain->data.world_maintain.zone_slot_name);
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
        (void)find_zone_domain_slot(zone_decl, refresh->data.zone_refresh.object_slot_name);
        (void)find_zone_domain_slot(zone_decl, refresh->data.zone_refresh.source_slot_name);
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        if (state == NULL || state->type != AST_ZONE_STATE)
            continue;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            state->data.zone_state.layer_slot_name);
        (void)find_zone_domain_slot(zone_decl,
            state->data.zone_state.left_or_target_slot_name);
        if (state->data.zone_state.is_relation)
            (void)find_zone_domain_slot(zone_decl,
                state->data.zone_state.right_slot_name);
    }

    for (size_t i = 0; i < maintained_state_count; i++) {
        ASTNode *maintain = maintained_states[i];
        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_STATE)
            continue;
        (void)semantic_zone_find_state_local(zone_decl,
            maintain->data.zone_maintain_state.state_name);
    }
}

void
semantic_stage_world_local_contract_from_label(ASTNode *world_decl,
                                               const char *label,
                                               SemanticContext *ctx)
{
    const char *suffix;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL
        || label == NULL || ctx == NULL) {
        return;
    }

    suffix = strstr(label, ".zone.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 6;
        (void)semantic_world_find_zone_slot_local(world_decl, slot_name);
        return;
    }

    suffix = strstr(label, ".state.");
    if (suffix != NULL) {
        ASTNode *state = semantic_world_find_state_local(world_decl, suffix + 7);
        ASTNode *zone_slot_decl = NULL;

        if (state == NULL || state->type != AST_WORLD_STATE)
            return;

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            state->data.world_state.zone_slot_name);
        if (zone_slot_decl != NULL && zone_slot_decl->type == AST_WORLD_ZONE) {
            ASTNode *zone_decl = semantic_stage_domain_find_zone_decl(
                ctx,
                zone_slot_decl->data.world_zone.zone_type);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL
                && state->data.world_state.detail_name != NULL) {
                if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_PROJECTION) {
                    (void)find_zone_domain_slot(zone_decl, state->data.world_state.detail_name);
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_LAYER) {
                    (void)semantic_zone_find_layer_slot_local(zone_decl,
                        state->data.world_state.detail_name);
                } else if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_STATE) {
                    (void)semantic_zone_find_state_local(zone_decl,
                        state->data.world_state.detail_name);
                }
            }
        }

        if (state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
            || state->data.world_state.source_kind == WORLD_STATE_SOURCE_ANY) {
            for (size_t i = 0; i < state->data.world_state.input_count; i++) {
                const char *input_name = state->data.world_state.input_names[i];
                if (semantic_world_find_state_local(world_decl, input_name) == NULL)
                    (void)semantic_world_find_zone_slot_local(world_decl, input_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".activate.");
    if (suffix != NULL) {
        const char *target = suffix + 10;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
        return;
    }

    suffix = strstr(label, ".deactivate.");
    if (suffix != NULL) {
        const char *target = suffix + 12;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
        return;
    }

    suffix = strstr(label, ".maintain.");
    if (suffix != NULL) {
        const char *target = suffix + 10;
        if (semantic_world_find_state_local(world_decl, target) == NULL)
            (void)semantic_world_find_zone_slot_local(world_decl, target);
    }
}

void
semantic_stage_zone_local_contract_from_label(ASTNode *zone_decl,
                                              const char *label,
                                              SemanticContext *ctx)
{
    const char *suffix;
    ASTNode **refreshes;
    ASTNode **applies;
    ASTNode **links;
    ASTNode **detaches;
    ASTNode **unlinks;
    ASTNode **maintained_effects;
    ASTNode **maintained_relations;
    size_t refresh_count;
    size_t apply_count;
    size_t link_count;
    size_t detach_count;
    size_t unlink_count;
    size_t maintained_effect_count;
    size_t maintained_relation_count;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || label == NULL || ctx == NULL) {
        return;
    }

    refreshes = ast_zone_refreshes(zone_decl, &refresh_count);
    applies = ast_zone_applies(zone_decl, &apply_count);
    links = ast_zone_links(zone_decl, &link_count);
    detaches = ast_zone_detaches(zone_decl, &detach_count);
    unlinks = ast_zone_unlinks(zone_decl, &unlink_count);
    maintained_effects = ast_zone_maintained_effects(zone_decl,
                                                     &maintained_effect_count);
    maintained_relations = ast_zone_maintained_relations(zone_decl,
        &maintained_relation_count);

    suffix = strstr(label, ".slot.");
    if (suffix != NULL && strstr(label, ".field.") == NULL) {
        (void)find_zone_domain_slot(zone_decl, suffix + 6);
        return;
    }

    suffix = strstr(label, ".layer.");
    if (suffix != NULL) {
        (void)semantic_zone_find_layer_slot_local(zone_decl, suffix + 7);
        return;
    }

    suffix = strstr(label, ".state.");
    if (suffix != NULL) {
        ASTNode *state = semantic_zone_find_state_local(zone_decl, suffix + 7);
        if (state == NULL || state->type != AST_ZONE_STATE)
            return;
        (void)semantic_zone_find_layer_slot_local(zone_decl,
            state->data.zone_state.layer_slot_name);
        (void)find_zone_domain_slot(zone_decl,
            state->data.zone_state.left_or_target_slot_name);
        if (state->data.zone_state.is_relation)
            (void)find_zone_domain_slot(zone_decl,
                state->data.zone_state.right_slot_name);
        return;
    }

    suffix = strstr(label, ".refresh.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 9;
        for (size_t i = 0; i < refresh_count; i++) {
            ASTNode *refresh = refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            if (refresh->data.zone_refresh.object_slot_name != NULL
                && strcmp(refresh->data.zone_refresh.object_slot_name, slot_name) == 0) {
                (void)find_zone_domain_slot(zone_decl,
                    refresh->data.zone_refresh.object_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    refresh->data.zone_refresh.source_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".apply.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 7;
        for (size_t i = 0; i < apply_count; i++) {
            ASTNode *apply = applies[i];
            if (apply == NULL || apply->type != AST_ZONE_APPLY)
                continue;
            if (apply->data.zone_apply.effect_slot_name != NULL
                && strcmp(apply->data.zone_apply.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    apply->data.zone_apply.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    apply->data.zone_apply.target_slot_name);
                if (apply->data.zone_apply.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        apply->data.zone_apply.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".link.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 6;
        for (size_t i = 0; i < link_count; i++) {
            ASTNode *link = links[i];
            if (link == NULL || link->type != AST_ZONE_LINK)
                continue;
            if (link->data.zone_link.relation_slot_name != NULL
                && strcmp(link->data.zone_link.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    link->data.zone_link.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    link->data.zone_link.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    link->data.zone_link.right_slot_name);
                if (link->data.zone_link.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        link->data.zone_link.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".detach.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 8;
        for (size_t i = 0; i < detach_count; i++) {
            ASTNode *detach = detaches[i];
            if (detach == NULL || detach->type != AST_ZONE_DETACH)
                continue;
            if (detach->data.zone_detach.effect_slot_name != NULL
                && strcmp(detach->data.zone_detach.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    detach->data.zone_detach.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    detach->data.zone_detach.target_slot_name);
                if (detach->data.zone_detach.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        detach->data.zone_detach.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".unlink.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 8;
        for (size_t i = 0; i < unlink_count; i++) {
            ASTNode *unlink = unlinks[i];
            if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
                continue;
            if (unlink->data.zone_unlink.relation_slot_name != NULL
                && strcmp(unlink->data.zone_unlink.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    unlink->data.zone_unlink.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    unlink->data.zone_unlink.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    unlink->data.zone_unlink.right_slot_name);
                if (unlink->data.zone_unlink.state_name != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        unlink->data.zone_unlink.state_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-effect.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 17;
        for (size_t i = 0; i < maintained_effect_count; i++) {
            ASTNode *maintain = maintained_effects[i];
            if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
                continue;
            if (maintain->data.zone_maintain_effect.effect_slot_name != NULL
                && strcmp(maintain->data.zone_maintain_effect.effect_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    maintain->data.zone_maintain_effect.effect_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_effect.target_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-relation.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 19;
        for (size_t i = 0; i < maintained_relation_count; i++) {
            ASTNode *maintain = maintained_relations[i];
            if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
                continue;
            if (maintain->data.zone_maintain_relation.relation_slot_name != NULL
                && strcmp(maintain->data.zone_maintain_relation.relation_slot_name, slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    maintain->data.zone_maintain_relation.relation_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_relation.left_slot_name);
                (void)find_zone_domain_slot(zone_decl,
                    maintain->data.zone_maintain_relation.right_slot_name);
            }
        }
        return;
    }

    suffix = strstr(label, ".maintain-state.");
    if (suffix != NULL) {
        (void)semantic_zone_find_state_local(zone_decl, suffix + 15);
        return;
    }

    if (strstr(label, ".projection.") != NULL) {
        for (size_t i = 0; i < refresh_count; i++) {
            ASTNode *refresh = refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            (void)find_zone_domain_slot(zone_decl,
                refresh->data.zone_refresh.object_slot_name);
            (void)find_zone_domain_slot(zone_decl,
                refresh->data.zone_refresh.source_slot_name);
        }
        return;
    }

    if (strstr(label, ".field.") != NULL) {
        const char *slot_part = strstr(label, ".slot.");
        const char *field_part = strstr(label, ".field.");
        if (slot_part != NULL && field_part != NULL && field_part > slot_part) {
            size_t slot_len = (size_t)(field_part - (slot_part + 6));
            if (slot_len > SIZE_MAX - 1)
                return;
            char *slot_name = calloc(slot_len + 1, 1);
            if (slot_name == NULL)
                return;
            memcpy(slot_name, slot_part + 6, slot_len);
            if (semantic_type_resolution_projection_source_decl(zone_decl, slot_name, ctx) == NULL)
                (void)find_zone_domain_slot(zone_decl, slot_name);
            free(slot_name);
        }
    }
}
