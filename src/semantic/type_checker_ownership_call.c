/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Call-argument ownership-boundary checks.
 */

#include "slot_analyzer.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_support_internal.h"

static unsigned
callable_param_escape_summary_local(ASTNode *callee_decl,
                                    size_t arg_index,
                                    SemanticContext *ctx)
{
    if (callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL
        || callee_decl->data.func_decl.body == NULL
        || arg_index >= callee_decl->data.func_decl.param_count) {
        return 0u;
    }

    return slot_analyze_param_summary_in_program(
        callee_decl->data.func_decl.body,
        callee_decl->data.func_decl.params[arg_index] != NULL
            ? callee_decl->data.func_decl.params[arg_index]->name
            : NULL,
        ctx != NULL ? ctx->program_root : NULL);
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
        unsigned callee_mask = callable_param_escape_summary_local(
            callee_decl, arg_index, ctx);
        if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                            | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                            | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0
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
