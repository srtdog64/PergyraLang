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
