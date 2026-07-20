#include "mir_decl_headers.h"

size_t
mir_decl_header_world_directive_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->world_directive_metadata_count : 0;
}

size_t
mir_decl_header_world_directive_declared_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->world_directive_count : 0;
}

const MIRDeclWorldDirective *
mir_decl_header_world_directive(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->world_directive_metadata == NULL
        || index >= header->world_directive_metadata_count)
        return NULL;
    return &header->world_directive_metadata[index];
}

const char *
mir_decl_world_directive_owner_name(
    const MIRDeclWorldDirective *directive)
{
    return directive != NULL ? directive->owner_name : NULL;
}

MIRDeclWorldDirectiveKind
mir_decl_world_directive_kind(const MIRDeclWorldDirective *directive)
{
    return directive != NULL
        ? directive->kind : MIR_DECL_WORLD_DIRECTIVE_ACTIVATE;
}

const char *
mir_decl_world_directive_zone_slot_name(
    const MIRDeclWorldDirective *directive)
{
    return directive != NULL ? directive->zone_slot_name : NULL;
}

const char *
mir_decl_world_directive_state_name(
    const MIRDeclWorldDirective *directive)
{
    return directive != NULL ? directive->state_name : NULL;
}
