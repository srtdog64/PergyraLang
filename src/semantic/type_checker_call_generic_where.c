/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Generic function call-site where-clause validation.
 */

#include "type_checker_generic_diag_internal.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

void
semantic_validate_function_call_generic_where(ASTNode *expr,
                                              SemanticContext *ctx,
                                              const char *display_name,
                                              size_t provided,
                                              Type **call_arg_types)
{
    if (ctx == NULL || ctx->program_root == NULL)
        return;

    ASTNode *prog = ctx->program_root;
    for (size_t si = 0; si < prog->data.program.count; si++) {
        ASTNode *stmt = prog->data.program.statements[si];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL
            || stmt->data.func_decl.name == NULL
            || strcmp(stmt->data.func_decl.name, display_name) != 0
            || stmt->data.func_decl.generic_params == NULL
            || stmt->data.func_decl.generic_params->count == 0
            || stmt->data.func_decl.where_clause == NULL) {
            continue;
        }

        GenericParams *decl_gp = stmt->data.func_decl.generic_params;
        WhereClause *wc = stmt->data.func_decl.where_clause;
        char *expected_sig = format_generic_subject_signature(display_name, decl_gp);
        Type **effective_generic_types =
            calloc(decl_gp->count > 0 ? decl_gp->count : 1, sizeof(Type *));
        if (effective_generic_types == NULL) {
            free(expected_sig);
            break;
        }

        for (size_t gi = 0; gi < decl_gp->count; gi++) {
            GenericParam *gp = decl_gp->params[gi];
            if (gp != NULL && gp->default_type != NULL) {
                effective_generic_types[gi] =
                    semantic_type_resolution_lookup_annotation_or_unknown(ctx, gp->default_type);
            }
        }
        for (size_t ai = 0; ai < provided; ai++) {
            FuncParam *fp = (ai < stmt->data.func_decl.param_count)
                ? stmt->data.func_decl.params[ai] : NULL;
            int param_index;
            if (fp == NULL || fp->type == NULL
                || fp->type->type != AST_TYPE
                || fp->type->data.type.name == NULL) {
                continue;
            }
            param_index = find_generic_param_index(
                decl_gp, fp->type->data.type.name);
            if (param_index < 0)
                continue;
            effective_generic_types[param_index] =
                (call_arg_types != NULL) ? call_arg_types[ai] : NULL;
        }

        for (size_t ci = 0; ci < wc->count; ci++) {
            TypeConstraint *tc = wc->constraints[ci];
            int param_index;
            const char *param_name;
            Type *concrete_type;
            const char *actual_sig =
                format_effective_generic_type_list_scratch(
                    ctx, display_name, effective_generic_types, decl_gp->count);

            if (tc == NULL || tc->type_param == NULL)
                continue;

            param_name = tc->type_param;
            param_index = find_generic_param_index(decl_gp, param_name);
            if (param_index < 0 || (size_t)param_index >= decl_gp->count) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
                    PGY_CAUSE_CLASS_CONTRACT,
                    PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN,
                    expr,
                    "Call site of '%s' could not validate generic parameter '%s'.\n"
                    "Reason:\n"
                    "- function '%s' where-clause references '%s'\n"
                    "- that parameter does not exist in the function generic parameter list\n"
                    "Fix:\n"
                    "- change the function where-clause to reference an existing generic parameter\n"
                    "- or add generic parameter '%s' to function '%s'",
                    display_name,
                    param_name != NULL ? param_name : "<param>",
                    display_name,
                    param_name != NULL ? param_name : "<param>",
                    param_name != NULL ? param_name : "<param>",
                    display_name);
                continue;
            }

            concrete_type = effective_generic_types[param_index];
            if (concrete_type == NULL) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
                    PGY_CAUSE_CLASS_CONTRACT,
                    PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN,
                    expr,
                    "Call site of '%s' could not materialize generic argument for '%s'.\n"
                    "Reason:\n"
                    "- function '%s' where-clause validation depends on an effective type argument for '%s'\n"
                    "- neither explicit call-site evidence nor declaration defaults produced a concrete type\n"
                    "Fix:\n"
                    "- pass arguments that make '%s' concrete\n"
                    "- or add/fix a default type argument for '%s'",
                    display_name,
                    param_name != NULL ? param_name : "<param>",
                    display_name,
                    param_name != NULL ? param_name : "<param>",
                    param_name != NULL ? param_name : "<param>",
                    param_name != NULL ? param_name : "<param>");
                continue;
            }

            for (size_t bi = 0; bi < tc->bound_count; bi++) {
                char *bounds_text = format_type_constraint_bounds(tc);
                const char *bound_name =
                    (tc->bounds[bi] != NULL
                     && tc->bounds[bi]->type == AST_TYPE
                     && tc->bounds[bi]->data.type.name != NULL)
                        ? tc->bounds[bi]->data.type.name
                        : NULL;
                bool satisfies = concrete_type_satisfies_bound(
                    concrete_type, tc->bounds[bi], ctx);
                if (!satisfies) {
                    semantic_report_function_generic_bound_failure(
                        ctx,
                        expr,
                        display_name,
                        param_name,
                        bound_name != NULL ? bound_name : "<constraint>",
                        bounds_text,
                        expected_sig != NULL ? expected_sig : display_name,
                        actual_sig != NULL ? actual_sig : display_name,
                        concrete_type->name != NULL ? concrete_type->name : "<type>");
                }
                free(bounds_text);
            }
        }

        free(expected_sig);
        free(effective_generic_types);
        break;
    }
}
