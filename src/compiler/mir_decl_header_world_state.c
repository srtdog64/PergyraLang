#include "mir_decl_header_world_state.h"

#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_decl_world_state_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL)
        return NULL;
    length = strlen(text);
    copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, length + 1);
    return copy;
}

static void
mir_decl_world_state_clear(MIRDeclWorldState *meta)
{
    if (meta == NULL)
        return;
    free(meta->name);
    free(meta->zone_slot_name);
    free(meta->detail_name);
    if (meta->input_names != NULL) {
        for (size_t i = 0; i < meta->input_count; i++)
            free(meta->input_names[i]);
    }
    meta->name = NULL;
    meta->zone_slot_name = NULL;
    meta->detail_name = NULL;
    free(meta->input_names);
    meta->input_names = NULL;
    meta->input_count = 0;
}

void
mir_decl_header_free_world_states(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    for (size_t i = 0; i < header->world_state_metadata_count; i++)
        mir_decl_world_state_clear(&header->world_state_metadata[i]);
    free(header->world_state_metadata);
    header->world_state_metadata = NULL;
    header->world_state_metadata_count = 0;
    header->world_state_count = 0;
}

static bool
mir_decl_world_state_capture(MIRDeclWorldState *meta,
                             const MIRDeclHeader *header,
                             ASTNode *state)
{
    size_t input_count;

    if (meta == NULL || header == NULL || state == NULL
        || state->type != AST_WORLD_STATE)
        return false;

    meta->owner_name = header->name;
    meta->name = mir_decl_world_state_strdup(ast_world_state_name(state));
    if (meta->name == NULL)
        return false;
    if (ast_world_state_zone_slot_name(state) != NULL) {
        meta->zone_slot_name = mir_decl_world_state_strdup(
            ast_world_state_zone_slot_name(state));
        if (meta->zone_slot_name == NULL)
            return false;
    }
    meta->source_kind = ast_world_state_source_kind(state);
    if (ast_world_state_detail_name(state) != NULL) {
        meta->detail_name = mir_decl_world_state_strdup(
            ast_world_state_detail_name(state));
        if (meta->detail_name == NULL)
            return false;
    }
    input_count = ast_world_state_input_count(state);
    meta->input_count = input_count;
    if (input_count > 0) {
        if (input_count > SIZE_MAX / sizeof(char *))
            return false;
        meta->input_names = calloc(input_count, sizeof(char *));
        if (meta->input_names == NULL)
            return false;
        for (size_t i = 0; i < input_count; i++) {
            const char *input_name = ast_world_state_input_name(state, i);
            if (input_name == NULL)
                return false;
            meta->input_names[i] = mir_decl_world_state_strdup(input_name);
            if (meta->input_names[i] == NULL)
                return false;
        }
    }

    if (meta->owner_name == NULL)
        return false;
    if ((meta->source_kind == WORLD_STATE_SOURCE_ZONE
            || meta->source_kind == WORLD_STATE_SOURCE_PROJECTION
            || meta->source_kind == WORLD_STATE_SOURCE_LAYER
            || meta->source_kind == WORLD_STATE_SOURCE_STATE)
        && meta->zone_slot_name == NULL)
        return false;
    if ((meta->source_kind == WORLD_STATE_SOURCE_PROJECTION
            || meta->source_kind == WORLD_STATE_SOURCE_LAYER
            || meta->source_kind == WORLD_STATE_SOURCE_STATE)
        && meta->detail_name == NULL)
        return false;
    if ((meta->source_kind == WORLD_STATE_SOURCE_ALL
            || meta->source_kind == WORLD_STATE_SOURCE_ANY)
        && input_count == 0)
        return false;
    return true;
}

bool
mir_decl_header_set_world_states(MIRDeclHeader *header, ASTNode *decl)
{
    ASTNode **states = NULL;
    size_t state_count = 0;

    if (header == NULL)
        return false;
    header->world_state_count = 0;
    header->world_state_metadata = NULL;
    header->world_state_metadata_count = 0;

    if (decl == NULL || decl->type != AST_WORLD_DECL)
        return true;

    states = ast_world_states(decl, &state_count);
    header->world_state_count = state_count;
    if (state_count == 0)
        return true;
    if (state_count > SIZE_MAX / sizeof(MIRDeclWorldState))
        return false;
    header->world_state_metadata =
        calloc(state_count, sizeof(MIRDeclWorldState));
    if (header->world_state_metadata == NULL)
        return false;

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states != NULL ? states[i] : NULL;
        if (!mir_decl_world_state_capture(
                &header->world_state_metadata[i], header, state)) {
            header->world_state_metadata_count = i + 1;
            mir_decl_header_free_world_states(header);
            return false;
        }
        header->world_state_metadata_count = i + 1;
    }
    return true;
}
