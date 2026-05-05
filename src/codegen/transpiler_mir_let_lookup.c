/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR let-declaration lookup helpers.
 */

#include "transpiler_mir_let_lookup.h"

#include <string.h>

static ASTNode *
transpiler_find_let_decl_by_name_in_block(ASTNode *body, const char *name)
{
    if (body == NULL || name == NULL)
        return NULL;

    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_let_decl_by_name_in_block(
                body->data.block.statements[i], name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }

    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, name) == 0) {
        return body;
    }

    if (body->type == AST_WITH_STMT)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.with_stmt.body, name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.then_branch, name);
        if (found != NULL)
            return found;
        return transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.else_branch, name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.while_loop.body, name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.for_loop.body, name);

    return NULL;
}

ASTNode *
transpiler_find_let_decl_by_name(const ASTNode *func_decl, const char *name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    return transpiler_find_let_decl_by_name_in_block(
        func_decl->data.func_decl.body, name);
}
