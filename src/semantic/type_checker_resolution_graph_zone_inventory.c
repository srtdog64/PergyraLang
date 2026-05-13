#include <stdlib.h>

#include "type_checker_internal.h"
#include "parser/ast_api.h"

void
semantic_type_resolution_precollect_zone_inventory(ASTNode *zone_decl,
                                                   SemanticContext *ctx)
{
    ASTNode **slots;
    ASTNode **shared_fields;
    ASTNode **layer_slots;
    size_t slot_count;
    size_t shared_count;
    size_t layer_slot_count;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || ctx == NULL)
        return;
    slots = ast_zone_slots(zone_decl, &slot_count);
    shared_fields = ast_zone_shared_fields(zone_decl, &shared_count);
    layer_slots = ast_zone_layer_slots(zone_decl, &layer_slot_count);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        char *slot_label;

        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;

        slot_label = semantic_type_resolution_zone_slot_label(
            zone_decl,
            slot_name);
        if (slot_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, slot_label);
            free(slot_label);
        }

        semantic_type_resolution_collect_type_refs(
            ast_domain_slot_type(slot),
            ctx,
            slot,
            slot_name != NULL ? slot_name : "<zone-slot>",
            "zone slot type lookup");
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
                ? ast_party_shared_name(field) : "<zone-shared>",
            "zone shared field type lookup");
    }

    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        char *layer_label;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT)
            continue;
        layer_label = semantic_type_resolution_zone_layer_label(
            zone_decl,
            ast_zone_layer_slot_name(slot));
        if (layer_label != NULL) {
            semantic_type_resolution_register_local_contract_node(
                ctx, slot, layer_label);
            free(layer_label);
        }
        semantic_type_resolution_record_string_dependency(
            ctx,
            slot,
            ast_zone_layer_slot_name(slot) != NULL
                ? ast_zone_layer_slot_name(slot) : "<zone-layer>",
            ast_zone_layer_slot_layer_type(slot),
            "zone layer lookup");
    }

    semantic_type_resolution_precollect_zone_command_inventory(zone_decl, ctx);

    semantic_type_resolution_precollect_zone_state_authority_inventory(
        zone_decl, ctx);
}
