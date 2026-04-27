#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
tc_zone_inventory_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
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

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.refresh.%s",
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

        semantic_type_resolution_precollect_zone_refresh_projection_map(
            zone_decl,
            refresh,
            ctx,
            consumer_label);
        free(consumer_label);
    }

    for (size_t i = 0; i < zone_decl->data.zone_decl.apply_count; i++) {
        ASTNode *apply = zone_decl->data.zone_decl.applies[i];
        char *consumer_label;

        if (apply == NULL || apply->type != AST_ZONE_APPLY)
            continue;

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.apply.%s",
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

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.link.%s",
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

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.detach.%s",
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

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.unlink.%s",
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

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.maintain-effect.%s",
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

        consumer_label = tc_zone_inventory_strdup_fmt(
            "zone %s.maintain-relation.%s",
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

    semantic_type_resolution_precollect_zone_state_authority_inventory(
        zone_decl, ctx);
}
