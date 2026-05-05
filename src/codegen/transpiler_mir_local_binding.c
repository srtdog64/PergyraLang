/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local-binding discovery helpers.
 */

#include <string.h>

#include "transpiler_mir_local_binding.h"

bool
transpiler_has_local_binding_in_block(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return false;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            if (transpiler_has_local_binding_in_block(
                    body->data.block.statements[i], base_name)) {
                return true;
            }
        }
        return false;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        return true;
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL
            && strcmp(body->data.with_stmt.alias, base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(body->data.with_stmt.body, base_name);
    }
    if (body->type == AST_IF_STMT) {
        return transpiler_has_local_binding_in_block(body->data.if_stmt.then_branch, base_name)
            || transpiler_has_local_binding_in_block(body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_has_local_binding_in_block(body->data.while_loop.body, base_name);
    if (body->type == AST_FOR_LOOP) {
        if (body->data.for_loop.variable != NULL
            && strcmp(body->data.for_loop.variable, base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(body->data.for_loop.body, base_name);
    }
    return false;
}

bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        return false;
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0)
            return true;
    }
    return transpiler_has_local_binding_in_block(func_decl->data.func_decl.body,
        base_name);
}

void
transpiler_register_with_alias_bindings_in_block(TranspilerSSANameMap *ssa_map,
                                                 ASTNode *body)
{
    if (ssa_map == NULL || body == NULL)
        return;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++)
            transpiler_register_with_alias_bindings_in_block(ssa_map,
                body->data.block.statements[i]);
        return;
    }
    if (body->type == AST_WITH_STMT) {
        if (body->data.with_stmt.alias != NULL)
            transpiler_ssa_name_map_set(ssa_map,
                body->data.with_stmt.alias, body->data.with_stmt.alias);
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.with_stmt.body);
        return;
    }
    if (body->type == AST_LET_DESTRUCTURE) {
        for (size_t i = 0; i < body->data.let_destructure.name_count; i++) {
            const char *pname = body->data.let_destructure.names[i];
            if (pname != NULL)
                transpiler_ssa_name_map_set(ssa_map, pname, pname);
        }
        return;
    }
    if (body->type == AST_IF_STMT) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.if_stmt.then_branch);
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.if_stmt.else_branch);
        return;
    }
    if (body->type == AST_WHILE_LOOP) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.while_loop.body);
        return;
    }
    if (body->type == AST_FOR_LOOP) {
        if (body->data.for_loop.variable != NULL) {
            transpiler_ssa_name_map_set(ssa_map,
                body->data.for_loop.variable,
                body->data.for_loop.variable);
        }
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            body->data.for_loop.body);
    }
}
