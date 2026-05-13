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
            ast_domain_slot_type(slot),
            ctx,
            slot,
            ast_domain_slot_name(slot) != NULL
                ? ast_domain_slot_name(slot) : "<domain-slot>",
            slot_reason != NULL ? slot_reason : "domain slot type lookup");
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
                ? ast_party_shared_name(field) : "<domain-shared>",
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
    ASTNode **slots;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t shared_count;
    size_t method_count;

    if (relation_decl == NULL || relation_decl->type != AST_RELATION_DECL || ctx == NULL)
        return;
    slots = ast_relation_slots(relation_decl, &slot_count);
    shared_fields = ast_relation_shared_fields(relation_decl, &shared_count);
    methods = ast_relation_methods(relation_decl, &method_count);

    semantic_type_resolution_collect_type_refs(
        relation_decl->data.relation_decl.between_left_type,
        ctx,
        relation_decl,
        ast_relation_name(relation_decl) != NULL
            ? ast_relation_name(relation_decl) : "<relation>",
        "relation between-left type lookup");
    semantic_type_resolution_collect_type_refs(
        relation_decl->data.relation_decl.between_right_type,
        ctx,
        relation_decl,
        ast_relation_name(relation_decl) != NULL
            ? ast_relation_name(relation_decl) : "<relation>",
        "relation between-right type lookup");

    semantic_type_resolution_precollect_domain_inventory(
        slots,
        slot_count,
        shared_fields,
        shared_count,
        methods,
        method_count,
        ctx,
        "relation",
        ast_relation_name(relation_decl),
        "relation slot type lookup",
        "relation shared field type lookup");
}

void
semantic_type_resolution_precollect_effect_inventory(ASTNode *effect_decl,
                                                     SemanticContext *ctx)
{
    ASTNode **slots;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t shared_count;
    size_t method_count;

    if (effect_decl == NULL || effect_decl->type != AST_EFFECT_DECL || ctx == NULL)
        return;
    slots = ast_effect_slots(effect_decl, &slot_count);
    shared_fields = ast_effect_shared_fields(effect_decl, &shared_count);
    methods = ast_effect_methods(effect_decl, &method_count);

    semantic_type_resolution_precollect_domain_inventory(
        slots,
        slot_count,
        shared_fields,
        shared_count,
        methods,
        method_count,
        ctx,
        "effect",
        ast_effect_name(effect_decl),
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

    type_node = ast_domain_slot_type(slot);
    if (type_node == NULL || type_node->type != AST_TYPE
        || type_node->data.type.name == NULL) {
        return NULL;
    }

    return find_type_decl_by_name(ctx->program_root, type_node->data.type.name);
}
