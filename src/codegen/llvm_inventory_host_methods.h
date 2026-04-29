/* LLVM MIR declaration method metadata helpers. */

#ifndef PGY_LLVM_INVENTORY_HOST_METHODS_H
#define PGY_LLVM_INVENTORY_HOST_METHODS_H

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
llvm_mir_decl_method_name(const MIRDeclMethod *method)
{
    if (method != NULL && method->name != NULL)
        return method->name;
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
        if (method != NULL)
            return method->ast;
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

#endif /* PGY_LLVM_INVENTORY_HOST_METHODS_H */
