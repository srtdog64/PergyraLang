/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type-alias statement validation.
 */

#include "type_checker_internal.h"
#include "diag_codes.h"

bool
type_check_type_alias_stmt(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *target_type;
    const char *name;
    Symbol *existing;

    if (node == NULL || node->type != AST_TYPE_ALIAS)
        return true;

    name = ast_type_alias_name(node);
    existing = ctx != NULL ? scope_lookup_current(ctx->scope, name) : NULL;
    if (existing != NULL
        && !symbol_is_forward_declaration_for(existing,
            SYMBOL_CLASS, ast_node_stable_id(node))) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_SCOPE_DUPLICATE_SYMBOL,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of type alias '%s'", name);
        return false;
    }
    symbol_complete_forward_declaration(existing);

    target_type = ast_type_alias_target_type(node);
    if (target_type != NULL)
        (void)semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                target_type);
    return ctx == NULL || !ctx->has_error;
}
