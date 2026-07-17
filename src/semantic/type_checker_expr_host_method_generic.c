#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_generic_diag_internal.h"
#include "type_checker_internal.h"

#include <stdio.h>

static void
expr_host_method_generic_error(SemanticContext *ctx,
                               ASTNode *site,
                               const char *method_name,
                               const char *detail)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC,
        PGY_CAUSE_GENERIC_ARGS_INVALID, PGY_FIX_ALIGN_GENERIC_ARG_LIST,
        site,
        "Method '%s' could not materialize its generic arguments.\n"
        "Reason:\n"
        "- %s\n"
        "Fix:\n"
        "- provide type arguments that match the method declaration\n"
        "- or pass values whose types determine every generic parameter",
        method_name != NULL ? method_name : "<method>",
        detail != NULL ? detail : "generic argument evidence is incomplete");
}

static Type *
expr_host_method_class_generic(const Type *type,
                               ASTNode *host_decl,
                               const Type *receiver_type,
                               GenericParams *method_params)
{
    GenericParams *host_params;
    size_t host_count;
    size_t receiver_count;

    if (type == NULL || type->kind != TYPE_KIND_GENERIC
        || type->name == NULL || host_decl == NULL
        || host_decl->type != AST_CLASS_DECL || receiver_type == NULL
        || receiver_type->kind != TYPE_KIND_CONSTRUCTED) {
        return NULL;
    }
    if (find_generic_param_index(method_params, type->name) >= 0)
        return NULL;

    host_params = ast_class_generic_params(host_decl);
    host_count = ast_generic_param_count(host_params);
    receiver_count = type_constructed_arg_count(receiver_type);
    for (size_t i = 0; i < host_count && i < receiver_count; i++) {
        GenericParam *param = ast_generic_param_at(host_params, i);
        const char *name = ast_generic_param_name(param);
        if (name != NULL && strcmp(name, type->name) == 0)
            return type_constructed_arg(receiver_type, i);
    }
    return NULL;
}

Type *
expr_host_method_generic_substitute(
    Type *type,
    ASTNode *host_decl,
    const Type *receiver_type,
    const ExprHostMethodGenericBindings *bindings)
{
    GenericParams *method_params = bindings != NULL ? bindings->params : NULL;

    if (type == NULL)
        return NULL;

    if (type->kind == TYPE_KIND_GENERIC && type->name != NULL) {
        int method_index = find_generic_param_index(method_params, type->name);
        Type *host_binding;

        if (method_index >= 0 && bindings != NULL
            && (size_t)method_index < bindings->count
            && bindings->types[method_index] != NULL
            && bindings->types[method_index] != TYPE_UNKNOWN) {
            return bindings->types[method_index];
        }
        host_binding = expr_host_method_class_generic(
            type, host_decl, receiver_type, method_params);
        return host_binding != NULL ? host_binding : type;
    }

    if (type->kind == TYPE_KIND_CONSTRUCTED) {
        size_t count = type_constructed_arg_count(type);
        Type **args = count > 0 ? calloc(count, sizeof(Type *)) : NULL;
        bool changed = false;
        Type *result;

        if (count > 0 && args == NULL)
            return type;
        for (size_t i = 0; i < count; i++) {
            Type *original = type_constructed_arg(type, i);
            args[i] = expr_host_method_generic_substitute(
                original, host_decl, receiver_type, bindings);
            if (args[i] != original)
                changed = true;
        }
        if (!changed) {
            free(args);
            return type;
        }
        result = type_create_constructed(
            type_constructed_constructor(type), args, count);
        free(args);
        return result != NULL ? result : type;
    }

    return type;
}

bool
expr_host_method_generic_bindings_init(
    ExprHostMethodGenericBindings *bindings,
    ASTNode *call,
    ASTNode *method,
    SemanticContext *ctx,
    const char *method_name)
{
    size_t provided;

    if (bindings == NULL || method == NULL || method->type != AST_FUNC_DECL)
        return false;
    memset(bindings, 0, sizeof(*bindings));
    bindings->params = ast_func_generic_params(method);
    bindings->count = ast_generic_param_count(bindings->params);
    bindings->valid = true;
    provided = ast_call_generic_arg_count(call);

    if (bindings->count == 0) {
        if (provided > 0) {
            expr_host_method_generic_error(
                ctx, call, method_name,
                "the method declares no generic parameters, but the call supplies type arguments");
            bindings->valid = false;
        }
        return bindings->valid;
    }
    if (provided > bindings->count) {
        expr_host_method_generic_error(
            ctx, call, method_name,
            "the call supplies more type arguments than the method declares");
        bindings->valid = false;
        return false;
    }

    bindings->types = calloc(bindings->count, sizeof(Type *));
    if (bindings->types == NULL) {
        expr_host_method_generic_error(
            ctx, call, method_name,
            "semantic analysis could not allocate the method-generic binding row");
        bindings->valid = false;
        return false;
    }

    for (size_t i = 0; i < provided; i++) {
        GenericParam *actual = ast_call_generic_arg(call, i);
        ASTNode *type_ref = ast_generic_param_constraint(actual);
        Type *resolved;

        if (type_ref == NULL) {
            expr_host_method_generic_error(
                ctx, call, method_name,
                "a supplied method type argument has no type reference");
            bindings->valid = false;
            continue;
        }
        semantic_type_resolution_record_type_ref_dependency(
            ctx, call, method_name, type_ref,
            "provided method generic argument lookup");
        resolved = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, type_ref);
        if (resolved == NULL || resolved == TYPE_UNKNOWN
            || type_equals(resolved, TYPE_VOID)) {
            expr_host_method_generic_error(
                ctx, type_ref, method_name,
                "a supplied method type argument did not resolve to a value type");
            bindings->valid = false;
            continue;
        }
        bindings->types[i] = resolved;
    }
    return bindings->valid;
}

static void
expr_host_method_generic_infer_type(
    ExprHostMethodGenericBindings *bindings,
    Type *formal,
    Type *actual)
{
    int index;

    if (bindings == NULL || formal == NULL || actual == NULL
        || actual == TYPE_UNKNOWN) {
        return;
    }
    if (formal->kind == TYPE_KIND_GENERIC && formal->name != NULL) {
        index = find_generic_param_index(bindings->params, formal->name);
        if (index >= 0 && (size_t)index < bindings->count
            && (bindings->types[index] == NULL
                || bindings->types[index] == TYPE_UNKNOWN)) {
            bindings->types[index] = actual;
        }
        return;
    }
    if (formal->kind != TYPE_KIND_CONSTRUCTED
        || actual->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(type_constructed_constructor(formal),
                        type_constructed_constructor(actual))
        || type_constructed_arg_count(formal)
            != type_constructed_arg_count(actual)) {
        return;
    }
    for (size_t i = 0; i < type_constructed_arg_count(formal); i++) {
        expr_host_method_generic_infer_type(
            bindings,
            type_constructed_arg(formal, i),
            type_constructed_arg(actual, i));
    }
}

void
expr_host_method_generic_infer_argument(
    ExprHostMethodGenericBindings *bindings,
    Type *formal,
    Type *actual,
    ASTNode *host_decl,
    const Type *receiver_type)
{
    Type *host_substituted;

    if (bindings == NULL || !bindings->valid)
        return;
    host_substituted = expr_host_method_generic_substitute(
        formal, host_decl, receiver_type, bindings);
    expr_host_method_generic_infer_type(
        bindings, host_substituted, actual);
}

bool
expr_host_method_generic_bindings_finalize(
    ExprHostMethodGenericBindings *bindings,
    ASTNode *call,
    ASTNode *method,
    ASTNode *host_decl,
    const Type *receiver_type,
    SemanticContext *ctx,
    const char *method_name)
{
    if (bindings == NULL || !bindings->valid)
        return false;

    for (size_t i = 0; i < bindings->count; i++) {
        GenericParam *param;
        ASTNode *default_type;
        Type *resolved;

        if (bindings->types[i] != NULL
            && bindings->types[i] != TYPE_UNKNOWN) {
            continue;
        }
        param = ast_generic_param_at(bindings->params, i);
        default_type = ast_generic_param_default_type(param);
        if (default_type == NULL)
            continue;
        semantic_type_resolution_record_type_ref_dependency(
            ctx, call, method_name, default_type,
            "omitted method generic default lookup");
        resolved = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, default_type);
        if (resolved != NULL && resolved != TYPE_UNKNOWN) {
            bindings->types[i] = expr_host_method_generic_substitute(
                resolved, host_decl, receiver_type, bindings);
        }
    }

    for (size_t i = 0; i < bindings->count; i++) {
        GenericParam *param = ast_generic_param_at(bindings->params, i);
        const char *param_name = ast_generic_param_name(param);
        if (bindings->types[i] == NULL
            || bindings->types[i] == TYPE_UNKNOWN) {
            char detail[192];
            snprintf(detail, sizeof(detail),
                "generic parameter '%s' has no explicit argument, inferable argument, or usable default",
                param_name != NULL ? param_name : "<type-param>");
            expr_host_method_generic_error(
                ctx, call, method_name, detail);
            bindings->valid = false;
        }
    }
    return bindings->valid;
}

static void
expr_host_method_generic_validate_bound(
    const ExprHostMethodGenericBindings *bindings,
    size_t index,
    ASTNode *bound,
    const char *bounds_text,
    ASTNode *call,
    SemanticContext *ctx,
    const char *method_name,
    const char *expected_signature)
{
    GenericParam *param;
    const char *param_name;
    const char *bound_name;
    const char *actual_signature;
    Type *concrete;

    if (bindings == NULL || index >= bindings->count || bound == NULL)
        return;
    concrete = bindings->types[index];
    if (concrete == NULL || concrete == TYPE_UNKNOWN)
        return;
    if (concrete_type_satisfies_bound(concrete, bound, ctx))
        return;

    param = ast_generic_param_at(bindings->params, index);
    param_name = ast_generic_param_name(param);
    bound_name = ast_type_name(bound);
    actual_signature = format_effective_generic_type_list_scratch(
        ctx, method_name, bindings->types, bindings->count);
    semantic_report_function_generic_bound_failure(
        ctx, call, method_name, param_name,
        bound_name != NULL ? bound_name : "<constraint>",
        bounds_text,
        expected_signature,
        actual_signature,
        concrete->name != NULL ? concrete->name : "<type>");
}

void
expr_host_method_generic_validate_constraints(
    const ExprHostMethodGenericBindings *bindings,
    ASTNode *call,
    ASTNode *method,
    SemanticContext *ctx,
    const char *method_name)
{
    WhereClause *where;
    char *expected_signature;

    if (bindings == NULL || !bindings->valid || bindings->count == 0)
        return;
    expected_signature = format_generic_subject_signature(
        method_name, bindings->params);

    for (size_t i = 0; i < bindings->count; i++) {
        GenericParam *param = ast_generic_param_at(bindings->params, i);
        ASTNode *constraint = ast_generic_param_constraint(param);
        const char *bound_name;
        if (constraint == NULL)
            continue;
        bound_name = ast_type_name(constraint);
        expr_host_method_generic_validate_bound(
            bindings, i, constraint,
            bound_name != NULL ? bound_name : "<constraint>",
            call, ctx, method_name,
            expected_signature != NULL ? expected_signature : method_name);
    }

    where = ast_func_where_clause(method);
    if (where != NULL) {
        for (size_t i = 0; i < where->count; i++) {
            TypeConstraint *constraint = where->constraints[i];
            int param_index;
            char *bounds_text;
            if (constraint == NULL || constraint->type_param == NULL)
                continue;
            param_index = find_generic_param_index(
                bindings->params, constraint->type_param);
            if (param_index < 0) {
                expr_host_method_generic_error(
                    ctx, call, method_name,
                    "the method where-clause references an undeclared generic parameter");
                continue;
            }
            bounds_text = format_type_constraint_bounds(constraint);
            for (size_t bi = 0; bi < constraint->bound_count; bi++) {
                expr_host_method_generic_validate_bound(
                    bindings, (size_t)param_index, constraint->bounds[bi],
                    bounds_text,
                    call, ctx, method_name,
                    expected_signature != NULL
                        ? expected_signature : method_name);
            }
            free(bounds_text);
        }
    }
    free(expected_signature);
}

void
expr_host_method_generic_bindings_destroy(
    ExprHostMethodGenericBindings *bindings)
{
    if (bindings == NULL)
        return;
    free(bindings->types);
    memset(bindings, 0, sizeof(*bindings));
}
