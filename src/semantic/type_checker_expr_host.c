#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

static Type *
expr_host_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved =
        semantic_type_resolution_lookup_annotation_or_unknown(ctx, type_ref);
    return resolved != NULL ? resolved : TYPE_UNKNOWN;
}

static Type *
expr_host_resolve_named_type_metadata_or_unknown(const char *name,
                                                 SemanticContext *ctx,
                                                 ASTNode *site)
{
    Type *resolved;

    if (name == NULL || name[0] == '\0')
        return TYPE_UNKNOWN;
    resolved = semantic_type_resolution_lookup_metadata_name_or_alias(ctx,
                                                                      name);
    if (resolved != NULL)
        return resolved;
    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
        PGY_CAUSE_TYPE_UNKNOWN, PGY_FIX_IMPORT_OR_DECLARE_TYPE, site,
        "Unknown type '%s'", name);
    return TYPE_UNKNOWN;
}

static Type *
expr_host_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
expr_host_resolve_class_field_type(ClassField *field, SemanticContext *ctx)
{
    if (field == NULL)
        return NULL;
    return expr_host_resolve_type_ref(field->type, ctx);
}

static Type *
expr_host_resolve_func_return_type(ASTNode *method, SemanticContext *ctx)
{
    if (method == NULL || method->type != AST_FUNC_DECL
        || method->data.func_decl.return_type == NULL) {
        return TYPE_VOID;
    }
    return expr_host_resolve_type_ref(method->data.func_decl.return_type, ctx);
}

static Type *
expr_host_resolve_func_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return expr_host_resolve_type_ref(param->type, ctx);
}

Type *
expr_current_host_field_type(SemanticContext *ctx, const char *field_name)
{
    ASTNode *decl = current_host_decl(ctx);
    if (decl == NULL || field_name == NULL)
        return NULL;

    if (decl->type == AST_CLASS_DECL) {
        size_t field_count = projection_source_field_count(decl);
        for (size_t i = 0; i < field_count; i++) {
            ClassField *field = projection_source_field_at(decl, i);
            if (field != NULL && field->name != NULL
                && strcmp(field->name, field_name) == 0) {
                return expr_host_resolve_class_field_type(field, ctx);
            }
        }
        return NULL;
    }

    if (decl->type == AST_WORLD_DECL) {
        for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
            ASTNode *slot = decl->data.world_decl.rosters[i];
            if (slot != NULL && slot->data.world_roster.slot_name != NULL
                && strcmp(slot->data.world_roster.slot_name, field_name) == 0) {
                return expr_host_resolve_named_type_metadata_or_unknown(
                    slot->data.world_roster.roster_type, ctx, slot);
            }
        }
        for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
            ASTNode *slot = decl->data.world_decl.zones[i];
            if (slot != NULL && slot->data.world_zone.slot_name != NULL
                && strcmp(slot->data.world_zone.slot_name, field_name) == 0) {
                return expr_host_resolve_named_type_metadata_or_unknown(
                    slot->data.world_zone.zone_type, ctx, slot);
            }
        }
    }

    for (size_t i = 0; i < overlay_field_count(decl); i++) {
        const char *name = NULL;
        ASTNode *type_node = overlay_field_decl_at(decl, i, &name);
        if (name != NULL && strcmp(name, field_name) == 0)
            return expr_host_resolve_type_ref(type_node, ctx);
    }

    return NULL;
}

ASTNode *
expr_current_host_method_decl(SemanticContext *ctx, const char *method_name)
{
    ASTNode *decl = current_host_decl(ctx);
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (decl == NULL || method_name == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        methods = decl->data.class_decl.methods;
        method_count = decl->data.class_decl.method_count;
        break;
    case AST_ENUM_DECL:
        methods = decl->data.enum_decl.methods;
        method_count = decl->data.enum_decl.method_count;
        break;
    case AST_RELATION_DECL:
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
        break;
    case AST_EFFECT_DECL:
        methods = decl->data.effect_decl.methods;
        method_count = decl->data.effect_decl.method_count;
        break;
    case AST_ZONE_DECL:
        methods = decl->data.zone_decl.methods;
        method_count = decl->data.zone_decl.method_count;
        break;
    case AST_WORLD_DECL:
        methods = decl->data.world_decl.methods;
        method_count = decl->data.world_decl.method_count;
        break;
    default:
        return NULL;
    }

    if (methods == NULL)
        return NULL;

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

Type *
expr_type_check_host_method_call(ASTNode *expr,
                                 ASTNode *method,
                                 SemanticContext *ctx)
{
    size_t implicit_self = 0;
    size_t expected;
    size_t provided;
    uint32_t declared_effects;

    if (expr == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return TYPE_UNKNOWN;

    declared_effects = declared_effects_from_function_node(method, ctx, NULL);
    semantic_record_effect(ctx, declared_effects);
    semantic_record_callable_decl_summary(ctx, method, declared_effects);
    if (ctx->in_parallel && type_effect_mask_has(declared_effects, EFFECT_SECURE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN,
            PGY_CAUSE_PARALLEL_SECURE_IN_TASK,
            PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit calling secure-effect method '%s'; serialize authority-bearing operations outside the parallel block",
            method->data.func_decl.name != NULL
                ? method->data.func_decl.name : "<method>");
        return expr_host_resolve_func_return_type(method, ctx);
    }

    if (method->data.func_decl.param_count > 0
        && method->data.func_decl.params[0] != NULL
        && method->data.func_decl.params[0]->name != NULL
        && strcmp(method->data.func_decl.params[0]->name, "self") == 0
        && method->data.func_decl.params[0]->type == NULL) {
        implicit_self = 1;
    }

    expected = method->data.func_decl.param_count - implicit_self;
    provided = expr->data.call.arg_count;
    if (provided != expected) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu",
            method->data.func_decl.name != NULL
                ? method->data.func_decl.name : "<method>",
            (unsigned long long) expected, (unsigned long long) provided);
        return expr_host_resolve_func_return_type(method, ctx);
    }

    for (size_t i = 0; i < provided; i++) {
        FuncParam *param = method->data.func_decl.params[i + implicit_self];
        Type *param_type = expr_host_resolve_func_param_type(param, ctx);
        Type *arg_type = expr_host_normalize_type(
            type_check_expression(expr->data.call.arguments[i], ctx));
        if (param_type != NULL
            && !type_is_assignable(arg_type, param_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_CALL_ARG_TYPE_MISMATCH, PGY_FIX_ALIGN_ARG_TYPE,
                expr->data.call.arguments[i],
                "Argument %llu for '%s' expects '%s', got '%s'",
                (unsigned long long) (i + 1),
                method->data.func_decl.name != NULL
                    ? method->data.func_decl.name : "<method>",
                type_name_or_unknown(param_type),
                type_name_or_unknown(arg_type));
        }
    }

    return expr_host_resolve_func_return_type(method, ctx);
}

bool
expr_type_is_nominal_host_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL
        || (type->kind != TYPE_KIND_CLASS && type->kind != TYPE_KIND_ENUM)
        || type->name == NULL || ctx == NULL)
        return false;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl != NULL && decl->type == AST_ENUM_DECL)
        return true;
    if (decl != NULL && decl->type == AST_CLASS_DECL)
        return !decl->data.class_decl.is_struct
            || decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL
            || decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
    return false;
}

bool
expr_member_is_static_access(const ASTNode *expr)
{
    if (expr == NULL || expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL) {
        return false;
    }

    if (expr->data.member.object->type == AST_IDENTIFIER) {
        const char *name = expr->data.member.object->data.identifier.name;
        return name != NULL && name[0] >= 'A' && name[0] <= 'Z';
    }

    return expr_member_is_static_access(expr->data.member.object);
}
