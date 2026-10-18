#include "parallel_capture_facts.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static bool
parallel_capture_boundary_storage_current(const SemanticContext *ctx)
{
    return ctx != NULL && ctx->parallel_capture_boundary_count <=
        ctx->parallel_capture_boundary_capacity &&
        (ctx->parallel_capture_boundaries == NULL) ==
            (ctx->parallel_capture_boundary_capacity == 0);
}

static bool
parallel_capture_row_storage_current(const SemanticParallelCaptureBoundaryFact *fact)
{
    return fact != NULL && fact->row_count <= fact->row_capacity &&
        (fact->rows == NULL) == (fact->row_capacity == 0);
}

static SemanticParallelCaptureBoundaryFact *
parallel_capture_boundary_find(SemanticContext *ctx, uint32_t stable_id)
{
    if (!parallel_capture_boundary_storage_current(ctx) || stable_id == 0)
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
    if (fact->rows != NULL)
        for (size_t i = 0; i < fact->row_capacity; i++)
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

    if (!parallel_capture_boundary_storage_current(ctx) || boundary == NULL
        || boundary->type != AST_PARALLEL_BLOCK)
        return false;
    stable_id = ast_node_stable_id(boundary);
    if (stable_id == 0)
        return false;
    fact = parallel_capture_boundary_find(ctx, stable_id);
    if (fact == NULL) {
        if (ctx->parallel_capture_boundary_count
            == ctx->parallel_capture_boundary_capacity) {
            size_t previous = ctx->parallel_capture_boundary_capacity;
            size_t next = previous == 0 ? 8 : previous * 2;
            if (next < previous
                || next > SIZE_MAX / sizeof(*ctx->parallel_capture_boundaries))
                return false;
            SemanticParallelCaptureBoundaryFact *grown = realloc(
                ctx->parallel_capture_boundaries,
                next * sizeof(*ctx->parallel_capture_boundaries));
            if (grown == NULL)
                return false;
            memset(grown + previous, 0,
                (next - previous) * sizeof(*grown));
            ctx->parallel_capture_boundaries = grown;
            ctx->parallel_capture_boundary_capacity = next;
        }
        fact = &ctx->parallel_capture_boundaries[
            ctx->parallel_capture_boundary_count++];
        memset(fact, 0, sizeof(*fact));
        fact->source_stable_id = stable_id;
    } else {
        if (!parallel_capture_row_storage_current(fact))
            return false;
        parallel_capture_boundary_rows_clear(fact);
    }
    fact->task_count = ast_parallel_task_count(boundary);
    return true;
}

const SemanticParallelCaptureDispositionRow *
semantic_parallel_capture_disposition_find(
    const SemanticContext *ctx,
    const ASTNode *boundary,
    const char *name,
    SemanticParallelCaptureDispositionKind kind)
{
    uint32_t stable_id;

    if (!parallel_capture_boundary_storage_current(ctx) ||
        boundary == NULL || name == NULL)
        return NULL;
    stable_id = ast_node_stable_id(boundary);
    for (size_t i = 0; i < ctx->parallel_capture_boundary_count; i++) {
        const SemanticParallelCaptureBoundaryFact *fact =
            &ctx->parallel_capture_boundaries[i];
        if (fact->source_stable_id != stable_id)
            continue;
        if (!parallel_capture_row_storage_current(fact))
            return NULL;
        for (size_t j = 0; j < fact->row_count; j++) {
            const SemanticParallelCaptureDispositionRow *row =
                &fact->rows[j];
            if (row->name == NULL)
                return NULL;
            if (row->kind == kind && row->name != NULL
                && strcmp(row->name, name) == 0)
                return row;
        }
    }
    return NULL;
}

bool
semantic_parallel_capture_disposition_add(
    SemanticContext *ctx,
    const ASTNode *boundary,
    const char *name,
    SemanticParallelCaptureDispositionKind kind,
    size_t writer_task)
{
    SemanticParallelCaptureBoundaryFact *fact;

    if (ctx == NULL || boundary == NULL || name == NULL)
        return false;
    fact = parallel_capture_boundary_find(ctx, ast_node_stable_id(boundary));
    if (!parallel_capture_row_storage_current(fact))
        return false;
    if (kind == SEMANTIC_PARALLEL_CAPTURE_SNAPSHOT_COPY) {
        if (writer_task >= fact->task_count)
            return false;
    } else if ((kind != SEMANTIC_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT
                && kind != SEMANTIC_PARALLEL_CAPTURE_JOIN_READONLY)
               || writer_task != 0) {
        return false;
    }
    for (size_t i = 0; i < fact->row_count; i++) {
        if (fact->rows[i].name == NULL)
            return false;
        if (strcmp(fact->rows[i].name, name) == 0) {
            return fact->rows[i].kind == kind
                && fact->rows[i].writer_task == writer_task;
        }
    }
    if (fact->sealed)
        return false;
    if (fact->row_count == fact->row_capacity) {
        size_t previous = fact->row_capacity;
        size_t next = previous == 0 ? 4 : previous * 2;
        if (next < previous
            || next > SIZE_MAX / sizeof(*fact->rows))
            return false;
        SemanticParallelCaptureDispositionRow *grown = realloc(
            fact->rows, next * sizeof(*fact->rows));
        if (grown == NULL)
            return false;
        memset(grown + previous, 0,
            (next - previous) * sizeof(*grown));
        fact->rows = grown;
        fact->row_capacity = next;
    }
    SemanticParallelCaptureDispositionRow *row = &fact->rows[fact->row_count];
    row->name = pergyra_strdup(name);
    if (row->name == NULL)
        return false;
    row->kind = kind;
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
    if (!parallel_capture_row_storage_current(fact) || fact->sealed
        || fact->task_count != ast_parallel_task_count(boundary))
        return false;
    for (size_t i = 0; i < fact->row_count; i++)
        if (fact->rows[i].name == NULL)
            return false;
    fact->sealed = true;
    return true;
}

void
semantic_parallel_capture_facts_clear(
    SemanticParallelCaptureBoundaryFact *facts,
    size_t capacity)
{
    if (facts == NULL)
        return;
    for (size_t i = 0; i < capacity; i++)
        parallel_capture_boundary_rows_clear(&facts[i]);
    free(facts);
}
