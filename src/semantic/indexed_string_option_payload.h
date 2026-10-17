/* Subordinate immutable Option inputs of the existing String-result owner. */
#ifndef PGY_INDEXED_STRING_OPTION_PAYLOAD_H
#define PGY_INDEXED_STRING_OPTION_PAYLOAD_H
#include "indexed_string_result_inputs.h"

typedef struct {
    const ASTNode *declaration, *initializer, *caller;
    uint32_t binding, initializer_id, caller_id;
    FunctionParamFlowIndexClaims *claims;
    bool carrier;
} IndexedStringOptionBirth;
typedef struct {
    const ASTNode *caller, *statement, *value, *payload;
    uint32_t caller_id, statement_id, value_id, payload_id;
    bool none;
} IndexedStringOptionReturn;
typedef struct {
    const ASTNode *call, *argument;
    uint32_t call_id, argument_id;
    uint32_t argument_binding;
    ASTNodeType argument_shape;
    char *input_name, *output_name;
    bool unwrap;
} IndexedStringOptionRelation;
typedef struct {
    const ASTNode *caller, *sink, *value;
    uint32_t caller_id, sink_id, value_id, binding;
    size_t ordinal;
} IndexedStringOptionUse;
struct IndexedStringOptionInputs {
    IndexedStringOptionBirth *births;
    IndexedStringOptionReturn *returns;
    IndexedStringOptionRelation *relations;
    IndexedStringOptionUse *uses;
    size_t birth_count, return_count, relation_count, use_count;
    size_t birth_capacity, return_capacity, relation_capacity, use_capacity;
    size_t sealed_births, sealed_returns, sealed_relations, sealed_uses;
    const ASTNode *unwrap;
    uint32_t unwrap_id;
    bool sealed;
};

/* These operations share b53's actual derivation and collector, not a second
 * result-source checker. The Option mode still checks formal->actual returns. */
bool indexed_string_result_derive(SemanticContext *ctx, const ASTNode *call,
    uint32_t *source, bool prepare, bool option_result, IndexedStringResultInputs *inputs);
bool indexed_string_result_capture_dependency(SemanticContext *ctx, ASTNode *decl,
    FunctionParamFlowIndexClaims *claims, IndexedStringResultInputs *inputs, bool prepare);
bool indexed_string_result_capture_call(SemanticContext *ctx, const ASTNode *call,
    IndexedStringResultInputs *inputs, bool prepare);
bool indexed_string_option_source(SemanticContext *ctx, const ASTNode *call,
    uint32_t *source, IndexedStringResultInputs *inputs, bool prepare);
bool indexed_string_option_relation_record(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const ASTNode *call, bool unwrap, bool prepare);
bool indexed_string_option_syntax(const IndexedStringResultInputs *inputs);
void indexed_string_option_destroy(IndexedStringOptionInputs *option);
bool indexed_string_option_birth_uses(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const IndexedStringOptionBirth *birth, bool prepare);
bool indexed_string_option_use_record(IndexedStringOptionInputs *option,
    const IndexedStringOptionUse *row, bool prepare);
#endif
