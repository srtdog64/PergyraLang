#include "parallel_capture_facts.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static SemanticParallelCaptureBoundaryFact *
parallel_capture_boundary_find(SemanticContext *ctx, uint32_t stable_id)
{
    if (ctx == NULL || stable_id == 0)
        return NULL;
    for (size_t i = 0; i < ctx->parallel_capture_boundary_count; i++) {
        SemanticParallelCaptureBoundaryFact *fact =
            &ctx->parallel_capture_boundaries[i];
        if (fact->source_stable_id == stable_id)
            return fact;
    }
    return NULL;
}

static void
parallel_capture_boundary_rows_clear(
    SemanticParallelCaptureBoundaryFact *fact)
{
    if (fact == NULL)
        return;
    for (size_t i = 0; i < fact->row_count; i++)
        free(fact->rows[i].name);
    free(fact->rows);
    fact->rows = NULL;
    fact->row_count = 0;
    fact->row_capacity = 0;
    fact->sealed = false;
}

bool
semantic_parallel_capture_facts_reset(SemanticContext *ctx,
                                      const ASTNode *boundary)
{
    SemanticParallelCaptureBoundaryFact *fact;
    uint32_t stable_id;

    if (ctx == NULL || boundary == NULL
        || boundary->type != AST_PARALLEL_BLOCK)
        return false;
    stable_id = ast_node_stable_id(boundary);
    if (stable_id == 0)
        return false;
    fact = parallel_capture_boundary_find(ctx, stable_id);
    if (fact == NULL) {
        if (ctx->parallel_capture_boundary_count
            == ctx->parallel_capture_boundary_capacity) {
            size_t next = ctx->parallel_capture_boundary_capacity == 0
                ? 8 : ctx->parallel_capture_boundary_capacity * 2;
            if (next < ctx->parallel_capture_boundary_capacity
                || next > SIZE_MAX / sizeof(*ctx->parallel_capture_boundaries))
                return false;
            SemanticParallelCaptureBoundaryFact *grown = realloc(
                ctx->parallel_capture_boundaries,
                next * sizeof(*ctx->parallel_capture_boundaries));
            if (grown == NULL)
                return false;
            ctx->parallel_capture_boundaries = grown;
            ctx->parallel_capture_boundary_capacity = next;
        }
        fact = &ctx->parallel_capture_boundaries[
            ctx->parallel_capture_boundary_count++];
        memset(fact, 0, sizeof(*fact));
        fact->source_stable_id = stable_id;
    } else {
        parallel_capture_boundary_rows_clear(fact);
    }
    fact->task_count = ast_parallel_task_count(boundary);
    return true;
}

bool
semantic_parallel_capture_facts_add_snapshot(SemanticContext *ctx,
                                             const ASTNode *boundary,
                                             const char *name,
                                             size_t writer_task)
{
    SemanticParallelCaptureBoundaryFact *fact;

    if (ctx == NULL || boundary == NULL || name == NULL)
        return false;
    fact = parallel_capture_boundary_find(ctx, ast_node_stable_id(boundary));
    if (fact == NULL || fact->sealed || writer_task >= fact->task_count)
        return false;
    for (size_t i = 0; i < fact->row_count; i++) {
        if (strcmp(fact->rows[i].name, name) == 0)
            return fact->rows[i].writer_task == writer_task;
    }
    if (fact->row_count == fact->row_capacity) {
        size_t next = fact->row_capacity == 0 ? 4 : fact->row_capacity * 2;
        if (next < fact->row_capacity
            || next > SIZE_MAX / sizeof(*fact->rows))
            return false;
        SemanticParallelCaptureDispositionRow *grown = realloc(
            fact->rows, next * sizeof(*fact->rows));
        if (grown == NULL)
            return false;
        fact->rows = grown;
        fact->row_capacity = next;
    }
    SemanticParallelCaptureDispositionRow *row = &fact->rows[fact->row_count];
    row->name = pergyra_strdup(name);
    if (row->name == NULL)
        return false;
    row->kind = SEMANTIC_PARALLEL_CAPTURE_SNAPSHOT_COPY;
    row->writer_task = writer_task;
    fact->row_count++;
    return true;
}

bool
semantic_parallel_capture_facts_seal(SemanticContext *ctx,
                                     const ASTNode *boundary)
{
    if (ctx == NULL || boundary == NULL)
        return false;
    SemanticParallelCaptureBoundaryFact *fact =
        parallel_capture_boundary_find(ctx, ast_node_stable_id(boundary));
    if (fact == NULL || fact->sealed
        || fact->task_count != ast_parallel_task_count(boundary))
        return false;
    fact->sealed = true;
    return true;
}

void
semantic_parallel_capture_facts_clear(
    SemanticParallelCaptureBoundaryFact *facts,
    size_t count)
{
    if (facts == NULL)
        return;
    for (size_t i = 0; i < count; i++)
        parallel_capture_boundary_rows_clear(&facts[i]);
    free(facts);
}
