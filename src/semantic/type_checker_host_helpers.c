#include <string.h>

#include "type_checker_internal.h"
#include "diag_codes.h"

static Type *
host_helper_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
}

Type *
semantic_host_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;

    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;
    resolved = host_helper_resolve_type_ref(type_ref, ctx);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
host_helper_resolve_func_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return host_helper_resolve_type_ref(param->type, ctx);
}

ASTNode *
current_host_decl(SemanticContext *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_nominal_decl != NULL)
        return ctx->current_nominal_decl;
    if (ctx->current_relation != NULL)
        return ctx->current_relation;
    if (ctx->current_effect != NULL)
        return ctx->current_effect;
    if (ctx->current_party != NULL)
        return ctx->current_party;
    if (ctx->current_roster != NULL)
        return ctx->current_roster;
    if (ctx->current_zone != NULL)
        return ctx->current_zone;
    if (ctx->current_world != NULL)
        return ctx->current_world;
    return NULL;
}

ASTNode *
semantic_host_decl_for_type(SemanticContext *ctx, const Type *type)
{
    ASTNode *decl;
    const char *lookup_name;
    /* For generic-instantiated types like `Box<Int>`, the type->name is the
     * instantiated form ("Box<Int>") which never matches a declared host name.
     * Fall back to the base constructor's name ("Box") so that
     * `Box(7).Get()` and similar method calls find the original class decl. */
    const Type *constructor;

    if (ctx == NULL || type == NULL || type->name == NULL)
        return NULL;
    if (type->kind == TYPE_KIND_ENUM)
        return semantic_find_enum_decl_by_name(ctx, type->name);
    if (type->kind != TYPE_KIND_CLASS
        && type->kind != TYPE_KIND_CONSTRUCTED)
        return NULL;

    lookup_name = type->name;
    constructor = type_constructed_constructor(type);
    if (constructor != NULL && constructor != type
        && constructor->name != NULL
        && constructor->name[0] != '\0') {
        decl = semantic_find_class_decl_by_name(ctx, type->name);
        if (decl != NULL)
            return decl;
        lookup_name = constructor->name;
    }
    decl = semantic_find_class_decl_by_name(ctx, lookup_name);
    if (decl != NULL)
        return decl;
    decl = semantic_find_party_decl_by_name(ctx, lookup_name);
    if (decl != NULL)
        return decl;
    decl = semantic_find_roster_decl_by_name(ctx, lookup_name);
    if (decl != NULL)
        return decl;
    decl = semantic_find_world_decl_by_name(ctx, lookup_name);
    if (decl != NULL)
        return decl;
    decl = semantic_find_zone_decl_by_name(ctx, lookup_name);
    if (decl != NULL)
        return decl;
    decl = semantic_find_relation_decl_by_name(ctx, lookup_name);
    if (decl != NULL)
        return decl;
    return semantic_find_effect_decl_by_name(ctx, lookup_name);
}

ASTNode **
semantic_host_decl_methods(ASTNode *decl, size_t *method_count)
{
    if (method_count != NULL)
        *method_count = 0;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return ast_class_methods(decl, method_count);
    case AST_ENUM_DECL:
        return ast_enum_methods(decl, method_count);
    case AST_PARTY_DECL:
        return ast_party_methods(decl, method_count);
    case AST_ROSTER_DECL:
        return ast_roster_methods(decl, method_count);
    case AST_ZONE_DECL:
        return ast_zone_methods(decl, method_count);
    case AST_WORLD_DECL:
        return ast_world_methods(decl, method_count);
    case AST_RELATION_DECL:
        return ast_relation_methods(decl, method_count);
    case AST_EFFECT_DECL:
        return ast_effect_methods(decl, method_count);
    default:
        return NULL;
    }
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

bool
type_is_subject_host_slot_handle(const Type *type, SemanticContext *ctx)
{
    if (!type_is_owned_slot_handle(type) || ctx == NULL)
        return false;
    return type_is_subject_type(type_slot_inner_type(type), ctx);
}

bool
type_is_class_object_type(const Type *type, SemanticContext *ctx)
{
    return type_is_subject_type(type, ctx);
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL || type->kind != TYPE_KIND_CLASS
        || type->name == NULL || ctx == NULL)
        return false;

    if (type->nominal_flavor == TYPE_NOMINAL_SUBJECT)
        return true;

    decl = semantic_host_decl_for_type(ctx, type);
    return decl_is_subject_host(decl);
}

const char *
find_action_binding_type_name(ASTNode *func, ASTNode *enclosing_nominal,
                              SemanticContext *ctx, const char *binding_name)
{
    if (func == NULL || func->type != AST_FUNC_DECL || ctx == NULL
        || binding_name == NULL) {
        return NULL;
    }

    if (strcmp(binding_name, "self") == 0) {
        return enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && ast_class_name(enclosing_nominal) != NULL
                ? ast_class_name(enclosing_nominal)
                : NULL;
    }

    for (size_t i = 0; i < ast_func_param_count(func); i++) {
        FuncParam *param = ast_func_param(func, i);
        Type *param_type;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, binding_name) != 0) {
            continue;
        }
        if (param->type == NULL)
            return NULL;
        param_type = host_helper_resolve_func_param_type(param, ctx);
        if (param_type == NULL || !type_is_subject_type(param_type, ctx))
            return NULL;
        return param_type->name;
    }

    return NULL;
}
