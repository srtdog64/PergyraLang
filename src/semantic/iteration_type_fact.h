#ifndef PERGYRA_ITERATION_TYPE_FACT_H
#define PERGYRA_ITERATION_TYPE_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;
typedef struct Type Type;

/* Semantic-owned, function-anchored type fact for one for-loop header.
 * Strings are owned by the containing semantic context/result and copied at
 * each IR boundary.  The loop syntax id is the only row identity; no source
 * re-scan or Symbol pointer is a valid substitute. */
typedef struct PgyIterationTypeFact
{
    uint32_t function_syntax_id;
    uint32_t iteration_syntax_id;
    char    *binding_type_name;
    char    *iterable_type_name;
    bool     collection_hoisted;
} PgyIterationTypeFact;

bool semantic_iteration_type_fact_record(
    SemanticContext *ctx,
    const ASTNode *iteration_node,
    const Type *binding_type,
    const Type *iterable_type,
    bool collection_hoisted);

void pgy_iteration_type_facts_destroy(PgyIterationTypeFact *facts,
                                      size_t count);

#endif
