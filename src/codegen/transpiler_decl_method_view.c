/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Hosted-method MIR metadata view helpers for the C backend.
 */

#include "transpiler_decl_lookup.h"
#include "host_decl_compat.h"

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
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && ast_compat_count > 0;

    header = transpiler_active_decl_header(ctx, host_name);
    if (header != NULL && transpiler_is_host_decl_type(header->ast_type)) {
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
transpiler_mir_decl_method_source_ast(const MIRDeclMethod *method)
{
    if (method != NULL)
        return method->source_ast;
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
    if (ctx == NULL || !transpiler_active_has_mir(ctx) || method == NULL)
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
    PgyHostMethodCompatView compat = pgy_host_method_compat_view_from_decl(
        decl, transpiler_active_has_mir(ctx));

    return transpiler_hosted_method_view(ctx, host_name,
        compat.methods, compat.count);
}

ASTNode *
transpiler_hosted_method_view_source_ast(
    const TranspilerHostedMethodView *view,
    size_t index)
{
    const MIRDeclMethod *method =
        transpiler_hosted_method_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (method != NULL)
        return method->source_ast;
    if (view->requires_mir_metadata)
        return NULL;
    return view->ast_compat_methods != NULL
        ? view->ast_compat_methods[index]
        : NULL;
}
