#include "mir_decl_header_authority.h"

#include "mir_ability_ref.h"

#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>

static void
mir_decl_zone_authority_clear(MIRDeclZoneAuthority *authority)
{
    if (authority == NULL)
        return;
    if (authority->required_ability_refs != NULL) {
        for (size_t i = 0; i < authority->required_ability_ref_count; i++)
            mir_ability_ref_clear(&authority->required_ability_refs[i]);
        free(authority->required_ability_refs);
    }
    authority->required_ability_refs = NULL;
    authority->required_ability_ref_count = 0;
}

void
mir_decl_header_free_authorities(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    for (size_t i = 0; header->zone_authority_metadata != NULL
         && i < header->zone_authority_metadata_count; i++) {
        mir_decl_zone_authority_clear(&header->zone_authority_metadata[i]);
    }
    free(header->zone_authority_metadata);
    header->zone_authority_metadata = NULL;
    header->zone_authority_metadata_count = 0;
    header->zone_authority_count = 0;
}

static bool
mir_decl_zone_authority_capture(MIRDeclZoneAuthority *meta,
                                const MIRDeclHeader *header,
                                ASTNode *authority)
{
    size_t ability_count;

    if (meta == NULL || header == NULL || authority == NULL
        || authority->type != AST_ZONE_AUTHORITY) {
        return false;
    }

    meta->owner_name = header->name;
    meta->subject_slot_name =
        ast_zone_authority_subject_slot_name(authority);
    meta->required_ability_refs = NULL;
    meta->required_ability_ref_count = 0;
    if (meta->owner_name == NULL || meta->subject_slot_name == NULL)
        return false;

    ability_count = ast_zone_authority_ability_count(authority);
    if (ability_count == 0)
        return true;
    if (ability_count > SIZE_MAX / sizeof(MIRAbilityRef))
        return false;

    meta->required_ability_refs =
        calloc(ability_count, sizeof(MIRAbilityRef));
    if (meta->required_ability_refs == NULL)
        return false;
    meta->required_ability_ref_count = ability_count;

    for (size_t i = 0; i < ability_count; i++) {
        ASTNode *ability =
            ast_zone_authority_required_ability(authority, i);
        if (!mir_ability_ref_capture(
                &meta->required_ability_refs[i], ability)) {
            return false;
        }
    }
    return true;
}

bool
mir_decl_header_set_authorities(MIRDeclHeader *header, ASTNode *decl)
{
    ASTNode **authorities;
    size_t authority_count;

    if (header == NULL)
        return false;

    header->zone_authority_count = 0;
    header->zone_authority_metadata = NULL;
    header->zone_authority_metadata_count = 0;

    if (decl == NULL || decl->type != AST_ZONE_DECL)
        return true;

    authorities = ast_zone_authorities(decl, &authority_count);
    header->zone_authority_count = authority_count;
    if (authority_count == 0)
        return true;
    if (authority_count > SIZE_MAX / sizeof(MIRDeclZoneAuthority))
        return false;

    header->zone_authority_metadata =
        calloc(authority_count, sizeof(MIRDeclZoneAuthority));
    if (header->zone_authority_metadata == NULL)
        return false;

    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *authority = authorities != NULL ? authorities[i] : NULL;
        if (!mir_decl_zone_authority_capture(
                &header->zone_authority_metadata[i], header, authority)) {
            header->zone_authority_metadata_count = i + 1;
            mir_decl_header_free_authorities(header);
            return false;
        }
        header->zone_authority_metadata_count = i + 1;
    }
    return true;
}
