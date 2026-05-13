/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR-backed declaration lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static const ASTNodeType kLLVMHostDeclTypes[] = {
    AST_CLASS_DECL,
    AST_ENUM_DECL,
    AST_PARTY_DECL,
    AST_ROLE_DECL,
    AST_ROSTER_DECL,
    AST_RELATION_DECL,
    AST_EFFECT_DECL,
    AST_ZONE_DECL,
    AST_WORLD_DECL,
};

static size_t
llvm_host_decl_type_count(void)
{
    return sizeof(kLLVMHostDeclTypes) / sizeof(kLLVMHostDeclTypes[0]);
}

ASTNode *
llvm_bind_current_host_decl(LLVMGenCtx *ctx, ASTNode *host_decl)
{
    ASTNode *saved_decl = NULL;

    if (ctx == NULL)
        return NULL;
    saved_decl = ctx->current_host_decl;
    ctx->current_host_decl = host_decl;
    ctx->current_class_name =
        host_decl != NULL ? llvm_decl_node_name(host_decl) : NULL;
    return saved_decl;
}

void
llvm_restore_current_host_decl(LLVMGenCtx *ctx, ASTNode *saved_decl)
{
    if (ctx == NULL)
        return;
    ctx->current_host_decl = saved_decl;
    ctx->current_class_name =
        saved_decl != NULL ? llvm_decl_node_name(saved_decl) : NULL;
}

void
llvm_active_inventory(const LLVMGenCtx *ctx,
                      ASTNodeType decl_type,
                      ASTNode ***nodes_out,
                      size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL)
        mir_active_inventory(ctx->mir, decl_type, &nodes, &count);

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

const char *
llvm_decl_node_name(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_FUNC_DECL:
        return node->data.func_decl.name;
    case AST_INTENT_DECL:
        return node->data.intent_decl.name;
    case AST_ABILITY_DECL:
        return ast_ability_name(node);
    case AST_ROLE_DECL:
        return ast_role_name(node);
    case AST_PARTY_DECL:
        return ast_party_name(node);
    case AST_ROSTER_DECL:
        return ast_roster_name(node);
    case AST_WORLD_DECL:
        return ast_world_name(node);
    case AST_RELATION_DECL:
        return ast_relation_name(node);
    case AST_EFFECT_DECL:
        return ast_effect_name(node);
    case AST_ZONE_DECL:
        return ast_zone_name(node);
    case AST_EVENT_DECL:
        return node->data.event_decl.name;
    case AST_CLASS_DECL:
        return ast_class_name(node);
    case AST_ENUM_DECL:
        return ast_enum_name(node);
    case AST_TYPE_ALIAS:
        return node->data.type_alias.name;
    default:
        return NULL;
    }
}

ASTNode *
llvm_find_decl_in_active_inventory(const LLVMGenCtx *ctx,
                                   ASTNodeType decl_type,
                                   const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx == NULL || name == NULL)
        return NULL;

    if (ctx->mir != NULL) {
        decl_header = mir_find_decl_header(ctx->mir, name);
        if (decl_header != NULL)
            return decl_header->ast_type == decl_type
                ? decl_header->source_ast
                : NULL;
    }

    llvm_active_inventory(ctx, decl_type, &nodes, &count);
    for (size_t i = 0; i < count; i++) {
        ASTNode *node = nodes != NULL ? nodes[i] : NULL;
        const char *node_name;
        if (node == NULL || node->type != decl_type)
            continue;
        node_name = llvm_decl_node_name(node);
        if (node_name != NULL && strcmp(node_name, name) == 0)
            return node;
    }

    return NULL;
}

bool
llvm_param_is_implicit_self(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

bool
llvm_is_host_decl_type(ASTNodeType decl_type)
{
    for (size_t i = 0; i < llvm_host_decl_type_count(); i++) {
        if (kLLVMHostDeclTypes[i] == decl_type)
            return true;
    }
    return false;
}

const MIRDeclHeader *
llvm_find_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    return mir_find_decl_header(ctx->mir, name);
}

const MIRDeclHeader *
llvm_find_host_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header =
        llvm_find_decl_header_in_context(ctx, name);

    if (decl_header == NULL || !llvm_is_host_decl_type(decl_header->ast_type))
        return NULL;
    return decl_header;
}

ASTNode *
llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header = NULL;

    if (ctx == NULL || name == NULL)
        return NULL;

    decl_header = llvm_find_host_decl_header_in_context(ctx, name);
    if (decl_header != NULL)
        return decl_header->source_ast;

    for (size_t i = 0; i < llvm_host_decl_type_count(); i++) {
        ASTNode *decl = llvm_find_decl_in_active_inventory(
            ctx, kLLVMHostDeclTypes[i], name);
        if (decl != NULL)
            return decl;
    }

    return NULL;
}

ASTNode *
llvm_current_host_decl(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    if (ctx->current_host_decl != NULL)
        return ctx->current_host_decl;

    if (ctx->current_func_decl != NULL
        && ctx->current_func_decl->type == AST_FUNC_DECL
        && ctx->current_func_decl->data.func_decl.within_zone != NULL) {
        decl = llvm_find_decl_in_active_inventory(
            ctx, AST_ZONE_DECL, ctx->current_func_decl->data.func_decl.within_zone);
        if (decl != NULL)
            return decl;
    }

    return NULL;
}

const char *
llvm_current_host_decl_name(const LLVMGenCtx *ctx)
{
    ASTNode *decl = NULL;

    if (ctx == NULL)
        return NULL;

    decl = llvm_current_host_decl(ctx);
    if (decl == NULL) {
        if (ctx->current_func_decl != NULL
            && ctx->current_func_decl->type == AST_FUNC_DECL
            && ctx->current_func_decl->data.func_decl.within_zone != NULL) {
            return ctx->current_func_decl->data.func_decl.within_zone;
        }
        return NULL;
    }

    return llvm_is_host_decl_type(decl->type)
        ? llvm_decl_node_name(decl)
        : NULL;
}

#endif /* PGY_LLVM_ENABLED */
