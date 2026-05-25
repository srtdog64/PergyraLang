/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic validation for `use stdlib ...` declarations.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker.h"

static ASTNode *
stdlib_use_program(SemanticContext *ctx)
{
    return ctx != NULL ? ctx->program_root : NULL;
}

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

static void
validate_stdlib_use_decl(ASTNode *stmt, SemanticContext *ctx)
{
    ASTNode *program;

    if (stmt == NULL || stmt->type != AST_USE_DECL || ctx == NULL
        || ast_use_module_name(stmt) == NULL) {
        return;
    }

    if (!semantic_is_known_stdlib_use_module(ast_use_module_name(stmt))) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
            PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            stmt,
            "Unknown stdlib use '%s'; expected one of datetime, device_adapter, http, ledger, money, obligation, page, spray, storage, timer, versioning",
            ast_use_module_name(stmt));
        return;
    }

    program = stdlib_use_program(ctx);
    if (program == NULL || program->type != AST_PROGRAM)
        return;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *prev = ast_program_statement(program, i);
        if (prev == stmt)
            break;
        if (prev != NULL && prev->type == AST_USE_DECL
            && ast_use_module_name(prev) != NULL
            && strcmp(ast_use_module_name(prev),
                      ast_use_module_name(stmt)) == 0) {
            semantic_warning(ctx, stmt,
                "Duplicate stdlib use '%s'; resolver will merge it once",
                ast_use_module_name(stmt));
            break;
        }
    }
}

bool
type_check_use_decl(ASTNode *node, SemanticContext *ctx)
{
    validate_stdlib_use_decl(node, ctx);
    return ctx == NULL || !ctx->has_error;
}
