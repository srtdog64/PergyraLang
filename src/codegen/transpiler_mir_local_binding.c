/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local-binding discovery helpers.
 */

#include <string.h>

#include "transpiler_mir_local_binding.h"
#include "../parser/ast_api.h"

static bool
transpiler_select_case_has_receive_binding(ASTNode *node,
                                           const char *base_name)
{
    if (node == NULL || base_name == NULL)
        return false;
    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (transpiler_select_case_has_receive_binding(
                    node->data.block.statements[i], base_name)) {
                return true;
            }
        }
        return false;
    }
    if (node->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(node);
        ASTNode *value = ast_assignment_value(node);
        return target != NULL
            && target->type == AST_IDENTIFIER
            && target->data.identifier.name != NULL
            && strcmp(target->data.identifier.name, base_name) == 0
            && value != NULL
            && value->type == AST_CHANNEL_RECV;
    }
    return false;
}

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
        if (ast_with_alias(body) != NULL
            && strcmp(ast_with_alias(body), base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(ast_with_body(body), base_name);
    }
    if (body->type == AST_IF_STMT) {
        return transpiler_has_local_binding_in_block(ast_if_then_branch(body), base_name)
            || transpiler_has_local_binding_in_block(ast_if_else_branch(body), base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_has_local_binding_in_block(ast_while_body(body), base_name);
    if (body->type == AST_FOR_LOOP) {
        if (ast_for_variable(body) != NULL
            && strcmp(ast_for_variable(body), base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(ast_for_body(body), base_name);
    }
    if (body->type == AST_SELECT_STMT) {
        for (size_t i = 0; i < ast_select_case_count(body); i++) {
            if (transpiler_select_case_has_receive_binding(
                    ast_select_case(body, i), base_name)) {
                return true;
            }
            if (transpiler_has_local_binding_in_block(
                    ast_select_case(body, i), base_name)) {
                return true;
            }
        }
        return transpiler_has_local_binding_in_block(
            ast_select_default_case(body), base_name);
    }
    return false;
}

bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        return false;
    for (size_t i = 0; i < ast_func_param_count(func_decl); i++) {
        FuncParam *p = ast_func_param(func_decl, i);
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0)
            return true;
    }
    return transpiler_has_local_binding_in_block(ast_func_body(func_decl),
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
        if (ast_with_alias(body) != NULL)
            transpiler_ssa_name_map_set(ssa_map,
                ast_with_alias(body), ast_with_alias(body));
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_with_body(body));
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
            ast_if_then_branch(body));
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_if_else_branch(body));
        return;
    }
    if (body->type == AST_WHILE_LOOP) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_while_body(body));
        return;
    }
    if (body->type == AST_FOR_LOOP) {
        if (ast_for_variable(body) != NULL) {
            transpiler_ssa_name_map_set(ssa_map,
                ast_for_variable(body),
                ast_for_variable(body));
        }
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_for_body(body));
    }
}
