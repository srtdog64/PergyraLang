#ifndef PGY_TRANSPILER_OVERLAY_PROJECTION_H
#define PGY_TRANSPILER_OVERLAY_PROJECTION_H

/* C backend overlay/projection invalidation and zone-layer bind helpers.
 * Included inside transpiler.c after declaration lookup/projection seams. */

#include "transpiler_overlay_host_fields.h"
#include "transpiler_overlay_zone_bind.h"
#include "transpiler_projection_field_path.h"
#include "parser/ast_api.h"

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
    slot_type = ast_domain_slot_type(slot_decl);
    if (slot_decl == NULL
        || slot_type == NULL
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

static char *
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

#include "transpiler_overlay_world_projection.h"

#endif /* PGY_TRANSPILER_OVERLAY_PROJECTION_H */
