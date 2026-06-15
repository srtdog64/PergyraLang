/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend declaration lookup helpers.
 */

#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "host_decl_compat.h"
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

const char *
transpiler_decl_name_local(ASTNode *decl)
{
    const char *host_name;

    if (decl == NULL)
        return NULL;

    host_name = pgy_host_decl_compat_name(decl);
    if (host_name != NULL)
        return host_name;

    switch (decl->type) {
    case AST_FUNC_DECL:
        return ast_declaration_name(decl);
    case AST_INTENT_DECL:
        return ast_intent_decl_name(decl);
    case AST_TYPE_ALIAS:
        return ast_type_alias_name(decl);
    case AST_ABILITY_DECL:
        return ast_ability_name(decl);
    case AST_EVENT_DECL:
        return ast_event_name(decl);
    default:
        return NULL;
    }
}

static bool
transpiler_named_decl_matches(ASTNode *stmt, ASTNodeType decl_type,
                              const char *name)
{
    const char *stmt_name;

    if (stmt == NULL || name == NULL || stmt->type != decl_type)
        return false;

    stmt_name = transpiler_decl_name_local(stmt);
    return stmt_name != NULL && strcmp(stmt_name, name) == 0;
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
    decl_header = transpiler_active_decl_header_of_type(ctx, decl_type, name);
    if (transpiler_active_has_mir(ctx) && decl_header == NULL)
        return NULL;

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

bool
transpiler_decl_exists_local(TranspilerCtx *ctx, ASTNodeType decl_type,
                             const char *name)
{
    ASTNode **decls = NULL;
    size_t decl_count = 0;

    if (ctx == NULL || name == NULL)
        return false;
    if (transpiler_active_has_mir(ctx)) {
        return transpiler_active_decl_header_of_type(ctx, decl_type, name)
            != NULL;
    }

    transpiler_active_inventory(ctx, decl_type, &decls, &decl_count);
    for (size_t i = 0; decls != NULL && i < decl_count; i++) {
        if (transpiler_named_decl_matches(decls[i], decl_type, name))
            return true;
    }
    return false;
}

const MIRDeclField *
transpiler_find_decl_field_metadata(const TranspilerCtx *ctx,
                                    const char *host_name,
                                    const char *field_name)
{
    const MIRDeclHeader *header;

    if (ctx == NULL || host_name == NULL || field_name == NULL)
        return NULL;
    header = transpiler_active_host_decl_header(ctx, host_name);
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        const char *name = mir_decl_field_name(field);
        if (name != NULL && strcmp(name, field_name) == 0)
            return field;
    }
    return NULL;
}

ASTNode *
transpiler_mir_decl_field_type(const MIRDeclField *field)
{
    return mir_decl_field_type(field);
}

const char *
transpiler_mir_decl_field_type_name(const MIRDeclField *field)
{
    return mir_decl_field_type_name(field);
}

MIRDeclFieldKind
transpiler_mir_decl_field_kind_or(const MIRDeclField *field,
                                  MIRDeclFieldKind fallback)
{
    return mir_decl_field_kind_or(field, fallback);
}

bool
transpiler_mir_decl_field_is_subject_like(const MIRDeclField *field)
{
    return mir_decl_field_is_subject_like(field);
}

bool
transpiler_mir_decl_field_is_tobject_like(const MIRDeclField *field)
{
    return mir_decl_field_is_tobject_like(field);
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

bool
transpiler_function_decl_exists_local(TranspilerCtx *ctx,
                                      const char *function_name)
{
    if (ctx == NULL || function_name == NULL)
        return false;
    if (transpiler_decl_exists_local(ctx, AST_FUNC_DECL, function_name))
        return true;
    return find_extern_function_decl(ctx, function_name) != NULL;
}

bool
transpiler_decl_is_extern_function(const TranspilerCtx *ctx,
                                   const ASTNode *decl)
{
    ASTNode **externs = NULL;
    size_t extern_count = 0;

    if (ctx == NULL || decl == NULL || decl->type != AST_FUNC_DECL)
        return false;

    transpiler_active_inventory(ctx, AST_EXTERN_BLOCK, &externs,
                                &extern_count);
    for (size_t i = 0; externs != NULL && i < extern_count; i++) {
        ASTNode *block = externs[i];
        size_t block_decl_count = 0;
        if (block == NULL || block->type != AST_EXTERN_BLOCK)
            continue;
        (void)ast_extern_block_declarations(block, &block_decl_count);
        for (size_t j = 0; j < block_decl_count; j++) {
            if (ast_extern_block_declaration(block, j) == decl)
                return true;
        }
    }
    return false;
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

bool
transpiler_callable_decl_exists_local(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return false;
    return transpiler_function_decl_exists_local(ctx, name)
        || transpiler_decl_exists_local(ctx, AST_INTENT_DECL, name);
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
transpiler_find_projection_nominal_decl_local(TranspilerCtx *ctx,
                                              const char *name)
{
    return transpiler_find_named_decl_local(ctx, AST_CLASS_DECL, name);
}

bool
transpiler_projection_nominal_decl_exists_local(TranspilerCtx *ctx,
                                                const char *name)
{
    return transpiler_decl_exists_local(ctx, AST_CLASS_DECL, name);
}

ASTNode *
transpiler_find_type_alias_decl(TranspilerCtx *ctx, const char *alias_name)
{
    return transpiler_find_named_decl_local(ctx, AST_TYPE_ALIAS, alias_name);
}

const char *
transpiler_type_alias_target_type_name_from_headers(TranspilerCtx *ctx,
                                                    const char *alias_name)
{
    MIRDeclHeaderInventory inventory;

    if (ctx == NULL || alias_name == NULL || !transpiler_active_has_mir(ctx))
        return NULL;
    transpiler_active_decl_header_inventory(ctx, &inventory);
    return mir_decl_header_inventory_resolve_type_alias_target_type_name(
        &inventory, alias_name);
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
    ASTNode *decl = transpiler_find_projection_nominal_decl_local(
        ctx, subject_name);
    if (decl != NULL && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT) {
        return decl;
    }
    return NULL;
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
    const ASTNodeType *host_lookup_types = NULL;
    size_t host_lookup_type_count = 0;

    if (ctx == NULL || name == NULL)
        return false;

    host_lookup_types =
        pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count);
    for (size_t i = 0; host_lookup_types != NULL
         && i < host_lookup_type_count; i++) {
        if (transpiler_active_has_mir(ctx)) {
            if (transpiler_active_decl_header_of_type(
                    ctx, host_lookup_types[i], name) != NULL) {
                return true;
            }
            continue;
        }
        if (transpiler_find_decl_in_inventory_local(
                ctx, host_lookup_types[i], name)
            != NULL) {
            return true;
        }
    }
    return false;
}

ASTNode *
transpiler_find_domain_constructor_decl_local(TranspilerCtx *ctx,
                                              const char *name)
{
    const ASTNodeType *constructor_types = NULL;
    size_t constructor_type_count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    constructor_types =
        pgy_host_decl_compat_constructor_domain_types(
            &constructor_type_count);
    for (size_t i = 0; constructor_types != NULL
         && i < constructor_type_count; i++) {
        ASTNode *decl = transpiler_find_named_decl_local(
            ctx, constructor_types[i], name);
        if (decl != NULL)
            return decl;
    }
    return NULL;
}

bool
transpiler_domain_constructor_decl_exists_local(TranspilerCtx *ctx,
                                                const char *name)
{
    const ASTNodeType *constructor_types = NULL;
    size_t constructor_type_count = 0;

    if (ctx == NULL || name == NULL)
        return false;

    constructor_types =
        pgy_host_decl_compat_constructor_domain_types(
            &constructor_type_count);
    for (size_t i = 0; constructor_types != NULL
         && i < constructor_type_count; i++) {
        if (transpiler_decl_exists_local(ctx, constructor_types[i], name))
            return true;
    }
    return false;
}

bool
transpiler_is_host_decl_type(ASTNodeType decl_type)
{
    return pgy_host_decl_compat_is_type(decl_type);
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
    if (transpiler_active_has_mir(ctx))
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
    if (transpiler_active_has_mir(ctx))
        return transpiler_find_named_decl_local(ctx, decl_type, name);

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
