#include "mir_decl_header_refresh.h"

#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>

void
mir_decl_header_free_refreshes(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    for (size_t i = 0; header->zone_refresh_metadata != NULL
         && i < header->zone_refresh_metadata_count; i++) {
        free(header->zone_refresh_metadata[i].field_maps);
        header->zone_refresh_metadata[i].field_maps = NULL;
        header->zone_refresh_metadata[i].field_map_count = 0;
    }
    free(header->zone_refresh_metadata);
    header->zone_refresh_metadata = NULL;
    header->zone_refresh_metadata_count = 0;
    header->zone_refresh_count = 0;
}

static bool
mir_decl_zone_refresh_capture(MIRDeclZoneRefresh *meta,
                              const MIRDeclHeader *header,
                              ASTNode *refresh)
{
    size_t field_map_count;

    if (meta == NULL || header == NULL || refresh == NULL
        || refresh->type != AST_ZONE_REFRESH) {
        return false;
    }

    meta->owner_name = header->name;
    meta->object_slot_name = ast_zone_refresh_object_slot_name(refresh);
    meta->source_slot_name = ast_zone_refresh_source_slot_name(refresh);
    meta->participant_slot_name =
        ast_zone_refresh_participant_slot_name(refresh);
    meta->requires_dto = ast_zone_refresh_requires_dto(refresh);
    meta->derives_target_kind =
        ast_zone_refresh_derives_target_kind(refresh);
    meta->field_maps = NULL;
    meta->field_map_count = 0;

    if (meta->owner_name == NULL || meta->object_slot_name == NULL
        || meta->source_slot_name == NULL) {
        return false;
    }

    field_map_count = ast_zone_refresh_field_map_count(refresh);
    if (field_map_count == 0)
        return true;
    if (field_map_count > SIZE_MAX / sizeof(MIRDeclZoneRefreshFieldMap))
        return false;

    meta->field_maps =
        calloc(field_map_count, sizeof(MIRDeclZoneRefreshFieldMap));
    if (meta->field_maps == NULL)
        return false;
    meta->field_map_count = field_map_count;

    for (size_t i = 0; i < field_map_count; i++) {
        meta->field_maps[i].target_field_name =
            ast_zone_refresh_mapped_target_field(refresh, i);
        meta->field_maps[i].source_field_name =
            ast_zone_refresh_mapped_source_field(refresh, i);
        if (meta->field_maps[i].target_field_name == NULL
            || meta->field_maps[i].source_field_name == NULL) {
            return false;
        }
    }
    return true;
}

bool
mir_decl_header_set_refreshes(MIRDeclHeader *header, ASTNode *decl)
{
    ASTNode **refreshes;
    size_t refresh_count;

    if (header == NULL)
        return false;

    header->zone_refresh_count = 0;
    header->zone_refresh_metadata = NULL;
    header->zone_refresh_metadata_count = 0;

    if (decl == NULL)
        return true;

    switch (decl->type) {
    case AST_RELATION_DECL:
        refreshes = ast_relation_refreshes(decl, &refresh_count);
        break;
    case AST_EFFECT_DECL:
        refreshes = ast_effect_refreshes(decl, &refresh_count);
        break;
    case AST_ZONE_DECL:
        refreshes = ast_zone_refreshes(decl, &refresh_count);
        break;
    default:
        return true;
    }
    header->zone_refresh_count = refresh_count;
    if (refresh_count == 0)
        return true;
    if (refresh_count > SIZE_MAX / sizeof(MIRDeclZoneRefresh))
        return false;

    header->zone_refresh_metadata =
        calloc(refresh_count, sizeof(MIRDeclZoneRefresh));
    if (header->zone_refresh_metadata == NULL)
        return false;

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes != NULL ? refreshes[i] : NULL;
        if (!mir_decl_zone_refresh_capture(
                &header->zone_refresh_metadata[i], header, refresh)) {
            header->zone_refresh_metadata_count = i + 1;
            mir_decl_header_free_refreshes(header);
            return false;
        }
        header->zone_refresh_metadata_count = i + 1;
    }
    return true;
}
