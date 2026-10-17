#ifndef PGY_INDEXED_STRING_PERMANENT_INTERNAL_H
#define PGY_INDEXED_STRING_PERMANENT_INTERNAL_H
#include "indexed_string_permanent_element.h"
#include "array_storage_deferred_preservation_owner.h"
typedef struct {
    const ASTNode *caller, *declaration, *initializer;
    uint32_t caller_id, binding_id, initializer_id;
    FunctionParamFlowIndexClaims *claims;
} IndexedStringPermanentBirth;
typedef struct {
    const ASTNode *caller, *statement, *value, *literal;
    uint32_t caller_id, statement_id, value_id, literal_id;
    uint32_t binding_id, callee_id;
    ArrayStorageReturnSource source;
} IndexedStringPermanentReturn;
struct IndexedStringPermanentElementRecipe {
    const ASTNode *caller, *some, *value, *receiver, *index, *declaration, *producer;
    uint32_t caller_id, some_id, value_id, receiver_id, index_id, binding_id, source_id, producer_id;
    IndexedStringResultInputs inputs;
    IndexedStringPermanentBirth *births;
    IndexedStringPermanentReturn *returns;
    size_t birth_count, birth_capacity, sealed_births;
    size_t return_count, return_capacity, sealed_returns;
    bool prepared;
};
bool indexed_string_permanent_history(SemanticContext *ctx,
    IndexedStringPermanentElementRecipe *recipe, const IndexedStringPermanentBirth *birth, bool prepare);
bool indexed_string_permanent_birth(IndexedStringPermanentElementRecipe *recipe,
    SemanticContext *ctx, const ASTNode *caller, const ASTNode *declaration,
    const FunctionParamFlowIndexClaims *claims, bool prepare);
#endif
