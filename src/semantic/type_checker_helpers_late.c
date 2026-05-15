/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker ??late / call-path helpers.
 * Owns the large call-typing and borrowed-boundary argument validator logic
 * as a focused translation unit. See docs/101_semantic_split_template.md.
 */

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "type_checker_generic_diag_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "slot_analyzer.h"
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
    if (ctx->program_root != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = find_callable_decl_by_name(ctx->program_root, display_name);
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
    if (ctx->program_root != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = find_callable_decl_by_name(ctx->program_root, display_name);
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
                            domain_resolve_type_ref(default_type, ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < provided; i++) {
        ASTNode *arg_expr = ast_call_argument(expr, i);
        Type *param_type = type_function_param_type(sym->type, i);
        OwnershipTypeClass declared_param_ownership =
            semantic_classify_ownership_type(param_type, ctx);
        Type *arg_type = declared_param_ownership == OWNERSHIP_TYPE_MOVE_ONLY
            ? type_check_qubit_use(arg_expr, ctx)
            : type_check_expression(arg_expr, ctx);
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

        if (param_ownership == OWNERSHIP_TYPE_MOVE_ONLY
            || arg_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (arg_ownership != OWNERSHIP_TYPE_MOVE_ONLY
                || param_ownership != OWNERSHIP_TYPE_MOVE_ONLY) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_MOVABLE_HANDLE_REQUIRED, PGY_FIX_PROVIDE_MOVABLE_HANDLE,
                    arg_expr,
                    "Movable resource argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            if (arg_expr->type != AST_IDENTIFIER
                && semantic_borrowed_boundary_root_name(
                       arg_expr, ctx) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_MOVE_SOURCE_NOT_NAMED, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                    arg_expr,
                    "%s arguments must be moved from a named variable; bind the value first, then pass that variable",
                    resource_handle_display_name(param_type));
                continue;
            }
            callee_decl = semantic_lookup_function_param_contract(
                ctx, display_name, i, &pmode);
            if (pmode == PARAM_MODE_REF
                && callee_decl != NULL
                && ast_func_body(callee_decl) != NULL) {
                const char *borrowed_name =
                    semantic_borrowed_boundary_root_name(
                        arg_expr, ctx);
                unsigned callee_mask = semantic_callable_param_escape_summary(
                    callee_decl, i, ctx);
                if (borrowed_name != NULL
                    && (callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                       | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                       | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    semantic_validate_borrowed_escape(
                        arg_expr,
                        arg_expr,
                        ctx,
                        arg_type,
                        borrowed_name,
                        OWNERSHIP_CONSUMER_HELPER_CALL,
                        NULL,
                        display_name != NULL ? display_name : "<callee>",
                        NULL,
                        true,
                        NULL,
                        "slot handle (movable)");
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF) {
                const char *borrowed_name =
                    semantic_borrowed_boundary_root_name(
                        arg_expr, ctx);
                if (borrowed_name != NULL) {
                    semantic_validate_borrowed_escape(
                        arg_expr,
                        arg_expr,
                        ctx,
                        arg_type,
                        borrowed_name,
                        OWNERSHIP_CONSUMER_HELPER_CALL,
                        NULL,
                        display_name != NULL ? display_name : "<callee>",
                        NULL,
                        false,
                        pmode == PARAM_MODE_OWN ? "own" : "default",
                        "slot handle (movable)");
                    continue;
                }
            }
            if (pmode == PARAM_MODE_REF)
                continue;
            consume_qubit_value(arg_expr, ctx, "moved");
            continue;
        }

        if (param_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (arg_ownership != OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_SUBJECT_ARG_MISMATCH, PGY_FIX_ALIGN_SUBJECT_ARG_TYPE,
                    arg_expr,
                    "Subject argument type mismatch: expected '%s', got '%s'.\n"
                    "Reason:\n"
                    "- subject boundary passing requires the caller value and callee contract to agree on the same subject type\n"
                    "- ownership/borrow provenance cannot be preserved across mismatched subject types\n"
                    "Fix:\n"
                    "- pass a value of type '%s'\n"
                    "- or change the callee contract to accept '%s'",
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type),
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type));
                continue;
            }
            callee_decl = semantic_lookup_function_param_contract(
                ctx, display_name, i, &pmode);
            if (!semantic_validate_borrowed_boundary_call_argument(
                    arg_expr,
                    ctx,
                    callee_decl,
                    display_name,
                    i,
                    pmode,
                    arg_type,
                    "subject",
                    true)) {
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                consume_qubit_value(arg_expr, ctx, "moved");
            }
            continue;
        }

        if (param_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            bool track_boundary_borrow = true;
            if (!type_is_general_boundary_type(arg_type, ctx)
                || arg_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
                || arg_ownership == OWNERSHIP_TYPE_MOVE_ONLY
                || !type_is_assignable(arg_type, param_type)
                || !type_is_assignable(param_type, arg_type)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_BOUNDARY_ARG_MISMATCH, PGY_FIX_ALIGN_BOUNDARY_ARG_TYPE,
                    arg_expr,
                    "Boundary value argument type mismatch: expected '%s', got '%s'.\n"
                    "Reason:\n"
                    "- own/ref value boundaries require the caller value and callee contract to agree on the same boundary value type\n"
                    "- provenance cannot be preserved across mismatched boundary value types\n"
                    "Fix:\n"
                    "- pass a value of type '%s'\n"
                    "- or change the callee contract to accept '%s'",
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type),
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type));
                continue;
            }
            callee_decl = semantic_lookup_function_param_contract(
                ctx, display_name, i, &pmode);
            if (!semantic_validate_borrowed_boundary_call_argument(
                    arg_expr,
                    ctx,
                    callee_decl,
                    display_name,
                    i,
                    pmode,
                    arg_type,
                    "value",
                    track_boundary_borrow)) {
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                const char *borrowed_name =
                    semantic_borrowed_boundary_root_name(
                        arg_expr, ctx);
                if (track_boundary_borrow || borrowed_name == NULL)
                    consume_qubit_value(arg_expr, ctx, "moved");
            }
            continue;
        }

        if (param_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
            || arg_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            if (arg_ownership != OWNERSHIP_TYPE_ANCHORED_HANDLE
                || param_ownership != OWNERSHIP_TYPE_ANCHORED_HANDLE) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_RESOURCE_HANDLE_ARG_MISMATCH, PGY_FIX_ALIGN_RESOURCE_HANDLE_ARG,
                    arg_expr,
                    "Resource handle argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            {
                ParamMode pmode = PARAM_MODE_DEFAULT;
                ASTNode *callee_decl = semantic_lookup_function_param_contract(
                    ctx, display_name, i, &pmode);
                if (pmode == PARAM_MODE_DEFAULT) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                        PGY_CAUSE_SLOT_PARAM_QUALIFIER_MISSING,
                        PGY_FIX_ANNOTATE_SLOT_PARAM_QUALIFIER,
                        arg_expr,
                        "Slot-handle (anchored) arguments require 'own' or 'ref' in the callee signature.\n"
                        "Reason:\n"
                        "- slot-handle (anchored) boundaries must declare whether the call borrows or transfers ownership\n"
                        "- implicit handle passing would hide that boundary contract\n"
                        "Fix:\n"
                        "- use 'own' in the callee signature to transfer the handle\n"
                        "- or use 'ref' to borrow it");
                    continue;
                }
                if (arg_expr->type == AST_IDENTIFIER) {
                    const char *src = ast_identifier_name(arg_expr);
                    Symbol *src_sym = scope_lookup(ctx->scope, src);
                    const char *active_view_name = NULL;
                    const char *active_view_kind = NULL;
                    if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT
                        && semantic_find_active_slot_view_for_source(
                            ctx->scope,
                            src_sym->name,
                            &active_view_name,
                            &active_view_kind,
                            NULL)) {
                        semantic_error_with_hints(ctx,
                            PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                            PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                            PGY_FIX_SERIALIZE_PIN_ACCESS,
                            arg_expr,
                            "Cannot pass slot '%s' to '%s' while %s '%s' is live.\n"
                            "Reason:\n"
                            "- slot helper calls may read, write, release, or forward the owner handle\n"
                            "- passing the owner while a view is live would bypass the view's aliasing contract\n"
                            "Fix:\n"
                            "- pass the active view to a view-typed helper\n"
                            "- or end the pin/view scope before calling '%s'",
                            src_sym->name,
                            display_name != NULL ? display_name : "<callee>",
                            active_view_kind != NULL ? active_view_kind : "view",
                            active_view_name != NULL ? active_view_name : "<view>",
                            display_name != NULL ? display_name : "<callee>");
                        continue;
                    }
                }
                if (!semantic_validate_borrowed_boundary_call_argument(
                        arg_expr,
                        ctx,
                        callee_decl,
                        display_name,
                        i,
                        pmode,
                        arg_type,
                        "slot handle (anchored)",
                        true)) {
                    continue;
                }
                if (arg_expr->type == AST_IDENTIFIER) {
                    const char *src = ast_identifier_name(arg_expr);
                    Symbol *src_sym = scope_lookup(ctx->scope, src);
                    if (pmode == PARAM_MODE_OWN) {
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
                            if (src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, arg_expr,
                                    "Cannot move from released slot '%s'", src);
                            } else {
                                src_sym->slot_info.state = SLOT_STATE_RELEASED;
                            }
                        } else if (src_sym != NULL) {
                            src_sym->is_consumed = true;
                        }
                    } else if (pmode == PARAM_MODE_REF) {
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT
                            && src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_BORROW_RELEASED,
                                PGY_FIX_RECLAIM_SOURCE_OR_TRACE_EARLIER_RELEASE,
                                arg_expr,
                                "Cannot borrow released slot '%s'.\n"
                                "Reason:\n"
                                "- slot '%s' was already released or invalidated earlier in this scope\n"
                                "- a 'ref' borrow requires a live source handle\n"
                                "Fix:\n"
                                "- reacquire the slot before borrowing it\n"
                                "- or remove the earlier release/invalidating transfer",
                                src,
                                src);
                        }
                    }
                }
                continue;
            }
        }

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
