/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST world constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>

ASTNode* ast_create_world_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_WORLD_DECL);
    node->data.world_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.world_decl.rosters = NULL;
    node->data.world_decl.roster_count = 0;
    node->data.world_decl.zones = NULL;
    node->data.world_decl.zone_count = 0;
    node->data.world_decl.shared_fields = NULL;
    node->data.world_decl.shared_count = 0;
    node->data.world_decl.methods = NULL;
    node->data.world_decl.method_count = 0;
    node->data.world_decl.activations = NULL;
    node->data.world_decl.activate_count = 0;
    node->data.world_decl.deactivations = NULL;
    node->data.world_decl.deactivate_count = 0;
    node->data.world_decl.maintained_zones = NULL;
    node->data.world_decl.maintained_zone_count = 0;
    node->data.world_decl.states = NULL;
    node->data.world_decl.state_count = 0;
    node->data.world_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_world_roster(const char* slot_name, const char* roster_type) {
    ASTNode* node = ast_create_node(AST_WORLD_SYSTEMIC);
    node->data.world_roster.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.world_roster.roster_type = roster_type ? pergyra_strdup(roster_type) : NULL;
    node->data.world_roster.initializer = NULL;
    return node;
}

ASTNode* ast_create_world_zone(const char* slot_name, const char* zone_type) {
    ASTNode* node = ast_create_node(AST_WORLD_ZONE);
    node->data.world_zone.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.world_zone.zone_type = zone_type ? pergyra_strdup(zone_type) : NULL;
    node->data.world_zone.initializer = NULL;
    return node;
}

ASTNode* ast_create_world_activate(const char* zone_slot_name) {
    ASTNode* node = ast_create_node(AST_WORLD_ACTIVATE);
    node->data.world_activate.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_activate.state_name = NULL;
    return node;
}

ASTNode* ast_create_world_deactivate(const char* zone_slot_name) {
    ASTNode* node = ast_create_node(AST_WORLD_DEACTIVATE);
    node->data.world_deactivate.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_deactivate.state_name = NULL;
    return node;
}

ASTNode* ast_create_world_maintain(const char* zone_slot_name) {
    ASTNode* node = ast_create_node(AST_WORLD_MAINTAIN);
    node->data.world_maintain.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_maintain.state_name = NULL;
    return node;
}

ASTNode* ast_create_world_state(const char* state_name, const char* zone_slot_name,
                                WorldStateSourceKind source_kind,
                                const char* detail_name) {
    ASTNode* node = ast_create_node(AST_WORLD_STATE);
    node->data.world_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.world_state.zone_slot_name =
        zone_slot_name ? pergyra_strdup(zone_slot_name) : NULL;
    node->data.world_state.source_kind = source_kind;
    node->data.world_state.detail_name =
        detail_name ? pergyra_strdup(detail_name) : NULL;
    node->data.world_state.input_names = NULL;
    node->data.world_state.input_count = 0;
    return node;
}

ASTNode* ast_create_world_state_compose(const char* state_name,
                                        WorldStateSourceKind source_kind,
                                        const char** input_names,
                                        size_t input_count) {
    ASTNode* node = ast_create_node(AST_WORLD_STATE);
    node->data.world_state.state_name =
        state_name ? pergyra_strdup(state_name) : NULL;
    node->data.world_state.zone_slot_name = NULL;
    node->data.world_state.source_kind = source_kind;
    node->data.world_state.detail_name = NULL;
    node->data.world_state.input_names = NULL;
    node->data.world_state.input_count = input_count;
    if (input_count > 0) {
        node->data.world_state.input_names = calloc(input_count, sizeof(char*));
        for (size_t i = 0; i < input_count; i++) {
            node->data.world_state.input_names[i] =
                input_names != NULL && input_names[i] != NULL
                    ? pergyra_strdup(input_names[i]) : NULL;
        }
    }
    return node;
}
