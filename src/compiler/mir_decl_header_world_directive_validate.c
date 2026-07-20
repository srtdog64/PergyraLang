#include "mir_decl_header_world_directive_validate.h"

#include "mir_fact_validate_internal.h"

#include <string.h>

static bool
mir_decl_world_directive_has_valid_shape(
    const MIRDeclWorldDirective *directive)
{
    if (directive == NULL || directive->owner_name == NULL)
        return false;
    if ((directive->zone_slot_name == NULL)
        == (directive->state_name == NULL))
        return false;
    switch (directive->kind) {
    case MIR_DECL_WORLD_DIRECTIVE_ACTIVATE:
    case MIR_DECL_WORLD_DIRECTIVE_MAINTAIN:
    case MIR_DECL_WORLD_DIRECTIVE_DEACTIVATE:
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
            && strcmp(field->name, name) == 0)
            return true;
    }
    return false;
}

bool
mir_decl_header_validate_world_directives(
    const MIRDeclHeader *header,
    size_t header_index,
    char **error_message)
{
    if (header == NULL)
        return false;
    if (header->world_directive_metadata_count > 0
        && header->world_directive_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' has world directive metadata rows but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->ast_type != AST_WORLD_DECL
        && header->world_directive_metadata_count != 0) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' has world directive metadata on a non-world declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->world_directive_metadata_count
        != header->world_directive_count) {
        if (error_message != NULL) {
            *error_message = mir_fact_strdup_fmt(
                "MIR declaration header[%zu] '%s' world directive metadata count %zu does not match declaration directive count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->world_directive_metadata_count,
                header->world_directive_count);
        }
        return false;
    }
    for (size_t i = 0; i < header->world_directive_metadata_count; i++) {
        const MIRDeclWorldDirective *directive =
            &header->world_directive_metadata[i];
        if (!mir_decl_world_directive_has_valid_shape(directive)
            || header->name == NULL
            || strcmp(directive->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR declaration header[%zu] world directive[%zu] has incomplete directive metadata",
                    header_index, i);
            }
            return false;
        }
        if (directive->zone_slot_name != NULL
            && !mir_decl_header_has_world_zone_slot_name(
                header, directive->zone_slot_name)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR declaration header[%zu] world directive[%zu] references unknown zone slot '%s'",
                    header_index, i, directive->zone_slot_name);
            }
            return false;
        }
        if (directive->state_name != NULL
            && !mir_decl_header_has_world_state_name(header,
                                                     directive->state_name)
            && !mir_decl_header_has_world_zone_slot_name(
                header, directive->state_name)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR declaration header[%zu] world directive[%zu] references unknown state or zone slot '%s'",
                    header_index, i, directive->state_name);
            }
            return false;
        }
    }
    return true;
}
