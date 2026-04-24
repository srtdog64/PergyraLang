#include <stddef.h>

#include "type_checker_internal.h"

static void
semantic_type_resolution_precollect_domain_inventory(ASTNode **slots,
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

void
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

void
semantic_type_resolution_precollect_effect_inventory(ASTNode *effect_decl,
                                                     SemanticContext *ctx)
{
    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL || ctx == NULL)
        return;

    semantic_type_resolution_precollect_domain_inventory(
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

ASTNode *
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
