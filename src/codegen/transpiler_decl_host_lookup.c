/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Host and method declaration lookup helpers for the C backend.
 */

#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"

static bool
transpiler_decl_header_is_nominal_host(const MIRDeclHeader *header)
{
    if (header == NULL)
        return false;
    return transpiler_is_host_decl_type(
        mir_decl_header_ast_type_or(header, AST_PROGRAM));
}

static const MIRDeclMethod *
transpiler_find_method_metadata_in_header(const MIRDeclHeader *header,
                                          const char *method_name)
{
    if (header == NULL || method_name == NULL)
        return NULL;

    for (size_t i = 0; i < mir_decl_header_method_count(header); i++) {
        const MIRDeclMethod *method = mir_decl_header_method(header, i);
        const char *name = transpiler_mir_decl_method_name(method);
        if (name != NULL && strcmp(name, method_name) == 0)
            return method;
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

    if (pgy_host_decl_compat_is_type(owner_ast_type)) {
        return transpiler_find_decl_in_active_inventory_only_local(
            ctx, owner_ast_type, owner_name);
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
    const ASTNodeType *host_lookup_types = NULL;
    size_t host_lookup_type_count = 0;

    if (ctx == NULL || host_type_name == NULL)
        return NULL;

    if (ctx->last_nominal_host_decl != NULL
        && ctx->last_nominal_host_mir == transpiler_active_mir_identity(ctx)
        && strcmp(ctx->last_nominal_host_name, host_type_name) == 0) {
        return ctx->last_nominal_host_decl;
    }

    host_lookup_types =
        pgy_host_decl_compat_nominal_lookup_types(&host_lookup_type_count);
    for (size_t i = 0; host_lookup_types != NULL
         && i < host_lookup_type_count; i++) {
        decl = transpiler_find_named_decl_local(
            ctx, host_lookup_types[i], host_type_name);
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
    header = transpiler_active_host_decl_header(ctx, host_name);
    if (transpiler_decl_header_is_nominal_host(header)) {
        const MIRDeclMethod *method_meta =
            transpiler_find_method_metadata_in_header(header, method_name);
        return transpiler_mir_decl_method_body_decl(ctx, method_meta);
    }

    method_view = transpiler_hosted_method_view_from_decl(ctx, host_name, decl);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method = NULL;
        const char *candidate_name = NULL;
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        if (method_meta != NULL) {
            candidate_name = transpiler_mir_decl_method_name(method_meta);
            if (candidate_name != NULL
                && strcmp(candidate_name, method_name) == 0) {
                return transpiler_mir_decl_method_body_decl(ctx, method_meta);
            }
            continue;
        }
        method = method_view.ast_compat_methods != NULL
            ? method_view.ast_compat_methods[i]
            : NULL;
        candidate_name = ast_declaration_name(method);
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

    header = transpiler_active_host_decl_header(ctx, host_type_name);
    if (transpiler_decl_header_is_nominal_host(header)) {
        const MIRDeclMethod *method_meta =
            transpiler_find_method_metadata_in_header(header, method_name);
        method_from_mir = transpiler_mir_decl_method_body_decl(ctx,
            method_meta);
        if (method_from_mir == NULL)
            return NULL;
        transpiler_cache_nominal_method_decl(ctx, host_type_name,
            method_name, method_from_mir);
        return method_from_mir;
    }

    decl = transpiler_find_nominal_host_decl_local(ctx, host_type_name);
    method_view = transpiler_hosted_method_view_from_decl(ctx, host_type_name, decl);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method = NULL;
        const char *candidate_name = NULL;
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        if (method_meta != NULL) {
            candidate_name = transpiler_mir_decl_method_name(method_meta);
            if (candidate_name != NULL
                && strcmp(candidate_name, method_name) == 0) {
                method = transpiler_mir_decl_method_body_decl(ctx,
                    method_meta);
                if (method == NULL)
                    return NULL;
                transpiler_cache_nominal_method_decl(ctx, host_type_name,
                    method_name, method);
                return method;
            }
            continue;
        }
        method = method_view.ast_compat_methods != NULL
            ? method_view.ast_compat_methods[i]
            : NULL;
        candidate_name = ast_declaration_name(method);
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
