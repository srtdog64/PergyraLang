#include "mir_decl_headers.h"

ASTNode *
mir_decl_header_source_ast(const MIRDeclHeader *header)
{
    return header != NULL ? header->source_ast : NULL;
}

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

ASTNode *
mir_decl_generic_param_constraint(const MIRDeclGenericParam *param)
{
    return param != NULL ? param->bound_ast : NULL;
}

ASTNode *
mir_decl_generic_param_default_type(const MIRDeclGenericParam *param)
{
    return param != NULL ? param->default_arg_ast : NULL;
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

ASTNode *
mir_decl_method_source_ast(const MIRDeclMethod *method)
{
    return method != NULL ? method->source_ast : NULL;
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

ASTNode *
mir_decl_field_source_ast(const MIRDeclField *field)
{
    return field != NULL ? field->source_ast : NULL;
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
