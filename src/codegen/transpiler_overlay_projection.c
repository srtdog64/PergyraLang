/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend overlay projection invalidation owner.
 */

#include "transpiler_overlay_projection.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_receiver_query.h"
#include "transpiler_format.h"
#include "transpiler_projection.h"
#include "transpiler_projection_field_path.h"

static ASTNode **
current_overlay_refresh_list(TranspilerCtx *ctx, size_t *refresh_count_out)
{
    ASTNode *decl;

    if (refresh_count_out != NULL)
        *refresh_count_out = 0;
    if (ctx == NULL)
        return NULL;

    decl = transpiler_current_host_decl_local(ctx);
    if (decl != NULL && decl->type == AST_RELATION_DECL)
        return ast_relation_refreshes(decl, refresh_count_out);
    if (decl != NULL && decl->type == AST_EFFECT_DECL)
        return ast_effect_refreshes(decl, refresh_count_out);
    if (decl != NULL && decl->type == AST_ZONE_DECL)
        return ast_zone_refreshes(decl, refresh_count_out);

    return NULL;
}

static bool
projection_target_mentions_source_field(TranspilerCtx *ctx,
                                        ASTNode *refresh,
                                        const char *target_slot_name,
                                        const char *source_field_name)
{
    ASTNode *slot_decl;
    ASTNode *slot_type;
    ASTNode *target_decl;
    const char *target_type_name;

    if (ctx == NULL || target_slot_name == NULL)
        return false;
    if (source_field_name == NULL)
        return true;

    slot_decl = transpiler_current_overlay_domain_slot_decl(ctx, target_slot_name);
    if (slot_decl == NULL)
        return true;
    slot_type = ast_domain_slot_type(slot_decl);
    if (slot_type == NULL
        || slot_type->type != AST_TYPE
        || ast_type_name(slot_type) == NULL) {
        return true;
    }

    target_type_name = ast_type_name(slot_type);
    target_decl = find_class_decl(ctx, target_type_name);
    if (target_decl == NULL || target_decl->type != AST_CLASS_DECL)
        return true;

    size_t field_count = 0;
    ClassField **fields = ast_class_fields(target_decl, &field_count);
    for (size_t i = 0; i < field_count; i++) {
        ClassField *field = fields != NULL ? fields[i] : NULL;
        const char *mapped_source_name = field != NULL ? field->name : NULL;
        if (refresh != NULL && refresh->type == AST_ZONE_REFRESH && field != NULL
            && field->name != NULL) {
            for (size_t j = 0; j < ast_zone_refresh_field_map_count(refresh); j++) {
                const char *mapped_target =
                    ast_zone_refresh_mapped_target_field(refresh, j);
                const char *mapped_source =
                    ast_zone_refresh_mapped_source_field(refresh, j);
                if (mapped_target != NULL && mapped_source != NULL
                    && strcmp(mapped_target, field->name) == 0) {
                    mapped_source_name = mapped_source;
                    break;
                }
            }
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
    ASTNode **refreshes;
    size_t refresh_count = 0;
    CodeBuf *buf;
    const char *receiver_expr;

    if (ctx == NULL || source_slot_name == NULL)
        return NULL;

    receiver_expr = ctx->current_overlay_receiver_expr != NULL
        ? ctx->current_overlay_receiver_expr
        : "self";

    refreshes = current_overlay_refresh_list(ctx, &refresh_count);
    if (refreshes == NULL || refresh_count == 0)
        return NULL;

    buf = codebuf_create();
    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        const char *target_name;
        const char *refresh_source_name;

        if (refresh == NULL)
            continue;
        target_name = ast_zone_refresh_object_slot_name(refresh);
        refresh_source_name = ast_zone_refresh_source_slot_name(refresh);
        if (target_name == NULL || refresh_source_name == NULL
            || strcmp(refresh_source_name, source_slot_name) != 0) {
            continue;
        }
        if (!projection_target_mentions_source_field(ctx, refresh,
                target_name, source_field_name))
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
                ASTNode *zone_decl = find_zone_decl(ctx, zone_type_name);
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

    if (ctx == NULL || receiver == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl == NULL || host_decl->type != AST_WORLD_DECL)
        return NULL;

    if (!transpiler_resolve_world_zone_subject_receiver(ctx, receiver,
            &zone_slot_name, &zone_type_name,
            &source_slot_name, &source_type_name)
        || zone_slot_name == NULL
        || zone_type_name == NULL) {
        return NULL;
    }

    zone_decl = find_zone_decl(ctx, zone_type_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return NULL;

    buf = codebuf_create();
    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(zone_decl, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || ast_domain_slot_is_subject(slot)) {
            continue;
        }
        const char *slot_name = ast_domain_slot_name(slot);
        if (slot_name == NULL)
            continue;
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
        ast_world_name(host_decl) != NULL
            ? ast_world_name(host_decl) : "World");

    {
        char *result = pergyra_strdup(buf->data);
        codebuf_destroy(buf);
        return result;
    }
}
