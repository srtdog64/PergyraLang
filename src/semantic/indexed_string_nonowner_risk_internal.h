/* One local-risk premise in the existing collection/indexed-String owner.
 * It does not identify a String source or confer element cleanup authority. */
#ifndef PGY_INDEXED_STRING_NONOWNER_RISK_INTERNAL_H
#define PGY_INDEXED_STRING_NONOWNER_RISK_INTERNAL_H

#include "function_param_flow_summary_internal.h"

typedef struct {
    const ASTNode *call, *statement;
    uint32_t call_id, statement_id;
    size_t actual_ordinal;
    SemanticNominalResultSnapshot *mapping; /* original constructor owner */
} IndexedStringNonownerTerminal;

typedef struct {
    const ASTNode *value, *sink;
    uint32_t value_id, sink_id, binding_id;
    size_t actual_ordinal;
    ASTNodeType shape;
} IndexedStringNonownerUse;

/* One declared read-only Array<String> actual, sealed with the typed call
 * claim that proved it neither mutates, releases, retains nor forwards the
 * receiver's elements. */
typedef struct {
    const ASTNode *call, *callee;
    uint32_t call_id, callee_id, formal_id;
    size_t actual_ordinal, claim_ordinal, formal_ordinal;
    bool physical;
} IndexedStringNonownerRead;

typedef struct IndexedStringNonownerRisk {
    const ASTNode *caller, *definition, *initializer;
    uint32_t caller_id, body_id, binding_id, initializer_id;
    const Type *array_type;
    char *binding_name;
    FunctionParamFlowIndexClaims *claims; /* original index payload; moved once */
    IndexedStringNonownerTerminal *terminals;
    size_t terminal_count, terminal_capacity, sealed_terminals;
    IndexedStringNonownerUse *uses;
    size_t use_count, use_capacity, sealed_uses;
    IndexedStringNonownerRead *reads;
    size_t read_count, read_capacity, sealed_reads;
    bool prepared;
} IndexedStringNonownerRisk;

bool semantic_indexed_string_nonowner_receipts(SemanticContext *ctx, bool prepare);
bool semantic_indexed_string_nonowner_prepare(SemanticContext *ctx,
    IndexedStringNonownerRisk *risk);
bool semantic_indexed_string_nonowner_current(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk);
bool semantic_indexed_string_nonowner_syntax_current(
    const IndexedStringNonownerRisk *risk);
void semantic_indexed_string_nonowner_destroy(IndexedStringNonownerRisk *risk);

#endif
