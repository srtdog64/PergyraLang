/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR declaration method metadata view.
 */

#ifdef PGY_LLVM_ENABLED

#include "host_decl_compat.h"
#include "llvm_internal.h"
#include "../compiler/mir_decl_headers.h"

const MIRDeclMethod *
llvm_find_host_method_metadata_in_context(const LLVMGenCtx *ctx,
                                          const char *host_type_name,
                                          const char *method_name)
{
    const MIRDeclHeader *decl_header;
    size_t method_count;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    decl_header = llvm_find_host_decl_header_in_context(ctx, host_type_name);
    method_count = mir_decl_header_method_count(decl_header);
    for (size_t i = 0; i < method_count; i++) {
        const char *candidate = llvm_mir_decl_method_name(
            mir_decl_header_method(decl_header, i));
        if (candidate != NULL && strcmp(candidate, method_name) == 0)
            return mir_decl_header_method(decl_header, i);
    }

    return NULL;
}

LLVMHostedMethodView
llvm_hosted_method_view(const LLVMGenCtx *ctx,
                        const char *host_type_name,
                        ASTNode **ast_compat_methods,
                        size_t ast_compat_count)
{
    LLVMHostedMethodView view;
    const MIRDeclHeader *decl_header = NULL;

    view.decl_header = NULL;
    view.ast_compat_methods = ast_compat_methods;
    view.ast_compat_count = ast_compat_count;
    view.count = ast_compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && ast_compat_count > 0;

    if (llvm_active_has_mir(ctx) && host_type_name != NULL)
        decl_header = llvm_find_host_decl_header_in_context(ctx, host_type_name);
    if (decl_header != NULL) {
        view.decl_header = decl_header;
        view.count = mir_decl_header_method_count(decl_header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_method_view_missing_mir_metadata(const LLVMHostedMethodView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

LLVMHostedMethodView
llvm_hosted_method_view_from_decl(const LLVMGenCtx *ctx,
                                  const char *host_type_name,
                                  ASTNode *decl)
{
    PgyHostMethodCompatView compat =
        pgy_host_method_compat_view_from_decl(decl, llvm_active_has_mir(ctx));

    return llvm_hosted_method_view(ctx, host_type_name,
        compat.methods, compat.count);
}

const MIRDeclMethod *
llvm_hosted_method_view_metadata(const LLVMHostedMethodView *view,
                                 size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_method(view->decl_header, index);
}

ASTNode *
llvm_hosted_method_view_source_ast(const LLVMHostedMethodView *view,
                                   size_t index)
{
    const MIRDeclMethod *method = llvm_hosted_method_view_metadata(view, index);

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

const char *
llvm_mir_decl_method_name(const MIRDeclMethod *method)
{
    return mir_decl_method_name(method);
}

ASTNode *
llvm_mir_decl_method_source_ast(const MIRDeclMethod *method)
{
    if (method != NULL)
        return mir_decl_method_source_ast(method);
    return NULL;
}

size_t
llvm_mir_decl_method_param_count(const MIRDeclMethod *method)
{
    return mir_decl_method_param_count(method);
}

FuncParam *
llvm_mir_decl_method_param(const MIRDeclMethod *method, size_t index)
{
    return mir_decl_method_param(method, index);
}

ASTNode *
llvm_mir_decl_method_return_type(const MIRDeclMethod *method)
{
    return mir_decl_method_return_type(method);
}

bool
llvm_mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    return mir_decl_method_is_action_like(method);
}

const MIRRoutine *
llvm_mir_decl_method_routine(const LLVMGenCtx *ctx,
                             const MIRDeclMethod *method)
{
    LLVMMIRRoutineInventory inventory;
    size_t routine_index = 0;

    if (!llvm_active_has_mir(ctx) || method == NULL)
        return NULL;
    if (!mir_decl_method_routine_index(method, &routine_index))
        return NULL;
    llvm_active_routine_inventory(ctx, &inventory);
    return llvm_routine_inventory_get(&inventory, routine_index);
}

const MIRRoutine *
llvm_hosted_method_view_routine(const LLVMGenCtx *ctx,
                                const LLVMHostedMethodView *view,
                                size_t index)
{
    return llvm_mir_decl_method_routine(
        ctx, llvm_hosted_method_view_metadata(view, index));
}

ASTNode *
llvm_find_host_method_decl_in_context(const LLVMGenCtx *ctx,
                                      const char *host_type_name,
                                      const char *method_name)
{
    const MIRDeclMethod *method = NULL;

    if (ctx == NULL || host_type_name == NULL || method_name == NULL)
        return NULL;

    method = llvm_find_host_method_metadata_in_context(
        ctx, host_type_name, method_name);
    return llvm_mir_decl_method_source_ast(method);
}

#endif /* PGY_LLVM_ENABLED */
