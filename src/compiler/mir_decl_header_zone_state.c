#include "mir_decl_header_zone_state.h"

#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>

void
mir_decl_header_free_zone_states(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    free(header->zone_state_metadata);
    header->zone_state_metadata = NULL;
    header->zone_state_metadata_count = 0;
    header->zone_state_count = 0;
}

static bool
mir_decl_zone_state_capture(MIRDeclZoneState *meta,
                            const MIRDeclHeader *header,
                            ASTNode *state)
{
    if (meta == NULL || header == NULL || state == NULL
        || state->type != AST_ZONE_STATE) {
        return false;
    }

    meta->owner_name = header->name;
    meta->name = ast_zone_state_name(state);
    meta->layer_slot_name = ast_zone_state_layer_slot_name(state);
    meta->left_or_target_slot_name =
        ast_zone_state_left_or_target_slot_name(state);
    meta->right_slot_name = ast_zone_state_right_slot_name(state);
    meta->is_relation = ast_zone_state_is_relation(state);

    if (meta->owner_name == NULL
        || meta->name == NULL
        || meta->layer_slot_name == NULL
        || meta->left_or_target_slot_name == NULL) {
        return false;
    }
    if (meta->is_relation && meta->right_slot_name == NULL)
        return false;
    return true;
}

bool
mir_decl_header_set_zone_states(MIRDeclHeader *header, ASTNode *decl)
{
    ASTNode **states = NULL;
    size_t state_count = 0;

    if (header == NULL)
        return false;

    header->zone_state_count = 0;
    header->zone_state_metadata = NULL;
    header->zone_state_metadata_count = 0;

    if (decl == NULL || decl->type != AST_ZONE_DECL)
        return true;

    states = ast_zone_states(decl, &state_count);
    header->zone_state_count = state_count;
    if (state_count == 0)
        return true;
    if (state_count > SIZE_MAX / sizeof(MIRDeclZoneState))
        return false;

    header->zone_state_metadata =
        calloc(state_count, sizeof(MIRDeclZoneState));
    if (header->zone_state_metadata == NULL)
        return false;

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states != NULL ? states[i] : NULL;
        if (!mir_decl_zone_state_capture(
                &header->zone_state_metadata[i], header, state)) {
            header->zone_state_metadata_count = i + 1;
            mir_decl_header_free_zone_states(header);
            return false;
        }
        header->zone_state_metadata_count = i + 1;
    }
    return true;
}
