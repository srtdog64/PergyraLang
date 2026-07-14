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

static bool
ast_reserve_call_argument_capacity(ASTNode *call, size_t required_capacity)
{
    size_t new_capacity;
    ASTNode **grown_arguments;
    char **grown_names = NULL;

    if (call == NULL || call->type != AST_CALL)
        return false;
    if (required_capacity <= call->data.call.arg_capacity)
        return true;

    new_capacity = call->data.call.arg_capacity == 0
        ? 4
        : call->data.call.arg_capacity;
    while (new_capacity < required_capacity) {
        if (new_capacity > SIZE_MAX / 2)
            return false;
        new_capacity *= 2;
    }
    if (new_capacity > SIZE_MAX / sizeof(ASTNode *)
        || new_capacity > SIZE_MAX / sizeof(char *)) {
        return false;
    }

    grown_arguments = realloc(call->data.call.arguments,
        new_capacity * sizeof(ASTNode *));
    if (grown_arguments == NULL)
        return false;

    if (call->data.call.arg_names != NULL) {
        grown_names = realloc(call->data.call.arg_names,
            new_capacity * sizeof(char *));
        if (grown_names == NULL) {
            call->data.call.arguments = grown_arguments;
            return false;
        }
        call->data.call.arg_names = grown_names;
    }

    call->data.call.arguments = grown_arguments;
    call->data.call.arg_capacity = new_capacity;
    return true;
}

bool
ast_program_append_statement(ASTNode* node, ASTNode* statement)
{
    if (node == NULL || node->type != AST_PROGRAM || statement == NULL)
        return false;
    return ast_append_node(&node->data.program.statements,
                           &node->data.program.count,
                           &node->data.program.capacity,
                           statement);
}

ASTNode*
ast_program_detach_statement(ASTNode* node, size_t index)
{
    ASTNode *statement;

    if (node == NULL || node->type != AST_PROGRAM
        || index >= node->data.program.count) {
        return NULL;
    }

    statement = node->data.program.statements[index];
    node->data.program.statements[index] = NULL;
    return statement;
}

bool
ast_program_splice_take(ASTNode* node,
                        size_t index,
                        ASTNode* replacement_program)
{
    ASTNode **merged = NULL;
    ASTNode **old_statements;
    ASTNode **replacement_statements;
    ASTNode *removed;
    size_t old_count;
    size_t replacement_count;
    size_t merged_count;

    if (node == NULL || node->type != AST_PROGRAM
        || replacement_program == NULL
        || replacement_program->type != AST_PROGRAM
        || node == replacement_program
        || index >= node->data.program.count
        || node->data.program.statements[index] == replacement_program) {
        return false;
    }

    old_count = node->data.program.count;
    replacement_count = replacement_program->data.program.count;
    if (replacement_count > SIZE_MAX - (old_count - 1))
        return false;
    merged_count = old_count - 1 + replacement_count;
    if (merged_count > SIZE_MAX / sizeof(ASTNode *))
        return false;

    if (merged_count > 0) {
        merged = malloc(merged_count * sizeof(ASTNode *));
        if (merged == NULL)
            return false;
    }

    old_statements = node->data.program.statements;
    replacement_statements = replacement_program->data.program.statements;
    removed = old_statements[index];

    for (size_t i = 0; i < index; i++)
        merged[i] = old_statements[i];
    for (size_t i = 0; i < replacement_count; i++)
        merged[index + i] = replacement_statements[i];
    for (size_t i = index + 1; i < old_count; i++)
        merged[i - 1 + replacement_count] = old_statements[i];

    node->data.program.statements = merged;
    node->data.program.count = merged_count;
    node->data.program.capacity = merged_count;
    replacement_program->data.program.statements = NULL;
    replacement_program->data.program.count = 0;
    replacement_program->data.program.capacity = 0;

    free(old_statements);
    free(replacement_statements);
    ast_destroy(removed);
    return true;
}

bool
ast_program_replace_statements(ASTNode* node,
                               ASTNode** statements,
                               size_t count,
                               size_t capacity)
{
    if (node == NULL || node->type != AST_PROGRAM || capacity < count)
        return false;
    free(node->data.program.statements);
    node->data.program.statements = statements;
    node->data.program.count = count;
    node->data.program.capacity = capacity;
    return true;
}

void ast_add_statement(ASTNode* parent, ASTNode* statement) {
    if (parent->type == AST_PROGRAM) {
        (void)ast_program_append_statement(parent, statement);
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
    size_t old_count;

    if (call->type != AST_CALL) return;

    old_count = call->data.call.arg_count;
    if (old_count == SIZE_MAX
        || !ast_reserve_call_argument_capacity(call, old_count + 1)) {
        return;
    }

    call->data.call.arguments[call->data.call.arg_count++] = arg;
    if (call->data.call.arg_names != NULL) {
        call->data.call.arg_names[old_count] = NULL;
    }
}
