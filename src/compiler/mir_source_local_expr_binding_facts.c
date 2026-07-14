#include "mir_source_local_expr_binding_facts.h"

#include <string.h>

#include "mir_decl_headers.h"
#include "mir_source_local_type_shape.h"

static const MIRDeclHeader *
mir_source_local_decl_header(const MIRProgram *program, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < program->decl_header_count; i++) {
        const MIRDeclHeader *header = &program->decl_headers[i];
        if (header->name != NULL && strcmp(header->name, name) == 0)
            return header;
    }
    return NULL;
}

static bool
mir_source_local_decl_header_is_constructor_type(const MIRDeclHeader *header)
{
    if (header == NULL)
        return false;
    switch (header->ast_type) {
    case AST_CLASS_DECL:
    case AST_ZONE_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
        return true;
    default:
        return false;
    }
}

static const MIRDeclHeader *
mir_source_local_decl_header_for_value_type(const MIRProgram *program,
                                            const char *type_name)
{
    char unwrapped[128];
    const MIRDeclHeader *header =
        mir_source_local_decl_header(program, type_name);

    if (header != NULL)
        return header;
    if (!mir_source_local_unwrap_slot_like_type(type_name, unwrapped,
            sizeof(unwrapped))) {
        return NULL;
    }
    return mir_source_local_decl_header(program, unwrapped);
}

static const MIRDeclField *
mir_source_local_header_field(const MIRDeclHeader *header, const char *name)
{
    if (header == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < header->field_metadata_count; i++) {
        const MIRDeclField *field = &header->field_metadata[i];
        if (field->name != NULL && strcmp(field->name, name) == 0)
            return field;
    }
    return NULL;
}

static const MIRDeclMethod *
mir_source_local_header_method(const MIRDeclHeader *header, const char *name)
{
    if (header == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];
        if (method->name != NULL && strcmp(method->name, name) == 0)
            return method;
    }
    return NULL;
}

static const char *
mir_source_local_param_type_name(const MIRRoutine *routine, const char *name)
{
    if (routine == NULL || name == NULL || !routine->has_signature)
        return NULL;
    for (size_t i = 0; i < routine->param_count; i++) {
        FuncParam *param = routine->params != NULL ? routine->params[i] : NULL;
        if (param != NULL && param->name != NULL
            && strcmp(param->name, name) == 0) {
            return routine->param_type_names != NULL
                ? routine->param_type_names[i]
                : NULL;
        }
    }
    return NULL;
}

static const char *
mir_source_local_routine_owner_name(const MIRRoutine *routine)
{
    if (routine == NULL)
        return NULL;
    if (routine->owner_name != NULL)
        return routine->owner_name;
    return routine->hir_routine != NULL ? routine->hir_routine->owner_name
                                        : NULL;
}

static const char *
mir_source_local_owner_field_type_name(const MIRProgram *program,
                                       const MIRRoutine *routine,
                                       const char *name)
{
    const char *owner_name = mir_source_local_routine_owner_name(routine);
    const MIRDeclHeader *owner;
    const MIRDeclField *field;

    if (name == NULL || owner_name == NULL)
        return NULL;
    owner = mir_source_local_decl_header(program, owner_name);
    field = mir_source_local_header_field(owner, name);
    return field != NULL ? field->type_name : NULL;
}

const char *
mir_source_local_identifier_type_name(const MIRProgram *program,
                                      const MIRRoutine *routine,
                                      const char *name)
{
    const char *type_name = mir_source_local_param_type_name(routine, name);

    if (type_name != NULL)
        return type_name;
    if (name != NULL && strcmp(name, "self") == 0) {
        const char *owner_name = mir_source_local_routine_owner_name(routine);
        if (owner_name != NULL && owner_name[0] != '\0')
            return owner_name;
    }
    type_name = mir_routine_source_local_type_name(routine, name);
    if (type_name != NULL)
        return type_name;
    return mir_source_local_owner_field_type_name(program, routine, name);
}

const char *
mir_source_local_member_field_type_name(const MIRProgram *program,
                                        const char *owner_type_name,
                                        const char *member_name)
{
    const MIRDeclField *field = mir_source_local_header_field(
        mir_source_local_decl_header_for_value_type(program, owner_type_name),
        member_name);
    return field != NULL ? field->type_name : NULL;
}

const char *
mir_source_local_member_method_return_type_name(const MIRProgram *program,
                                                const char *owner_type_name,
                                                const char *member_name)
{
    const MIRDeclMethod *method = mir_source_local_header_method(
        mir_source_local_decl_header_for_value_type(program, owner_type_name),
        member_name);
    return method != NULL ? method->return_type_name : NULL;
}

const char *
mir_source_local_decl_call_type_name(const MIRProgram *program,
                                     const char *name)
{
    const MIRDeclHeader *header = mir_source_local_decl_header(program, name);
    const MIRDeclMethod *method;

    if (mir_source_local_decl_header_is_constructor_type(header))
        return header->name;
    if (header != NULL && header->ast_type == AST_INTENT_DECL)
        return "Bool";
    method = mir_source_local_header_method(header, name);
    return method != NULL ? method->return_type_name : NULL;
}

const char *
mir_source_local_owner_method_return_type_name(const MIRProgram *program,
                                               const MIRRoutine *routine,
                                               const char *name)
{
    const char *owner_name = mir_source_local_routine_owner_name(routine);
    const MIRDeclHeader *owner;
    const MIRDeclMethod *method;

    if (name == NULL || owner_name == NULL)
        return NULL;
    owner = mir_source_local_decl_header(program, owner_name);
    method = mir_source_local_header_method(owner, name);
    return method != NULL ? method->return_type_name : NULL;
}
