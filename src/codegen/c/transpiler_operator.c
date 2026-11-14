/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Internal C backend operator-overload lookup helpers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/mir/mir_decl_headers.h"
#include "../../semantic/operator_overload_rules.h"
#include "../../semantic/diag_codes.h"
#include "transpiler_call_subject_arg_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_signature.h"
#include "transpiler_operator.h"

/* Match the free function's ABI before the binary owner sequences operands.
 * Role bridges keep their own value-receiver ABI. The input texts still name
 * original storage (or a constructor compound literal), never copied locals. */
bool
transpiler_operator_prepare_arguments(TranspilerCtx *ctx,
                                      ASTNode *expr, ASTNode *decl,
                                      char **left, char **right)
{
    if (decl == NULL || decl->type != AST_FUNC_DECL)
        return true;
    const MIRRoutine *routine = transpiler_find_mir_function(ctx, decl);
    bool is_extern = transpiler_decl_is_extern_function(ctx, decl);
    if (!is_extern && !transpiler_mir_routine_signature_metadata_complete_for(
            ctx, routine, decl, TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES,
            "MIR-only C path missing operator signature for '%s'", NULL,
            "MIR-only C path missing operator parameter type for '%s'"))
        return false;
    size_t count = is_extern ? ast_func_param_count(decl)
        : transpiler_mir_routine_param_count(routine);
    if (count != 2) {
        transpiler_set_mir_inventory_missing(ctx,
            "C operator '%s' requires two admitted parameters",
            ast_declaration_name(decl));
        return false;
    }
    ASTNode *nodes[2] = {ast_binary_left(expr), ast_binary_right(expr)};
    char **texts[2] = {left, right};
    for (size_t i = 0; i < 2; i++) {
        FuncParam *param = is_extern ? ast_func_param(decl, i)
            : transpiler_mir_routine_param(routine, i);
        const char *type = is_extern ? NULL
            : transpiler_mir_routine_param_type_name(routine, i);
        if (param == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "C operator '%s' is missing parameter ABI metadata",
                ast_declaration_name(decl));
            return false;
        }
        MIRParamCarriage carriage = is_extern
            ? mir_param_carriage_from_source_mode(param->mode)
            : transpiler_mir_routine_param_carriage(routine, i);
        if (carriage == MIR_PARAM_CARRIAGE_VALUE_RESULT
            && nodes[i]->type != AST_IDENTIFIER
            && nodes[i]->type != AST_MEMBER_ACCESS
            && nodes[i]->type != AST_ARRAY_ACCESS) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                "C operator argument %zu requires assignable inout storage", i + 1);
            return false;
        }
        bool indirect = !is_extern
            && transpiler_mir_routine_param_passes_indirect(routine, i);
        if (indirect || carriage == MIR_PARAM_CARRIAGE_VALUE_RESULT
            || transpiler_call_arg_needs_subject_address(
                ctx, param, type, NULL, NULL)) {
            char *address = transpiler_call_arg_address(
                ctx, nodes[i], *texts[i], ast_declaration_name(decl), i);
            if (address == NULL)
                return false;
            free(*texts[i]);
            *texts[i] = address;
        }
    }
    return true;
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
                && pgy_operator_method_name_matches(op, method_name)) {
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
            && pgy_operator_method_name_matches(op, method_name)) {
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
    const char *suffix = pgy_operator_overload_suffix(op);
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
