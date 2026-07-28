#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

static Type *
expr_host_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
expr_host_resolve_class_field_type(ASTNode *field_type, SemanticContext *ctx)
{
    if (field_type == NULL)
        return NULL;
    return semantic_host_resolve_type_ref(field_type, ctx);
}

static Type *
expr_host_resolve_func_return_type(ASTNode *method, SemanticContext *ctx)
{
    if (method == NULL || method->type != AST_FUNC_DECL
        || ast_func_return_type(method) == NULL) {
        return TYPE_VOID;
    }
    return type_check_func_resolve_return_type(method, ctx);
}

static Type *
expr_host_resolve_func_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return type_check_func_resolve_param_type(param, ctx);
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
            PgyDeclField field = projection_source_field_at(decl, i);
            if (field.name != NULL
                && strcmp(field.name, field_name) == 0) {
                return expr_host_resolve_class_field_type(field.type_ast, ctx);
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
                return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                    ctx, ast_world_roster_type_name(slot), slot);
            }
        }
        for (size_t i = 0; i < zone_count; i++) {
            ASTNode *slot = zones[i];
            const char *slot_name = ast_world_zone_slot_name(slot);
            if (slot != NULL && slot_name != NULL
                && strcmp(slot_name, field_name) == 0) {
                return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                    ctx, ast_world_zone_type_name(slot), slot);
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
                return semantic_type_resolution_lookup_metadata_name_or_alias_or_unknown(
                    ctx, ast_zone_layer_slot_layer_type(slot), slot);
            }
        }
    }

    for (size_t i = 0; i < overlay_field_count(decl); i++) {
        const char *name = NULL;
        ASTNode *type_node = overlay_field_decl_at(decl, i, &name);
        if (name != NULL && strcmp(name, field_name) == 0)
            return semantic_host_resolve_type_ref(type_node, ctx);
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
        const char *candidate_name = ast_declaration_name(method);
        if (method != NULL && method->type == AST_FUNC_DECL
            && candidate_name != NULL
            && strcmp(candidate_name, method_name) == 0) {
            return method;
        }
    }

    return NULL;
}

Type *
expr_host_method_function_type(SemanticContext *ctx,
                               ASTNode *host_decl,
                               const char *method_name)
{
    const char *host_name;
    char *mangled;
    Symbol *sym;

    if (ctx == NULL || host_decl == NULL || method_name == NULL)
        return NULL;

    host_name = ast_declaration_name(host_decl);
    if (host_name == NULL)
        return NULL;

    mangled = tc_strdup_fmt("%s_%s", host_name, method_name);
    if (mangled == NULL)
        return NULL;

    sym = scope_lookup(ctx->scope, mangled);
    free(mangled);
    if (sym == NULL || sym->kind != SYMBOL_FUNCTION)
        return NULL;
    if (sym->type == NULL || sym->type->kind != TYPE_KIND_FUNCTION)
        return NULL;
    return sym->type;
}

static bool
expr_host_type_contains_generic(const Type *type)
{
    if (type == NULL)
        return false;
    if (type->kind == TYPE_KIND_GENERIC)
        return true;
    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        size_t n = type_constructed_arg_count(type);
        for (size_t i = 0; i < n; i++)
            if (expr_host_type_contains_generic(
                    type_constructed_arg(type, i)))
                return true;
    }
    return false;
}

Type *
expr_type_check_host_method_call_on_host(ASTNode *expr,
                                         ASTNode *host_decl,
                                         ASTNode *method,
                                         const Type *receiver_type,
                                         SemanticContext *ctx)
{
    size_t implicit_self = 0;
    size_t param_count;
    size_t expected;
    size_t provided;
    FuncParam *first_param;
    uint32_t declared_effects;
    uint32_t method_effects;
    Type *method_func_type;
    const char *method_name;
    const char *method_display;
    char method_display_buf[256];
    ExprHostMethodGenericBindings generic_bindings;
    Type **param_types = NULL;
    Type **arg_types = NULL;

    if (expr == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return TYPE_UNKNOWN;

    if (ast_node_stable_id(method) == 0
        || !ast_call_set_semantic_callee_decl_id(
            expr, ast_node_stable_id(method))) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CALL_NOT_CALLABLE,
            PGY_FIX_USE_CALLABLE_DECLARATION,
            expr,
            "Resolved host method/action call has no stable declaration identity");
        return TYPE_UNKNOWN;
    }

    method_name = ast_declaration_name(method);
    method_display = method_name != NULL ? method_name : "<method>";
    if (host_decl != NULL && host_decl != current_host_decl(ctx)) {
        const char *host_name = ast_declaration_name(host_decl);
        if (host_name != NULL && method_name != NULL) {
            snprintf(method_display_buf, sizeof(method_display_buf),
                     "%s.%s", host_name, method_name);
            method_display = method_display_buf;
        }
    }
    method_func_type =
        expr_host_method_function_type(ctx, host_decl, method_name);
    declared_effects = declared_effects_from_function_node(method, ctx, NULL);
    method_effects = method_func_type != NULL
        ? type_function_effects(method_func_type)
        : declared_effects;
    semantic_record_effect(ctx, method_effects);
    semantic_record_callee_body_summary(ctx, method_func_type);
    semantic_record_callable_decl_summary(ctx, method, method_func_type,
        method_effects);
    if (ctx->in_parallel && type_effect_mask_has(method_effects, EFFECT_SECURE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN,
            PGY_CAUSE_PARALLEL_SECURE_IN_TASK,
            PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit calling secure-effect method '%s'; serialize authority-bearing operations outside the parallel block",
            method_display);
        return expr_host_resolve_func_return_type(method, ctx);
    }

    param_count = ast_func_param_count(method);
    first_param = ast_func_param(method, 0);

    if (param_count > 0
        && first_param != NULL
        && first_param->name != NULL
        && strcmp(first_param->name, "self") == 0
        && first_param->type == NULL) {
        implicit_self = 1;
    }

    expected = param_count - implicit_self;
    provided = ast_call_arg_count(expr);
    if (provided != expected) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu",
            ast_declaration_name(method) != NULL
                ? ast_declaration_name(method) : "<method>",
            (unsigned long long) expected, (unsigned long long) provided);
        return TYPE_UNKNOWN;
    }

    if (!expr_host_method_generic_bindings_init(
            &generic_bindings, expr, method, ctx, method_display)) {
        expr_host_method_generic_bindings_destroy(&generic_bindings);
        return TYPE_UNKNOWN;
    }
    param_types = calloc(provided > 0 ? provided : 1, sizeof(Type *));
    arg_types = calloc(provided > 0 ? provided : 1, sizeof(Type *));
    if (param_types == NULL || arg_types == NULL) {
        free(param_types);
        free(arg_types);
        expr_host_method_generic_bindings_destroy(&generic_bindings);
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNKNOWN_TYPE,
            PGY_CAUSE_RESOLUTION_OOM, PGY_FIX_REDUCE_SCOPE_OR_RETRY, expr,
            "Could not allocate method-call type metadata for '%s'",
            method_display);
        return TYPE_UNKNOWN;
    }
    for (size_t i = 0; i < provided; i++) {
        FuncParam *param = ast_func_param(method, i + implicit_self);
        ASTNode *arg = ast_call_argument(expr, i);
        param_types[i] = expr_host_resolve_func_param_type(param, ctx);
        arg_types[i] = expr_host_normalize_type(
            type_check_expression(arg, ctx));
        expr_host_method_generic_infer_argument(
            &generic_bindings, param_types[i], arg_types[i],
            host_decl, receiver_type);
    }
    if (!expr_host_method_generic_bindings_finalize(
            &generic_bindings, expr, method, host_decl, receiver_type,
            ctx, method_display)) {
        free(param_types);
        free(arg_types);
        expr_host_method_generic_bindings_destroy(&generic_bindings);
        return TYPE_UNKNOWN;
    }
    for (size_t i = 0; i < provided; i++) {
        ASTNode *arg = ast_call_argument(expr, i);
        Type *param_type = expr_host_method_generic_substitute(
            param_types[i], host_decl, receiver_type, &generic_bindings);
        if (param_type != NULL
            && !expr_host_type_contains_generic(param_type)
            && !type_is_assignable(arg_types[i], param_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_CALL_ARG_TYPE_MISMATCH, PGY_FIX_ALIGN_ARG_TYPE,
                arg,
                "Argument %llu for '%s' expects '%s', got '%s'",
                (unsigned long long) (i + 1),
                ast_declaration_name(method) != NULL
                    ? ast_declaration_name(method) : "<method>",
                type_name_or_unknown(param_type),
                type_name_or_unknown(arg_types[i]));
        }
    }

    {
        Type *ret = expr_host_resolve_func_return_type(method, ctx);
        Type *resolved_ret;
        expr_host_method_generic_validate_constraints(
            &generic_bindings, expr, method, ctx, method_display);
        resolved_ret = expr_host_method_generic_substitute(
            ret, host_decl, receiver_type, &generic_bindings);
        free(param_types);
        free(arg_types);
        expr_host_method_generic_bindings_destroy(&generic_bindings);
        return resolved_ret;
    }
}

Type *
expr_type_check_host_method_call(ASTNode *expr,
                                 ASTNode *method,
                                 SemanticContext *ctx)
{
    return expr_type_check_host_method_call_on_host(
        expr, current_host_decl(ctx), method, NULL, ctx);
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
        || ast_member_object(expr) == NULL) {
        return false;
    }

    if (ast_member_object(expr)->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(ast_member_object(expr));
        return name != NULL && name[0] >= 'A' && name[0] <= 'Z';
    }

    return expr_member_is_static_access(ast_member_object(expr));
}
