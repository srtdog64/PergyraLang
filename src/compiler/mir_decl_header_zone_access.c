#include "mir_decl_headers.h"

size_t
mir_decl_header_zone_authority_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->zone_authority_metadata_count : 0;
}

const MIRDeclZoneAuthority *
mir_decl_header_zone_authority(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->zone_authority_metadata == NULL
        || index >= header->zone_authority_metadata_count) {
        return NULL;
    }
    return &header->zone_authority_metadata[index];
}

size_t
mir_decl_header_zone_refresh_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->zone_refresh_metadata_count : 0;
}

const MIRDeclZoneRefresh *
mir_decl_header_zone_refresh(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->zone_refresh_metadata == NULL
        || index >= header->zone_refresh_metadata_count) {
        return NULL;
    }
    return &header->zone_refresh_metadata[index];
}

size_t
mir_decl_header_zone_state_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->zone_state_metadata_count : 0;
}

const MIRDeclZoneState *
mir_decl_header_zone_state(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->zone_state_metadata == NULL
        || index >= header->zone_state_metadata_count) {
        return NULL;
    }
    return &header->zone_state_metadata[index];
}

const char *
mir_decl_zone_authority_owner_name(const MIRDeclZoneAuthority *authority)
{
    return authority != NULL ? authority->owner_name : NULL;
}

const char *
mir_decl_zone_authority_subject_slot_name(
    const MIRDeclZoneAuthority *authority)
{
    return authority != NULL ? authority->subject_slot_name : NULL;
}

size_t
mir_decl_zone_authority_required_ability_count(
    const MIRDeclZoneAuthority *authority)
{
    return authority != NULL ? authority->required_ability_ref_count : 0;
}

const MIRAbilityRef *
mir_decl_zone_authority_required_ability_ref(
    const MIRDeclZoneAuthority *authority, size_t index)
{
    if (authority == NULL || authority->required_ability_refs == NULL
        || index >= authority->required_ability_ref_count)
        return NULL;
    return &authority->required_ability_refs[index];
}

const char *
mir_decl_zone_refresh_owner_name(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL ? refresh->owner_name : NULL;
}

const char *
mir_decl_zone_refresh_object_slot_name(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL ? refresh->object_slot_name : NULL;
}

const char *
mir_decl_zone_refresh_source_slot_name(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL ? refresh->source_slot_name : NULL;
}

const char *
mir_decl_zone_refresh_participant_slot_name(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL ? refresh->participant_slot_name : NULL;
}

bool
mir_decl_zone_refresh_requires_dto(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL && refresh->requires_dto;
}

bool
mir_decl_zone_refresh_derives_target_kind(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL && refresh->derives_target_kind;
}

size_t
mir_decl_zone_refresh_field_map_count(const MIRDeclZoneRefresh *refresh)
{
    return refresh != NULL ? refresh->field_map_count : 0;
}

const char *
mir_decl_zone_refresh_mapped_target_field(const MIRDeclZoneRefresh *refresh,
                                          size_t index)
{
    if (refresh == NULL || refresh->field_maps == NULL
        || index >= refresh->field_map_count) {
        return NULL;
    }
    return refresh->field_maps[index].target_field_name;
}

const char *
mir_decl_zone_refresh_mapped_source_field(const MIRDeclZoneRefresh *refresh,
                                          size_t index)
{
    if (refresh == NULL || refresh->field_maps == NULL
        || index >= refresh->field_map_count) {
        return NULL;
    }
    return refresh->field_maps[index].source_field_name;
}

const char *
mir_decl_zone_state_owner_name(const MIRDeclZoneState *state)
{
    return state != NULL ? state->owner_name : NULL;
}

const char *
mir_decl_zone_state_name(const MIRDeclZoneState *state)
{
    return state != NULL ? state->name : NULL;
}

const char *
mir_decl_zone_state_layer_slot_name(const MIRDeclZoneState *state)
{
    return state != NULL ? state->layer_slot_name : NULL;
}

const char *
mir_decl_zone_state_left_or_target_slot_name(const MIRDeclZoneState *state)
{
    return state != NULL ? state->left_or_target_slot_name : NULL;
}

const char *
mir_decl_zone_state_right_slot_name(const MIRDeclZoneState *state)
{
    return state != NULL ? state->right_slot_name : NULL;
}

bool
mir_decl_zone_state_is_relation(const MIRDeclZoneState *state)
{
    return state != NULL && state->is_relation;
}

const char *
mir_ability_ref_base_name(const MIRAbilityRef *ref)
{
    return ref != NULL ? ref->base_name : NULL;
}

size_t
mir_ability_ref_actual_arg_count(const MIRAbilityRef *ref)
{
    return ref != NULL ? ref->actual_arg_count : 0;
}

const char *
mir_ability_ref_actual_arg_type_name(const MIRAbilityRef *ref, size_t index)
{
    if (ref == NULL || ref->actual_arg_type_names == NULL
        || index >= ref->actual_arg_count)
        return NULL;
    return ref->actual_arg_type_names[index];
}
