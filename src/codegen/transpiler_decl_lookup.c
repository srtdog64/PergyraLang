/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend declaration lookup helpers.
 */

#include <stdio.h>
#include <string.h>

#include "transpiler_decl_lookup.h"

static void
transpiler_decl_lookup_cache_store(TranspilerCtx *ctx,
                                   ASTNodeType decl_type,
                                   ASTNode **decls,
                                   size_t decl_count,
                                   const char *name,
                                   ASTNode *decl,
                                   bool active_only)
{
    size_t len;

    if (ctx == NULL || name == NULL || decl == NULL)
        return;

    len = strlen(name);
    if (len >= sizeof(ctx->last_decl_lookup_name)) {
        ctx->last_decl_lookup_result = NULL;
        ctx->last_decl_lookup_name[0] = '\0';
        return;
    }

    ctx->last_decl_lookup_type = decl_type;
    memcpy(ctx->last_decl_lookup_name, name, len + 1);
    ctx->last_decl_lookup_inventory = decls;
    ctx->last_decl_lookup_inventory_count = decl_count;
    ctx->last_decl_lookup_result = decl;
    ctx->last_decl_lookup_active_only = active_only;
}

static ASTNode *
find_extern_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || function_name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, AST_EXTERN_BLOCK, &decls, &decl_count);
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        ASTNode *block = decls[i];
        if (block == NULL || block->type != AST_EXTERN_BLOCK)
            continue;
        size_t extern_count = 0;
        (void)ast_extern_block_declarations(block, &extern_count);
        for (size_t j = 0; j < extern_count; j++) {
            ASTNode *stmt = ast_extern_block_declaration(block, j);
            const char *stmt_name = ast_declaration_name(stmt);
            if (stmt != NULL && stmt->type == AST_FUNC_DECL
                && stmt_name != NULL
                && strcmp(stmt_name, function_name) == 0) {
                return stmt;
            }
        }
    }
    return NULL;
}

static bool
transpiler_named_decl_matches(ASTNode *stmt, ASTNodeType decl_type,
                              const char *name)
{
    if (stmt == NULL || name == NULL || stmt->type != decl_type)
        return false;

    switch (decl_type) {
    case AST_PARTY_DECL:
        return ast_party_name(stmt) != NULL
            && strcmp(ast_party_name(stmt), name) == 0;
    case AST_ROSTER_DECL:
        return ast_roster_name(stmt) != NULL
            && strcmp(ast_roster_name(stmt), name) == 0;
    case AST_ENUM_DECL:
        return ast_enum_name(stmt) != NULL
            && strcmp(ast_enum_name(stmt), name) == 0;
    case AST_ROLE_DECL:
        return ast_role_name(stmt) != NULL
            && strcmp(ast_role_name(stmt), name) == 0;
    case AST_CLASS_DECL:
        return ast_class_name(stmt) != NULL
            && strcmp(ast_class_name(stmt), name) == 0;
    case AST_FUNC_DECL:
        return ast_declaration_name(stmt) != NULL
            && strcmp(ast_declaration_name(stmt), name) == 0;
    case AST_INTENT_DECL:
        return ast_intent_decl_name(stmt) != NULL
            && strcmp(ast_intent_decl_name(stmt), name) == 0;
    case AST_TYPE_ALIAS:
        return ast_type_alias_name(stmt) != NULL
            && strcmp(ast_type_alias_name(stmt), name) == 0;
    case AST_ABILITY_DECL:
        return ast_ability_name(stmt) != NULL
            && strcmp(ast_ability_name(stmt), name) == 0;
    case AST_EVENT_DECL:
        return ast_event_name(stmt) != NULL
            && strcmp(ast_event_name(stmt), name) == 0;
    case AST_ZONE_DECL:
        return ast_zone_name(stmt) != NULL
            && strcmp(ast_zone_name(stmt), name) == 0;
    case AST_WORLD_DECL:
        return ast_world_name(stmt) != NULL
            && strcmp(ast_world_name(stmt), name) == 0;
    case AST_RELATION_DECL:
        return ast_relation_name(stmt) != NULL
            && strcmp(ast_relation_name(stmt), name) == 0;
    case AST_EFFECT_DECL:
        return ast_effect_name(stmt) != NULL
            && strcmp(ast_effect_name(stmt), name) == 0;
    default:
        return false;
    }
}

ASTNode *
transpiler_find_named_decl_local(TranspilerCtx *ctx, ASTNodeType decl_type,
                                 const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;
    decl_header = transpiler_active_decl_header(ctx, name);
    if (decl_header != NULL)
        return decl_header->ast_type == decl_type
            ? decl_header->source_ast
            : NULL;
    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    if (decls == NULL)
        return NULL;

    for (size_t i = 0; i < decl_count; i++) {
        ASTNode *stmt = decls[i];
        if (transpiler_named_decl_matches(stmt, decl_type, name))
            return stmt;
    }
    return NULL;
}

ASTNode *
find_role_decl(TranspilerCtx *ctx, const char *role_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ROLE_DECL, role_name);
}

ASTNode *
find_function_decl(TranspilerCtx *ctx, const char *function_name)
{
    ASTNode *decl = transpiler_find_named_decl_local(ctx, AST_FUNC_DECL,
                                                    function_name);
    if (decl != NULL)
        return decl;
    return find_extern_function_decl(ctx, function_name);
}

ASTNode *
find_intent_decl(TranspilerCtx *ctx, const char *intent_name)
{
    return transpiler_find_named_decl_local(ctx, AST_INTENT_DECL, intent_name);
}

ASTNode *
find_callable_decl(TranspilerCtx *ctx, const char *name)
{
    ASTNode *decl = find_function_decl(ctx, name);
    if (decl != NULL)
        return decl;
    return find_intent_decl(ctx, name);
}

ASTNode *
find_party_decl(TranspilerCtx *ctx, const char *party_name)
{
    return transpiler_find_named_decl_local(ctx, AST_PARTY_DECL, party_name);
}

ASTNode *
find_roster_decl(TranspilerCtx *ctx, const char *roster_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ROSTER_DECL, roster_name);
}

ASTNode *
find_enum_decl(TranspilerCtx *ctx, const char *enum_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ENUM_DECL, enum_name);
}

ASTNode *
find_class_decl(TranspilerCtx *ctx, const char *class_name)
{
    return transpiler_find_named_decl_local(ctx, AST_CLASS_DECL, class_name);
}

ASTNode *
transpiler_find_type_alias_decl(TranspilerCtx *ctx, const char *alias_name)
{
    return transpiler_find_named_decl_local(ctx, AST_TYPE_ALIAS, alias_name);
}

ASTNode *
resolve_type_alias_target(TranspilerCtx *ctx, ASTNode *type_node)
{
    unsigned depth = 0;

    while (ctx != NULL && type_node != NULL
           && type_node->type == AST_TYPE
           && ast_type_name(type_node) != NULL
           && depth < 32) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(
            ctx, ast_type_name(type_node));
        if (alias_decl == NULL || ast_type_alias_target_type(alias_decl) == NULL)
            break;
        type_node = ast_type_alias_target_type(alias_decl);
        depth++;
    }

    return type_node;
}

ASTNode *
find_subject_host_decl(TranspilerCtx *ctx, const char *subject_name)
{
    ASTNode *decl = find_class_decl(ctx, subject_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT) {
        return decl;
    }
    return NULL;
}

ASTNode *
find_zone_decl(TranspilerCtx *ctx, const char *zone_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ZONE_DECL, zone_name);
}

ASTNode *
find_world_decl(TranspilerCtx *ctx, const char *world_name)
{
    return transpiler_find_named_decl_local(ctx, AST_WORLD_DECL, world_name);
}

ASTNode *
find_relation_decl(TranspilerCtx *ctx, const char *relation_name)
{
    return transpiler_find_named_decl_local(ctx, AST_RELATION_DECL,
                                            relation_name);
}

ASTNode *
find_effect_decl(TranspilerCtx *ctx, const char *effect_name)
{
    return transpiler_find_named_decl_local(ctx, AST_EFFECT_DECL, effect_name);
}

ASTNode *
find_ability_decl(TranspilerCtx *ctx, const char *ability_name)
{
    return transpiler_find_named_decl_local(ctx, AST_ABILITY_DECL, ability_name);
}

ASTNode *
find_event_decl(TranspilerCtx *ctx, const char *event_name)
{
    return transpiler_find_named_decl_local(ctx, AST_EVENT_DECL, event_name);
}

bool
transpiler_has_known_nominal_type(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;

    if (find_class_decl(ctx, name) != NULL
        || find_enum_decl(ctx, name) != NULL
        || find_role_decl(ctx, name) != NULL
        || find_zone_decl(ctx, name) != NULL
        || find_party_decl(ctx, name) != NULL
        || find_roster_decl(ctx, name) != NULL
        || find_world_decl(ctx, name) != NULL
        || find_relation_decl(ctx, name) != NULL
        || find_effect_decl(ctx, name) != NULL)
        return true;
    return false;
}

const char *
transpiler_decl_name_local(ASTNode *decl)
{
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return ast_class_name(decl);
    case AST_ENUM_DECL:
        return ast_enum_name(decl);
    case AST_ROLE_DECL:
        return ast_role_name(decl);
    case AST_PARTY_DECL:
        return ast_party_name(decl);
    case AST_ROSTER_DECL:
        return ast_roster_name(decl);
    case AST_RELATION_DECL:
        return ast_relation_name(decl);
    case AST_EFFECT_DECL:
        return ast_effect_name(decl);
    case AST_ZONE_DECL:
        return ast_zone_name(decl);
    case AST_WORLD_DECL:
        return ast_world_name(decl);
    default:
        return NULL;
    }
}

bool
transpiler_is_host_decl_type(ASTNodeType decl_type)
{
    switch (decl_type) {
    case AST_CLASS_DECL:
    case AST_ENUM_DECL:
    case AST_PARTY_DECL:
    case AST_ROLE_DECL:
    case AST_ROSTER_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ZONE_DECL:
    case AST_WORLD_DECL:
        return true;
    default:
        return false;
    }
}

ASTNode *
transpiler_find_decl_in_inventory_local(TranspilerCtx *ctx,
                                        ASTNodeType decl_type,
                                        const char *name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    if (ctx->last_decl_lookup_result != NULL
        && ctx->last_decl_lookup_type == decl_type
        && !ctx->last_decl_lookup_active_only
        && ctx->last_decl_lookup_inventory == decls
        && ctx->last_decl_lookup_inventory_count == decl_count
        && strcmp(ctx->last_decl_lookup_name, name) == 0) {
        return ctx->last_decl_lookup_result;
    }
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        ASTNode *decl = decls[i];
        const char *decl_name = transpiler_decl_name_local(decl);
        if (decl != NULL && decl->type == decl_type
            && decl_name != NULL
            && strcmp(decl_name, name) == 0) {
            transpiler_decl_lookup_cache_store(ctx, decl_type, decls,
                decl_count, name, decl, false);
            return decl;
        }
    }

    return transpiler_find_named_decl_local(ctx, decl_type, name);
}

ASTNode *
transpiler_find_decl_in_active_inventory_only_local(TranspilerCtx *ctx,
                                                    ASTNodeType decl_type,
                                                    const char *name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    if (ctx->last_decl_lookup_result != NULL
        && ctx->last_decl_lookup_type == decl_type
        && ctx->last_decl_lookup_active_only
        && ctx->last_decl_lookup_inventory == decls
        && ctx->last_decl_lookup_inventory_count == decl_count
        && strcmp(ctx->last_decl_lookup_name, name) == 0) {
        return ctx->last_decl_lookup_result;
    }
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        ASTNode *decl = decls[i];
        const char *decl_name = transpiler_decl_name_local(decl);
        if (decl != NULL && decl->type == decl_type
            && decl_name != NULL
            && strcmp(decl_name, name) == 0) {
            transpiler_decl_lookup_cache_store(ctx, decl_type, decls,
                decl_count, name, decl, true);
            return decl;
        }
    }

    return NULL;
}
