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

static bool
transpiler_decl_header_is_nominal_host(const MIRDeclHeader *header)
{
    if (header == NULL)
        return false;
    return transpiler_is_host_decl_type(header->ast_type);
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

static bool
transpiler_cache_name(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (dst == NULL || dst_size == 0 || src == NULL)
        return false;
    len = strlen(src);
    if (len >= dst_size) {
        dst[0] = '\0';
        return false;
    }
    memcpy(dst, src, len + 1);
    return true;
}

static void
transpiler_cache_nominal_host_decl(TranspilerCtx *ctx,
                                   const char *host_type_name,
                                   ASTNode *decl)
{
    if (ctx == NULL || host_type_name == NULL || decl == NULL)
        return;
    if (!transpiler_cache_name(ctx->last_nominal_host_name,
            sizeof(ctx->last_nominal_host_name), host_type_name)) {
        ctx->last_nominal_host_mir = NULL;
        ctx->last_nominal_host_decl = NULL;
        return;
    }
    ctx->last_nominal_host_mir = transpiler_active_mir_identity(ctx);
    ctx->last_nominal_host_decl = decl;
}

static void
transpiler_cache_nominal_method_decl(TranspilerCtx *ctx,
                                     const char *host_type_name,
                                     const char *method_name,
                                     ASTNode *method)
{
    if (ctx == NULL || host_type_name == NULL || method_name == NULL
        || method == NULL) {
        return;
    }
    if (!transpiler_cache_name(ctx->last_nominal_method_host_name,
            sizeof(ctx->last_nominal_method_host_name), host_type_name)
        || !transpiler_cache_name(ctx->last_nominal_method_name,
            sizeof(ctx->last_nominal_method_name), method_name)) {
        ctx->last_nominal_method_mir = NULL;
        ctx->last_nominal_method_decl = NULL;
        return;
    }
    ctx->last_nominal_method_mir = transpiler_active_mir_identity(ctx);
    ctx->last_nominal_method_decl = method;
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
        const char *subject_name;
        role_decl = transpiler_find_decl_in_active_inventory_only_local(
            ctx, AST_ROLE_DECL, owner_name);
        subject_name = transpiler_role_subject_type_name_local(role_decl);
        if (subject_name != NULL) {
            return transpiler_find_decl_in_active_inventory_only_local(
                ctx, AST_CLASS_DECL, subject_name);
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
transpiler_role_subject_type_name_local(ASTNode *role_decl)
{
    ASTNode *for_type = transpiler_role_subject_type_node_local(role_decl);

    if (for_type == NULL || for_type->type != AST_TYPE)
        return NULL;
    return ast_type_name(for_type);
}

ASTNode *
transpiler_role_subject_type_node_local(ASTNode *role_decl)
{
    return ast_role_for_type(role_decl);
}

const char *
transpiler_role_subject_name_local(TranspilerCtx *ctx, const char *role_name)
{
    ASTNode *role_decl;

    if (ctx == NULL || role_name == NULL)
        return NULL;

    role_decl = transpiler_find_decl_in_active_inventory_only_local(
        ctx, AST_ROLE_DECL, role_name);
    return transpiler_role_subject_type_name_local(role_decl);
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
        && ctx->last_nominal_host_mir == transpiler_active_mir_identity(ctx)
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
    transpiler_cache_nominal_host_decl(ctx, host_type_name, decl);
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
    header = transpiler_active_decl_header(ctx, host_name);
    if (transpiler_decl_header_is_nominal_host(header))
        return transpiler_find_method_source_ast_in_mir_header(
            header, method_name);

    method_view = transpiler_hosted_method_view_from_decl(ctx, host_name, decl);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        const char *candidate_name = ast_declaration_name(method);
        if (method != NULL && method->type == AST_FUNC_DECL
            && candidate_name != NULL
            && strcmp(candidate_name, method_name) == 0) {
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
        && ctx->last_nominal_method_mir == transpiler_active_mir_identity(ctx)
        && strcmp(ctx->last_nominal_method_host_name, host_type_name) == 0
        && strcmp(ctx->last_nominal_method_name, method_name) == 0) {
        return ctx->last_nominal_method_decl;
    }

    header = transpiler_active_decl_header(ctx, host_type_name);
    if (transpiler_decl_header_is_nominal_host(header)) {
        method_from_mir = transpiler_find_method_source_ast_in_mir_header(
            header, method_name);
        if (method_from_mir == NULL)
            return NULL;
        transpiler_cache_nominal_method_decl(ctx, host_type_name,
            method_name, method_from_mir);
        return method_from_mir;
    }

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    method_view = transpiler_hosted_method_view_from_decl(ctx, host_type_name, decl);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        const char *candidate_name = ast_declaration_name(method);
        if (method != NULL && method->type == AST_FUNC_DECL
            && candidate_name != NULL
            && strcmp(candidate_name, method_name) == 0) {
            transpiler_cache_nominal_method_decl(ctx, host_type_name,
                method_name, method);
            return method;
        }
    }

    return NULL;
}
