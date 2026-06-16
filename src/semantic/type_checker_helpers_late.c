/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker late / call-path helpers.
 * Owns the large call-typing and borrowed-boundary argument validator logic
 * as a focused translation unit. See docs/101_semantic_split_template.md.
 */

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "type_checker_generic_diag_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out);

Type *
type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                const char *display_name,
                                SemanticContext *ctx)
{
    ASTNode *callable_decl = NULL;

    if (sym == NULL) {
        if (name_looks_qualified(display_name)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                "Undefined function '%s' (expected a callable symbol). "
                "Check namespace spelling/export visibility or define/import a callable declaration before use.",
                display_name);
        } else {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                "Undefined function '%s' (expected a callable symbol in current scope). "
                "Declare '%s' first, or use a qualified name for a namespaced declaration.",
                display_name, display_name);
        }
        return TYPE_UNKNOWN;
    }

    {
        Type *constructor_type = NULL;
        if (type_check_constructor_symbol_call(expr, sym, display_name, ctx,
                &constructor_type)) {
            return constructor_type != NULL ? constructor_type : TYPE_UNKNOWN;
        }
    }

    if (sym->type->kind != TYPE_KIND_FUNCTION) {
        char sig_buf[256];
        semantic_format_function_signature(sym->type, sig_buf, sizeof(sig_buf));
        if (sig_buf[0] == '\0')
            snprintf(sig_buf, sizeof(sig_buf), "fn(<signature unavailable>)");
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CALL_NOT_CALLABLE, PGY_FIX_USE_CALLABLE_DECLARATION,
            expr,
            "'%s' resolves to a %s '%s' of type '%s', but function calls require a callable declaration. "
            "Expected a function signature matching %s. Use a function name, a callable variable, "
            "or adjust this symbol declaration before calling.",
            display_name,
            semantic_symbol_kind_label(sym->kind),
            display_name,
            type_name_or_unknown(sym->type),
            sig_buf);
        return TYPE_UNKNOWN;
    }
    if (ctx != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = semantic_find_callable_decl_by_name(ctx, display_name);
        if (callable_decl != NULL && !explicit_type_reference_allowed(callable_decl, expr, ctx)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY,
                PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER,
                expr,
                "Callable '%s' is not accessible across the current visibility boundary",
                display_name);
            return type_function_return_type(sym->type);
        }
    }
    sym->is_used = true;
    semantic_record_effect(ctx, type_function_effects(sym->type));
    semantic_record_callee_body_summary(ctx, sym->type);
    semantic_record_callable_decl_summary(ctx, callable_decl,
        type_function_effects(sym->type));
    if (ctx->in_parallel
        && type_effect_mask_has(type_function_effects(sym->type), EFFECT_SECURE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit calling secure-effect function '%s'; serialize authority-bearing operations outside the parallel block",
            display_name);
        return type_function_return_type(sym->type);
    }

    size_t expected = type_function_param_count(sym->type);
    size_t provided = ast_call_arg_count(expr);
    GenericParams *callable_generic_params = NULL;
    size_t callable_generic_count = 0;
    Type **effective_generic_types = NULL;
    if (provided != expected) {
        char sig_buf[256];
        semantic_format_function_signature(sym->type, sig_buf, sizeof(sig_buf));
        if (sig_buf[0] == '\0')
            snprintf(sig_buf, sizeof(sig_buf), "fn(<signature unavailable>)");
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu. Declared signature: %s. "
            "Adjust caller arguments to satisfy the declaration.",
            display_name, (unsigned long long) expected, (unsigned long long) provided, sig_buf);
        return type_function_return_type(sym->type);
    }

    Type **call_arg_types = calloc(provided > 0 ? provided : 1, sizeof(Type *));
    if (call_arg_types == NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_UNKNOWN_TYPE,
            PGY_CAUSE_RESOLUTION_OOM,
            PGY_FIX_REDUCE_SCOPE_OR_RETRY,
            expr,
            "Could not allocate call argument type metadata for '%s'.\n"
            "Reason:\n"
            "- semantic call checking ran out of memory while recording argument types\n"
            "Fix:\n"
            "- reduce this compilation unit size and retry\n"
            "- or report the input if this happens on a small program",
            display_name != NULL ? display_name : "<call>");
        return type_function_return_type(sym->type);
    }
    if (ctx != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = semantic_find_callable_decl_by_name(ctx, display_name);
        callable_generic_params = ast_func_generic_params(callable_decl);
        callable_generic_count = ast_generic_param_count(callable_generic_params);
        if (callable_generic_count > 0) {
            effective_generic_types = calloc(
                callable_generic_count > 0
                    ? callable_generic_count : 1,
                sizeof(Type *));
            if (effective_generic_types != NULL) {
                for (size_t gi = 0; gi < callable_generic_count; gi++) {
                    GenericParam *gp =
                        ast_generic_param_at(callable_generic_params, gi);
                    ASTNode *default_type = ast_generic_param_default_type(gp);
                    if (default_type != NULL)
                        effective_generic_types[gi] =
                            semantic_type_resolution_lookup_metadata_type_ref(
                                ctx, default_type);
                    if (effective_generic_types[gi] == NULL)
                        effective_generic_types[gi] = TYPE_UNKNOWN;
                }
            }
        }
    }

    const char *mut_ref_arg_names[64];
    int mut_ref_arg_count = 0;
    for (size_t i = 0; i < provided; i++) {
        ASTNode *arg_expr = ast_call_argument(expr, i);
        if (callable_decl != NULL
            && i < ast_func_param_count(callable_decl)
            && arg_expr != NULL
            && arg_expr->type == AST_IDENTIFIER) {
            FuncParam *mp = ast_func_param(callable_decl, i);
            if (mp != NULL && mp->mode == PARAM_MODE_MUT_REF) {
                const char *an = ast_identifier_name(arg_expr);
                for (int mj = 0; an != NULL && mj < mut_ref_arg_count; mj++) {
                    if (mut_ref_arg_names[mj] != NULL
                        && strcmp(mut_ref_arg_names[mj], an) == 0) {
                        semantic_error_with_hints(ctx,
                            PGY_CODE_SEM_BORROW_ESCAPE,
                            PGY_CAUSE_BORROW_ESCAPE,
                            PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                            arg_expr,
                            "'%s' is passed as '&mut' more than once in the same call; each '&mut' argument must be a distinct variable to avoid a lost update",
                            an);
                        break;
                    }
                }
                if (an != NULL && mut_ref_arg_count < 64)
                    mut_ref_arg_names[mut_ref_arg_count++] = an;
            }
        }
        Type *param_type = type_function_param_type(sym->type, i);
        OwnershipTypeClass declared_param_ownership =
            semantic_classify_ownership_type(param_type, ctx);
        Type *arg_type = declared_param_ownership == OWNERSHIP_TYPE_MOVE_ONLY
            ? type_check_qubit_use(arg_expr, ctx)
            : type_check_expression(arg_expr, ctx);
        if (type_equals(arg_type, TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ASSIGNABILITY_CHECK,
                PGY_FIX_ALIGN_OPERAND_TYPE,
                arg_expr,
                "Void expression cannot be passed as a call argument; split the side effect into a statement before the call");
            arg_type = TYPE_UNKNOWN;
        }
        if (effective_generic_types != NULL
            && callable_decl != NULL
            && i < ast_func_param_count(callable_decl)) {
            FuncParam *fp = ast_func_param(callable_decl, i);
            int param_index = -1;
            if (fp != NULL && fp->type != NULL
                && ast_type_name(fp->type) != NULL) {
                param_index = find_generic_param_index(
                    callable_generic_params,
                    ast_type_name(fp->type));
            }
            if (param_index >= 0
                && (size_t)param_index < callable_generic_count) {
                effective_generic_types[param_index] = arg_type;
                if (arg_type != NULL) {
                    param_type = arg_type;
                }
            }
        }
        if (call_arg_types != NULL)
            call_arg_types[i] = arg_type;

        OwnershipTypeClass param_ownership =
            semantic_classify_ownership_type(param_type, ctx);
        OwnershipTypeClass arg_ownership =
            semantic_classify_ownership_type(arg_type, ctx);
        bool ownership_handled = false;

        semantic_check_function_call_ownership_argument(
            arg_expr,
            ctx,
            display_name,
            i,
            param_type,
            arg_type,
            param_ownership,
            arg_ownership,
            &ownership_handled);
        if (ownership_handled)
            continue;

        require_assignable(arg_type, param_type, arg_expr, ctx);
    }

    semantic_validate_function_call_generic_where(
        expr, ctx, display_name, provided, call_arg_types);

    Type *return_type = type_function_return_type(sym->type);
    if (effective_generic_types != NULL
        && callable_decl != NULL
        && callable_decl->type == AST_FUNC_DECL
        && ast_func_return_type(callable_decl) != NULL
        && ast_type_name(ast_func_return_type(callable_decl)) != NULL) {
        int return_index = find_generic_param_index(
            callable_generic_params,
            ast_type_name(ast_func_return_type(callable_decl)));
        if (return_index >= 0
            && (size_t)return_index < callable_generic_count
            && effective_generic_types[return_index] != NULL) {
            return_type = effective_generic_types[return_index];
        }
    }

    free(effective_generic_types);
    free(call_arg_types);
    return return_type;
}
