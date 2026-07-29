#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "type_checker_internal.h"

static ASTNode *
stage_lookup_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

uint32_t
semantic_program_syntax_id(SemanticContext *ctx)
{
    ASTNode *program = stage_lookup_program(ctx);

    if (program == NULL || program->type != AST_PROGRAM)
        return 0;
    return ast_node_stable_id(program);
}

static const char *
stage_decl_label(ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;

    switch (stmt->type) {
    case AST_TYPE_ALIAS:
        return ast_type_alias_name(stmt);
    case AST_CLASS_DECL:
        return ast_class_name(stmt);
    case AST_FUNC_DECL:
        return ast_declaration_name(stmt);
    case AST_EVENT_DECL:
        return ast_event_name(stmt);
    case AST_ENUM_DECL:
        return ast_enum_name(stmt);
    case AST_ABILITY_DECL:
        return ast_ability_name(stmt);
    case AST_ROLE_DECL:
        return ast_role_name(stmt);
    case AST_PARTY_DECL:
        return ast_party_name(stmt);
    case AST_ROSTER_DECL:
        return ast_roster_name(stmt);
    case AST_WORLD_DECL:
        return ast_world_name(stmt);
    case AST_INTENT_DECL:
        return ast_intent_decl_name(stmt);
    case AST_RELATION_DECL:
        return ast_relation_name(stmt);
    case AST_EFFECT_DECL:
        return ast_effect_name(stmt);
    case AST_ZONE_DECL:
        return ast_zone_name(stmt);
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

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
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
semantic_find_top_level_decl_by_label_in_context(SemanticContext *ctx,
                                                 const char *label,
                                                 TypeResolutionNodeKind kind)
{
    ASTNode *decl;

    if (ctx == NULL)
        return NULL;
    decl = semantic_host_index_find_top_level_decl_by_label(ctx, label, kind);
    if (decl != NULL || ctx->host_decl_index.count > 0)
        return decl;
    return semantic_find_top_level_decl_by_label(stage_lookup_program(ctx),
                                                 label, kind);
}

ASTNode *
semantic_find_graph_host_decl(SemanticContext *ctx,
                              const char *label)
{
    const char *space;
    const char *dot;
    size_t name_len;
    char *host_name;
    ASTNode *decl = NULL;

    if (ctx == NULL || label == NULL)
        return NULL;

    if (strncmp(label, "world ", 6) == 0) {
        space = label + 6;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        if (name_len > SIZE_MAX - 1)
            return NULL;
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = semantic_find_world_decl_by_name(ctx, host_name);
        free(host_name);
        return decl;
    }

    if (strncmp(label, "zone ", 5) == 0) {
        space = label + 5;
        dot = strchr(space, '.');
        if (dot == NULL || dot == space)
            return NULL;
        name_len = (size_t)(dot - space);
        if (name_len > SIZE_MAX - 1)
            return NULL;
        host_name = calloc(name_len + 1, 1);
        if (host_name == NULL)
            return NULL;
        memcpy(host_name, space, name_len);
        decl = semantic_find_zone_decl_by_name(ctx, host_name);
        free(host_name);
        return decl;
    }

    return NULL;
}
