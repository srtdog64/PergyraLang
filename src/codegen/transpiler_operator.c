/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend operator-overload lookup helpers.
 */

#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_operator.h"

const char *
operator_overload_suffix(PgyTokenType op)
{
    switch (op) {
    case TOKEN_PLUS:          return "add";
    case TOKEN_MINUS:         return "sub";
    case TOKEN_STAR:          return "mul";
    case TOKEN_SLASH:         return "div";
    case TOKEN_PERCENT:       return "mod";
    case TOKEN_EQUAL:         return "eq";
    case TOKEN_NOT_EQUAL:     return "ne";
    case TOKEN_LESS:          return "lt";
    case TOKEN_LESS_EQUAL:    return "le";
    case TOKEN_GREATER:       return "gt";
    case TOKEN_GREATER_EQUAL: return "ge";
    default:                  return NULL;
    }
}

bool
operator_method_name_matches(PgyTokenType op, const char *name)
{
    static const struct {
        PgyTokenType op;
        const char *names[10];
    } aliases[] = {
        { TOKEN_PLUS, { "Add", "add", "OperatorAdd", "operator_add", NULL } },
        { TOKEN_MINUS, { "Sub", "sub", "Subtract", "subtract",
                         "OperatorSub", "operator_sub", NULL } },
        { TOKEN_STAR, { "Mul", "mul", "Multiply", "multiply",
                        "OperatorMul", "operator_mul", NULL } },
        { TOKEN_SLASH, { "Div", "div", "Divide", "divide",
                         "OperatorDiv", "operator_div", NULL } },
        { TOKEN_PERCENT, { "Mod", "mod", "Modulo", "modulo",
                           "OperatorMod", "operator_mod", NULL } },
        { TOKEN_EQUAL, { "Eq", "eq", "Equal", "equal", "Equals", "equals",
                         "OperatorEq", "operator_eq", NULL } },
        { TOKEN_NOT_EQUAL, { "Ne", "ne", "NotEqual", "notEqual",
                             "NotEquals", "notEquals",
                             "OperatorNe", "operator_ne", NULL } },
        { TOKEN_LESS, { "Lt", "lt", "LessThan", "lessThan",
                        "OperatorLt", "operator_lt", NULL } },
        { TOKEN_LESS_EQUAL, { "Le", "le", "LessEqual", "lessEqual",
                              "LessThanOrEqual", "lessThanOrEqual",
                              "OperatorLe", "operator_le", NULL } },
        { TOKEN_GREATER, { "Gt", "gt", "GreaterThan", "greaterThan",
                           "OperatorGt", "operator_gt", NULL } },
        { TOKEN_GREATER_EQUAL, { "Ge", "ge", "GreaterEqual", "greaterEqual",
                                 "GreaterThanOrEqual", "greaterThanOrEqual",
                                 "OperatorGe", "operator_ge", NULL } },
    };

    if (name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); i++) {
        if (aliases[i].op != op)
            continue;
        for (size_t j = 0; aliases[i].names[j] != NULL; j++) {
            if (strcmp(aliases[i].names[j], name) == 0)
                return true;
        }
        break;
    }
    return false;
}

ASTNode *
find_role_operator_method_decl(TranspilerCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *method_name = ast_declaration_name(method);
            if (method != NULL && method->type == AST_FUNC_DECL
                && method_name != NULL
                && operator_method_name_matches(op, method_name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *include_stmt = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(include_stmt);
        if (role_name == NULL)
            continue;
        ASTNode *included_role = find_role_decl(ctx, role_name);
        ASTNode *method = find_role_operator_method_decl(ctx, included_role,
            op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static const MIRDeclMethod *
find_role_operator_method_metadata_in_header(TranspilerCtx *ctx,
                                             const char *role_name,
                                             const MIRDeclHeader *role_header,
                                             PgyTokenType op,
                                             int depth)
{
    if (ctx == NULL || depth > 16)
        return NULL;
    if (role_header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing role operator method metadata for role '%s'",
            role_name != NULL ? role_name : "(anonymous-role)");
        return NULL;
    }

    for (size_t i = 0; i < mir_decl_header_method_count(role_header); i++) {
        const MIRDeclMethod *method =
            mir_decl_header_method(role_header, i);
        const char *method_name = transpiler_mir_decl_method_name(method);

        if (method_name != NULL
            && operator_method_name_matches(op, method_name)) {
            return method;
        }
    }

    for (size_t i = 0;
         i < mir_decl_header_role_include_count(role_header);
         i++) {
        const MIRDeclRoleInclude *include_meta =
            mir_decl_header_role_include(role_header, i);
        const char *included_name =
            mir_decl_role_include_name(include_meta);
        const MIRDeclHeader *included_header;
        const MIRDeclMethod *method;

        if (included_name == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing included role name metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return NULL;
        }
        included_header = transpiler_active_decl_header_of_type(
            ctx, AST_ROLE_DECL, included_name);
        if (included_header == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role include metadata header for included role '%s'",
                included_name);
            return NULL;
        }
        method = find_role_operator_method_metadata_in_header(
            ctx, included_name, included_header, op, depth + 1);
        if (method != NULL)
            return method;
        if (ctx != NULL && ctx->backend_error != NULL)
            return NULL;
    }

    return NULL;
}

const MIRDeclMethod *
find_role_operator_method_metadata(TranspilerCtx *ctx,
                                   ASTNode *role,
                                   PgyTokenType op,
                                   int depth)
{
    const char *role_name;
    TranspilerHostedMethodView view;

    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL
        || depth > 16) {
        return NULL;
    }

    role_name = transpiler_decl_name_local(role);
    view = transpiler_hosted_method_view_from_decl(ctx, role_name, role);
    if (transpiler_hosted_method_view_missing_mir_metadata(&view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing role operator method metadata for role '%s'",
            role_name != NULL ? role_name : "(anonymous-role)");
        return NULL;
    }
    return find_role_operator_method_metadata_in_header(
        ctx, role_name, view.decl_header, op, depth);
}

ASTNode *
find_operator_overload_decl(TranspilerCtx *ctx, const char *type_name, PgyTokenType op)
{
    const char *suffix = operator_overload_suffix(op);
    char fn_name[256];
    ASTNode **roles = NULL;
    size_t role_count = 0;

    if (ctx == NULL || type_name == NULL || suffix == NULL)
        return NULL;

    snprintf(fn_name, sizeof(fn_name), "operator_%s_%s", suffix, type_name);
    ASTNode *fn = find_function_decl(ctx, fn_name);
    if (fn != NULL)
        return fn;

    transpiler_active_inventory(ctx, AST_ROLE_DECL, &roles, &role_count);
    if (roles != NULL) {
        for (size_t i = 0; i < role_count; i++) {
            ASTNode *role = roles[i];
            const char *role_type =
                transpiler_role_subject_type_name_local(role);
            if (role_type == NULL || strcmp(role_type, type_name) != 0) {
                continue;
            }
            if (transpiler_active_has_mir(ctx)) {
                if (find_role_operator_method_metadata(ctx, role, op, 0) != NULL)
                    return role;
            } else if (find_role_operator_method_decl(ctx, role, op, 0) != NULL) {
                return role;
            }
        }
    }

    return NULL;
}
