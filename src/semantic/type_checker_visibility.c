/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type checker visibility / access-control helpers (implementation).
 *
 * Visibility checks are owned by this translation unit and exported through a
 * narrow internal header so the rest of the type checker does not depend on
 * include-order side effects.
 */

#include <string.h>

#include "type_checker_visibility.h"

bool
nominal_decl_matches_runtime_type(ASTNode *decl, Type *object_type)
{
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && object_type != NULL
        && object_type->kind == TYPE_KIND_CLASS
        && ast_class_name(decl) != NULL
        && object_type->name != NULL
        && strcmp(ast_class_name(decl), object_type->name) == 0;
}

bool
private_member_access_allowed(ASTNode *decl, Type *object_type, SemanticContext *ctx)
{
    ASTNode *host;

    if (!nominal_decl_matches_runtime_type(decl, object_type) || ctx == NULL)
        return false;

    host = current_host_decl(ctx);
    if (host == NULL || host->type != AST_CLASS_DECL)
        return false;

    if (host == decl)
        return true;

    return ast_class_name(host) != NULL
        && ast_class_name(decl) != NULL
        && strcmp(ast_class_name(host), ast_class_name(decl)) == 0;
}

bool
same_module_origin(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

bool
cross_module_member_access(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || ctx == NULL)
        return false;
    if (ctx->current_module_path == NULL || decl->origin_path == NULL)
        return false;
    return !same_module_origin(ctx->current_module_path, decl->origin_path);
}

bool
explicit_member_access_allowed(ASTNode *decl,
                               Type *object_type,
                               AccessModifier access,
                               bool has_explicit_access,
                               SemanticContext *ctx)
{
    if (!has_explicit_access)
        return true;
    if (access == ACCESS_PUBLIC)
        return true;
    if (access == ACCESS_PRIVATE)
        return private_member_access_allowed(decl, object_type, ctx);
    if (access == ACCESS_PROTECTED) {
        if (private_member_access_allowed(decl, object_type, ctx))
            return true;
        return !cross_module_member_access(decl, ctx);
    }
    return true;
}

bool
explicit_type_reference_allowed(ASTNode *decl, const ASTNode *site, SemanticContext *ctx)
{
    const char *site_module = NULL;

    if (decl == NULL || site == NULL || ctx == NULL)
        return true;
    site_module = site->origin_path != NULL
        ? site->origin_path
        : ctx->current_module_path;
    if (site_module == NULL || decl->origin_path == NULL)
        return true;
    if (same_module_origin(site_module, decl->origin_path))
        return true;
    if (decl->type == AST_ABILITY_DECL) {
        if (!decl->data.ability_decl.has_explicit_access)
            return true;
        return decl->data.ability_decl.access == ACCESS_PUBLIC;
    }
    return decl->is_exported;
}
