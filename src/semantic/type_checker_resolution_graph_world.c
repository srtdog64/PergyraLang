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

        consumer_label = resolution_world_strdup_fmt("world %s.activate.%s",
                                                     world_decl->data.world_decl.name != NULL
                                                         ? world_decl->data.world_decl.name
                                                         : "<world>",
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

        consumer_label = resolution_world_strdup_fmt("world %s.deactivate.%s",
                                                     world_decl->data.world_decl.name != NULL
                                                         ? world_decl->data.world_decl.name
                                                         : "<world>",
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

        consumer_label = resolution_world_strdup_fmt("world %s.maintain.%s",
                                                     world_decl->data.world_decl.name != NULL
                                                         ? world_decl->data.world_decl.name
                                                         : "<world>",
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
