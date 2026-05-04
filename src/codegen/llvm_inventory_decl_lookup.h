/* LLVM MIR-backed declaration lookup helpers. */

#ifndef PGY_LLVM_INVENTORY_DECL_LOOKUP_H
#define PGY_LLVM_INVENTORY_DECL_LOOKUP_H

static inline const char *
llvm_decl_node_name(ASTNode *node);

static inline ASTNode *
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

static inline void
llvm_restore_current_host_decl(LLVMGenCtx *ctx, ASTNode *saved_decl)
{
    if (ctx == NULL)
        return;
    ctx->current_host_decl = saved_decl;
    ctx->current_class_name =
        saved_decl != NULL ? llvm_decl_node_name(saved_decl) : NULL;
}

static inline void
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

static inline const char *
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
        return node->data.ability_decl.name;
    case AST_ROLE_DECL:
        return node->data.role_decl.name;
    case AST_PARTY_DECL:
        return node->data.party_decl.name;
    case AST_ROSTER_DECL:
        return node->data.roster_decl.name;
    case AST_WORLD_DECL:
        return node->data.world_decl.name;
    case AST_RELATION_DECL:
        return node->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return node->data.effect_decl.name;
    case AST_ZONE_DECL:
        return node->data.zone_decl.name;
    case AST_EVENT_DECL:
        return node->data.event_decl.name;
    case AST_CLASS_DECL:
        return node->data.class_decl.name;
    case AST_ENUM_DECL:
        return node->data.enum_decl.name;
    case AST_TYPE_ALIAS:
        return node->data.type_alias.name;
    default:
        return NULL;
    }
}

static inline ASTNode *
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
        if (decl_header != NULL && decl_header->ast_type == decl_type)
            return decl_header->ast;
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

static inline bool
llvm_param_is_implicit_self(const FuncParam *param)
{
    return param != NULL
        && param->type == NULL
        && param->name != NULL
        && strcmp(param->name, "self") == 0;
}

static inline bool
llvm_is_host_decl_type(ASTNodeType decl_type)
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

static inline const MIRDeclHeader *
llvm_find_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || ctx->mir == NULL || name == NULL)
        return NULL;
    return mir_find_decl_header(ctx->mir, name);
}

static inline const MIRDeclHeader *
llvm_find_host_decl_header_in_context(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header =
        llvm_find_decl_header_in_context(ctx, name);

    if (decl_header == NULL || !llvm_is_host_decl_type(decl_header->ast_type))
        return NULL;
    return decl_header;
}

static inline ASTNode *
llvm_find_host_decl_in_active_inventory(const LLVMGenCtx *ctx, const char *name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode *decl = NULL;

    if (ctx == NULL || name == NULL)
        return NULL;

    decl_header = llvm_find_host_decl_header_in_context(ctx, name);
    if (decl_header != NULL)
        return decl_header->ast;

    decl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ENUM_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_PARTY_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ROLE_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ROSTER_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_RELATION_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_EFFECT_DECL, name);
    if (decl != NULL)
        return decl;
    decl = llvm_find_decl_in_active_inventory(ctx, AST_ZONE_DECL, name);
    if (decl != NULL)
        return decl;
    return llvm_find_decl_in_active_inventory(ctx, AST_WORLD_DECL, name);
}

static inline ASTNode *
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

static inline const char *
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

    switch (decl->type) {
    case AST_CLASS_DECL:
        return decl->data.class_decl.name;
    case AST_ENUM_DECL:
        return decl->data.enum_decl.name;
    case AST_PARTY_DECL:
        return decl->data.party_decl.name;
    case AST_ROLE_DECL:
        return decl->data.role_decl.name;
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

#endif /* PGY_LLVM_INVENTORY_DECL_LOOKUP_H */
