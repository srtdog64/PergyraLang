#ifndef PERGYRA_SEMANTIC_PARALLEL_CAPTURE_FACTS_H
#define PERGYRA_SEMANTIC_PARALLEL_CAPTURE_FACTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;

typedef enum
{
    SEMANTIC_PARALLEL_CAPTURE_SNAPSHOT_COPY
} SemanticParallelCaptureDispositionKind;

typedef struct
{
    char *name;
    SemanticParallelCaptureDispositionKind kind;
    size_t writer_task;
} SemanticParallelCaptureDispositionRow;

typedef struct
{
    uint32_t source_stable_id;
    size_t task_count;
    bool sealed;
    SemanticParallelCaptureDispositionRow *rows;
    size_t row_count;
    size_t row_capacity;
} SemanticParallelCaptureBoundaryFact;

bool semantic_parallel_capture_facts_reset(SemanticContext *ctx,
                                           const ASTNode *boundary);
bool semantic_parallel_capture_facts_add_snapshot(SemanticContext *ctx,
                                                  const ASTNode *boundary,
                                                  const char *name,
                                                  size_t writer_task);
bool semantic_parallel_capture_facts_seal(SemanticContext *ctx,
                                          const ASTNode *boundary);
void semantic_parallel_capture_facts_clear(
    SemanticParallelCaptureBoundaryFact *facts,
    size_t count);

#endif
