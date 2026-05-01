/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST (Abstract Syntax Tree) implementation
 */

#include "ast.h"
#include <stdint.h>
#include <stdlib.h>

// ============= AST mutation helpers =============

static bool
ast_append_node(ASTNode ***items, size_t *count, size_t *capacity,
                ASTNode *item)
{
    if (items == NULL || count == NULL || capacity == NULL)
        return false;

    if (*count >= *capacity) {
        size_t new_capacity = *capacity == 0 ? 4 : *capacity * 2;
        ASTNode **grown;

        if (new_capacity < *capacity
            || new_capacity > SIZE_MAX / sizeof(ASTNode *)) {
            return false;
        }

        grown = realloc(*items, new_capacity * sizeof(ASTNode *));
        if (grown == NULL)
            return false;

        *items = grown;
        *capacity = new_capacity;
    }

    (*items)[(*count)++] = item;
    return true;
}

void ast_add_statement(ASTNode* parent, ASTNode* statement) {
    if (parent->type == AST_PROGRAM) {
        ast_append_node(&parent->data.program.statements,
                        &parent->data.program.count,
                        &parent->data.program.capacity,
                        statement);
    } else if (parent->type == AST_BLOCK) {
        ast_append_node(&parent->data.block.statements,
                        &parent->data.block.count,
                        &parent->data.block.capacity,
                        statement);
    } else if (parent->type == AST_EXTERN_BLOCK) {
        ast_append_node(&parent->data.extern_block.declarations,
                        &parent->data.extern_block.count,
                        &parent->data.extern_block.capacity,
                        statement);
    } else if (parent->type == AST_NAMESPACE_DECL) {
        ast_append_node(&parent->data.namespace_decl.statements,
                        &parent->data.namespace_decl.count,
                        &parent->data.namespace_decl.capacity,
                        statement);
    }
}

void ast_add_parallel_task(ASTNode* parallel, ASTNode* task) {
    if (parallel->type != AST_PARALLEL_BLOCK) return;

    ast_append_node(&parallel->data.parallel.tasks,
                    &parallel->data.parallel.task_count,
                    &parallel->data.parallel.task_capacity,
                    task);
}

void ast_add_argument(ASTNode* call, ASTNode* arg) {
    if (call->type != AST_CALL) return;

    ast_append_node(&call->data.call.arguments,
                    &call->data.call.arg_count,
                    &call->data.call.arg_capacity,
                    arg);
}
