#include "type_checker_internal.h"

#include <string.h>

bool
type_is_constructed_named(const Type *type, const char *name)
{
    Type *constructor = type_constructed_constructor(type);

    return type != NULL
        && constructor != NULL
        && constructor->name != NULL
        && strcmp(constructor->name, name) == 0;
}

bool
type_is_qubit(const Type *type)
{
    if (type == NULL)
        return false;
    if (TYPE_QUBIT != NULL && type_equals(type, TYPE_QUBIT))
        return true;
    return type->name != NULL && strcmp(type->name, "QubitSlot") == 0;
}

bool
type_is_slot_handle(const Type *type)
{
    return type != NULL && type->kind == TYPE_KIND_SLOT;
}

bool
type_is_owned_slot_handle(const Type *type)
{
    return type_is_slot_handle(type)
        && type_slot_access_mode(type) == SLOT_ACCESS_OWNED;
}

bool
type_is_read_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type_slot_access_mode(type) == SLOT_ACCESS_READ_VIEW;
}

bool
type_is_write_view(const Type *type)
{
    return type_is_slot_handle(type)
        && type_slot_access_mode(type) == SLOT_ACCESS_WRITE_VIEW;
}

bool
type_is_move_token(const Type *type)
{
    return type_is_slot_handle(type)
        && type_slot_access_mode(type) == SLOT_ACCESS_MOVE_TOKEN;
}

bool
type_is_resource_handle(const Type *type)
{
    return type_is_qubit(type)
        || type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_anchored_resource_handle(const Type *type)
{
    return type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_movable_resource_handle(const Type *type)
{
    return type_is_qubit(type);
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_value_nominal_boundary_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;
    TypeNominalFlavor flavor;

    if (type == NULL || ctx == NULL || ctx->program_root == NULL)
        return false;
    if (type_is_subject_type(type, ctx))
        return false;
    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_ENUM:
    case TYPE_KIND_TUPLE:
        return true;
    case TYPE_KIND_CONSTRUCTED:
        return !type_is_movable_resource_handle(type);
    case TYPE_KIND_CLASS:
        break;
    default:
        return false;
    }
    if (type->name == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return false;

    flavor = nominal_flavor_from_decl(decl);
    return flavor == TYPE_NOMINAL_CLASS
        || flavor == TYPE_NOMINAL_STRUCT
        || flavor == TYPE_NOMINAL_OBJECT
        || flavor == TYPE_NOMINAL_TOBJECT
        || flavor == TYPE_NOMINAL_VESSEL;
}

bool
type_is_general_boundary_type(const Type *type, SemanticContext *ctx)
{
    return type_is_movable_resource_handle(type)
        || type_is_subject_type(type, ctx)
        || type_is_value_nominal_boundary_type(type, ctx);
}

bool
type_requires_boundary_borrow_tracking(const Type *type, SemanticContext *ctx)
{
    if (type == NULL || !type_is_general_boundary_type(type, ctx))
        return false;
    if (type_is_subject_type(type, ctx) || type_is_movable_resource_handle(type))
        return false;

    switch (type->kind) {
    case TYPE_KIND_PRIMITIVE:
    case TYPE_KIND_ENUM:
        return false;
    default:
        return true;
    }
}

bool
type_is_capability_bearing(const Type *type)
{
    return (type_is_owned_slot_handle(type) && type_slot_is_secure(type))
        || type_is_constructed_named(type, "Token");
}

const char *
resource_handle_display_name(const Type *type)
{
    if (type == NULL)
        return "resource";
    if (type_is_qubit(type))
        return "QubitSlot";
    if (type_is_read_view(type))
        return "ReadView";
    if (type_is_write_view(type))
        return "WriteView";
    if (type_is_move_token(type))
        return "MoveToken";
    if (type_is_owned_slot_handle(type))
        return type_slot_is_secure(type) ? "SecureSlot" : "Slot";
    if (type_is_constructed_named(type, "DeviceSlot"))
        return "DeviceSlot";
    return type->name != NULL ? type->name : "resource";
}

ASTNode *
find_type_decl_by_name(ASTNode *program, const char *type_name)
{
    if (program == NULL || program->type != AST_PROGRAM || type_name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt == NULL || stmt->type != AST_CLASS_DECL)
            continue;
        if (ast_class_name(stmt) != NULL
            && strcmp(ast_class_name(stmt), type_name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

ASTNode *
find_ability_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        const char *ability_name = ast_ability_name(stmt);
        if (stmt == NULL || stmt->type != AST_ABILITY_DECL
            || ability_name == NULL)
            continue;
        if (strcmp(ability_name, name) == 0)
            return stmt;
    }

    return NULL;
}

TypeNominalFlavor
nominal_flavor_from_decl(const ASTNode *decl)
{
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return TYPE_NOMINAL_NONE;

    switch (ast_class_nominal_kind(decl)) {
    case NOMINAL_DECL_SUBJECT:
        return TYPE_NOMINAL_SUBJECT;
    case NOMINAL_DECL_VESSEL:
        return TYPE_NOMINAL_VESSEL;
    case NOMINAL_DECL_STRUCT:
        return TYPE_NOMINAL_STRUCT;
    case NOMINAL_DECL_OBJECT:
        return TYPE_NOMINAL_OBJECT;
    case NOMINAL_DECL_TOBJECT:
        return TYPE_NOMINAL_TOBJECT;
    case NOMINAL_DECL_CLASS:
    default:
        return TYPE_NOMINAL_CLASS;
    }
}

static bool
decl_is_subject_type(const ASTNode *decl)
{
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_SUBJECT;
}

bool
decl_is_subject_host(const ASTNode *decl)
{
    return decl_is_subject_type(decl);
}

ASTNode *
find_subject_host_decl_by_name(ASTNode *program, const char *type_name)
{
    ASTNode *decl = find_type_decl_by_name(program, type_name);
    if (decl_is_subject_host(decl))
        return decl;
    return NULL;
}

ASTNode *
find_callable_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt == NULL)
            continue;
        const char *stmt_name = ast_declaration_name(stmt);
        if (stmt->type == AST_FUNC_DECL
            && stmt_name != NULL
            && strcmp(stmt_name, name) == 0)
            return stmt;
        if (stmt->type == AST_EVENT_DECL
            && ast_event_name(stmt) != NULL
            && strcmp(ast_event_name(stmt), name) == 0)
            return stmt;
        if (stmt->type == AST_INTENT_DECL
            && ast_intent_decl_name(stmt) != NULL
            && strcmp(ast_intent_decl_name(stmt), name) == 0)
            return stmt;
    }

    return NULL;
}

bool
decl_is_projection_source(const ASTNode *decl)
{
    if (decl_is_subject_host(decl))
        return true;
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_OBJECT;
}

ClassField *
subject_host_field_at(ASTNode *decl, size_t index)
{
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        size_t field_count = 0;
        ClassField **fields = ast_class_fields(decl, &field_count);
        if (index < field_count && fields != NULL)
            return fields[index];
        return NULL;
    }
    return NULL;
}
