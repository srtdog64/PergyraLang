/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-binding contract for move-only builtin owner values.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_ownership_let_internal.h"

bool
ownership_let_validate_builtin_owner_binding(ASTNode *node,
                                             SemanticContext *ctx,
                                             Type *decl_type,
                                             ASTNode *init)
{
    ASTNode *callee;
    const char *callee_name = NULL;

    if (!type_is_builtin_owner_handle(decl_type))
        return true;

    callee = init != NULL && init->type == AST_CALL
        ? ast_call_callee(init) : NULL;
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);

    if (!ast_let_is_mutable(node)
        && callee_name != NULL
        && strcmp(callee_name, "TextBuilderNew") == 0) {
        return true;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        node,
        "TextBuilder is a single-owner local and cannot be copied, rebound, or declared mutable.\n"
        "Reason:\n"
        "- the current bounded owner rung admits only a fresh TextBuilderNew(...) initializer\n"
        "- generic move, field, container, and return transfer are not yet proven\n"
        "Fix:\n"
        "- initialize an immutable local directly from TextBuilderNew(capacity)\n"
        "- finish or drop that same local in its declaration scope");
    return false;
}

/*
 * Channel<T> descriptors are by-value structs with interior cursors, so a
 * second let-binding to an existing channel silently copies the descriptor
 * and the copies drift (duplicate/split deliveries even in serial code) --
 * the same class as the rejected Channel parameters and aggregate fields
 * (docs/189 C12, board WO-RT-6). Fail closed: a Channel local may only be
 * born from a fresh constructor call, immutably. If the representation
 * ruling later promotes Channel to a heap handle, this rule relaxes to
 * legal aliasing; until then rejection is the only sound surface.
 */
bool
ownership_let_validate_channel_binding(ASTNode *node,
                                       SemanticContext *ctx,
                                       Type *decl_type,
                                       ASTNode *init)
{
    ASTNode *callee;
    const char *callee_name = NULL;

    if (!type_is_constructed_named(decl_type, "Channel"))
        return true;

    callee = init != NULL && init->type == AST_CALL
        ? ast_call_callee(init) : NULL;
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);

    if (!ast_let_is_mutable(node)
        && callee_name != NULL
        && strcmp(callee_name, "Channel") == 0) {
        return true;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
        PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
        PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
        node,
        "Channel is identity-bearing and cannot be re-bound, copied, or declared mutable.\n"
        "Reason:\n"
        "- binding an existing channel to another name copies its descriptor (buffer pointer + cursors), so the copies drift and deliveries silently split\n"
        "Fix:\n"
        "- initialize an immutable local directly from Channel(capacity)\n"
        "- use that one channel variable at every send/recv site");
    return false;
}
