#include "mir_decl_headers.h"

#include <string.h>

ASTNodeType
mir_decl_header_ast_type_or(const MIRDeclHeader *header, ASTNodeType fallback)
{
    return header != NULL ? header->ast_type : fallback;
}

const char *
mir_decl_header_name(const MIRDeclHeader *header)
{
    return header != NULL ? header->name : NULL;
}

const char *
mir_decl_header_type_alias_target_type_name(const MIRDeclHeader *header)
{
    return header != NULL && header->ast_type == AST_TYPE_ALIAS
        ? header->type_alias_target_type_name
        : NULL;
}

int
mir_decl_header_intent_retry_count(const MIRDeclHeader *header)
{
    return header != NULL && header->ast_type == AST_INTENT_DECL
        ? header->intent_retry_count
        : 0;
}

const char *
mir_decl_header_inventory_resolve_type_alias_target_type_name(
    const MIRDeclHeaderInventory *inventory,
    const char *alias_name)
{
    const char *current = alias_name;

    if (inventory == NULL || alias_name == NULL)
        return NULL;

    for (size_t depth = 0; depth < 32; depth++) {
        const MIRDeclHeader *alias_header = NULL;
        const char *target_type_name;

        for (size_t i = 0; i < inventory->count; i++) {
            const MIRDeclHeader *header = &inventory->headers[i];
            if (header->ast_type == AST_TYPE_ALIAS
                && header->name != NULL
                && strcmp(header->name, current) == 0) {
                alias_header = header;
                break;
            }
        }

        target_type_name =
            mir_decl_header_type_alias_target_type_name(alias_header);
        if (target_type_name == NULL)
            return depth == 0 ? NULL : current;
        current = target_type_name;
        if (strchr(current, '<') != NULL || strchr(current, '(') != NULL)
            return current;
    }

    return current;
}

const char *
mir_decl_header_resolve_type_alias_target_type_name(const MIRProgram *mir,
                                                    const char *alias_name)
{
    MIRDeclHeaderInventory inventory;

    if (mir == NULL)
        return NULL;
    mir_decl_header_inventory_from_program(mir, &inventory);
    return mir_decl_header_inventory_resolve_type_alias_target_type_name(
        &inventory, alias_name);
}

NominalDeclKind
mir_decl_header_nominal_kind_or(const MIRDeclHeader *header,
                                NominalDeclKind fallback)
{
    return header != NULL && header->ast_type == AST_CLASS_DECL
        ? header->nominal_kind
        : fallback;
}

bool
mir_decl_header_uses_pointer_self(const MIRDeclHeader *header)
{
    return header != NULL && header->uses_pointer_self;
}

size_t
mir_decl_header_generic_param_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->generic_metadata_count : 0;
}

const MIRDeclGenericParam *
mir_decl_header_generic_param(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->generic_metadata == NULL
        || index >= header->generic_metadata_count) {
        return NULL;
    }
    return &header->generic_metadata[index];
}

const char *
mir_decl_generic_param_name(const MIRDeclGenericParam *param)
{
    return param != NULL ? param->name : NULL;
}

const char *
mir_decl_generic_param_constraint_type_name(const MIRDeclGenericParam *param)
{
    return param != NULL ? param->bound_type_name : NULL;
}

const char *
mir_decl_generic_param_default_type_name(const MIRDeclGenericParam *param)
{
    return param != NULL ? param->default_arg_type_name : NULL;
}

size_t
mir_decl_header_method_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->method_metadata_count : 0;
}

const MIRDeclMethod *
mir_decl_header_method(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->method_metadata == NULL
        || index >= header->method_metadata_count) {
        return NULL;
    }
    return &header->method_metadata[index];
}

size_t
mir_decl_header_field_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->field_metadata_count : 0;
}

const MIRDeclField *
mir_decl_header_field(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->field_metadata == NULL
        || index >= header->field_metadata_count) {
        return NULL;
    }
    return &header->field_metadata[index];
}

size_t
mir_decl_header_field_claim_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->field_claim_metadata_count : 0;
}

const MIRDeclFieldClaim *
mir_decl_header_field_claim(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->field_claim_metadata == NULL
        || index >= header->field_claim_metadata_count) {
        return NULL;
    }
    return &header->field_claim_metadata[index];
}

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

size_t
mir_decl_header_enum_variant_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->variant_metadata_count : 0;
}

const MIRDeclEnumVariant *
mir_decl_header_enum_variant(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->variant_metadata == NULL
        || index >= header->variant_metadata_count) {
        return NULL;
    }
    return &header->variant_metadata[index];
}

const char *
mir_decl_enum_variant_name(const MIRDeclEnumVariant *variant)
{
    return variant != NULL ? variant->name : NULL;
}

size_t
mir_decl_enum_variant_param_count(const MIRDeclEnumVariant *variant)
{
    return variant != NULL ? variant->param_count : 0;
}

const char *
mir_decl_enum_variant_param_type_name(const MIRDeclEnumVariant *variant,
                                      size_t index)
{
    if (variant == NULL || variant->param_type_names == NULL
        || index >= variant->param_count) {
        return NULL;
    }
    return variant->param_type_names[index];
}

const char *
mir_decl_method_name(const MIRDeclMethod *method)
{
    return method != NULL ? method->name : NULL;
}

size_t
mir_decl_method_param_count(const MIRDeclMethod *method)
{
    return method != NULL ? method->param_count : 0;
}

FuncParam *
mir_decl_method_param(const MIRDeclMethod *method, size_t index)
{
    if (method == NULL || method->params == NULL
        || index >= method->param_count) {
        return NULL;
    }
    return method->params[index];
}

const char *
mir_decl_method_param_type_name(const MIRDeclMethod *method, size_t index)
{
    if (method == NULL || method->param_type_names == NULL
        || index >= method->param_count) {
        return NULL;
    }
    return method->param_type_names[index];
}

ASTNode *
mir_decl_method_return_type(const MIRDeclMethod *method)
{
    return method != NULL ? method->return_type : NULL;
}

const char *
mir_decl_method_return_type_name(const MIRDeclMethod *method)
{
    return method != NULL ? method->return_type_name : NULL;
}

bool
mir_decl_method_is_async(const MIRDeclMethod *method)
{
    return method != NULL && method->is_async;
}

bool
mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    return method != NULL && method->is_action_like;
}

const char *
mir_decl_method_within_zone(const MIRDeclMethod *method)
{
    return method != NULL ? method->within_zone : NULL;
}

const char *
mir_decl_method_causes_effect(const MIRDeclMethod *method)
{
    return method != NULL ? method->causes_effect : NULL;
}

bool
mir_decl_method_routine_index(const MIRDeclMethod *method, size_t *index_out)
{
    if (index_out != NULL)
        *index_out = 0;
    if (method == NULL || !method->has_routine)
        return false;
    if (index_out != NULL)
        *index_out = method->routine_index;
    return true;
}

size_t
mir_decl_method_projection_write_count(const MIRDeclMethod *method)
{
    return method != NULL ? method->projection_write_count : 0;
}

const char *
mir_decl_method_projection_write_root_name(const MIRDeclMethod *method,
                                           size_t index)
{
    if (method == NULL || method->projection_write_root_names == NULL
        || index >= method->projection_write_count) {
        return NULL;
    }
    return method->projection_write_root_names[index];
}

const char *
mir_decl_method_projection_write_member_name(const MIRDeclMethod *method,
                                             size_t index)
{
    if (method == NULL || method->projection_write_member_names == NULL
        || index >= method->projection_write_count) {
        return NULL;
    }
    return method->projection_write_member_names[index];
}

size_t
mir_decl_method_projection_call_count(const MIRDeclMethod *method)
{
    return method != NULL ? method->projection_call_count : 0;
}

const char *
mir_decl_method_projection_call_receiver_name(const MIRDeclMethod *method,
                                              size_t index)
{
    if (method == NULL || method->projection_call_receiver_names == NULL
        || index >= method->projection_call_count) {
        return NULL;
    }
    return method->projection_call_receiver_names[index];
}

const char *
mir_decl_method_projection_call_method_name(const MIRDeclMethod *method,
                                            size_t index)
{
    if (method == NULL || method->projection_call_method_names == NULL
        || index >= method->projection_call_count) {
        return NULL;
    }
    return method->projection_call_method_names[index];
}

const char *
mir_decl_field_owner_name(const MIRDeclField *field)
{
    return field != NULL ? field->owner_name : NULL;
}

const char *
mir_decl_field_name(const MIRDeclField *field)
{
    return field != NULL ? field->name : NULL;
}

ASTNode *
mir_decl_field_type(const MIRDeclField *field)
{
    return field != NULL ? field->type : NULL;
}

ASTNode *
mir_decl_field_initializer(const MIRDeclField *field)
{
    return field != NULL ? field->initializer : NULL;
}

const char *
mir_decl_field_type_name(const MIRDeclField *field)
{
    return field != NULL ? field->type_name : NULL;
}

MIRDeclFieldKind
mir_decl_field_kind_or(const MIRDeclField *field, MIRDeclFieldKind fallback)
{
    return field != NULL ? field->kind : fallback;
}

bool
mir_decl_field_is_dynamic(const MIRDeclField *field)
{
    return field != NULL && field->is_dynamic;
}

bool
mir_decl_field_is_subject_like(const MIRDeclField *field)
{
    return field != NULL && field->is_subject_like;
}

bool
mir_decl_field_is_tobject_like(const MIRDeclField *field)
{
    return field != NULL && field->is_tobject_like;
}

bool
mir_decl_field_is_binding_like(const MIRDeclField *field)
{
    return field != NULL && field->is_binding_like;
}

bool
mir_decl_field_is_relation_layer(const MIRDeclField *field)
{
    return field != NULL && field->is_relation_layer;
}

bool
mir_decl_field_is_pool_layer(const MIRDeclField *field)
{
    return field != NULL && field->is_pool_layer;
}

int
mir_decl_field_pool_capacity(const MIRDeclField *field)
{
    return field != NULL ? field->pool_capacity : 0;
}

size_t
mir_decl_field_required_ability_count(const MIRDeclField *field)
{
    return field != NULL ? field->required_ability_ref_count : 0;
}

const MIRAbilityRef *
mir_decl_field_required_ability_ref(const MIRDeclField *field, size_t index)
{
    if (field == NULL || field->required_ability_refs == NULL
        || index >= field->required_ability_ref_count)
        return NULL;
    return &field->required_ability_refs[index];
}

const char *
mir_decl_field_claim_slot_name(const MIRDeclFieldClaim *claim)
{
    return claim != NULL ? claim->slot_name : NULL;
}

const char *
mir_decl_field_claim_token_name(const MIRDeclFieldClaim *claim)
{
    return claim != NULL ? claim->token_name : NULL;
}

const char *
mir_decl_field_claim_inner_type_name(const MIRDeclFieldClaim *claim)
{
    return claim != NULL ? claim->inner_type_name : NULL;
}

bool
mir_decl_field_claim_is_secure(const MIRDeclFieldClaim *claim)
{
    return claim != NULL && claim->is_secure;
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
