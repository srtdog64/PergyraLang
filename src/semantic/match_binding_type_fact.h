#ifndef PERGYRA_MATCH_BINDING_TYPE_FACT_H
#define PERGYRA_MATCH_BINDING_TYPE_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;
typedef struct Type Type;

/* Semantic-owned positional type for one routine-local match binding.
 * The match-case syntax id plus binding index is the stable row identity.
 * MIR and self-host consumers must not recover it from variant spelling. */
typedef struct PgyMatchBindingTypeFact
{
    uint32_t function_syntax_id;
    uint32_t match_case_syntax_id;
    size_t   binding_index;
    size_t   binding_count;
    char    *binding_type_name;
} PgyMatchBindingTypeFact;

bool semantic_match_binding_type_fact_record(
    SemanticContext *ctx,
    const ASTNode *match_case_node,
    size_t binding_index,
    size_t binding_count,
    const Type *binding_type);

void pgy_match_binding_type_facts_destroy(PgyMatchBindingTypeFact *facts,
                                          size_t count);

#endif
