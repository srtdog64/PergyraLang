/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic validation for `use stdlib ...` declarations.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_stdlib_use_internal.h"

static bool
semantic_is_known_stdlib_use_module(const char *module_name)
{
    static const char *const modules[] = {
        "datetime",
        "device_adapter",
        "http",
        "ledger",
        "money",
        "obligation",
        "page",
        "spray",
        "storage",
        "timer",
        "versioning"
    };

    if (module_name == NULL)
        return false;

    for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
        if (strcmp(module_name, modules[i]) == 0)
            return true;
    }

    return false;
}

void
validate_stdlib_use_decl(ASTNode *stmt, SemanticContext *ctx)
{
    if (stmt == NULL || stmt->type != AST_USE_DECL || ctx == NULL
        || stmt->data.use_decl.module_name == NULL) {
        return;
    }

    if (!semantic_is_known_stdlib_use_module(stmt->data.use_decl.module_name)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
            PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            stmt,
            "Unknown stdlib use '%s'; expected one of datetime, device_adapter, http, ledger, money, obligation, page, spray, storage, timer, versioning",
            stmt->data.use_decl.module_name);
        return;
    }

    if (ctx->program_root == NULL || ctx->program_root->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < ctx->program_root->data.program.count; i++) {
        ASTNode *prev = ctx->program_root->data.program.statements[i];
        if (prev == stmt)
            break;
        if (prev != NULL && prev->type == AST_USE_DECL
            && prev->data.use_decl.module_name != NULL
            && strcmp(prev->data.use_decl.module_name,
                      stmt->data.use_decl.module_name) == 0) {
            semantic_warning(ctx, stmt,
                "Duplicate stdlib use '%s'; resolver will merge it once",
                stmt->data.use_decl.module_name);
            break;
        }
    }
}
