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

ASTNode *
mir_decl_method_return_type(const MIRDeclMethod *method)
{
    return method != NULL ? method->return_type : NULL;
}

bool
mir_decl_method_is_action_like(const MIRDeclMethod *method)
{
    return method != NULL && method->is_action_like;
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
