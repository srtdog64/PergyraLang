/*
 * Copyright (c) 2026 Pergyra Language Project
 * Parallel-node accessor owner: tasks and the join form (docs/181 SS1).
 * Split out of ast_async_lambda_accessors.c under the 550-line
 * responsibility rule.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

ASTNode**
ast_parallel_tasks(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.parallel.task_count;
    return node->data.parallel.tasks;
}

size_t
ast_parallel_task_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return 0;
    return node->data.parallel.task_count;
}

ASTNode*
ast_parallel_task(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    if (index >= node->data.parallel.task_count)
        return NULL;
    return node->data.parallel.tasks != NULL
        ? node->data.parallel.tasks[index]
        : NULL;
}

bool
ast_parallel_set_join_form(ASTNode* node, const char* element_name,
                           ASTNode* collection)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK
        || element_name == NULL)
        return false;
    node->data.parallel.join_element = pergyra_strdup(element_name);
    if (node->data.parallel.join_element == NULL)
        return false;
    node->data.parallel.join_collection = collection;
    node->data.parallel.is_join_form = true;
    return true;
}

bool
ast_parallel_is_join_form(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return false;
    return node->data.parallel.is_join_form;
}

const char*
ast_parallel_join_element(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    return node->data.parallel.join_element;
}

ASTNode*
ast_parallel_join_collection(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    return node->data.parallel.join_collection;
}

/* --- Index form (docs/181 R1): `parallel (i in lo..hi)` --------------- */

void
ast_parallel_set_join_range_end(ASTNode* node, ASTNode* range_end)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return;
    node->data.parallel.join_range_end = range_end;
}

ASTNode*
ast_parallel_join_range_end(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    return node->data.parallel.join_range_end;
}

bool
ast_parallel_is_index_join(const ASTNode* node)
{
    return ast_parallel_is_join_form(node)
        && node->data.parallel.join_range_end != NULL;
}

/* Index-disjointness fact rows: produced by the join admission checker
 * (single producer) and consumed by both backend emitters. */
void
ast_parallel_reset_join_index_arrays(ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return;
    for (size_t i = 0; i < node->data.parallel.join_index_array_count; i++)
        free(node->data.parallel.join_index_arrays[i]);
    free(node->data.parallel.join_index_arrays);
    node->data.parallel.join_index_arrays = NULL;
    node->data.parallel.join_index_array_count = 0;
}

bool
ast_parallel_add_join_index_array(ASTNode* node, const char* name)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || name == NULL)
        return false;
    if (ast_parallel_join_index_array_admitted(node, name))
        return true;
    {
        size_t count = node->data.parallel.join_index_array_count;
        char** grown = realloc(node->data.parallel.join_index_arrays,
                               (count + 1) * sizeof(*grown));
        if (grown == NULL)
            return false;
        node->data.parallel.join_index_arrays = grown;
        grown[count] = pergyra_strdup(name);
        if (grown[count] == NULL)
            return false;
        node->data.parallel.join_index_array_count = count + 1;
    }
    return true;
}

bool
ast_parallel_join_index_array_admitted(const ASTNode* node, const char* name)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || name == NULL)
        return false;
    for (size_t i = 0; i < node->data.parallel.join_index_array_count; i++) {
        if (node->data.parallel.join_index_arrays[i] != NULL
            && strcmp(node->data.parallel.join_index_arrays[i], name) == 0)
            return true;
    }
    return false;
}

/* Snapshot-read fact rows (docs/181 R5): same producer/consumer split as
 * the index-disjointness family -- the join admission checker is the
 * single producer, both backend emitters consume. */
void
ast_parallel_reset_join_readonly_arrays(ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return;
    for (size_t i = 0;
         i < node->data.parallel.join_readonly_array_count; i++)
        free(node->data.parallel.join_readonly_arrays[i]);
    free(node->data.parallel.join_readonly_arrays);
    node->data.parallel.join_readonly_arrays = NULL;
    node->data.parallel.join_readonly_array_count = 0;
}

bool
ast_parallel_add_join_readonly_array(ASTNode* node, const char* name)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || name == NULL)
        return false;
    if (ast_parallel_join_readonly_array_admitted(node, name))
        return true;
    {
        size_t count = node->data.parallel.join_readonly_array_count;
        char** grown = realloc(node->data.parallel.join_readonly_arrays,
                               (count + 1) * sizeof(*grown));
        if (grown == NULL)
            return false;
        node->data.parallel.join_readonly_arrays = grown;
        grown[count] = pergyra_strdup(name);
        if (grown[count] == NULL)
            return false;
        node->data.parallel.join_readonly_array_count = count + 1;
    }
    return true;
}

bool
ast_parallel_join_readonly_array_admitted(const ASTNode* node,
                                          const char* name)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || name == NULL)
        return false;
    for (size_t i = 0;
         i < node->data.parallel.join_readonly_array_count; i++) {
        if (node->data.parallel.join_readonly_arrays[i] != NULL
            && strcmp(node->data.parallel.join_readonly_arrays[i],
                      name) == 0)
            return true;
    }
    return false;
}

/* --- Expression form (docs/181 R2): checker-sealed give result type --- */

bool
ast_parallel_set_join_give_type(ASTNode* node, const char* type_name)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK
        || type_name == NULL)
        return false;
    free(node->data.parallel.join_give_type_name);
    node->data.parallel.join_give_type_name = pergyra_strdup(type_name);
    return node->data.parallel.join_give_type_name != NULL;
}

const char*
ast_parallel_join_give_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    return node->data.parallel.join_give_type_name;
}

/* --- Reduce combinator (docs/181 R4): parse-time closed set --- */

bool
ast_parallel_set_join_reduce_op(ASTNode* node, const char* op)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || op == NULL)
        return false;
    free(node->data.parallel.join_reduce_op);
    node->data.parallel.join_reduce_op = pergyra_strdup(op);
    return node->data.parallel.join_reduce_op != NULL;
}

const char*
ast_parallel_join_reduce_op(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    return node->data.parallel.join_reduce_op;
}

/* --- any-join (docs/181 R3): parse-time combinator flag --- */

bool
ast_parallel_set_join_any(ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return false;
    node->data.parallel.join_is_any = true;
    return true;
}

bool
ast_parallel_join_is_any(const ASTNode* node)
{
    return node != NULL && node->type == AST_PARALLEL_BLOCK
        && node->data.parallel.join_is_any;
}
