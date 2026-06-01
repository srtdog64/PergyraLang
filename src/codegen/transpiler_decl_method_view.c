/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Hosted-method MIR metadata view helpers for the C backend.
 */

#include "transpiler_decl_lookup.h"
#include "host_decl_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "transpiler_context.h"

TranspilerHostedMethodView
transpiler_hosted_method_view(const TranspilerCtx *ctx,
                              const char *host_name,
                              ASTNode **ast_compat_methods,
                              size_t ast_compat_count)
{
    TranspilerHostedMethodView view;
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.ast_compat_methods = ast_compat_methods;
    view.ast_compat_count = ast_compat_count;
    view.count = ast_compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata =
        transpiler_active_has_mir(ctx) && ast_compat_count > 0;

    header = transpiler_active_decl_header(ctx, host_name);
    if (header != NULL
        && transpiler_is_host_decl_type(mir_decl_header_ast_type_or(
            header, AST_PROGRAM))) {
        view.decl_header = header;
        view.count = mir_decl_header_method_count(header);
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
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_method(view->decl_header, index);
}

bool
transpiler_hosted_method_view_missing_mir_method_row(
    const TranspilerHostedMethodView *view,
    size_t index)
{
    return view != NULL
        && view->uses_mir_metadata
        && transpiler_hosted_method_view_metadata(view, index) == NULL;
}

bool
transpiler_require_hosted_method_view_rows(
    TranspilerCtx *ctx,
    const TranspilerHostedMethodView *view,
    const char *message_fmt,
    const char *host_name)
{
    for (size_t i = 0; view != NULL && i < view->count; i++) {
        if (transpiler_hosted_method_view_missing_mir_method_row(view, i)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                message_fmt != NULL
                    ? message_fmt
                    : "MIR-only C path has invalid method declaration metadata row for '%s'",
                host_name != NULL ? host_name : "(anonymous)");
            return false;
        }
    }
    return true;
}

const char *
transpiler_mir_decl_method_name(const MIRDeclMethod *method)
{
    return mir_decl_method_name(method);
}

ASTNode *
transpiler_mir_decl_method_source_ast(const MIRDeclMethod *method)
{
    if (method != NULL)
        return mir_decl_method_source_ast(method);
    return NULL;
}

size_t
transpiler_mir_decl_method_param_count(const MIRDeclMethod *method)
{
    return mir_decl_method_param_count(method);
}

FuncParam *
transpiler_mir_decl_method_param(const MIRDeclMethod *method, size_t index)
{
    return mir_decl_method_param(method, index);
}

ASTNode *
transpiler_mir_decl_method_return_type(const MIRDeclMethod *method)
{
    return mir_decl_method_return_type(method);
}

bool
transpiler_mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    return mir_decl_method_is_action_like(method);
}

const MIRRoutine *
transpiler_mir_decl_method_routine(const TranspilerCtx *ctx,
                                   const MIRDeclMethod *method)
{
    TranspilerMIRRoutineInventory inventory;
    size_t routine_index = 0;

    if (ctx == NULL || !transpiler_active_has_mir(ctx) || method == NULL)
        return NULL;
    if (!mir_decl_method_routine_index(method, &routine_index))
        return NULL;
    transpiler_active_routine_inventory(ctx, &inventory);
    return transpiler_routine_inventory_get(&inventory, routine_index);
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
        return mir_decl_method_source_ast(method);
    if (view->requires_mir_metadata)
        return NULL;
    return view->ast_compat_methods != NULL
        ? view->ast_compat_methods[index]
        : NULL;
}
