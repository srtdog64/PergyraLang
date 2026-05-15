/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Assignment ownership-boundary checks.
 */

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_internal.h"

bool
semantic_check_assignment_borrow_rebind(ASTNode *expr,
                                        SemanticContext *ctx,
                                        Type *target_type,
                                        Type *value_type)
{
    OwnershipTypeClass target_ownership =
        semantic_classify_ownership_type(target_type, ctx);
    OwnershipTypeClass value_ownership =
        semantic_classify_ownership_type(value_type, ctx);
    bool anchored_boundary =
        target_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
        || value_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE;
    bool move_boundary =
        target_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || value_ownership == OWNERSHIP_TYPE_MOVE_ONLY;
    bool handle_or_move_boundary = anchored_boundary || move_boundary;

    if (handle_or_move_boundary) {
        if (semantic_validate_borrowed_escape(
                expr, ast_assignment_value(expr), ctx, value_type, NULL,
                OWNERSHIP_CONSUMER_ASSIGNMENT_REBIND,
                ast_assignment_target(expr), NULL, NULL,
                false, NULL, NULL)) {
            return true;
        }
        if (anchored_boundary) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, expr,
                "Slot-handle (anchored) assignment is not allowed.\n"
                "Reason:\n"
                "- slot handles (anchored) such as Slot/SecureSlot/DeviceSlot cannot be copied or rebound with '='\n"
                "- assignment rebind would create a second boundary-visible handle binding outside the original anchor\n"
                "Fix:\n"
                "- use Read/Write for Slot<T>\n"
                "- keep using the current slot handle (anchored) binding directly\n"
                "- or move the slot handle (anchored) through an explicit ownership boundary");
        } else {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_MOVE_ONLY_ASSIGNMENT_REBIND,
                PGY_FIX_BIND_THE_MOVED_VALUE_ONCE, expr,
                "Move-only assignment rebind is not allowed.\n"
                "Reason:\n"
                "- move-only values must transfer ownership through a fresh binding\n"
                "- rebinding with '=' would duplicate or obscure the single owning path\n"
                "Fix:\n"
                "- move the value into a new binding\n"
                "- or Claim... to create a fresh handle");
        }
        return true;
    }

    if (semantic_validate_borrowed_escape(
            expr, ast_assignment_value(expr), ctx, value_type, NULL,
            OWNERSHIP_CONSUMER_ASSIGNMENT_REBIND,
            ast_assignment_target(expr), NULL, NULL,
            false, NULL, NULL)) {
        return true;
    }

    return false;
}
