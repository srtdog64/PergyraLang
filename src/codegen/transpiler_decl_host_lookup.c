/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Host and method declaration lookup helpers for the C backend.
 */

#include <stdio.h>
#include <string.h>

#include "transpiler_decl_lookup.h"

typedef struct
{
    ASTNodeType owner_type;
    ASTNodeType lookup_type;
} TranspilerHostOwnerLookup;

static const TranspilerHostOwnerLookup kTranspilerHostOwnerLookups[] = {
    { AST_ZONE_DECL, AST_ZONE_DECL },
    { AST_RELATION_DECL, AST_RELATION_DECL },
    { AST_EFFECT_DECL, AST_EFFECT_DECL },
    { AST_WORLD_DECL, AST_WORLD_DECL },
    { AST_PARTY_DECL, AST_PARTY_DECL },
    { AST_ROSTER_DECL, AST_ROSTER_DECL },
    { AST_ENUM_DECL, AST_ENUM_DECL },
    { AST_CLASS_DECL, AST_CLASS_DECL },
};

static const ASTNodeType kTranspilerNominalHostLookupTypes[] = {
    AST_RELATION_DECL,
    AST_EFFECT_DECL,
    AST_ZONE_DECL,
    AST_WORLD_DECL,
    AST_PARTY_DECL,
    AST_ROLE_DECL,
    AST_ROSTER_DECL,
    AST_ENUM_DECL,
    AST_CLASS_DECL,
};

static size_t
transpiler_host_owner_lookup_count(void)
{
    return sizeof(kTranspilerHostOwnerLookups)
        / sizeof(kTranspilerHostOwnerLookups[0]);
}

static size_t
transpiler_nominal_host_lookup_type_count(void)
{
    return sizeof(kTranspilerNominalHostLookupTypes)
        / sizeof(kTranspilerNominalHostLookupTypes[0]);
}

static ASTNode *
transpiler_find_method_source_ast_in_mir_header(const MIRDeclHeader *header,
                                                const char *method_name)
{
    if (header == NULL || method_name == NULL)
        return NULL;

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        MIRDeclMethod *method = &header->method_metadata[i];
        if (method->name != NULL && strcmp(method->name, method_name) == 0)
            return method->source_ast;
    }

    return NULL;
}

ASTNode *
transpiler_find_host_decl_from_owner_local(TranspilerCtx *ctx,
                                           const char *owner_name,
                                           ASTNodeType owner_ast_type)
{
    ASTNode *current_host_decl;
    ASTNode *role_decl;

    if (ctx == NULL || owner_name == NULL)
        return NULL;

    current_host_decl = transpiler_current_host_decl_local(ctx);
    if (current_host_decl != NULL
        && current_host_decl->type == owner_ast_type) {
        const char *current_name = transpiler_decl_name_local(current_host_decl);
        if (current_name != NULL && strcmp(current_name, owner_name) == 0)
            return current_host_decl;
    }

    if (owner_ast_type == AST_ROLE_DECL) {
        role_decl = transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_ROLE_DECL, owner_name);
        if (role_decl != NULL
            && role_decl->type == AST_ROLE_DECL
            && role_decl->data.role_decl.for_type != NULL
            && role_decl->data.role_decl.for_type->type == AST_TYPE
            && role_decl->data.role_decl.for_type->data.type.name != NULL) {
            return transpiler_find_decl_in_active_inventory_only_local(
                ctx, AST_CLASS_DECL,
                role_decl->data.role_decl.for_type->data.type.name);
        }
        return NULL;
    }

    for (size_t i = 0; i < transpiler_host_owner_lookup_count(); i++) {
        const TranspilerHostOwnerLookup *lookup =
            &kTranspilerHostOwnerLookups[i];
        if (lookup->owner_type == owner_ast_type) {
            return transpiler_find_decl_in_active_inventory_only_local(
                ctx, lookup->lookup_type, owner_name);
        }
    }

    return transpiler_find_decl_in_active_inventory_only_local(
        ctx, AST_CLASS_DECL, owner_name);
}

const char *
transpiler_role_subject_name_local(TranspilerCtx *ctx, const char *role_name)
{
    ASTNode *role_decl;

    if (ctx == NULL || role_name == NULL)
        return NULL;

    role_decl = transpiler_find_decl_in_active_inventory_only_local(
        ctx, AST_ROLE_DECL, role_name);
    if (role_decl == NULL
        || role_decl->type != AST_ROLE_DECL
        || role_decl->data.role_decl.for_type == NULL
        || role_decl->data.role_decl.for_type->type != AST_TYPE
        || role_decl->data.role_decl.for_type->data.type.name == NULL) {
        return NULL;
    }

    return role_decl->data.role_decl.for_type->data.type.name;
}

void
transpiler_bind_current_host_decl_local(TranspilerCtx *ctx, ASTNode *decl)
{
    if (ctx == NULL)
        return;

    ctx->current_host_decl = decl;
}

ASTNode *
transpiler_current_host_decl_local(TranspilerCtx *ctx)
{
    if (ctx == NULL)
        return NULL;
    return ctx->current_host_decl;
}

ASTNode *
transpiler_find_nominal_host_decl_local(TranspilerCtx *ctx,
                                        const char *host_type_name)
{
    ASTNode *decl = NULL;

    if (ctx == NULL || host_type_name == NULL)
        return NULL;

    if (ctx->last_nominal_host_decl != NULL
        && ctx->last_nominal_host_mir == ctx->mir
        && strcmp(ctx->last_nominal_host_name, host_type_name) == 0) {
        return ctx->last_nominal_host_decl;
    }

    for (size_t i = 0; i < transpiler_nominal_host_lookup_type_count(); i++) {
        decl = transpiler_find_decl_in_inventory_local(
            ctx, kTranspilerNominalHostLookupTypes[i], host_type_name);
        if (decl != NULL)
            goto cache_and_return;
    }

    return NULL;

cache_and_return:
    snprintf(ctx->last_nominal_host_name,
        sizeof(ctx->last_nominal_host_name), "%s", host_type_name);
    ctx->last_nominal_host_mir = ctx->mir;
    ctx->last_nominal_host_decl = decl;
    return decl;
}

ASTNode *
current_host_method_decl(TranspilerCtx *ctx, const char *method_name)
{
    ASTNode *decl = NULL;
    const char *host_name = NULL;
    const MIRDeclHeader *header = NULL;
    TranspilerHostedMethodView method_view;

    if (ctx == NULL || method_name == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    host_name = transpiler_decl_name_local(decl);
    if (ctx->mir != NULL && host_name != NULL) {
        header = mir_find_decl_header(ctx->mir, host_name);
        if (header != NULL)
            return transpiler_find_method_source_ast_in_mir_header(
                header, method_name);
    }

    method_view = transpiler_hosted_method_view_from_decl(ctx, host_name, decl);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

ASTNode *
find_nominal_host_method_decl(TranspilerCtx *ctx, const char *host_type_name,
                              const char *method_name)
{
    ASTNode *decl = NULL;
    const MIRDeclHeader *header = NULL;
    ASTNode *method_from_mir = NULL;
    TranspilerHostedMethodView method_view;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    if (ctx->last_nominal_method_decl != NULL
        && ctx->last_nominal_method_mir == ctx->mir
        && strcmp(ctx->last_nominal_method_host_name, host_type_name) == 0
        && strcmp(ctx->last_nominal_method_name, method_name) == 0) {
        return ctx->last_nominal_method_decl;
    }

    if (ctx->mir != NULL) {
        header = mir_find_decl_header(ctx->mir, host_type_name);
        if (header != NULL) {
            method_from_mir = transpiler_find_method_source_ast_in_mir_header(
                header, method_name);
            if (method_from_mir == NULL)
                return NULL;
            snprintf(ctx->last_nominal_method_host_name,
                sizeof(ctx->last_nominal_method_host_name), "%s", host_type_name);
            snprintf(ctx->last_nominal_method_name,
                sizeof(ctx->last_nominal_method_name), "%s", method_name);
            ctx->last_nominal_method_mir = ctx->mir;
            ctx->last_nominal_method_decl = method_from_mir;
            return method_from_mir;
        }
    }

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    method_view = transpiler_hosted_method_view_from_decl(ctx, host_type_name, decl);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            snprintf(ctx->last_nominal_method_host_name,
                sizeof(ctx->last_nominal_method_host_name), "%s", host_type_name);
            snprintf(ctx->last_nominal_method_name,
                sizeof(ctx->last_nominal_method_name), "%s", method_name);
            ctx->last_nominal_method_mir = ctx->mir;
            ctx->last_nominal_method_decl = method;
            return method;
        }
    }

    return NULL;
}
