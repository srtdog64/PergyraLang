/*
 * Copyright (c) 2026 Pergyra Language Project
 * Parallel-node accessor owner: tasks, checker-sealed capture
 * dispositions (docs/178), and the join form (docs/181 SS1). Split out
 * of ast_async_lambda_accessors.c under the 550-line responsibility
 * rule.
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

void
ast_parallel_reset_dispositions(ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return;
    for (size_t i = 0; i < node->data.parallel.snapshot_row_count; i++)
        free(node->data.parallel.snapshot_rows[i].name);
    free(node->data.parallel.snapshot_rows);
    node->data.parallel.snapshot_rows = NULL;
    node->data.parallel.snapshot_row_count = 0;
    node->data.parallel.snapshot_row_capacity = 0;
    node->data.parallel.dispositions_sealed = false;
}

bool
ast_parallel_add_snapshot_row(ASTNode* node, const char* name,
                              size_t writer_task)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || name == NULL)
        return false;
    /* The checker's scope walk may revisit a shadowed name; the verdict is
     * identical, so the first row wins. */
    if (ast_parallel_snapshot_row_find(node, name) != NULL)
        return true;
    if (node->data.parallel.snapshot_row_count
            == node->data.parallel.snapshot_row_capacity) {
        size_t new_cap = node->data.parallel.snapshot_row_capacity == 0
            ? 4 : node->data.parallel.snapshot_row_capacity * 2;
        ASTParallelSnapshotRow* grown = realloc(
            node->data.parallel.snapshot_rows,
            new_cap * sizeof(*grown));
        if (grown == NULL)
            return false;
        node->data.parallel.snapshot_rows = grown;
        node->data.parallel.snapshot_row_capacity = new_cap;
    }
    {
        ASTParallelSnapshotRow* row = &node->data.parallel.snapshot_rows[
            node->data.parallel.snapshot_row_count];
        row->name = pergyra_strdup(name);
        if (row->name == NULL)
            return false;
        row->writer_task = writer_task;
        node->data.parallel.snapshot_row_count++;
    }
    return true;
}

void
ast_parallel_seal_dispositions(ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return;
    node->data.parallel.dispositions_sealed = true;
}

bool
ast_parallel_dispositions_sealed(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return false;
    return node->data.parallel.dispositions_sealed;
}

const ASTParallelSnapshotRow*
ast_parallel_snapshot_row_find(const ASTNode* node, const char* name)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK || name == NULL)
        return NULL;
    for (size_t i = 0; i < node->data.parallel.snapshot_row_count; i++) {
        const ASTParallelSnapshotRow* row =
            &node->data.parallel.snapshot_rows[i];
        if (row->name != NULL && strcmp(row->name, name) == 0)
            return row;
    }
    return NULL;
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
 * (single producer), consumed by both backend emitters. Reset lives here
 * and NOT in ast_parallel_reset_dispositions -- that reset belongs to the
 * scalar-race checker, which runs after the join admission and must not
 * wipe this family's rows. */
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
