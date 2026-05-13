/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for zone declarations.
 */

#include "ast_constructors_internal.h"

const char*
ast_zone_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    return node->data.zone_decl.name;
}

ASTNode**
ast_zone_slots(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.slot_count;
    return node->data.zone_decl.slots;
}

ASTNode**
ast_zone_layer_slots(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.layer_slot_count;
    return node->data.zone_decl.layer_slots;
}

ASTNode**
ast_zone_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.shared_count;
    return node->data.zone_decl.shared_fields;
}

ASTNode**
ast_zone_authorities(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.authority_count;
    return node->data.zone_decl.authorities;
}

ASTNode**
ast_zone_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.method_count;
    return node->data.zone_decl.methods;
}

ASTNode**
ast_zone_refreshes(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.refresh_count;
    return node->data.zone_decl.refreshes;
}

ASTNode**
ast_zone_states(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.state_count;
    return node->data.zone_decl.states;
}

ASTNode**
ast_zone_applies(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.apply_count;
    return node->data.zone_decl.applies;
}

ASTNode**
ast_zone_links(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.link_count;
    return node->data.zone_decl.links;
}

ASTNode**
ast_zone_detaches(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.detach_count;
    return node->data.zone_decl.detaches;
}

ASTNode**
ast_zone_unlinks(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.unlink_count;
    return node->data.zone_decl.unlinks;
}

ASTNode**
ast_zone_maintained_effects(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.maintained_effect_count;
    return node->data.zone_decl.maintained_effects;
}

ASTNode**
ast_zone_maintained_relations(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.maintained_relation_count;
    return node->data.zone_decl.maintained_relations;
}

ASTNode**
ast_zone_maintained_states(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.maintained_state_count;
    return node->data.zone_decl.maintained_states;
}
