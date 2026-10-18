#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/mir_parallel_capture_facts.h"
#include "semantic/semantic.h"

static size_t calloc_call_count;
static size_t fail_calloc_call;

void *
pgy_test_calloc(size_t count, size_t size)
{
    calloc_call_count++;
    if (fail_calloc_call != 0 && calloc_call_count == fail_calloc_call)
        return NULL;
    return calloc(count, size);
}

char *
mir_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;
    char *text;

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return NULL;
    }
    text = malloc((size_t)needed + 1);
    if (text != NULL)
        (void)vsnprintf(text, (size_t)needed + 1, fmt, args);
    va_end(args);
    return text;
}

size_t
semantic_result_parallel_capture_boundary_count(const SemanticResult *result)
{
    return result != NULL ? result->parallel_capture_boundary_count : 0;
}

const SemanticParallelCaptureBoundaryFact *
semantic_result_parallel_capture_boundary_at(const SemanticResult *result,
                                             size_t index)
{
    if (result == NULL
        || result->parallel_capture_boundary_count
            > result->parallel_capture_boundary_capacity
        || (result->parallel_capture_boundaries == NULL)
            != (result->parallel_capture_boundary_capacity == 0)
        || index >= result->parallel_capture_boundary_count)
        return NULL;
    const SemanticParallelCaptureBoundaryFact *boundary =
        &result->parallel_capture_boundaries[index];
    if (boundary->row_count > boundary->row_capacity
        || (boundary->rows == NULL) != (boundary->row_capacity == 0))
        return NULL;
    return boundary;
}

static char *
owned_text(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy != NULL)
        memcpy(copy, text, length + 1);
    return copy;
}

static bool
mir_is_empty(const MIRProgram *mir)
{
    return mir != NULL
        && mir->parallel_capture_boundaries == NULL
        && mir->parallel_capture_boundary_count == 0
        && mir->parallel_capture_boundary_capacity == 0
        && !mir->parallel_capture_facts_imported;
}

int
main(void)
{
    SemanticParallelCaptureDispositionRow rows[2] = {
        { "left", SEMANTIC_PARALLEL_CAPTURE_JOIN_READONLY, 0 },
        { "right", SEMANTIC_PARALLEL_CAPTURE_JOIN_READONLY, 0 },
    };
    SemanticParallelCaptureBoundaryFact boundary = {
        .source_stable_id = 7,
        .task_count = 1,
        .sealed = true,
        .rows = rows,
        .row_count = 2,
        .row_capacity = 2,
    };
    SemanticResult semantic = {
        .parallel_capture_boundaries = &boundary,
        .parallel_capture_boundary_count = 1,
        .parallel_capture_boundary_capacity = 1,
    };
    MIRProgram imported = {0};
    char *error = NULL;

    SemanticParallelCaptureBoundaryFact *saved_boundaries =
        semantic.parallel_capture_boundaries;
    semantic.parallel_capture_boundaries = NULL;
    if (mir_import_parallel_capture_facts(&imported, &semantic, &error)
        || !mir_is_empty(&imported))
        return 1;
    free(error);
    error = NULL;
    semantic.parallel_capture_boundaries = saved_boundaries;

    SemanticParallelCaptureDispositionRow *saved_rows = boundary.rows;
    boundary.rows = NULL;
    if (mir_import_parallel_capture_facts(&imported, &semantic, &error)
        || !mir_is_empty(&imported))
        return 2;
    free(error);
    error = NULL;
    boundary.rows = saved_rows;

    calloc_call_count = 0;
    fail_calloc_call = 2;
    if (mir_import_parallel_capture_facts(&imported, &semantic, &error)
        || calloc_call_count != 2 || !mir_is_empty(&imported))
        return 3;
    free(error);
    error = NULL;

    calloc_call_count = 0;
    fail_calloc_call = 0;
    if (!mir_import_parallel_capture_facts(&imported, &semantic, &error)
        || imported.parallel_capture_boundary_count != 1
        || imported.parallel_capture_boundary_capacity != 1
        || imported.parallel_capture_boundaries[0].row_count != 2
        || imported.parallel_capture_boundaries[0].row_capacity != 2)
        return 4;
    mir_parallel_capture_facts_clear(&imported);
    free(error);
    error = NULL;

    SemanticResult empty_semantic = {0};
    if (!mir_import_parallel_capture_facts(
            &imported, &empty_semantic, &error)
        || !imported.parallel_capture_facts_imported
        || mir_import_parallel_capture_facts(
            &imported, &empty_semantic, &error))
        return 5;
    free(error);
    error = NULL;
    mir_parallel_capture_facts_clear(&imported);
    if (!mir_import_parallel_capture_facts(
            &imported, &empty_semantic, &error))
        return 6;
    mir_parallel_capture_facts_clear(&imported);
    free(error);
    error = NULL;

    MIRProgram partial = {0};
    partial.parallel_capture_boundaries = calloc(
        3, sizeof(*partial.parallel_capture_boundaries));
    if (partial.parallel_capture_boundaries == NULL)
        return 7;
    partial.parallel_capture_boundary_count = 1;
    partial.parallel_capture_boundary_capacity = 3;
    partial.parallel_capture_boundaries[0].rows = calloc(
        3, sizeof(*partial.parallel_capture_boundaries[0].rows));
    partial.parallel_capture_boundaries[2].rows = calloc(
        2, sizeof(*partial.parallel_capture_boundaries[2].rows));
    if (partial.parallel_capture_boundaries[0].rows == NULL
        || partial.parallel_capture_boundaries[2].rows == NULL)
        return 8;
    partial.parallel_capture_boundaries[0].row_count = 1;
    partial.parallel_capture_boundaries[0].row_capacity = 3;
    partial.parallel_capture_boundaries[2].row_count = 0;
    partial.parallel_capture_boundaries[2].row_capacity = 2;
    partial.parallel_capture_boundaries[0].rows[0].name = owned_text("active");
    partial.parallel_capture_boundaries[0].rows[2].name = owned_text("row-tail");
    partial.parallel_capture_boundaries[2].rows[1].name = owned_text("boundary-tail");
    if (partial.parallel_capture_boundaries[0].rows[0].name == NULL
        || partial.parallel_capture_boundaries[0].rows[2].name == NULL
        || partial.parallel_capture_boundaries[2].rows[1].name == NULL)
        return 9;
    mir_parallel_capture_facts_clear(&partial);
    if (!mir_is_empty(&partial))
        return 10;

    puts("MIR parallel capture allocation lifecycle: PASS");
    return 0;
}
