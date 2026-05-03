/* LLVM MIR declaration method metadata helpers. */

#ifndef PGY_LLVM_INVENTORY_HOST_METHODS_H
#define PGY_LLVM_INVENTORY_HOST_METHODS_H

typedef struct
{
    const MIRDeclMethod *metadata;
    ASTNode           **ast_compat_methods;
    size_t             ast_compat_count;
    size_t             count;
    bool               uses_mir_metadata;
    bool               requires_mir_metadata;
} LLVMHostedMethodView;

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

static inline LLVMHostedMethodView
llvm_hosted_method_view(const LLVMGenCtx *ctx,
                        const char *host_type_name,
                        ASTNode **ast_compat_methods,
                        size_t ast_compat_count)
{
    LLVMHostedMethodView view;
    const MIRDeclHeader *decl_header = NULL;

    view.metadata = NULL;
    view.ast_compat_methods = ast_compat_methods;
    view.ast_compat_count = ast_compat_count;
    view.count = ast_compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = ctx != NULL && ctx->mir != NULL
        && ast_compat_count > 0;

    if (ctx != NULL && ctx->mir != NULL && host_type_name != NULL)
        decl_header = llvm_find_host_decl_header_in_context(ctx, host_type_name);
    if (decl_header != NULL) {
        llvm_host_decl_method_metadata(decl_header,
            &view.metadata, &view.count);
        view.uses_mir_metadata = true;
    }

    return view;
}

static inline bool
llvm_hosted_method_view_missing_mir_metadata(const LLVMHostedMethodView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

static inline LLVMHostedMethodView
llvm_hosted_method_view_from_decl(const LLVMGenCtx *ctx,
                                  const char *host_type_name,
                                  ASTNode *decl)
{
    ASTNode **ast_compat_methods = NULL;
    size_t ast_compat_count = 0;

    if (decl != NULL) {
        switch (decl->type) {
        case AST_CLASS_DECL:
            ast_compat_methods = decl->data.class_decl.methods;
            ast_compat_count = decl->data.class_decl.method_count;
            break;
        case AST_ENUM_DECL:
            ast_compat_methods = decl->data.enum_decl.methods;
            ast_compat_count = decl->data.enum_decl.method_count;
            break;
        case AST_PARTY_DECL:
            ast_compat_methods = decl->data.party_decl.methods;
            ast_compat_count = decl->data.party_decl.method_count;
            break;
        case AST_ROSTER_DECL:
            ast_compat_methods = decl->data.roster_decl.methods;
            ast_compat_count = decl->data.roster_decl.method_count;
            break;
        case AST_WORLD_DECL:
            ast_compat_methods = decl->data.world_decl.methods;
            ast_compat_count = decl->data.world_decl.method_count;
            break;
        case AST_RELATION_DECL:
            ast_compat_methods = decl->data.relation_decl.methods;
            ast_compat_count = decl->data.relation_decl.method_count;
            break;
        case AST_EFFECT_DECL:
            ast_compat_methods = decl->data.effect_decl.methods;
            ast_compat_count = decl->data.effect_decl.method_count;
            break;
        case AST_ZONE_DECL:
            ast_compat_methods = decl->data.zone_decl.methods;
            ast_compat_count = decl->data.zone_decl.method_count;
            break;
        default:
            break;
        }
    }

    return llvm_hosted_method_view(ctx, host_type_name,
        ast_compat_methods, ast_compat_count);
}

static inline const MIRDeclMethod *
llvm_hosted_method_view_metadata(const LLVMHostedMethodView *view,
                                 size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->metadata == NULL || index >= view->count) {
        return NULL;
    }
    return &view->metadata[index];
}

static inline ASTNode *
llvm_hosted_method_view_ast(const LLVMHostedMethodView *view, size_t index)
{
    const MIRDeclMethod *method = llvm_hosted_method_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (method != NULL)
        return method->ast;
    if (view->requires_mir_metadata)
        return NULL;
    return view->ast_compat_methods != NULL
        ? view->ast_compat_methods[index]
        : NULL;
}

static inline const char *
llvm_mir_decl_method_name(const MIRDeclMethod *method)
{
    if (method != NULL && method->name != NULL)
        return method->name;
    return NULL;
}

static inline ASTNode *
llvm_mir_decl_method_ast(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->ast;
    return NULL;
}

static inline size_t
llvm_mir_decl_method_param_count(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->param_count;
    return 0;
}

static inline FuncParam *
llvm_mir_decl_method_param(const MIRDeclMethod *method, size_t index)
{
    if (method != NULL && method->params != NULL && index < method->param_count)
        return method->params[index];
    return NULL;
}

static inline ASTNode *
llvm_mir_decl_method_return_type(const MIRDeclMethod *method)
{
    if (method != NULL && method->return_type != NULL)
        return method->return_type;
    return NULL;
}

static inline bool
llvm_mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->is_action_like;
    return false;
}

static inline ASTNode *
llvm_find_host_method_decl_in_context(const LLVMGenCtx *ctx,
                                      const char *host_type_name,
                                      const char *method_name)
{
    const MIRDeclMethod *method = NULL;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    method = llvm_find_host_method_metadata_in_context(
        ctx, host_type_name, method_name);
    return llvm_mir_decl_method_ast(method);
}

#endif /* PGY_LLVM_INVENTORY_HOST_METHODS_H */
