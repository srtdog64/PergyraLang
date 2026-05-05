/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Hosted-method MIR metadata view helpers for the C backend.
 */

#include "transpiler_decl_lookup.h"

TranspilerHostedMethodView
transpiler_hosted_method_view(const TranspilerCtx *ctx,
                              const char *host_name,
                              ASTNode **ast_compat_methods,
                              size_t ast_compat_count)
{
    TranspilerHostedMethodView view;
    const MIRDeclHeader *header = NULL;

    view.metadata = NULL;
    view.ast_compat_methods = ast_compat_methods;
    view.ast_compat_count = ast_compat_count;
    view.count = ast_compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = ctx != NULL && ctx->mir != NULL
        && ast_compat_count > 0;

    if (ctx != NULL && ctx->mir != NULL && host_name != NULL)
        header = mir_find_decl_header(ctx->mir, host_name);
    if (header != NULL) {
        view.metadata = header->method_metadata;
        view.count = header->method_metadata_count;
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_method_view_missing_mir_metadata(
    const TranspilerHostedMethodView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclMethod *
transpiler_hosted_method_view_metadata(const TranspilerHostedMethodView *view,
                                       size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->metadata == NULL || index >= view->count) {
        return NULL;
    }
    return &view->metadata[index];
}

const char *
transpiler_mir_decl_method_name(const MIRDeclMethod *method)
{
    if (method != NULL && method->name != NULL)
        return method->name;
    return NULL;
}

ASTNode *
transpiler_mir_decl_method_ast(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->ast;
    return NULL;
}

size_t
transpiler_mir_decl_method_param_count(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->param_count;
    return 0;
}

FuncParam *
transpiler_mir_decl_method_param(const MIRDeclMethod *method, size_t index)
{
    if (method == NULL || method->params == NULL
        || index >= method->param_count) {
        return NULL;
    }
    return method->params[index];
}

ASTNode *
transpiler_mir_decl_method_return_type(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->return_type;
    return NULL;
}

bool
transpiler_mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    return method != NULL && method->is_action_like;
}

const MIRRoutine *
transpiler_mir_decl_method_routine(const TranspilerCtx *ctx,
                                   const MIRDeclMethod *method)
{
    TranspilerMIRRoutineInventory inventory;
    if (ctx == NULL || ctx->mir == NULL || method == NULL)
        return NULL;
    if (!method->has_routine)
        return NULL;
    transpiler_active_routine_inventory(ctx, &inventory);
    return transpiler_routine_inventory_get(&inventory, method->routine_index);
}

const MIRRoutine *
transpiler_hosted_method_view_routine(const TranspilerCtx *ctx,
                                      const TranspilerHostedMethodView *view,
                                      size_t index)
{
    return transpiler_mir_decl_method_routine(
        ctx, transpiler_hosted_method_view_metadata(view, index));
}

TranspilerHostedMethodView
transpiler_hosted_method_view_from_decl(const TranspilerCtx *ctx,
                                        const char *host_name,
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

    return transpiler_hosted_method_view(ctx, host_name,
        ast_compat_methods, ast_compat_count);
}

ASTNode *
transpiler_hosted_method_view_ast(const TranspilerHostedMethodView *view,
                                  size_t index)
{
    const MIRDeclMethod *method =
        transpiler_hosted_method_view_metadata(view, index);

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
