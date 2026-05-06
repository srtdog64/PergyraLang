/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend declaration lookup helpers.
 */

#include <stdio.h>
#include <string.h>

#include "transpiler_decl_lookup.h"

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
        for (size_t j = 0; j < block->data.extern_block.count; j++) {
            ASTNode *stmt = block->data.extern_block.declarations[j];
            if (stmt != NULL && stmt->type == AST_FUNC_DECL
                && stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, function_name) == 0) {
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
        return stmt->data.party_decl.name != NULL
            && strcmp(stmt->data.party_decl.name, name) == 0;
    case AST_ROSTER_DECL:
        return stmt->data.roster_decl.name != NULL
            && strcmp(stmt->data.roster_decl.name, name) == 0;
    case AST_ENUM_DECL:
        return stmt->data.enum_decl.name != NULL
            && strcmp(stmt->data.enum_decl.name, name) == 0;
    case AST_ROLE_DECL:
        return stmt->data.role_decl.name != NULL
            && strcmp(stmt->data.role_decl.name, name) == 0;
    case AST_CLASS_DECL:
        return stmt->data.class_decl.name != NULL
            && strcmp(stmt->data.class_decl.name, name) == 0;
    case AST_FUNC_DECL:
        return stmt->data.func_decl.name != NULL
            && strcmp(stmt->data.func_decl.name, name) == 0;
    case AST_INTENT_DECL:
        return stmt->data.intent_decl.name != NULL
            && strcmp(stmt->data.intent_decl.name, name) == 0;
    case AST_TYPE_ALIAS:
        return stmt->data.type_alias.name != NULL
            && strcmp(stmt->data.type_alias.name, name) == 0;
    case AST_ABILITY_DECL:
        return stmt->data.ability_decl.name != NULL
            && strcmp(stmt->data.ability_decl.name, name) == 0;
    case AST_EVENT_DECL:
        return stmt->data.event_decl.name != NULL
            && strcmp(stmt->data.event_decl.name, name) == 0;
    case AST_ZONE_DECL:
        return stmt->data.zone_decl.name != NULL
            && strcmp(stmt->data.zone_decl.name, name) == 0;
    case AST_WORLD_DECL:
        return stmt->data.world_decl.name != NULL
            && strcmp(stmt->data.world_decl.name, name) == 0;
    case AST_RELATION_DECL:
        return stmt->data.relation_decl.name != NULL
            && strcmp(stmt->data.relation_decl.name, name) == 0;
    case AST_EFFECT_DECL:
        return stmt->data.effect_decl.name != NULL
            && strcmp(stmt->data.effect_decl.name, name) == 0;
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
    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, name);
        if (decl_header != NULL && decl_header->ast_type == decl_type)
            return decl_header->source_ast;
    }
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
           && type_node->data.type.name != NULL
           && depth < 32) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(
            ctx, type_node->data.type.name);
        if (alias_decl == NULL || alias_decl->data.type_alias.target_type == NULL)
            break;
        type_node = alias_decl->data.type_alias.target_type;
        depth++;
    }

    return type_node;
}

ASTNode *
find_subject_host_decl(TranspilerCtx *ctx, const char *subject_name)
{
    ASTNode *decl = find_class_decl(ctx, subject_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL
        && decl->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT) {
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
        return decl->data.class_decl.name;
    case AST_ENUM_DECL:
        return decl->data.enum_decl.name;
    case AST_ROLE_DECL:
        return decl->data.role_decl.name;
    case AST_PARTY_DECL:
        return decl->data.party_decl.name;
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.name;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.name;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.name;
    case AST_WORLD_DECL:
        return decl->data.world_decl.name;
    default:
        return NULL;
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
            ctx->last_decl_lookup_type = decl_type;
            snprintf(ctx->last_decl_lookup_name,
                sizeof(ctx->last_decl_lookup_name), "%s", name);
            ctx->last_decl_lookup_inventory = decls;
            ctx->last_decl_lookup_inventory_count = decl_count;
            ctx->last_decl_lookup_result = decl;
            ctx->last_decl_lookup_active_only = false;
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
            ctx->last_decl_lookup_type = decl_type;
            snprintf(ctx->last_decl_lookup_name,
                sizeof(ctx->last_decl_lookup_name), "%s", name);
            ctx->last_decl_lookup_inventory = decls;
            ctx->last_decl_lookup_inventory_count = decl_count;
            ctx->last_decl_lookup_result = decl;
            ctx->last_decl_lookup_active_only = true;
            return decl;
        }
    }

    return NULL;
}
