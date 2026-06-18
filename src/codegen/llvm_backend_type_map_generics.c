/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend generic-default type-map resolution.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "host_decl_compat.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_internal.h"
#include "llvm_internal.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

#include <string.h>

static const char *
llvm_generic_default_name_from_header(const MIRDeclHeader *header,
                                      const char *type_name)
{
    if (header == NULL || type_name == NULL)
        return NULL;
    for (size_t i = 0; i < mir_decl_header_generic_param_count(header); i++) {
        const MIRDeclGenericParam *param =
            mir_decl_header_generic_param(header, i);
        const char *param_name = mir_decl_generic_param_name(param);
        if (param_name == NULL || strcmp(param_name, type_name) != 0)
            continue;
        {
            const char *default_name =
                mir_decl_generic_param_default_type_name(param);
            if (default_name != NULL)
                return default_name;
        }
        return mir_decl_generic_param_constraint_type_name(param);
    }
    return NULL;
}

static ASTNode *
llvm_generic_default_from_params(GenericParams *params, const char *type_name)
{
    if (params == NULL || type_name == NULL)
        return NULL;
    size_t param_count = ast_generic_param_count(params);
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        if (param == NULL || ast_generic_param_name(param) == NULL)
            continue;
        if (strcmp(ast_generic_param_name(param), type_name) == 0) {
            if (ast_generic_param_default_type(param) != NULL)
                return ast_generic_param_default_type(param);
            if (ast_generic_param_constraint(param) != NULL)
                return ast_generic_param_constraint(param);
            return NULL;
        }
    }
    return NULL;
}

static ASTNode *
llvm_generic_default_from_decl(ASTNode *decl, const char *type_name)
{
    if (decl == NULL || type_name == NULL)
        return NULL;

    return llvm_generic_default_from_params(
        ast_declaration_generic_params(decl), type_name);
}

static bool
llvm_generic_default_candidate_keep(LLVMGenCtx *ctx,
                                    ASTNode **candidate,
                                    ASTNode *resolved)
{
    char *candidate_name;
    char *resolved_name;

    if (candidate == NULL || resolved == NULL)
        return true;
    if (*candidate == NULL) {
        *candidate = resolved;
        return true;
    }

    candidate_name =
        llvm_render_type_name_scratch_in_ctx(ctx, *candidate, &ctx->scratch);
    resolved_name =
        llvm_render_type_name_scratch_in_ctx(ctx, resolved, &ctx->scratch);
    return candidate_name != NULL
        && resolved_name != NULL
        && strcmp(candidate_name, resolved_name) == 0;
}

static bool
llvm_generic_default_name_candidate_keep(const char **candidate,
                                         const char *resolved)
{
    if (candidate == NULL || resolved == NULL)
        return true;
    if (*candidate == NULL) {
        *candidate = resolved;
        return true;
    }
    return strcmp(*candidate, resolved) == 0;
}

static const char *
llvm_find_generic_default_name_in_mir_inventory(LLVMGenCtx *ctx,
                                                const char *type_name)
{
    LLVMMIRDeclHeaderInventory headers;
    const char *candidate = NULL;

    llvm_active_decl_header_inventory(ctx, &headers);
    for (size_t i = 0; i < headers.count; i++) {
        const MIRDeclHeader *header =
            llvm_decl_header_inventory_get(&headers, i);
        const char *resolved =
            llvm_generic_default_name_from_header(header, type_name);
        if (resolved == NULL)
            continue;
        if (!llvm_generic_default_name_candidate_keep(&candidate, resolved))
            return NULL;
    }

    return candidate;
}

static ASTNode *
llvm_find_generic_default_in_compat_inventory(LLVMGenCtx *ctx,
                                              const char *type_name)
{
    ASTNode *candidate = NULL;
    ASTNode *resolved = NULL;
    const ASTNodeType direct_decl_types[] = {
        AST_FUNC_DECL,
        AST_ABILITY_DECL
    };
    const ASTNodeType *host_decl_types = NULL;
    size_t host_decl_type_count = 0;

    for (size_t kind = 0;
         kind < sizeof(direct_decl_types) / sizeof(direct_decl_types[0]);
         kind++) {
        ASTNode **nodes = NULL;
        size_t count = 0;
        llvm_active_inventory(ctx, direct_decl_types[kind], &nodes, &count);
        for (size_t i = 0; i < count; i++) {
            resolved = llvm_generic_default_from_decl(
                nodes != NULL ? nodes[i] : NULL, type_name);
            if (resolved == NULL)
                continue;
            if (!llvm_generic_default_candidate_keep(ctx, &candidate, resolved))
                return NULL;
        }
    }

    host_decl_types = pgy_host_decl_compat_types(&host_decl_type_count);
    for (size_t kind = 0; host_decl_types != NULL
         && kind < host_decl_type_count; kind++) {
        ASTNode **nodes = NULL;
        size_t count = 0;
        llvm_active_inventory(ctx, host_decl_types[kind], &nodes, &count);
        for (size_t i = 0; i < count; i++) {
            resolved = llvm_generic_default_from_decl(
                nodes != NULL ? nodes[i] : NULL, type_name);
            if (resolved == NULL)
                continue;
            if (!llvm_generic_default_candidate_keep(ctx, &candidate, resolved))
                return NULL;
        }
    }

    return candidate;
}

static const char *
llvm_find_generic_default_name_in_mir_context(LLVMGenCtx *ctx,
                                              const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return NULL;

    if (llvm_active_has_mir(ctx)) {
        if (ctx->current_host_decl != NULL) {
            const char *decl_name =
                llvm_decl_node_name(ctx->current_host_decl);
            const MIRDeclHeader *header =
                llvm_find_decl_header_in_context_of_type(
                    ctx, ctx->current_host_decl->type, decl_name);
            const char *resolved =
                llvm_generic_default_name_from_header(header, type_name);
            if (resolved != NULL)
                return resolved;
        }
    }
    return llvm_find_generic_default_name_in_mir_inventory(ctx, type_name);
}

static ASTNode *
llvm_find_generic_default_in_compat_context(LLVMGenCtx *ctx,
                                            const char *type_name)
{
    ASTNode *resolved = NULL;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    resolved = llvm_generic_default_from_decl(ctx->current_host_decl, type_name);
    if (resolved != NULL)
        return resolved;

    return llvm_find_generic_default_in_compat_inventory(ctx, type_name);
}

LLVMTypeRef
llvm_resolve_generic_formal_default(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *default_type;
    const char *default_type_name;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    if (llvm_active_has_mir(ctx)) {
        default_type_name =
            llvm_find_generic_default_name_in_mir_context(ctx, type_name);
        if (default_type_name == NULL
            || strcmp(default_type_name, type_name) == 0) {
            return NULL;
        }
        return pergyra_type_to_llvm(ctx, default_type_name);
    }

    default_type = llvm_find_generic_default_in_compat_context(ctx, type_name);
    if (default_type == NULL)
        return NULL;

    return ast_type_to_llvm(ctx, default_type);
}


#endif /* PGY_LLVM_ENABLED */
