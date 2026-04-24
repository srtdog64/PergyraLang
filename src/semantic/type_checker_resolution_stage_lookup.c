#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"

static const char *
stage_decl_label(ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        return stmt->data.type_alias.name;
    case AST_CLASS_DECL:
        return stmt->data.class_decl.name;
    case AST_FUNC_DECL:
        return stmt->data.func_decl.name;
    case AST_EVENT_DECL:
        return stmt->data.event_decl.name;
    case AST_ENUM_DECL:
        return stmt->data.enum_decl.name;
    case AST_ABILITY_DECL:
        return stmt->data.ability_decl.name;
    case AST_ROLE_DECL:
        return stmt->data.role_decl.name;
    case AST_PARTY_DECL:
        return stmt->data.party_decl.name;
    case AST_ROSTER_DECL:
        return stmt->data.roster_decl.name;
    case AST_WORLD_DECL:
        return stmt->data.world_decl.name;
    case AST_INTENT_DECL:
        return stmt->data.intent_decl.name;
    case AST_RELATION_DECL:
        return stmt->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return stmt->data.effect_decl.name;
    case AST_ZONE_DECL:
        return stmt->data.zone_decl.name;
    default:
        return NULL;
    }
}

static TypeResolutionNodeKind
stage_decl_kind(ASTNode *stmt)
{
    if (stmt != NULL && stmt->type == AST_TYPE_ALIAS)
        return TYPE_RES_NODE_ALIAS;
    return TYPE_RES_NODE_DECL;
}

ASTNode *
semantic_find_top_level_decl_by_label(ASTNode *program,
                                      const char *label,
                                      TypeResolutionNodeKind kind)
{
    if (program == NULL || program->type != AST_PROGRAM || label == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        const char *stmt_label;

        if (stmt == NULL)
            continue;
        if (stage_decl_kind(stmt) != kind)
            continue;

        stmt_label = stage_decl_label(stmt);
        if (stmt_label != NULL && strcmp(stmt_label, label) == 0)
            return stmt;
    }

    return NULL;
}

ASTNode *
semantic_find_graph_host_decl(ASTNode *program,
                              const char *label)
{
    const char *space;
    const char *dot;
    size_t name_len;
    char *host_name;
    ASTNode *decl = NULL;

    if (program == NULL || label == NULL)
        return NULL;

    if (strncmp(label, "world ", 6) == 0) {
        space = label + 6;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_WORLD_DECL, host_name);
        free(host_name);
        return decl;
    }

    if (strncmp(label, "zone ", 5) == 0) {
        space = label + 5;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = find_domain_decl_by_name(program, AST_ZONE_DECL, host_name);
        free(host_name);
        return decl;
    }

    return NULL;
}
