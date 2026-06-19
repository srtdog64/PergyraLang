/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM hosted zone refresh metadata view.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

LLVMHostedZoneRefreshView
llvm_hosted_zone_refresh_view_from_decl(const LLVMGenCtx *ctx,
                                        const char *host_name,
                                        ASTNode *decl)
{
    LLVMHostedZoneRefreshView view;
    ASTNode **compat_refreshes = NULL;
    size_t compat_count = 0;
    const MIRDeclHeader *header = NULL;

    if (decl != NULL && decl->type == AST_ZONE_DECL)
        compat_refreshes = ast_zone_refreshes(decl, &compat_count);

    view.decl_header = NULL;
    view.ast_compat_refreshes = compat_refreshes;
    view.ast_compat_count = compat_count;
    view.count = compat_count;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = llvm_active_has_mir(ctx)
        && compat_count > 0;

    header = llvm_find_host_decl_header_in_context(ctx, host_name);
    if (header != NULL
        && mir_decl_header_ast_type_or(header, AST_PROGRAM)
            == AST_ZONE_DECL) {
        view.decl_header = header;
        view.count = mir_decl_header_zone_refresh_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
llvm_hosted_zone_refresh_view_missing_mir_metadata(
    const LLVMHostedZoneRefreshView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && (!view->uses_mir_metadata
            || view->count != view->ast_compat_count)
        && view->ast_compat_count > 0;
}

const MIRDeclZoneRefresh *
llvm_hosted_zone_refresh_view_metadata(const LLVMHostedZoneRefreshView *view,
                                       size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_zone_refresh(view->decl_header, index);
}

const char *
llvm_hosted_zone_refresh_view_object_slot_name(
    const LLVMHostedZoneRefreshView *view,
    size_t index)
{
    const MIRDeclZoneRefresh *refresh =
        llvm_hosted_zone_refresh_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (refresh != NULL)
        return mir_decl_zone_refresh_object_slot_name(refresh);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_refreshes != NULL)
        return ast_zone_refresh_object_slot_name(
            view->ast_compat_refreshes[index]);
    return NULL;
}

const char *
llvm_hosted_zone_refresh_view_source_slot_name(
    const LLVMHostedZoneRefreshView *view,
    size_t index)
{
    const MIRDeclZoneRefresh *refresh =
        llvm_hosted_zone_refresh_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (refresh != NULL)
        return mir_decl_zone_refresh_source_slot_name(refresh);
    if (view->requires_mir_metadata)
        return NULL;
    if (view->ast_compat_refreshes != NULL)
        return ast_zone_refresh_source_slot_name(
            view->ast_compat_refreshes[index]);
    return NULL;
}

#endif
