#ifndef PGY_TYPED_MAP_QUERY_FACT_OWNER_H
#define PGY_TYPED_MAP_QUERY_FACT_OWNER_H
#include "type_checker.h"
typedef enum { PGY_TYPED_MAP_QUERY_INT = 1, PGY_TYPED_MAP_QUERY_KEYS = 2 } PgyTypedMapQueryKind;
bool semantic_typed_map_query_record(SemanticContext *ctx, ASTNode *call,
    PgyTypedMapQueryKind kind, Type *receiver, Type *result);
bool semantic_typed_map_query_receiver_read(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal);
bool semantic_typed_map_query_result_independent(const SemanticContext *ctx, const ASTNode *call);
bool semantic_typed_map_query_facts_ready(const SemanticContext *ctx);
void semantic_typed_map_query_destroy(SemanticContext *ctx);
#endif
