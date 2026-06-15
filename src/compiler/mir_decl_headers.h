#ifndef PGY_MIR_DECL_HEADERS_H
#define PGY_MIR_DECL_HEADERS_H

#include "mir.h"

bool mir_record_decl_header(MIRProgram *mir, ASTNode *decl);
void mir_link_decl_method_routines(MIRProgram *mir);
ASTNode *mir_decl_header_source_decl(const MIRDeclHeader *header);
ASTNodeType mir_decl_header_ast_type_or(const MIRDeclHeader *header,
                                        ASTNodeType fallback);
const char *mir_decl_header_name(const MIRDeclHeader *header);
const char *mir_decl_header_type_alias_target_type_name(
    const MIRDeclHeader *header);
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
size_t mir_decl_header_generic_param_count(const MIRDeclHeader *header);
const MIRDeclGenericParam *mir_decl_header_generic_param(
    const MIRDeclHeader *header,
    size_t index);
const char *mir_decl_generic_param_name(const MIRDeclGenericParam *param);
ASTNode *mir_decl_generic_param_constraint(const MIRDeclGenericParam *param);
ASTNode *mir_decl_generic_param_default_type(const MIRDeclGenericParam *param);
size_t mir_decl_header_method_count(const MIRDeclHeader *header);
const MIRDeclMethod *mir_decl_header_method(const MIRDeclHeader *header,
                                            size_t index);
size_t mir_decl_header_field_count(const MIRDeclHeader *header);
const MIRDeclField *mir_decl_header_field(const MIRDeclHeader *header,
                                          size_t index);
size_t mir_decl_header_enum_variant_count(const MIRDeclHeader *header);
const MIRDeclEnumVariant *mir_decl_header_enum_variant(
    const MIRDeclHeader *header,
    size_t index);
const char *mir_decl_enum_variant_name(const MIRDeclEnumVariant *variant);
size_t mir_decl_enum_variant_param_count(const MIRDeclEnumVariant *variant);
const char *mir_decl_enum_variant_param_type_name(
    const MIRDeclEnumVariant *variant, size_t index);
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
const char *mir_ability_ref_base_name(const MIRAbilityRef *ref);
size_t mir_ability_ref_actual_arg_count(const MIRAbilityRef *ref);
const char *mir_ability_ref_actual_arg_type_name(
    const MIRAbilityRef *ref, size_t index);

#endif /* PGY_MIR_DECL_HEADERS_H */
