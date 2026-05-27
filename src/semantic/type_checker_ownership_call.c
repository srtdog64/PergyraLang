/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Call-argument ownership-boundary checks.
 */

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_support_internal.h"

static bool
semantic_call_arg_is_named_move_source(ASTNode *arg_expr,
                                       SemanticContext *ctx)
{
    return arg_expr != NULL
        && (arg_expr->type == AST_IDENTIFIER
            || semantic_borrowed_boundary_root_name(arg_expr, ctx) != NULL);
}

static bool
semantic_check_movable_call_argument(ASTNode *arg_expr,
                                     SemanticContext *ctx,
                                     const char *display_name,
                                     size_t arg_index,
                                     Type *param_type,
                                     Type *arg_type)
{
    ParamMode pmode = PARAM_MODE_DEFAULT;
    ASTNode *callee_decl = NULL;
    const char *borrowed_name;

    if (!semantic_call_arg_is_named_move_source(arg_expr, ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_MOVE_SOURCE_NOT_NAMED,
            PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            arg_expr,
            "%s arguments must be moved from a named variable; bind the value first, then pass that variable",
            resource_handle_display_name(param_type));
        return true;
    }

    callee_decl = semantic_lookup_function_param_contract(
        ctx, display_name, arg_index, &pmode);
    if (pmode == PARAM_MODE_REF
        && callee_decl != NULL
        && ast_func_body(callee_decl) != NULL) {
        unsigned callee_mask;
        borrowed_name = semantic_borrowed_boundary_root_name(arg_expr, ctx);
        callee_mask = semantic_legacy_ast_callable_param_escape_summary(
            callee_decl, arg_index, ctx);
        if (borrowed_name != NULL
            && semantic_param_summary_has_any_escape(callee_mask)) {
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
            return true;
        }
    }
    if (pmode != PARAM_MODE_REF) {
        borrowed_name = semantic_borrowed_boundary_root_name(arg_expr, ctx);
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
            return true;
        }
    }
    if (pmode != PARAM_MODE_REF)
        consume_qubit_value(arg_expr, ctx, "moved");
    return true;
}

static bool
semantic_check_subject_call_argument(ASTNode *arg_expr,
                                     SemanticContext *ctx,
                                     const char *display_name,
                                     size_t arg_index,
                                     Type *arg_type)
{
    ParamMode pmode = PARAM_MODE_DEFAULT;
    ASTNode *callee_decl = semantic_lookup_function_param_contract(
        ctx, display_name, arg_index, &pmode);

    if (!semantic_validate_borrowed_boundary_call_argument(
            arg_expr,
            ctx,
            callee_decl,
            display_name,
            arg_index,
            pmode,
            arg_type,
            "subject",
            true)) {
        return true;
    }
    if (pmode == PARAM_MODE_OWN)
        consume_qubit_value(arg_expr, ctx, "moved");
    return true;
}

static bool
semantic_check_value_boundary_call_argument(ASTNode *arg_expr,
                                            SemanticContext *ctx,
                                            const char *display_name,
                                            size_t arg_index,
                                            Type *arg_type)
{
    ParamMode pmode = PARAM_MODE_DEFAULT;
    ASTNode *callee_decl = semantic_lookup_function_param_contract(
        ctx, display_name, arg_index, &pmode);

    if (!semantic_validate_borrowed_boundary_call_argument(
            arg_expr,
            ctx,
            callee_decl,
            display_name,
            arg_index,
            pmode,
            arg_type,
            "value",
            true)) {
        return true;
    }
    if (pmode == PARAM_MODE_OWN)
        consume_qubit_value(arg_expr, ctx, "moved");
    return true;
}

static bool
semantic_check_anchored_call_owner_state(ASTNode *arg_expr,
                                         SemanticContext *ctx,
                                         ParamMode pmode)
{
    const char *src;
    Symbol *src_sym;

    if (arg_expr == NULL || arg_expr->type != AST_IDENTIFIER)
        return true;

    src = ast_identifier_name(arg_expr);
    src_sym = scope_lookup(ctx->scope, src);
    if (pmode == PARAM_MODE_OWN) {
        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
            if (src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED,
                    PGY_CAUSE_MOVE_FROM_RELEASED,
                    PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                    arg_expr,
                    "Cannot move from released slot '%s'", src);
            } else {
                src_sym->slot_info.state = SLOT_STATE_RELEASED;
            }
        } else if (src_sym != NULL) {
            src_sym->is_consumed = true;
        }
        return true;
    }
    if (pmode == PARAM_MODE_REF
        && src_sym != NULL
        && src_sym->kind == SYMBOL_SLOT
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
    return true;
}

static bool
semantic_check_anchored_live_view_conflict(ASTNode *arg_expr,
                                           SemanticContext *ctx,
                                           const char *display_name)
{
    const char *src;
    Symbol *src_sym;
    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;

    if (arg_expr == NULL || arg_expr->type != AST_IDENTIFIER)
        return true;

    src = ast_identifier_name(arg_expr);
    src_sym = scope_lookup(ctx->scope, src);
    if (src_sym == NULL || src_sym->kind != SYMBOL_SLOT)
        return true;
    if (!semantic_find_active_slot_view_for_source(ctx->scope,
            src_sym->name, &active_view_name, &active_view_kind, NULL)) {
        return true;
    }

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
    return false;
}

static bool
semantic_check_anchored_call_argument(ASTNode *arg_expr,
                                      SemanticContext *ctx,
                                      const char *display_name,
                                      size_t arg_index,
                                      Type *arg_type)
{
    ParamMode pmode = PARAM_MODE_DEFAULT;
    ASTNode *callee_decl = semantic_lookup_function_param_contract(
        ctx, display_name, arg_index, &pmode);

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
        return true;
    }

    if (!semantic_check_anchored_live_view_conflict(
            arg_expr, ctx, display_name)) {
        return true;
    }
    if (!semantic_validate_borrowed_boundary_call_argument(
            arg_expr,
            ctx,
            callee_decl,
            display_name,
            arg_index,
            pmode,
            arg_type,
            "slot handle (anchored)",
            true)) {
        return true;
    }
    return semantic_check_anchored_call_owner_state(
        arg_expr, ctx, pmode);
}

bool
semantic_check_function_call_ownership_argument(ASTNode *arg_expr,
                                                SemanticContext *ctx,
                                                const char *display_name,
                                                size_t arg_index,
                                                Type *param_type,
                                                Type *arg_type,
                                                OwnershipTypeClass param_ownership,
                                                OwnershipTypeClass arg_ownership,
                                                bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = false;

    if (param_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || arg_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        if (handled_out != NULL)
            *handled_out = true;
        if (arg_ownership != OWNERSHIP_TYPE_MOVE_ONLY
            || param_ownership != OWNERSHIP_TYPE_MOVE_ONLY) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_TYPE_MOVABLE_HANDLE_REQUIRED,
                PGY_FIX_PROVIDE_MOVABLE_HANDLE,
                arg_expr,
                "Movable resource argument type mismatch: expected '%s', got '%s'",
                resource_handle_display_name(param_type),
                resource_handle_display_name(arg_type));
            return true;
        }
        return semantic_check_movable_call_argument(
            arg_expr, ctx, display_name, arg_index, param_type, arg_type);
    }

    if (param_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
        if (handled_out != NULL)
            *handled_out = true;
        if (arg_ownership != OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_TYPE_SUBJECT_ARG_MISMATCH,
                PGY_FIX_ALIGN_SUBJECT_ARG_TYPE,
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
            return true;
        }
        return semantic_check_subject_call_argument(
            arg_expr, ctx, display_name, arg_index, arg_type);
    }

    if (param_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
        if (handled_out != NULL)
            *handled_out = true;
        if (!type_is_general_boundary_type(arg_type, ctx)
            || arg_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
            || arg_ownership == OWNERSHIP_TYPE_MOVE_ONLY
            || !type_is_assignable(arg_type, param_type)
            || !type_is_assignable(param_type, arg_type)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_TYPE_BOUNDARY_ARG_MISMATCH,
                PGY_FIX_ALIGN_BOUNDARY_ARG_TYPE,
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
            return true;
        }
        return semantic_check_value_boundary_call_argument(
            arg_expr, ctx, display_name, arg_index, arg_type);
    }

    if (param_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
        || arg_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
        if (handled_out != NULL)
            *handled_out = true;
        if (arg_ownership != OWNERSHIP_TYPE_ANCHORED_HANDLE
            || param_ownership != OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_TYPE_RESOURCE_HANDLE_ARG_MISMATCH,
                PGY_FIX_ALIGN_RESOURCE_HANDLE_ARG,
                arg_expr,
                "Resource handle argument type mismatch: expected '%s', got '%s'",
                resource_handle_display_name(param_type),
                resource_handle_display_name(arg_type));
            return true;
        }
        return semantic_check_anchored_call_argument(
            arg_expr, ctx, display_name, arg_index, arg_type);
    }

    return true;
}

bool
semantic_validate_borrowed_boundary_call_argument(ASTNode *arg_expr,
                                                  SemanticContext *ctx,
                                                  ASTNode *callee_decl,
                                                  const char *display_name,
                                                  size_t arg_index,
                                                  ParamMode pmode,
                                                  Type *arg_type,
                                                  const char *local_fix_label,
                                                  bool track_borrow_provenance)
{
    const char *borrowed_name = NULL;
    OwnershipTypeClass ownership_class;
    const char *named_value_cap = "Value";
    const char *named_value_lower = "value";
    const char *bind_fix = "bind the value to a local variable first";

    if (arg_expr == NULL)
        return true;

    ownership_class = semantic_classify_ownership_type(arg_type, ctx);
    switch (ownership_class) {
    case OWNERSHIP_TYPE_MOVE_ONLY:
        named_value_cap = "Slot handle (movable)";
        named_value_lower = "slot handle (movable)";
        bind_fix = "bind the slot handle (movable) to a local variable first";
        break;
    case OWNERSHIP_TYPE_SUBJECT_IDENTITY:
        named_value_cap = "Subject";
        named_value_lower = "subject";
        bind_fix = "bind the subject to a local variable first";
        break;
    case OWNERSHIP_TYPE_ANCHORED_HANDLE:
        named_value_cap = "Slot handle (anchored)";
        named_value_lower = "slot handle (anchored)";
        bind_fix = "bind the slot handle (anchored) to a local variable first";
        break;
    case OWNERSHIP_TYPE_BORROW_TRACKED:
    case OWNERSHIP_TYPE_COPY_ONLY:
    default:
        break;
    }

    if (ownership_class == OWNERSHIP_TYPE_COPY_ONLY)
        return true;

    if (arg_expr->type != AST_IDENTIFIER) {
        const char *borrowed_root_name =
            semantic_borrowed_boundary_root_name(arg_expr, ctx);
        if (pmode == PARAM_MODE_DEFAULT)
            return true;
        if (borrowed_root_name == NULL) {
            semantic_report_named_boundary_argument_required(
                arg_expr,
                arg_expr,
                ctx,
                named_value_cap,
                named_value_lower,
                bind_fix);
            return false;
        }
    }

    borrowed_name = semantic_borrowed_boundary_root_name(arg_expr, ctx);
    if (pmode == PARAM_MODE_REF && track_borrow_provenance) {
        unsigned callee_mask =
            semantic_legacy_ast_callable_param_escape_summary(
                callee_decl, arg_index, ctx);
        if (semantic_param_summary_has_any_escape(callee_mask)
            && borrowed_name != NULL) {
            semantic_validate_borrowed_escape(
                arg_expr, arg_expr, ctx, arg_type, borrowed_name,
                OWNERSHIP_CONSUMER_HELPER_CALL, NULL,
                display_name != NULL ? display_name : "<callee>",
                NULL, true, NULL, local_fix_label);
            return false;
        }
    }

    if (pmode != PARAM_MODE_REF && track_borrow_provenance
        && borrowed_name != NULL) {
        semantic_validate_borrowed_escape(
            arg_expr, arg_expr, ctx, arg_type, borrowed_name,
            OWNERSHIP_CONSUMER_HELPER_CALL, NULL,
            display_name != NULL ? display_name : "<callee>",
            NULL, false, pmode == PARAM_MODE_OWN ? "own" : "default",
            local_fix_label);
        return false;
    }

    return true;
}
