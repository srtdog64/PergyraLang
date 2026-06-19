/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend overlay projection invalidation owner.
 */

#include "transpiler_overlay_projection.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_receiver_query.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "transpiler_projection.h"
#include "transpiler_projection_field_path.h"

typedef struct CurrentOverlayRefreshView
{
    size_t count;
    TranspilerHostedZoneRefreshView zone_refresh_view;
} CurrentOverlayRefreshView;

static CurrentOverlayRefreshView
current_overlay_refresh_view(TranspilerCtx *ctx)
{
    CurrentOverlayRefreshView view = {0};
    ASTNode *decl;

    if (ctx == NULL)
        return view;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL
        && (decl->type == AST_RELATION_DECL
            || decl->type == AST_EFFECT_DECL
            || decl->type == AST_ZONE_DECL)) {
        const char *decl_name = transpiler_decl_name_local(decl);
        view.zone_refresh_view =
            transpiler_hosted_zone_refresh_view_from_decl(ctx, decl_name, decl);
        view.count = view.zone_refresh_view.count;
        if (transpiler_hosted_zone_refresh_view_missing_mir_metadata(
                &view.zone_refresh_view)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing overlay domain refresh metadata for '%s'",
                decl_name != NULL ? decl_name : "(anonymous-domain)");
            view.count = 0;
        }
    }

    return view;
}

static bool
overlay_projection_field_view(TranspilerCtx *ctx,
                              ASTNode *target_decl,
                              TranspilerHostedFieldView *view)
{
    const char *target_name;

    if (view == NULL)
        return false;
    memset(view, 0, sizeof(*view));

    if (ctx == NULL || target_decl == NULL || target_decl->type != AST_CLASS_DECL)
        return false;

    target_name = transpiler_decl_name_local(target_decl);
    *view = transpiler_hosted_class_field_view_from_decl(
        ctx, target_name, target_decl);
    if (transpiler_hosted_field_view_missing_mir_metadata(view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing overlay projection field metadata for '%s'",
            target_name != NULL ? target_name : "(anonymous-class)");
        return false;
    }
    return true;
}

static size_t
overlay_projection_field_count(TranspilerCtx *ctx, ASTNode *target_decl)
{
    TranspilerHostedFieldView view;

    return overlay_projection_field_view(ctx, target_decl, &view)
        ? view.count : 0;
}

static const char *
overlay_projection_field_name(TranspilerCtx *ctx, ASTNode *target_decl,
                              size_t index)
{
    TranspilerHostedFieldView view;

    if (!overlay_projection_field_view(ctx, target_decl, &view))
        return NULL;

    return transpiler_hosted_field_view_name(&view, index);
}

static bool
projection_target_mentions_source_field(TranspilerCtx *ctx,
                                        const TranspilerHostedZoneRefreshView *view,
                                        size_t refresh_index,
                                        const char *target_slot_name,
                                        const char *source_field_name)
{
    ASTNode *target_decl;
    const char *target_type_name;

    if (ctx == NULL || target_slot_name == NULL)
        return false;
    if (source_field_name == NULL)
        return true;

    target_type_name = transpiler_current_overlay_domain_slot_type_name(
        ctx, target_slot_name);
    if (target_type_name == NULL) {
        return true;
    }

    target_decl = transpiler_find_projection_nominal_decl_local(
        ctx, target_type_name);
    if (target_decl == NULL || target_decl->type != AST_CLASS_DECL)
        return true;

    size_t field_count = overlay_projection_field_count(ctx, target_decl);
    for (size_t i = 0; i < field_count; i++) {
        const char *target_field_name =
            overlay_projection_field_name(ctx, target_decl, i);
        const char *mapped_source_name = target_field_name;
        const char *mapped_from_view =
            transpiler_hosted_zone_refresh_view_mapped_source_field(
                view, refresh_index, target_field_name);
        if (mapped_from_view != NULL) {
            mapped_source_name = mapped_from_view;
        }
        if (mapped_source_name != NULL
            && strcmp(mapped_source_name, source_field_name) == 0) {
            return true;
        }
    }

    return false;
}

char *
emit_current_overlay_projection_invalidation(TranspilerCtx *ctx,
                                            const char *source_slot_name,
                                            const char *source_field_name)
{
    CurrentOverlayRefreshView refresh_view;
    CodeBuf *buf;
    const char *receiver_expr;

    if (ctx == NULL || source_slot_name == NULL)
        return NULL;

    receiver_expr = ctx->current_overlay_receiver_expr != NULL
        ? ctx->current_overlay_receiver_expr
        : "self";

    refresh_view = current_overlay_refresh_view(ctx);
    if (refresh_view.count == 0)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < refresh_view.count; i++) {
        const char *target_name;
        const char *refresh_source_name;

        target_name =
            transpiler_hosted_zone_refresh_view_object_slot_name(
                &refresh_view.zone_refresh_view, i);
        refresh_source_name =
            transpiler_hosted_zone_refresh_view_source_slot_name(
                &refresh_view.zone_refresh_view, i);
        if (target_name == NULL || refresh_source_name == NULL
            || strcmp(refresh_source_name, source_slot_name) != 0) {
            continue;
        }
        if (!projection_target_mentions_source_field(ctx,
                &refresh_view.zone_refresh_view, i, target_name,
                source_field_name))
            continue;
        codebuf_write(buf,
            "%s->__projection_dirty_%s = true; "
            "%s->__projection_ready_%s = false; ",
            receiver_expr,
            target_name,
            receiver_expr,
            target_name);
    }

    if (buf->len == 0) {
        codebuf_destroy(buf);
        return NULL;
    }

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}

static bool
resolve_world_embedded_projection_invalidation(TranspilerCtx *ctx,
                                               ASTNode *target,
                                               const char **zone_slot_name_out,
                                               const char **zone_type_name_out,
                                               const char **source_slot_name_out,
                                               const char **source_field_name_out)
{
    ASTNode *cursor = target;
    const char *source_field_name = NULL;

    if (zone_slot_name_out != NULL)
        *zone_slot_name_out = NULL;
    if (zone_type_name_out != NULL)
        *zone_type_name_out = NULL;
    if (source_slot_name_out != NULL)
        *source_slot_name_out = NULL;
    if (source_field_name_out != NULL)
        *source_field_name_out = NULL;

    if (ctx == NULL || target == NULL)
        return false;

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        const char *zone_slot_name = NULL;
        const char *zone_type_name = NULL;
        const char *source_slot_name = NULL;
        const char *source_type_name = NULL;

        if (transpiler_resolve_world_zone_subject_receiver(ctx, cursor,
                &zone_slot_name, &zone_type_name,
                &source_slot_name, &source_type_name)
            && zone_slot_name != NULL
            && zone_type_name != NULL
            && source_slot_name != NULL) {
            if (zone_slot_name_out != NULL)
                *zone_slot_name_out = zone_slot_name;
            if (zone_type_name_out != NULL)
                *zone_type_name_out = zone_type_name;
            if (source_slot_name_out != NULL)
                *source_slot_name_out = source_slot_name;
            if (source_field_name_out != NULL)
                *source_field_name_out = source_field_name;
            return true;
        }

        source_field_name = ast_member_name(cursor);
        cursor = ast_member_object(cursor);
    }

    return false;
}

char *
emit_assignment_projection_invalidation(TranspilerCtx *ctx, ASTNode *target)
{
    const char *root_slot_name;
    const char *root_subfield_name;
    char *invalidation;

    if (ctx == NULL || target == NULL)
        return NULL;

    root_slot_name = assignment_target_root_slot_name(target);
    root_subfield_name = assignment_target_root_subfield_name(target);
    invalidation = emit_current_overlay_projection_invalidation(
        ctx, root_slot_name, root_subfield_name);
    if (invalidation != NULL)
        return invalidation;

    {
        ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
        const char *saved_receiver_expr = ctx->current_overlay_receiver_expr;

        if (saved_host_decl != NULL && saved_host_decl->type == AST_WORLD_DECL) {
            const char *zone_slot_name = NULL;
            const char *zone_type_name = NULL;
            const char *source_slot_name = NULL;
            const char *source_field_name = NULL;

            if (resolve_world_embedded_projection_invalidation(ctx, target,
                    &zone_slot_name, &zone_type_name,
                    &source_slot_name, &source_field_name)
                && zone_slot_name != NULL
                && zone_type_name != NULL
                && source_slot_name != NULL) {
                ASTNode *zone_decl = transpiler_resolve_world_zone_decl(
                    ctx, saved_host_decl, zone_slot_name);
                if (zone_decl != NULL)
                    transpiler_bind_current_host_decl_local(ctx, zone_decl);
                ctx->current_overlay_receiver_expr =
                    strdup_fmt("(&self->%s)", zone_slot_name);
                invalidation = emit_current_overlay_projection_invalidation(
                    ctx, source_slot_name, source_field_name);
                if (ctx->current_overlay_receiver_expr != NULL
                    && ctx->current_overlay_receiver_expr != saved_receiver_expr) {
                    free((char *)ctx->current_overlay_receiver_expr);
                }
                ctx->current_overlay_receiver_expr = saved_receiver_expr;
                transpiler_bind_current_host_decl_local(ctx, saved_host_decl);
                return invalidation;
            }
        }
    }

    return NULL;
}

char *
emit_world_embedded_assignment_sync(TranspilerCtx *ctx, ASTNode *target)
{
    ASTNode *host_decl;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_field_name = NULL;

    if (ctx == NULL || target == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!resolve_world_embedded_projection_invalidation(ctx, target,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_field_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL
        || source_slot_name == NULL) {
        return NULL;
    }

    return strdup_fmt("%s_sync(&self->%s); ", zone_type_name, zone_slot_name);
}

char *
emit_world_embedded_receiver_projection_sync(TranspilerCtx *ctx,
                                             ASTNode *receiver)
{
    ASTNode *host_decl;
    ASTNode *zone_decl;
    CodeBuf *buf;
    const char *zone_slot_name = NULL;
    const char *zone_type_name = NULL;
    const char *source_slot_name = NULL;
    const char *source_type_name = NULL;
    const char *world_name;
    const char *zone_decl_name;
    TranspilerHostedDomainSlotView slot_view;

    if (ctx == NULL || receiver == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;
    world_name = transpiler_decl_name_local(host_decl);
    if (world_name == NULL)
        return NULL;

    if (!transpiler_resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL) {
        return NULL;
    }

    zone_decl = transpiler_resolve_world_zone_decl(ctx, host_decl,
                                                   zone_slot_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    zone_decl_name = transpiler_decl_name_local(zone_decl);
    slot_view = transpiler_hosted_domain_slot_view_from_decl(ctx,
        zone_decl_name, zone_decl);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing embedded zone projection metadata for '%s'",
            zone_decl_name != NULL ? zone_decl_name : "(anonymous-zone)");
        return NULL;
    }

    buf = codebuf_create();
    for (size_t i = 0; i < slot_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        if (slot_name == NULL)
            continue;
        if (transpiler_hosted_domain_slot_view_is_subject_like(
                &slot_view, i)) {
            continue;
        }
        codebuf_write(buf,
            "self->%s.__projection_dirty_%s = true; "
            "self->%s.__projection_ready_%s = false; ",
            zone_slot_name, slot_name,
            zone_slot_name, slot_name);
    }
    codebuf_write(buf,
        "%s_sync(&self->%s); "
        "self->__zone_dirty_%s = true; "
        "self->__world_derived_dirty = true; "
        "%s_sync(self); ",
        zone_type_name, zone_slot_name,
        zone_slot_name,
        world_name);

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}
