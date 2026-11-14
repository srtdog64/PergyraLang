#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
resolution_world_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void
semantic_type_resolution_precollect_world_inventory(ASTNode *world_decl,
                                                    SemanticContext *ctx)
{
    const char *world_name;
    ASTNode **rosters;
    ASTNode **zones;
    ASTNode **shared_fields;
    ASTNode **states;
    ASTNode **activations;
    ASTNode **deactivations;
    ASTNode **maintained_zones;
    ASTNode **methods;
    size_t roster_count;
    size_t zone_count;
    size_t shared_count;
    size_t state_count;
    size_t activate_count;
    size_t deactivate_count;
    size_t maintained_zone_count;
    size_t method_count;

    if (world_decl == NULL || world_decl->type != AST_WORLD_DECL || ctx == NULL)
        return;
    world_name = ast_world_name(world_decl);
    rosters = ast_world_rosters(world_decl, &roster_count);
    zones = ast_world_zones(world_decl, &zone_count);
    shared_fields = ast_world_shared_fields(world_decl, &shared_count);
    states = ast_world_states(world_decl, &state_count);
    activations = ast_world_activations(world_decl, &activate_count);
    deactivations = ast_world_deactivations(world_decl, &deactivate_count);
    maintained_zones = ast_world_maintained_zones(world_decl,
                                                  &maintained_zone_count);
    methods = ast_world_methods(world_decl, &method_count);

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        char *zone_slot_label;

        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;

        zone_slot_label = semantic_type_resolution_world_zone_slot_label(
            world_decl,
            ast_world_zone_slot_name(zone));
        if (zone_slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, zone, zone_slot_label);
            free(zone_slot_label);
        }
    }

    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        semantic_type_resolution_collect_type_refs(
            ast_party_shared_type(field),
            ctx,
            field,
            ast_party_shared_name(field) != NULL
                ? ast_party_shared_name(field) : "<world-shared>",
            "world shared field type lookup");
    }

    for (size_t i = 0; i < roster_count; i++) {
        ASTNode *roster = rosters[i];
        if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            roster,
            ast_world_roster_slot_name(roster) != NULL
                ? ast_world_roster_slot_name(roster) : "<world-roster>",
            ast_world_roster_type_name(roster),
            "world roster lookup");
    }

    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;
        semantic_type_resolution_record_string_dependency(
            ctx,
            zone,
            ast_world_zone_slot_name(zone) != NULL
                ? ast_world_zone_slot_name(zone) : "<world-zone>",
            ast_world_zone_type_name(zone),
            "world zone lookup");
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        ASTNode *zone_slot_decl = NULL;
        char *state_label;

        if (state == NULL || state->type != AST_WORLD_STATE)
            continue;

        state_label = semantic_type_resolution_world_state_label(
            world_decl,
            ast_world_state_name(state));
        if (state_label == NULL)
            continue;

        semantic_type_resolution_register_local_contract_node(
            ctx, state, state_label);

        zone_slot_decl = semantic_world_find_zone_slot_local(
            world_decl,
            ast_world_state_zone_slot_name(state));

        if (ast_world_state_zone_slot_name(state) != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                ast_world_state_zone_slot_name(state));
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
            && ast_world_zone_type_name(zone_slot_decl) != NULL
            && ast_world_state_detail_name(state) != NULL) {
            ASTNode *zone_type_decl = semantic_find_zone_decl_by_name(
                ctx,
                ast_world_zone_type_name(zone_slot_decl));
            if (zone_type_decl != NULL) {
                if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_PROJECTION) {
                    char *projection_label = semantic_type_resolution_zone_slot_label(
                        zone_type_decl,
                        ast_world_state_detail_name(state));
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
                } else if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_LAYER) {
                    char *layer_label = semantic_type_resolution_zone_layer_label(
                        zone_type_decl,
                        ast_world_state_detail_name(state));
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
                } else if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_STATE) {
                    char *nested_state_label = semantic_type_resolution_zone_state_label(
                        zone_type_decl,
                        ast_world_state_detail_name(state));
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

        if (ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL
            || ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ANY) {
            for (size_t input_i = 0; input_i < ast_world_state_input_count(state); input_i++) {
                const char *input_name = ast_world_state_input_name(state, input_i);
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

    for (size_t i = 0; i < activate_count; i++) {
        ASTNode *activate = activations[i];
        char *consumer_label;

        if (activate == NULL || activate->type != AST_WORLD_ACTIVATE)
            continue;

        consumer_label = resolution_world_strdup_fmt("world %s.activate.%s",
                                                     world_name != NULL
                                                         ? world_name
                                                         : "<world>",
                                                     ast_world_directive_state_name(activate) != NULL
                                                         ? ast_world_directive_state_name(activate)
                                                         : (ast_world_directive_zone_slot_name(activate) != NULL
                                                             ? ast_world_directive_zone_slot_name(activate)
                                                             : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (ast_world_directive_state_name(activate) != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                ast_world_directive_state_name(activate));
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
        } else if (ast_world_directive_zone_slot_name(activate) != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                ast_world_directive_zone_slot_name(activate));
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

    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *deactivate = deactivations[i];
        char *consumer_label;

        if (deactivate == NULL || deactivate->type != AST_WORLD_DEACTIVATE)
            continue;

        consumer_label = resolution_world_strdup_fmt("world %s.deactivate.%s",
                                                     world_name != NULL
                                                         ? world_name
                                                         : "<world>",
                                                     ast_world_directive_state_name(deactivate) != NULL
                                                         ? ast_world_directive_state_name(deactivate)
                                                         : (ast_world_directive_zone_slot_name(deactivate) != NULL
                                                             ? ast_world_directive_zone_slot_name(deactivate)
                                                             : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (ast_world_directive_state_name(deactivate) != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                ast_world_directive_state_name(deactivate));
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
        } else if (ast_world_directive_zone_slot_name(deactivate) != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                ast_world_directive_zone_slot_name(deactivate));
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

    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *maintain = maintained_zones[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_WORLD_MAINTAIN)
            continue;

        consumer_label = resolution_world_strdup_fmt("world %s.maintain.%s",
                                                     world_name != NULL
                                                         ? world_name
                                                         : "<world>",
                                                     ast_world_directive_state_name(maintain) != NULL
                                                         ? ast_world_directive_state_name(maintain)
                                                         : (ast_world_directive_zone_slot_name(maintain) != NULL
                                                             ? ast_world_directive_zone_slot_name(maintain)
                                                             : "<target>"));
        if (consumer_label == NULL)
            continue;
        if (ast_world_directive_state_name(maintain) != NULL) {
            char *state_label = semantic_type_resolution_world_state_label(
                world_decl,
                ast_world_directive_state_name(maintain));
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
        } else if (ast_world_directive_zone_slot_name(maintain) != NULL) {
            char *zone_slot_label = semantic_type_resolution_world_zone_slot_label(
                world_decl,
                ast_world_directive_zone_slot_name(maintain));
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

    for (size_t i = 0; i < method_count; i++) {
        semantic_type_resolution_precollect_action_contract(
            methods[i],
            ctx,
            world_name);
    }
}
