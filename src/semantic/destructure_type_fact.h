#ifndef PERGYRA_DESTRUCTURE_TYPE_FACT_H
#define PERGYRA_DESTRUCTURE_TYPE_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;
typedef struct Type Type;

/* Semantic-owned positional type for one routine-local destructure binding.
 * The destructure syntax id plus binding index is the stable row identity.
 * Later stages must not recover this type from the initializer AST. */
typedef struct PgyDestructureTypeFact
{
    uint32_t function_syntax_id;
    uint32_t destructure_syntax_id;
    size_t   binding_index;
    size_t   binding_count;
    char    *binding_type_name;
} PgyDestructureTypeFact;

bool semantic_destructure_type_fact_record(
    SemanticContext *ctx,
    const ASTNode *destructure_node,
    size_t binding_index,
    size_t binding_count,
    const Type *binding_type);

void pgy_destructure_type_facts_destroy(PgyDestructureTypeFact *facts,
                                        size_t count);

#endif
