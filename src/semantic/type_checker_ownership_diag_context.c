/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Shared ownership diagnostic context helpers.
 */

#include "type_checker_ownership_diag_internal.h"

const char *
semantic_current_consumer_name(SemanticContext *ctx)
{
    if (ctx != NULL
        && ctx->current_function_decl != NULL
        && ast_declaration_name(ctx->current_function_decl) != NULL) {
        return ast_declaration_name(ctx->current_function_decl);
    }
    return "<anonymous>";
}
