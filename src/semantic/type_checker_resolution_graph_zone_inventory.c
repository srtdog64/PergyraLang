#include <stdlib.h>

#include "type_checker_internal.h"

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

    semantic_type_resolution_precollect_zone_command_inventory(zone_decl, ctx);

    semantic_type_resolution_precollect_zone_state_authority_inventory(
        zone_decl, ctx);
}
