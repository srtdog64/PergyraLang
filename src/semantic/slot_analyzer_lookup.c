/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer function declaration lookup.
 */

#include <string.h>

#include "slot_analyzer_internal.h"
#include "type_checker_internal.h"

ASTNode *
slot_analyzer_find_function_decl(const SlotFunctionLookup *lookup,
                                 const char *name)
{
    ASTNode *program;

    if (lookup == NULL || name == NULL)
        return NULL;

    if (lookup->ctx != NULL) {
        return semantic_host_index_find_decl_by_name(
            lookup->ctx, AST_FUNC_DECL, name);
    }

    program = lookup->program_root;
    if (program == NULL || program->type != AST_PROGRAM)
        return NULL;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;

        if (stmt->is_async_decl) {
            const char *async_name = ast_async_func_name(stmt);
            if (async_name != NULL && strcmp(async_name, name) == 0)
                return stmt;
        } else {
            const char *stmt_name = ast_declaration_name(stmt);
            if (stmt_name != NULL && strcmp(stmt_name, name) == 0)
                return stmt;
        }
    }

    return NULL;
}
