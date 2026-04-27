/* LLVM backend MIR/DIR inventory helper layer.
 * Included by llvm_internal.h after LLVMGenCtx is fully defined. */

#ifndef PGY_LLVM_INVENTORY_INTERNAL_H
#define PGY_LLVM_INVENTORY_INTERNAL_H

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

    if (ctx != NULL && ctx->mir != NULL) {
        switch (decl_type) {
        case AST_FUNC_DECL: nodes = ctx->mir->functions; count = ctx->mir->function_count; break;
        case AST_INTENT_DECL: nodes = ctx->mir->intents; count = ctx->mir->intent_count; break;
        case AST_ABILITY_DECL: nodes = ctx->mir->abilities; count = ctx->mir->ability_count; break;
        case AST_ROLE_DECL: nodes = ctx->mir->roles; count = ctx->mir->role_count; break;
        case AST_PARTY_DECL: nodes = ctx->mir->parties; count = ctx->mir->party_count; break;
        case AST_ROSTER_DECL: nodes = ctx->mir->rosters; count = ctx->mir->roster_count; break;
        case AST_WORLD_DECL: nodes = ctx->mir->worlds; count = ctx->mir->world_count; break;
        case AST_RELATION_DECL: nodes = ctx->mir->relations; count = ctx->mir->relation_count; break;
        case AST_EFFECT_DECL: nodes = ctx->mir->effects; count = ctx->mir->effect_count; break;
        case AST_ZONE_DECL: nodes = ctx->mir->zones; count = ctx->mir->zone_count; break;
        case AST_EVENT_DECL: nodes = ctx->mir->events; count = ctx->mir->event_count; break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:
        case AST_TYPE_ALIAS:
            nodes = ctx->mir->types; count = ctx->mir->type_count; break;
        default:
            break;
        }
    }

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

static inline void
llvm_host_decl_methods(const MIRDeclHeader *decl_header,
                       ASTNode *decl,
                       ASTNode ***methods_out,
                       size_t *method_count_out)
{
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (decl_header != NULL && llvm_is_host_decl_type(decl_header->ast_type)) {
        methods = decl_header->methods;
        method_count = decl_header->method_count;
    }

    if (methods == NULL && decl != NULL) {
        switch (decl->type) {
        case AST_CLASS_DECL:
            methods = decl->data.class_decl.methods;
            method_count = decl->data.class_decl.method_count;
            break;
        case AST_ENUM_DECL:
            methods = decl->data.enum_decl.methods;
            method_count = decl->data.enum_decl.method_count;
            break;
        case AST_RELATION_DECL:
            methods = decl->data.relation_decl.methods;
            method_count = decl->data.relation_decl.method_count;
            break;
        case AST_EFFECT_DECL:
            methods = decl->data.effect_decl.methods;
            method_count = decl->data.effect_decl.method_count;
            break;
        case AST_ZONE_DECL:
            methods = decl->data.zone_decl.methods;
            method_count = decl->data.zone_decl.method_count;
            break;
        case AST_WORLD_DECL:
            methods = decl->data.world_decl.methods;
            method_count = decl->data.world_decl.method_count;
            break;
        default:
            break;
        }
    }

    if (methods_out != NULL)
        *methods_out = methods;
    if (method_count_out != NULL)
        *method_count_out = method_count;
}

static inline void
llvm_host_decl_method_metadata(const MIRDeclHeader *decl_header,
                               const MIRDeclMethod **methods_out,
                               size_t *method_count_out)
{
    const MIRDeclMethod *methods = NULL;
    size_t method_count = 0;

    if (decl_header != NULL && llvm_is_host_decl_type(decl_header->ast_type)) {
        methods = decl_header->method_metadata;
        method_count = decl_header->method_metadata_count;
    }

    if (methods_out != NULL)
        *methods_out = methods;
    if (method_count_out != NULL)
        *method_count_out = method_count;
}

static inline const MIRDeclMethod *
llvm_find_host_method_metadata_in_context(const LLVMGenCtx *ctx,
                                          const char *host_type_name,
                                          const char *method_name)
{
    const MIRDeclHeader *decl_header;
    const MIRDeclMethod *methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    decl_header = llvm_find_host_decl_header_in_context(ctx, host_type_name);
    llvm_host_decl_method_metadata(decl_header, &methods, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        if (methods[i].name != NULL && strcmp(methods[i].name, method_name) == 0)
            return &methods[i];
    }

    return NULL;
}

static inline const char *
llvm_mir_decl_method_name(const MIRDeclMethod *method, ASTNode *fallback)
{
    if (method != NULL && method->name != NULL)
        return method->name;
    if (fallback != NULL && fallback->type == AST_FUNC_DECL)
        return fallback->data.func_decl.name;
    return NULL;
}

static inline size_t
llvm_mir_decl_method_param_count(const MIRDeclMethod *method, ASTNode *fallback)
{
    if (method != NULL)
        return method->param_count;
    if (fallback != NULL && fallback->type == AST_FUNC_DECL)
        return fallback->data.func_decl.param_count;
    return 0;
}

static inline FuncParam *
llvm_mir_decl_method_param(const MIRDeclMethod *method,
                           ASTNode *fallback,
                           size_t index)
{
    if (method != NULL && method->params != NULL && index < method->param_count)
        return method->params[index];
    if (fallback != NULL && fallback->type == AST_FUNC_DECL
        && fallback->data.func_decl.params != NULL
        && index < fallback->data.func_decl.param_count) {
        return fallback->data.func_decl.params[index];
    }
    return NULL;
}

static inline ASTNode *
llvm_mir_decl_method_return_type(const MIRDeclMethod *method, ASTNode *fallback)
{
    if (method != NULL && method->return_type != NULL)
        return method->return_type;
    if (fallback != NULL && fallback->type == AST_FUNC_DECL)
        return fallback->data.func_decl.return_type;
    return NULL;
}

static inline bool
llvm_mir_decl_method_is_action_like(const MIRDeclMethod *method,
                                    ASTNode *fallback)
{
    if (method != NULL)
        return method->is_action_like;
    if (fallback != NULL && fallback->type == AST_FUNC_DECL)
        return fallback->data.func_decl.is_action;
    return false;
}

static inline ASTNode *
llvm_find_host_method_decl_in_context(const LLVMGenCtx *ctx,
                                      const char *host_type_name,
                                      const char *method_name)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode *decl = NULL;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    decl_header = llvm_find_host_decl_header_in_context(ctx, host_type_name);
    if (decl_header != NULL) {
        const MIRDeclMethod *method =
            llvm_find_host_method_metadata_in_context(
                ctx, host_type_name, method_name);
        if (method != NULL) {
            return method->ast;
        }
        decl = decl_header->ast;
    }

    if (decl == NULL)
        decl = llvm_find_host_decl_in_active_inventory(ctx, host_type_name);
    if (decl == NULL)
        return NULL;

    llvm_host_decl_methods(decl_header, decl, &methods, &method_count);
    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

static inline void
llvm_find_host_decl_methods_in_context(const LLVMGenCtx *ctx,
                                       const char *host_type_name,
                                       ASTNode ***methods_out,
                                       size_t *method_count_out)
{
    const MIRDeclHeader *decl_header = NULL;
    ASTNode *decl = NULL;

    if (methods_out != NULL)
        *methods_out = NULL;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (ctx == NULL || host_type_name == NULL)
        return;

    decl_header = llvm_find_host_decl_header_in_context(ctx, host_type_name);
    if (decl_header != NULL) {
        llvm_host_decl_methods(decl_header, NULL, methods_out, method_count_out);
        return;
    }

    if (decl == NULL)
        decl = llvm_find_host_decl_in_active_inventory(ctx, host_type_name);
    if (decl == NULL)
        return;

    llvm_host_decl_methods(decl_header, decl, methods_out, method_count_out);
}

static inline void
llvm_active_nominal_inventory(const LLVMGenCtx *ctx,
                              ASTNode ***nodes_out,
                              size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        nodes = ctx->mir->types;
        count = ctx->mir->type_count;
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

typedef struct
{
    ASTNode **abilities;
    ASTNode **relations;
    ASTNode **effects;
    ASTNode **zones;
    ASTNode **worlds;
    ASTNode **parties;
    ASTNode **rosters;
    ASTNode **roles;
    ASTNode **events;
    size_t ability_count;
    size_t relation_count;
    size_t effect_count;
    size_t zone_count;
    size_t world_count;
    size_t party_count;
    size_t roster_count;
    size_t role_count;
    size_t event_count;
} LLVMDomainInventory;

typedef struct
{
    const MIRRoutine *routines;
    size_t            count;
} LLVMMIRRoutineInventory;

static inline void
llvm_active_routine_inventory(const LLVMGenCtx *ctx,
                              LLVMMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;

    if (ctx == NULL || ctx->mir == NULL)
        return;

    inventory->routines = ctx->mir->routines;
    inventory->count = ctx->mir->routine_count;
}

static inline void
llvm_mir_routine_inventory_from_program(const MIRProgram *mir,
                                        LLVMMIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;

    if (mir == NULL)
        return;

    inventory->routines = mir->routines;
    inventory->count = mir->routine_count;
}

static inline const MIRRoutine *
llvm_routine_inventory_get(const LLVMMIRRoutineInventory *inventory,
                           size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}

static inline void
llvm_active_domain_inventory(const LLVMGenCtx *ctx,
                             LLVMDomainInventory *inventory)
{
    if (inventory == NULL)
        return;
    memset(inventory, 0, sizeof(*inventory));
    llvm_active_inventory(ctx, AST_ABILITY_DECL,
        &inventory->abilities, &inventory->ability_count);
    llvm_active_inventory(ctx, AST_RELATION_DECL,
        &inventory->relations, &inventory->relation_count);
    llvm_active_inventory(ctx, AST_EFFECT_DECL,
        &inventory->effects, &inventory->effect_count);
    llvm_active_inventory(ctx, AST_ZONE_DECL,
        &inventory->zones, &inventory->zone_count);
    llvm_active_inventory(ctx, AST_WORLD_DECL,
        &inventory->worlds, &inventory->world_count);
    llvm_active_inventory(ctx, AST_PARTY_DECL,
        &inventory->parties, &inventory->party_count);
    llvm_active_inventory(ctx, AST_ROSTER_DECL,
        &inventory->rosters, &inventory->roster_count);
    llvm_active_inventory(ctx, AST_ROLE_DECL,
        &inventory->roles, &inventory->role_count);
    llvm_active_inventory(ctx, AST_EVENT_DECL,
        &inventory->events, &inventory->event_count);
}

static inline void
llvm_active_executables(const LLVMGenCtx *ctx,
                        ASTNode ***nodes_out,
                        size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    /* MIR-only: top-level exec is represented by __pgy_top_level_exec. */
    (void)ctx;

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline void
llvm_active_externs(const LLVMGenCtx *ctx,
                    ASTNode ***nodes_out,
                    size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (ctx != NULL && ctx->mir != NULL) {
        nodes = ctx->mir->externs;
        count = ctx->mir->extern_count;
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

static inline ASTNode *
llvm_active_synthetic_executable_func(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return mir_find_function_decl(ctx->mir, "__pgy_top_level_exec");
    return NULL;
}

static inline bool
llvm_active_has_main_function(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_main_function;
    return false;
}

static inline bool
llvm_active_has_top_level_exec(const LLVMGenCtx *ctx)
{
    if (ctx != NULL && ctx->mir != NULL)
        return ctx->mir->has_top_level_exec;
    return false;
}

#endif /* PGY_LLVM_INVENTORY_INTERNAL_H */
