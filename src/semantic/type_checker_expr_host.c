#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

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
    return domain_resolve_type_ref(field->type, ctx);
}

static Type *
expr_host_resolve_func_return_type(ASTNode *method, SemanticContext *ctx)
{
    if (method == NULL || method->type != AST_FUNC_DECL
        || method->data.func_decl.return_type == NULL) {
        return TYPE_VOID;
    }
    return domain_resolve_type_ref(method->data.func_decl.return_type, ctx);
}

static Type *
expr_host_resolve_func_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return domain_resolve_type_ref(param->type, ctx);
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
        ASTNode **rosters;
        ASTNode **zones;
        size_t roster_count;
        size_t zone_count;

        rosters = ast_world_rosters(decl, &roster_count);
        zones = ast_world_zones(decl, &zone_count);
        for (size_t i = 0; i < roster_count; i++) {
            ASTNode *slot = rosters[i];
            const char *slot_name = ast_world_roster_slot_name(slot);
            if (slot != NULL && slot_name != NULL
                && strcmp(slot_name, field_name) == 0) {
                return expr_host_resolve_named_type_metadata_or_unknown(
                    ast_world_roster_type_name(slot), ctx, slot);
            }
        }
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *slot = zones[i];
            const char *slot_name = ast_world_zone_slot_name(slot);
            if (slot != NULL && slot_name != NULL
                && strcmp(slot_name, field_name) == 0) {
                return expr_host_resolve_named_type_metadata_or_unknown(
                    ast_world_zone_type_name(slot), ctx, slot);
            }
        }
    }
    if (decl->type == AST_ZONE_DECL) {
        ASTNode **layer_slots;
        size_t layer_slot_count;

        layer_slots = ast_zone_layer_slots(decl, &layer_slot_count);
        for (size_t i = 0; i < layer_slot_count; i++) {
            ASTNode *slot = layer_slots[i];
            if (slot != NULL && ast_zone_layer_slot_name(slot) != NULL
                && strcmp(ast_zone_layer_slot_name(slot),
                          field_name) == 0) {
                return expr_host_resolve_named_type_metadata_or_unknown(
                    ast_zone_layer_slot_layer_type(slot), ctx, slot);
            }
        }
    }

    for (size_t i = 0; i < overlay_field_count(decl); i++) {
        const char *name = NULL;
        ASTNode *type_node = overlay_field_decl_at(decl, i, &name);
        if (name != NULL && strcmp(name, field_name) == 0)
            return domain_resolve_type_ref(type_node, ctx);
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

    methods = semantic_host_decl_methods(decl, &method_count);
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
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);

    if (decl == NULL)
        return false;
    if (decl->type == AST_ENUM_DECL)
        return true;
    if (decl->type == AST_CLASS_DECL)
        return !ast_class_is_struct(decl)
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_VESSEL
            || ast_class_nominal_kind(decl) == NOMINAL_DECL_OBJECT;
    return decl->type == AST_PARTY_DECL
        || decl->type == AST_ROSTER_DECL
        || decl->type == AST_WORLD_DECL
        || decl->type == AST_ZONE_DECL
        || decl->type == AST_RELATION_DECL
        || decl->type == AST_EFFECT_DECL;
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
