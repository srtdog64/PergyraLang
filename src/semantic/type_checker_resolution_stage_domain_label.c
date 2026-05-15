#include "type_checker_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
        (void)semantic_world_find_zone_slot_local(world_decl, suffix + 6);
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
            ast_world_state_zone_slot_name(state));
        if (zone_slot_decl != NULL && zone_slot_decl->type == AST_WORLD_ZONE) {
            ASTNode *zone_decl = semantic_stage_domain_find_zone_decl(
                ctx,
                ast_world_zone_type_name(zone_slot_decl));
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL
                && ast_world_state_detail_name(state) != NULL) {
                if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_PROJECTION) {
                    (void)find_zone_domain_slot(zone_decl, ast_world_state_detail_name(state));
                } else if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_LAYER) {
                    (void)semantic_zone_find_layer_slot_local(zone_decl,
                        ast_world_state_detail_name(state));
                } else if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_STATE) {
                    (void)semantic_zone_find_state_local(zone_decl,
                        ast_world_state_detail_name(state));
                }
            }
        }

        if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL
            || ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ANY) {
            for (size_t i = 0; i < ast_world_state_input_count(state); i++) {
                const char *input_name = ast_world_state_input_name(state, i);
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
            ast_zone_state_layer_slot_name(state));
        (void)find_zone_domain_slot(zone_decl,
            ast_zone_state_left_or_target_slot_name(state));
        if (ast_zone_state_is_relation(state))
            (void)find_zone_domain_slot(zone_decl,
                ast_zone_state_right_slot_name(state));
        return;
    }

    suffix = strstr(label, ".refresh.");
    if (suffix != NULL) {
        const char *slot_name = suffix + 9;
        for (size_t i = 0; i < refresh_count; i++) {
            ASTNode *refresh = refreshes[i];
            if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
                continue;
            if (ast_zone_refresh_object_slot_name(refresh) != NULL
                && strcmp(ast_zone_refresh_object_slot_name(refresh), slot_name) == 0) {
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_refresh_object_slot_name(refresh));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_refresh_source_slot_name(refresh));
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
            if (ast_zone_effect_slot_name(apply) != NULL
                && strcmp(ast_zone_effect_slot_name(apply), slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_zone_effect_slot_name(apply));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_effect_target_slot_name(apply));
                if (ast_zone_directive_state_name(apply) != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        ast_zone_directive_state_name(apply));
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
            if (ast_zone_relation_slot_name(link) != NULL
                && strcmp(ast_zone_relation_slot_name(link), slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_zone_relation_slot_name(link));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_relation_left_slot_name(link));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_relation_right_slot_name(link));
                if (ast_zone_directive_state_name(link) != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        ast_zone_directive_state_name(link));
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
            if (ast_zone_effect_slot_name(detach) != NULL
                && strcmp(ast_zone_effect_slot_name(detach), slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_zone_effect_slot_name(detach));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_effect_target_slot_name(detach));
                if (ast_zone_directive_state_name(detach) != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        ast_zone_directive_state_name(detach));
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
            if (ast_zone_relation_slot_name(unlink) != NULL
                && strcmp(ast_zone_relation_slot_name(unlink), slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_zone_relation_slot_name(unlink));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_relation_left_slot_name(unlink));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_relation_right_slot_name(unlink));
                if (ast_zone_directive_state_name(unlink) != NULL)
                    (void)semantic_zone_find_state_local(zone_decl,
                        ast_zone_directive_state_name(unlink));
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
            if (ast_zone_effect_slot_name(maintain) != NULL
                && strcmp(ast_zone_effect_slot_name(maintain), slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_zone_effect_slot_name(maintain));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_effect_target_slot_name(maintain));
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
            if (ast_zone_relation_slot_name(maintain) != NULL
                && strcmp(ast_zone_relation_slot_name(maintain), slot_name) == 0) {
                (void)semantic_zone_find_layer_slot_local(zone_decl,
                    ast_zone_relation_slot_name(maintain));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_relation_left_slot_name(maintain));
                (void)find_zone_domain_slot(zone_decl,
                    ast_zone_relation_right_slot_name(maintain));
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
                ast_zone_refresh_object_slot_name(refresh));
            (void)find_zone_domain_slot(zone_decl,
                ast_zone_refresh_source_slot_name(refresh));
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
            if (semantic_type_resolution_projection_source_decl(zone_decl,
                    slot_name,
                    ctx) == NULL) {
                (void)find_zone_domain_slot(zone_decl, slot_name);
            }
            free(slot_name);
        }
    }
}
