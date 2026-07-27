/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST zone constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>

ASTNode* ast_create_zone_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ZONE_DECL);
    node->data.zone_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.zone_decl.slots = NULL;
    node->data.zone_decl.slot_count = 0;
    node->data.zone_decl.layer_slots = NULL;
    node->data.zone_decl.layer_slot_count = 0;
    node->data.zone_decl.applies = NULL;
    node->data.zone_decl.apply_count = 0;
    node->data.zone_decl.links = NULL;
    node->data.zone_decl.link_count = 0;
    node->data.zone_decl.detaches = NULL;
    node->data.zone_decl.detach_count = 0;
    node->data.zone_decl.unlinks = NULL;
    node->data.zone_decl.unlink_count = 0;
    node->data.zone_decl.refreshes = NULL;
    node->data.zone_decl.refresh_count = 0;
    node->data.zone_decl.maintained_effects = NULL;
    node->data.zone_decl.maintained_effect_count = 0;
    node->data.zone_decl.maintained_relations = NULL;
    node->data.zone_decl.maintained_relation_count = 0;
    node->data.zone_decl.maintained_states = NULL;
    node->data.zone_decl.maintained_state_count = 0;
    node->data.zone_decl.authorities = NULL;
    node->data.zone_decl.authority_count = 0;
    node->data.zone_decl.states = NULL;
    node->data.zone_decl.state_count = 0;
    node->data.zone_decl.shared_fields = NULL;
    node->data.zone_decl.shared_count = 0;
    node->data.zone_decl.methods = NULL;
    node->data.zone_decl.method_count = 0;
    node->data.zone_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_zone_layer_slot(const char* slot_name, const char* layer_type, bool is_relation) {
    ASTNode* node = ast_create_node(AST_ZONE_LAYER_SLOT);
    node->data.zone_layer_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.zone_layer_slot.layer_type = layer_type ? pergyra_strdup(layer_type) : NULL;
    node->data.zone_layer_slot.is_relation = is_relation;
    node->data.zone_layer_slot.is_pool = false;
    node->data.zone_layer_slot.pool_capacity = 0;
    return node;
}

ASTNode* ast_create_zone_apply(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_APPLY);
    node->data.zone_apply.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_apply.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_apply.state_name = NULL;
    node->data.zone_apply.participant_slot_name = NULL;
    return node;
}

bool
ast_zone_apply_bind_resolved_state(ASTNode* node,
                                   const char* effect_slot_name,
                                   const char* target_slot_name)
{
    char *owned_effect;
    char *owned_target;

    if (node == NULL || node->type != AST_ZONE_APPLY
        || node->data.zone_apply.state_name == NULL
        || effect_slot_name == NULL || target_slot_name == NULL) {
        return false;
    }
    owned_effect = pergyra_strdup(effect_slot_name);
    owned_target = pergyra_strdup(target_slot_name);
    if (owned_effect == NULL || owned_target == NULL) {
        free(owned_effect);
        free(owned_target);
        return false;
    }
    free(node->data.zone_apply.effect_slot_name);
    free(node->data.zone_apply.target_slot_name);
    node->data.zone_apply.effect_slot_name = owned_effect;
    node->data.zone_apply.target_slot_name = owned_target;
    return true;
}

ASTNode* ast_create_zone_link(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_LINK);
    node->data.zone_link.relation_slot_name =
        relation_slot_name ? pergyra_strdup(relation_slot_name) : NULL;
    node->data.zone_link.left_slot_name =
        left_slot_name ? pergyra_strdup(left_slot_name) : NULL;
    node->data.zone_link.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    node->data.zone_link.state_name = NULL;
    node->data.zone_link.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_detach(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_DETACH);
    node->data.zone_detach.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_detach.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_detach.state_name = NULL;
    node->data.zone_detach.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_unlink(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_UNLINK);
    node->data.zone_unlink.relation_slot_name =
        relation_slot_name ? pergyra_strdup(relation_slot_name) : NULL;
    node->data.zone_unlink.left_slot_name =
        left_slot_name ? pergyra_strdup(left_slot_name) : NULL;
    node->data.zone_unlink.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    node->data.zone_unlink.state_name = NULL;
    node->data.zone_unlink.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_refresh(const char* object_slot_name, const char* source_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_REFRESH);
    node->data.zone_refresh.object_slot_name =
        object_slot_name ? pergyra_strdup(object_slot_name) : NULL;
    node->data.zone_refresh.source_slot_name =
        source_slot_name ? pergyra_strdup(source_slot_name) : NULL;
    node->data.zone_refresh.participant_slot_name = NULL;
    node->data.zone_refresh.requires_dto = false;
    node->data.zone_refresh.derive_target_kind = false;
    node->data.zone_refresh.mapped_target_fields = NULL;
    node->data.zone_refresh.mapped_source_fields = NULL;
    node->data.zone_refresh.field_map_count = 0;
    return node;
}

ASTNode* ast_create_zone_maintain_effect(const char* effect_slot_name, const char* target_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_EFFECT);
    node->data.zone_maintain_effect.effect_slot_name =
        effect_slot_name ? pergyra_strdup(effect_slot_name) : NULL;
    node->data.zone_maintain_effect.target_slot_name =
        target_slot_name ? pergyra_strdup(target_slot_name) : NULL;
    node->data.zone_maintain_effect.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_maintain_relation(const char* relation_slot_name, const char* left_slot_name, const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_RELATION);
    node->data.zone_maintain_relation.relation_slot_name =
        relation_slot_name ? pergyra_strdup(relation_slot_name) : NULL;
    node->data.zone_maintain_relation.left_slot_name =
        left_slot_name ? pergyra_strdup(left_slot_name) : NULL;
    node->data.zone_maintain_relation.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    node->data.zone_maintain_relation.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_maintain_state(const char* state_name) {
    ASTNode* node = ast_create_node(AST_ZONE_MAINTAIN_STATE);
    node->data.zone_maintain_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.zone_maintain_state.participant_slot_name = NULL;
    return node;
}

ASTNode* ast_create_zone_authority(const char* subject_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_AUTHORITY);
    node->data.zone_authority.subject_slot_name =
        subject_slot_name ? pergyra_strdup(subject_slot_name) : NULL;
    node->data.zone_authority.required_abilities = NULL;
    node->data.zone_authority.ability_count = 0;
    return node;
}

ASTNode* ast_create_zone_state(const char* state_name, bool is_relation,
                               const char* layer_slot_name,
                               const char* left_or_target_slot_name,
                               const char* right_slot_name) {
    ASTNode* node = ast_create_node(AST_ZONE_STATE);
    node->data.zone_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.zone_state.is_relation = is_relation;
    node->data.zone_state.layer_slot_name =
        layer_slot_name ? pergyra_strdup(layer_slot_name) : NULL;
    node->data.zone_state.left_or_target_slot_name =
        left_or_target_slot_name ? pergyra_strdup(left_or_target_slot_name) : NULL;
    node->data.zone_state.right_slot_name =
        right_slot_name ? pergyra_strdup(right_slot_name) : NULL;
    return node;
}
