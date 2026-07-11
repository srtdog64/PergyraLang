#include "type_checker_internal.h"
#include "diag_codes.h"
#include "../common/worker_boundary_storage_policy.h"

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
        || type_is_constructed_named(type, "DeviceSlot")
        || type_is_builtin_owner_handle(type);
}

bool
type_is_anchored_resource_handle(const Type *type)
{
    return type_is_owned_slot_handle(type)
        || type_is_constructed_named(type, "DeviceSlot");
}

bool
type_is_builtin_owner_handle(const Type *type)
{
    return type != NULL && TYPE_TEXT_BUILDER != NULL
        && type_equals(type, TYPE_TEXT_BUILDER);
}

bool
type_is_movable_resource_handle(const Type *type)
{
    return type_is_qubit(type) || type_is_builtin_owner_handle(type);
}

bool
semantic_require_no_live_text_builder(Scope *scope, ASTNode *site,
                                      SemanticContext *ctx,
                                      const char *boundary)
{
    if (scope == NULL || ctx == NULL)
        return true;
    for (Scope *current = scope; current != NULL; current = current->parent) {
        for (size_t i = 0; i < current->symbol_count; i++) {
            Symbol *symbol = current->symbols[i];
            if (symbol == NULL || symbol->is_consumed
                || !type_is_builtin_owner_handle(symbol->type))
                continue;
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_OWNER_NOT_CONSUMED,
                PGY_CAUSE_OWNER_NOT_CONSUMED,
                PGY_FIX_CONSUME_OWNER_BEFORE_EXIT,
                site,
                "TextBuilder owner '%s' is still live at %s.\n"
                "Reason:\n"
                "- the bounded TextBuilder rung requires exactly one Finish or Drop in its declaration scope\n"
                "- implicit cleanup is not yet a MIR-owned fact\n"
                "Fix:\n"
                "- call TextBuilderFinish(%s, resultAllocator) before %s\n"
                "- or call TextBuilderDrop(%s) before %s",
                symbol->name != NULL ? symbol->name : "<builder>",
                boundary != NULL ? boundary : "scope exit",
                symbol->name != NULL ? symbol->name : "builder",
                boundary != NULL ? boundary : "scope exit",
                symbol->name != NULL ? symbol->name : "builder",
                boundary != NULL ? boundary : "scope exit");
            return false;
        }
        if (current->kind == SCOPE_FUNCTION)
            break;
    }
    return true;
}

static bool
type_is_borrowed_slice_view(const Type *type)
{
    return type_is_constructed_named(type, "Slice");
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

static bool
type_is_value_nominal_boundary_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;
    TypeNominalFlavor flavor;

    if (type == NULL || ctx == NULL)
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

    decl = semantic_host_decl_for_type(ctx, type);
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
    if (type == NULL)
        return false;
    if (type_is_borrowed_slice_view(type))
        return true;
    if (!type_is_general_boundary_type(type, ctx))
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
worker_boundary_storage_display_name(const Type *type)
{
    Type *constructor = type_constructed_constructor(type);
    PgyWorkerBoundaryStorageKind kind;

    kind = pgy_worker_boundary_storage_kind_from_constructor_name(
        constructor != NULL ? constructor->name : NULL, false, false);
    return pgy_worker_boundary_storage_kind_name(kind);
}

bool
type_is_worker_boundary_unsafe_storage(const Type *type)
{
    return worker_boundary_storage_display_name(type) != NULL;
}

const char *
detached_worker_boundary_storage_display_name(const Type *type)
{
    Type *constructor = type_constructed_constructor(type);
    PgyWorkerBoundaryStorageKind kind;

    kind = pgy_worker_boundary_storage_kind_from_constructor_name(
        constructor != NULL ? constructor->name : NULL, true, false);
    return pgy_worker_boundary_storage_kind_name(kind);
}

bool
type_is_detached_worker_boundary_unsafe_storage(const Type *type)
{
    return detached_worker_boundary_storage_display_name(type) != NULL;
}

const char *
resource_handle_display_name(const Type *type)
{
    if (type == NULL)
        return "resource";
    if (type_is_qubit(type))
        return "QubitSlot";
    if (type_is_builtin_owner_handle(type))
        return "TextBuilder";
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

bool
decl_is_projection_source(const ASTNode *decl)
{
    if (decl_is_subject_host(decl))
        return true;
    return decl != NULL
        && decl->type == AST_CLASS_DECL
        && ast_class_nominal_kind(decl) == NOMINAL_DECL_OBJECT;
}

PgyDeclField
subject_host_field_at(ASTNode *decl, size_t index)
{
    PgyDeclField empty = {0};
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return empty;
    PgyDeclField *fields = NULL;
    size_t field_count = pgy_class_decl_field_model_build(decl, &fields);
    PgyDeclField result = (index < field_count) ? fields[index] : empty;
    pgy_decl_field_model_free(fields, field_count);
    return result;
}
