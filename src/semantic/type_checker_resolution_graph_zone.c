#include "type_checker_internal.h"

#include <stdlib.h>

void
semantic_type_resolution_precollect_zone_refresh_projection_map(
    ASTNode *zone_decl,
    ASTNode *refresh,
    SemanticContext *ctx,
    const char *consumer_label)
{
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL
        || refresh == NULL || refresh->type != AST_ZONE_REFRESH
        || ctx == NULL || consumer_label == NULL) {
        return;
    }

    for (size_t map_i = 0; map_i < ast_zone_refresh_field_map_count(refresh); map_i++) {
        const char *target_field = ast_zone_refresh_mapped_target_field(refresh, map_i);
        const char *source_field = ast_zone_refresh_mapped_source_field(refresh, map_i);
        char *projection_label = semantic_type_resolution_projection_path_label(
            zone_decl,
            ast_zone_refresh_object_slot_name(refresh),
            ast_zone_refresh_source_slot_name(refresh),
            target_field,
            source_field);
        if (projection_label == NULL)
            continue;

        (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                          TYPE_RES_NODE_PROJECTION_PATH,
                                          refresh,
                                          projection_label);
        semantic_type_resolution_record_named_dependency(
            ctx,
            refresh,
            consumer_label,
            TYPE_RES_NODE_PROJECTION_PATH,
            refresh,
            projection_label,
            "zone refresh projection-path lookup");

        if (ast_zone_refresh_object_slot_name(refresh) != NULL) {
            char *target_slot_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                ast_zone_refresh_object_slot_name(refresh));
            if (target_slot_label != NULL) {
                semantic_type_resolution_record_named_dependency(
                    ctx,
                    refresh,
                    projection_label,
                    TYPE_RES_NODE_LOCAL_CONTRACT,
                    refresh,
                    target_slot_label,
                    "projection target-slot carrier");
                free(target_slot_label);
            }
        }

        if (ast_zone_refresh_source_slot_name(refresh) != NULL) {
            char *source_slot_label = semantic_type_resolution_zone_slot_label(
                zone_decl,
                ast_zone_refresh_source_slot_name(refresh));
            if (source_slot_label != NULL) {
                semantic_type_resolution_record_named_dependency(
                    ctx,
                    refresh,
                    projection_label,
                    TYPE_RES_NODE_LOCAL_CONTRACT,
                    refresh,
                    source_slot_label,
                    "projection source-slot carrier");
                free(source_slot_label);
            }
        }

        if (target_field != NULL) {
            char *target_field_label = semantic_type_resolution_projection_slot_field_label(
                zone_decl,
                ast_zone_refresh_object_slot_name(refresh),
                target_field);
            if (target_field_label != NULL) {
                (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                  TYPE_RES_NODE_PROJECTION_PATH,
                                                  refresh,
                                                  target_field_label);
                semantic_type_resolution_record_named_dependency(
                    ctx,
                    refresh,
                    projection_label,
                    TYPE_RES_NODE_PROJECTION_PATH,
                    refresh,
                    target_field_label,
                    "projection target field-path lookup");
                free(target_field_label);
            }
        }

        if (source_field != NULL) {
            ASTNode *source_decl = semantic_type_resolution_projection_source_decl(
                zone_decl,
                ast_zone_refresh_source_slot_name(refresh),
                ctx);
            char *resolved_source_path = NULL;
            const char *source_path_text = source_field;

            if (source_decl != NULL) {
                size_t saved_diag = ctx->diagnostic_count;
                bool saved_error = ctx->has_error;
                Type *field_type = NULL;
                int path_status = resolve_projection_source_field_path(
                    ctx->program_root,
                    source_decl,
                    source_field,
                    ctx,
                    &resolved_source_path,
                    &field_type);
                (void)field_type;
                if (ctx->diagnostic_count > saved_diag) {
                    ctx->diagnostic_count = saved_diag;
                    ctx->has_error = saved_error;
                }
                if (path_status == 1 && resolved_source_path != NULL)
                    source_path_text = resolved_source_path;
            }

            if (source_path_text != NULL) {
                char *source_field_label = semantic_type_resolution_projection_slot_field_label(
                    zone_decl,
                    ast_zone_refresh_source_slot_name(refresh),
                    source_path_text);
                if (source_field_label != NULL) {
                    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                                      TYPE_RES_NODE_PROJECTION_PATH,
                                                      refresh,
                                                      source_field_label);
                    semantic_type_resolution_record_named_dependency(
                        ctx,
                        refresh,
                        projection_label,
                        TYPE_RES_NODE_PROJECTION_PATH,
                        refresh,
                        source_field_label,
                        "projection source field-path lookup");
                    free(source_field_label);
                }
            }

            if (resolved_source_path != NULL)
                free(resolved_source_path);
        }

        free(projection_label);
    }
}
