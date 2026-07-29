/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Semantic validation for `use stdlib ...` declarations.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker.h"
#include "../common/string_compat.h"

static bool
semantic_is_known_stdlib_use_module(const char *module_name)
{
    static const char *const modules[] = {
        "datetime",
        "device_adapter",
        "host_task_slot",
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

static bool
semantic_stdlib_use_seen(SemanticContext *ctx, const char *module_name)
{
    if (ctx == NULL || module_name == NULL)
        return false;

    for (size_t i = 0; i < ctx->stdlib_use_module_count; i++) {
        if (ctx->stdlib_use_module_names[i] != NULL
            && strcmp(ctx->stdlib_use_module_names[i], module_name) == 0) {
            return true;
        }
    }

    return false;
}

static bool
semantic_stdlib_use_record(SemanticContext *ctx, const char *module_name)
{
    char **grown;
    char *copy;
    size_t new_capacity;

    if (ctx == NULL || module_name == NULL)
        return false;
    if (semantic_stdlib_use_seen(ctx, module_name))
        return true;

    if (ctx->stdlib_use_module_count >= ctx->stdlib_use_module_capacity) {
        if (ctx->stdlib_use_module_capacity == 0) {
            new_capacity = 8;
        } else {
            if (ctx->stdlib_use_module_capacity > SIZE_MAX / 2)
                return false;
            new_capacity = ctx->stdlib_use_module_capacity * 2;
        }
        if (new_capacity > SIZE_MAX / sizeof(char *))
            return false;
        grown = realloc(ctx->stdlib_use_module_names,
                        new_capacity * sizeof(char *));
        if (grown == NULL)
            return false;
        ctx->stdlib_use_module_names = grown;
        ctx->stdlib_use_module_capacity = new_capacity;
    }

    copy = pergyra_strdup(module_name);
    if (copy == NULL)
        return false;
    ctx->stdlib_use_module_names[ctx->stdlib_use_module_count++] = copy;
    return true;
}

static void
validate_stdlib_use_decl(ASTNode *stmt, SemanticContext *ctx)
{
    const char *module_name;

    if (stmt == NULL || stmt->type != AST_USE_DECL || ctx == NULL
        || ast_use_module_name(stmt) == NULL) {
        return;
    }
    module_name = ast_use_module_name(stmt);

    if (!semantic_is_known_stdlib_use_module(module_name)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
            PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL,
            stmt,
            "Unknown stdlib use '%s'; expected one of datetime, device_adapter, host_task_slot, http, ledger, money, obligation, page, spray, storage, timer, versioning",
            module_name);
        return;
    }

    if (semantic_stdlib_use_seen(ctx, module_name)) {
        semantic_warning(ctx, stmt,
            "Duplicate stdlib use '%s'; resolver will merge it once",
            module_name);
        return;
    }

    if (!semantic_stdlib_use_record(ctx, module_name))
        semantic_error(ctx, stmt,
            "Could not record stdlib use '%s' for duplicate-use validation",
            module_name);
}

bool
type_check_use_decl(ASTNode *node, SemanticContext *ctx)
{
    validate_stdlib_use_decl(node, ctx);
    return ctx == NULL || !ctx->has_error;
}
