/*
 * Copyright (c) 2026 Pergyra Language Project
 * Hosted declaration zone refresh metadata view lowering.
 */

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "transpiler_decl_lookup.h"

#include <string.h>

TranspilerHostedZoneRefreshView
transpiler_hosted_zone_refresh_view_from_decl(const TranspilerCtx *ctx,
                                              const char *host_name,
                                              ASTNode *decl)
{
    TranspilerHostedZoneRefreshView view;
    const MIRDeclHeader *header = NULL;

    view.decl_header = NULL;
    view.count = 0;
    view.uses_mir_metadata = false;
    view.requires_mir_metadata = transpiler_active_has_mir(ctx)
        && decl != NULL
        && (decl->type == AST_RELATION_DECL
            || decl->type == AST_EFFECT_DECL
            || decl->type == AST_ZONE_DECL);

    header = transpiler_active_host_decl_header(ctx, host_name);
    if (header != NULL) {
        ASTNodeType header_type =
            mir_decl_header_ast_type_or(header, AST_PROGRAM);
        if (header_type != AST_RELATION_DECL
            && header_type != AST_EFFECT_DECL
            && header_type != AST_ZONE_DECL) {
            return view;
        }
        view.decl_header = header;
        view.count = mir_decl_header_zone_refresh_count(header);
        view.uses_mir_metadata = true;
    }

    return view;
}

bool
transpiler_hosted_zone_refresh_view_missing_mir_metadata(
    const TranspilerHostedZoneRefreshView *view)
{
    return view != NULL
        && view->requires_mir_metadata
        && !view->uses_mir_metadata;
}

const MIRDeclZoneRefresh *
transpiler_hosted_zone_refresh_view_metadata(
    const TranspilerHostedZoneRefreshView *view,
    size_t index)
{
    if (view == NULL || !view->uses_mir_metadata
        || view->decl_header == NULL || index >= view->count) {
        return NULL;
    }
    return mir_decl_header_zone_refresh(view->decl_header, index);
}

const char *
transpiler_hosted_zone_refresh_view_object_slot_name(
    const TranspilerHostedZoneRefreshView *view,
    size_t index)
{
    const MIRDeclZoneRefresh *refresh =
        transpiler_hosted_zone_refresh_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (refresh != NULL)
        return mir_decl_zone_refresh_object_slot_name(refresh);
    return NULL;
}

const char *
transpiler_hosted_zone_refresh_view_source_slot_name(
    const TranspilerHostedZoneRefreshView *view,
    size_t index)
{
    const MIRDeclZoneRefresh *refresh =
        transpiler_hosted_zone_refresh_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return NULL;
    if (refresh != NULL)
        return mir_decl_zone_refresh_source_slot_name(refresh);
    return NULL;
}

const char *
transpiler_hosted_zone_refresh_view_mapped_source_field(
    const TranspilerHostedZoneRefreshView *view,
    size_t index,
    const char *target_field_name)
{
    const MIRDeclZoneRefresh *refresh =
        transpiler_hosted_zone_refresh_view_metadata(view, index);

    if (view == NULL || index >= view->count || target_field_name == NULL)
        return NULL;
    if (refresh != NULL) {
        for (size_t i = 0; i < mir_decl_zone_refresh_field_map_count(refresh);
             i++) {
            const char *mapped_target =
                mir_decl_zone_refresh_mapped_target_field(refresh, i);
            const char *mapped_source =
                mir_decl_zone_refresh_mapped_source_field(refresh, i);
            if (mapped_target != NULL && mapped_source != NULL
                && strcmp(mapped_target, target_field_name) == 0) {
                return mapped_source;
            }
        }
        return NULL;
    }
    return NULL;
}

bool
transpiler_hosted_zone_refresh_view_mentions_source_field(
    const TranspilerHostedZoneRefreshView *view,
    size_t index,
    const char *source_field_name)
{
    const MIRDeclZoneRefresh *refresh =
        transpiler_hosted_zone_refresh_view_metadata(view, index);

    if (view == NULL || index >= view->count)
        return false;
    if (source_field_name == NULL)
        return true;
    if (refresh != NULL) {
        if (mir_decl_zone_refresh_field_map_count(refresh) == 0)
            return true;
        for (size_t i = 0; i < mir_decl_zone_refresh_field_map_count(refresh);
             i++) {
            const char *mapped_source =
                mir_decl_zone_refresh_mapped_source_field(refresh, i);
            if (mapped_source != NULL
                && strcmp(mapped_source, source_field_name) == 0) {
                return true;
            }
        }
        return false;
    }
    return false;
}
