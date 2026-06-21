#include "mir_decl_headers.h"

#include <stdint.h>
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
mir_decl_header_role_impl_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->role_impl_metadata_count : 0;
}

const MIRDeclRoleImpl *
mir_decl_header_role_impl(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->role_impl_metadata == NULL
        || index >= header->role_impl_metadata_count) {
        return NULL;
    }
    return &header->role_impl_metadata[index];
}

const MIRAbilityRef *
mir_decl_role_impl_ability_ref(const MIRDeclRoleImpl *impl)
{
    return impl != NULL ? &impl->ability_ref : NULL;
}

size_t
mir_decl_role_impl_method_start_index(const MIRDeclRoleImpl *impl)
{
    return impl != NULL ? impl->method_start_index : 0;
}

size_t
mir_decl_role_impl_method_count(const MIRDeclRoleImpl *impl)
{
    return impl != NULL ? impl->method_count : 0;
}

const MIRDeclMethod *
mir_decl_header_role_impl_method(const MIRDeclHeader *header,
                                 const MIRDeclRoleImpl *impl,
                                 size_t index)
{
    size_t method_index;
    if (header == NULL || impl == NULL || index >= impl->method_count)
        return NULL;
    if (impl->method_start_index > SIZE_MAX - index)
        return NULL;
    method_index = impl->method_start_index + index;
    return mir_decl_header_method(header, method_index);
}

size_t
mir_decl_header_role_include_count(const MIRDeclHeader *header)
{
    return header != NULL ? header->role_include_metadata_count : 0;
}

const MIRDeclRoleInclude *
mir_decl_header_role_include(const MIRDeclHeader *header, size_t index)
{
    if (header == NULL || header->role_include_metadata == NULL
        || index >= header->role_include_metadata_count) {
        return NULL;
    }
    return &header->role_include_metadata[index];
}

const char *
mir_decl_role_include_owner_name(const MIRDeclRoleInclude *include)
{
    return include != NULL ? include->owner_name : NULL;
}

const char *
mir_decl_role_include_name(const MIRDeclRoleInclude *include)
{
    return include != NULL ? include->role_name : NULL;
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
