#include "mir_decl_header_world_state_validate.h"

#include "mir_fact_validate_internal.h"

#include <string.h>

static bool
mir_decl_world_state_has_valid_shape(const MIRDeclWorldState *state)
{
    if (state->owner_name == NULL || state->name == NULL)
        return false;
    switch (state->source_kind) {
    case WORLD_STATE_SOURCE_ZONE:
        return state->zone_slot_name != NULL
            && state->input_count == 0;
    case WORLD_STATE_SOURCE_PROJECTION:
    case WORLD_STATE_SOURCE_LAYER:
    case WORLD_STATE_SOURCE_STATE:
        return state->zone_slot_name != NULL
            && state->detail_name != NULL
            && state->input_count == 0;
    case WORLD_STATE_SOURCE_ALL:
    case WORLD_STATE_SOURCE_ANY:
        if (state->zone_slot_name != NULL || state->input_count == 0
            || state->input_names == NULL)
            return false;
        for (size_t i = 0; i < state->input_count; i++) {
            if (state->input_names[i] == NULL)
                return false;
        }
        return true;
    }
    return false;
}

static bool
mir_decl_header_has_world_state_name(const MIRDeclHeader *header,
                                     const char *name)
{
    if (header == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < header->world_state_metadata_count; i++) {
        const MIRDeclWorldState *state = &header->world_state_metadata[i];
        if (state->name != NULL && strcmp(state->name, name) == 0)
            return true;
    }
    return false;
}

static bool
mir_decl_header_has_world_zone_slot_name(const MIRDeclHeader *header,
                                         const char *name)
{
    if (header == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < header->field_metadata_count; i++) {
        const MIRDeclField *field = &header->field_metadata[i];
        if (field->kind == MIR_DECL_FIELD_WORLD_ZONE_SLOT
            && field->name != NULL
            && strcmp(field->name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool
mir_decl_header_validate_world_states(const MIRDeclHeader *header,
                                      size_t header_index,
                                      char **error_message)
{
    if (header == NULL)
        return false;
    if (header->world_state_metadata_count > 0
        && header->world_state_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' has world state metadata rows but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->ast_type != AST_WORLD_DECL
        && header->world_state_metadata_count != 0) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' has world state metadata on a non-world declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->world_state_metadata_count != header->world_state_count) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' world state metadata count %zu does not match declaration state count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->world_state_metadata_count,
                header->world_state_count);
        }
        return false;
    }
    for (size_t i = 0; i < header->world_state_metadata_count; i++) {
        const MIRDeclWorldState *state = &header->world_state_metadata[i];
        if (!mir_decl_world_state_has_valid_shape(state)
            || header->name == NULL
            || strcmp(state->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR declaration header[%zu] world state[%zu] has incomplete state metadata",
                    header_index, i);
            }
            return false;
        }
        if (state->zone_slot_name != NULL
            && !mir_decl_header_has_world_zone_slot_name(
                header, state->zone_slot_name)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR declaration header[%zu] world state[%zu] references unknown zone slot '%s'",
                    header_index, i, state->zone_slot_name);
            }
            return false;
        }
        for (size_t input_i = 0; input_i < state->input_count; input_i++) {
            const char *input_name = state->input_names[input_i];
            if (!mir_decl_header_has_world_zone_slot_name(header, input_name)
                && !mir_decl_header_has_world_state_name(header, input_name)) {
                if (error_message != NULL) {
                    *error_message = mir_fact_strdup_fmt(
                        "MIR declaration header[%zu] world state[%zu] references unknown input '%s'",
                        header_index, i,
                        input_name != NULL ? input_name : "(missing)");
                }
                return false;
            }
        }
    }
    return true;
}
