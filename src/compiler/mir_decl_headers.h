#ifndef PGY_MIR_DECL_HEADERS_H
#define PGY_MIR_DECL_HEADERS_H

#include "mir.h"

bool mir_record_decl_header(MIRProgram *mir, ASTNode *decl);
void mir_link_decl_method_routines(MIRProgram *mir);
ASTNodeType mir_decl_header_ast_type_or(const MIRDeclHeader *header,
                                        ASTNodeType fallback);
const char *mir_decl_header_name(const MIRDeclHeader *header);
const char *mir_decl_header_type_alias_target_type_name(
    const MIRDeclHeader *header);
int mir_decl_header_intent_retry_count(const MIRDeclHeader *header);
const char *mir_decl_header_inventory_resolve_type_alias_target_type_name(
    const MIRDeclHeaderInventory *inventory,
    const char *alias_name);
const char *mir_decl_header_resolve_type_alias_target_type_name(
    const MIRProgram *mir,
    const char *alias_name);
NominalDeclKind mir_decl_header_nominal_kind_or(
    const MIRDeclHeader *header,
    NominalDeclKind fallback);
bool mir_decl_header_uses_pointer_self(const MIRDeclHeader *header);
const char *mir_decl_header_role_subject_type_name(
    const MIRDeclHeader *header);
size_t mir_decl_header_generic_param_count(const MIRDeclHeader *header);
const MIRDeclGenericParam *mir_decl_header_generic_param(
    const MIRDeclHeader *header,
    size_t index);
const char *mir_decl_generic_param_name(const MIRDeclGenericParam *param);
const char *mir_decl_generic_param_constraint_type_name(
    const MIRDeclGenericParam *param);
const char *mir_decl_generic_param_default_type_name(
    const MIRDeclGenericParam *param);
size_t mir_decl_header_method_count(const MIRDeclHeader *header);
const MIRDeclMethod *mir_decl_header_method(const MIRDeclHeader *header,
                                            size_t index);
size_t mir_decl_header_role_impl_count(const MIRDeclHeader *header);
const MIRDeclRoleImpl *mir_decl_header_role_impl(
    const MIRDeclHeader *header,
    size_t index);
const MIRAbilityRef *mir_decl_role_impl_ability_ref(
    const MIRDeclRoleImpl *impl);
size_t mir_decl_role_impl_method_start_index(const MIRDeclRoleImpl *impl);
size_t mir_decl_role_impl_method_count(const MIRDeclRoleImpl *impl);
const MIRDeclMethod *mir_decl_header_role_impl_method(
    const MIRDeclHeader *header,
    const MIRDeclRoleImpl *impl,
    size_t index);
size_t mir_decl_header_role_include_count(const MIRDeclHeader *header);
const MIRDeclRoleInclude *mir_decl_header_role_include(
    const MIRDeclHeader *header,
    size_t index);
const char *mir_decl_role_include_owner_name(
    const MIRDeclRoleInclude *include);
const char *mir_decl_role_include_name(const MIRDeclRoleInclude *include);
size_t mir_decl_header_field_count(const MIRDeclHeader *header);
const MIRDeclField *mir_decl_header_field(const MIRDeclHeader *header,
                                          size_t index);
size_t mir_decl_header_field_claim_count(const MIRDeclHeader *header);
const MIRDeclFieldClaim *mir_decl_header_field_claim(
    const MIRDeclHeader *header,
    size_t index);
size_t mir_decl_header_zone_authority_count(const MIRDeclHeader *header);
const MIRDeclZoneAuthority *mir_decl_header_zone_authority(
    const MIRDeclHeader *header,
    size_t index);
size_t mir_decl_header_zone_refresh_count(const MIRDeclHeader *header);
const MIRDeclZoneRefresh *mir_decl_header_zone_refresh(
    const MIRDeclHeader *header,
    size_t index);
size_t mir_decl_header_zone_state_count(const MIRDeclHeader *header);
const MIRDeclZoneState *mir_decl_header_zone_state(
    const MIRDeclHeader *header,
    size_t index);
size_t mir_decl_header_enum_variant_count(const MIRDeclHeader *header);
const MIRDeclEnumVariant *mir_decl_header_enum_variant(
    const MIRDeclHeader *header,
    size_t index);
const char *mir_decl_enum_variant_name(const MIRDeclEnumVariant *variant);
size_t mir_decl_enum_variant_param_count(const MIRDeclEnumVariant *variant);
const char *mir_decl_enum_variant_param_type_name(
    const MIRDeclEnumVariant *variant, size_t index);
size_t mir_decl_header_event_param_count(const MIRDeclHeader *header);
const char *mir_decl_header_event_param_name(const MIRDeclHeader *header,
                                             size_t index);
const char *mir_decl_header_event_param_type_name(const MIRDeclHeader *header,
                                                  size_t index);
const char *mir_decl_method_name(const MIRDeclMethod *method);
size_t mir_decl_method_param_count(const MIRDeclMethod *method);
FuncParam *mir_decl_method_param(const MIRDeclMethod *method, size_t index);
const char *mir_decl_method_param_type_name(const MIRDeclMethod *method,
                                            size_t index);
ASTNode *mir_decl_method_return_type(const MIRDeclMethod *method);
const char *mir_decl_method_return_type_name(const MIRDeclMethod *method);
bool mir_decl_method_is_async(const MIRDeclMethod *method);
bool mir_decl_method_is_action_like(const MIRDeclMethod *method);
const char *mir_decl_method_within_zone(const MIRDeclMethod *method);
const char *mir_decl_method_causes_effect(const MIRDeclMethod *method);
size_t mir_decl_method_authorized_by_count(const MIRDeclMethod *method);
const char *mir_decl_method_authorized_by(const MIRDeclMethod *method,
                                          size_t index);
size_t mir_decl_method_required_ability_count(const MIRDeclMethod *method);
const MIRAbilityRef *mir_decl_method_required_ability_ref(
    const MIRDeclMethod *method, size_t index);
bool mir_decl_method_has_caps_clause(const MIRDeclMethod *method);
uint32_t mir_decl_method_declared_capabilities(const MIRDeclMethod *method);
bool mir_decl_method_has_effects_clause(const MIRDeclMethod *method);
uint32_t mir_decl_method_declared_effects(const MIRDeclMethod *method);
void mir_decl_method_metadata_clear(MIRDeclMethod *method);
bool mir_decl_method_routine_index(const MIRDeclMethod *method,
                                   size_t *index_out);
size_t mir_decl_method_projection_write_count(const MIRDeclMethod *method);
const char *mir_decl_method_projection_write_root_name(
    const MIRDeclMethod *method, size_t index);
const char *mir_decl_method_projection_write_member_name(
    const MIRDeclMethod *method, size_t index);
size_t mir_decl_method_projection_call_count(const MIRDeclMethod *method);
const char *mir_decl_method_projection_call_receiver_name(
    const MIRDeclMethod *method, size_t index);
const char *mir_decl_method_projection_call_method_name(
    const MIRDeclMethod *method, size_t index);
const char *mir_decl_field_owner_name(const MIRDeclField *field);
const char *mir_decl_field_name(const MIRDeclField *field);
uint32_t mir_decl_field_source_syntax_id(const MIRDeclField *field);
ASTNode *mir_decl_field_type(const MIRDeclField *field);
ASTNode *mir_decl_field_initializer(const MIRDeclField *field);
const char *mir_decl_field_type_name(const MIRDeclField *field);
MIRDeclFieldKind mir_decl_field_kind_or(const MIRDeclField *field,
                                        MIRDeclFieldKind fallback);
bool mir_decl_field_is_dynamic(const MIRDeclField *field);
bool mir_decl_field_is_subject_like(const MIRDeclField *field);
bool mir_decl_field_is_tobject_like(const MIRDeclField *field);
bool mir_decl_field_is_binding_like(const MIRDeclField *field);
bool mir_decl_field_is_relation_layer(const MIRDeclField *field);
bool mir_decl_field_is_pool_layer(const MIRDeclField *field);
int mir_decl_field_pool_capacity(const MIRDeclField *field);
size_t mir_decl_field_required_ability_count(const MIRDeclField *field);
const MIRAbilityRef *mir_decl_field_required_ability_ref(
    const MIRDeclField *field, size_t index);
const char *mir_decl_field_claim_slot_name(const MIRDeclFieldClaim *claim);
const char *mir_decl_field_claim_token_name(const MIRDeclFieldClaim *claim);
const char *mir_decl_field_claim_inner_type_name(
    const MIRDeclFieldClaim *claim);
bool mir_decl_field_claim_is_secure(const MIRDeclFieldClaim *claim);
const char *mir_decl_zone_authority_owner_name(
    const MIRDeclZoneAuthority *authority);
const char *mir_decl_zone_authority_subject_slot_name(
    const MIRDeclZoneAuthority *authority);
size_t mir_decl_zone_authority_required_ability_count(
    const MIRDeclZoneAuthority *authority);
const MIRAbilityRef *mir_decl_zone_authority_required_ability_ref(
    const MIRDeclZoneAuthority *authority, size_t index);
const char *mir_decl_zone_refresh_owner_name(
    const MIRDeclZoneRefresh *refresh);
const char *mir_decl_zone_refresh_object_slot_name(
    const MIRDeclZoneRefresh *refresh);
const char *mir_decl_zone_refresh_source_slot_name(
    const MIRDeclZoneRefresh *refresh);
const char *mir_decl_zone_refresh_participant_slot_name(
    const MIRDeclZoneRefresh *refresh);
bool mir_decl_zone_refresh_requires_dto(const MIRDeclZoneRefresh *refresh);
bool mir_decl_zone_refresh_derives_target_kind(
    const MIRDeclZoneRefresh *refresh);
size_t mir_decl_zone_refresh_field_map_count(
    const MIRDeclZoneRefresh *refresh);
const char *mir_decl_zone_refresh_mapped_target_field(
    const MIRDeclZoneRefresh *refresh, size_t index);
const char *mir_decl_zone_refresh_mapped_source_field(
    const MIRDeclZoneRefresh *refresh, size_t index);
const char *mir_decl_zone_state_owner_name(const MIRDeclZoneState *state);
const char *mir_decl_zone_state_name(const MIRDeclZoneState *state);
const char *mir_decl_zone_state_layer_slot_name(
    const MIRDeclZoneState *state);
const char *mir_decl_zone_state_left_or_target_slot_name(
    const MIRDeclZoneState *state);
const char *mir_decl_zone_state_right_slot_name(
    const MIRDeclZoneState *state);
bool mir_decl_zone_state_is_relation(const MIRDeclZoneState *state);
size_t mir_decl_header_world_state_count(const MIRDeclHeader *header);
size_t mir_decl_header_world_state_declared_count(
    const MIRDeclHeader *header);
const MIRDeclWorldState *mir_decl_header_world_state(
    const MIRDeclHeader *header, size_t index);
const char *mir_decl_world_state_owner_name(const MIRDeclWorldState *state);
const char *mir_decl_world_state_name(const MIRDeclWorldState *state);
const char *mir_decl_world_state_zone_slot_name(
    const MIRDeclWorldState *state);
WorldStateSourceKind mir_decl_world_state_source_kind(
    const MIRDeclWorldState *state);
const char *mir_decl_world_state_detail_name(
    const MIRDeclWorldState *state);
size_t mir_decl_world_state_input_count(const MIRDeclWorldState *state);
const char *mir_decl_world_state_input_name(
    const MIRDeclWorldState *state, size_t index);
size_t mir_decl_header_world_directive_count(
    const MIRDeclHeader *header);
size_t mir_decl_header_world_directive_declared_count(
    const MIRDeclHeader *header);
const MIRDeclWorldDirective *mir_decl_header_world_directive(
    const MIRDeclHeader *header, size_t index);
const char *mir_decl_world_directive_owner_name(
    const MIRDeclWorldDirective *directive);
MIRDeclWorldDirectiveKind mir_decl_world_directive_kind(
    const MIRDeclWorldDirective *directive);
const char *mir_decl_world_directive_zone_slot_name(
    const MIRDeclWorldDirective *directive);
const char *mir_decl_world_directive_state_name(
    const MIRDeclWorldDirective *directive);
const char *mir_ability_ref_base_name(const MIRAbilityRef *ref);
size_t mir_ability_ref_actual_arg_count(const MIRAbilityRef *ref);
const char *mir_ability_ref_actual_arg_type_name(
    const MIRAbilityRef *ref, size_t index);

#endif /* PGY_MIR_DECL_HEADERS_H */
