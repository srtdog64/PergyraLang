#include "mir_decl_header_zone_state_validate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_decl_header_zone_state_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    int written;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    written = vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != length) {
        free(result);
        return NULL;
    }
    return result;
}

bool
mir_decl_header_validate_zone_states(const MIRDeclHeader *header,
                                     size_t header_index,
                                     char **error_message)
{
    if (header == NULL)
        return false;

    if (header->zone_state_metadata_count > 0
        && header->zone_state_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_zone_state_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu zone state metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->zone_state_metadata_count);
        }
        return false;
    }

    if (header->ast_type != AST_ZONE_DECL
        && header->zone_state_metadata_count != 0) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_zone_state_strdup_fmt(
                "MIR declaration header[%zu] '%s' has zone state metadata on a non-zone declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }

    if (header->zone_state_metadata_count != header->zone_state_count) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_zone_state_strdup_fmt(
                "MIR declaration header[%zu] '%s' zone state metadata count %zu does not match declaration state count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->zone_state_metadata_count,
                header->zone_state_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->zone_state_metadata_count; i++) {
        const MIRDeclZoneState *state = &header->zone_state_metadata[i];
        if (state->owner_name == NULL
            || header->name == NULL
            || strcmp(state->owner_name, header->name) != 0
            || state->name == NULL
            || state->layer_slot_name == NULL
            || state->left_or_target_slot_name == NULL
            || (state->is_relation && state->right_slot_name == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_decl_header_zone_state_strdup_fmt(
                    "MIR declaration header[%zu] zone state[%zu] has incomplete state metadata",
                    header_index, i);
            }
            return false;
        }
    }

    return true;
}
