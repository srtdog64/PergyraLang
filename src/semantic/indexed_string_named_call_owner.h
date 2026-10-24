#ifndef PGY_INDEXED_STRING_NAMED_CALL_OWNER_H
#define PGY_INDEXED_STRING_NAMED_CALL_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;
typedef struct IndexedStringNamedCallStore IndexedStringNamedCallStore;

/* A String value whose Array owner no other owner proves alive is passed to
 * a declared callee. Typing records the argument here instead of refusing
 * it; once the parameter-flow summaries are final, the call is admitted only
 * if the callee reads that String without keeping it and cannot change or
 * release any Array the caller passes it. Every other receipt is refused at
 * its argument with the original diagnostic. */
bool semantic_indexed_string_named_call_begin(SemanticContext *ctx);
bool semantic_indexed_string_named_call_record(SemanticContext *ctx,
    ASTNode *expression, ASTNode *call, ASTNode *callee, size_t ordinal);
bool semantic_indexed_string_named_call_finalize(SemanticContext *ctx);
/* The one rule both receipts and Array payload consumers use: the declared
 * callee only reads the String at `ordinal`, and no other argument can
 * change or release an Array during the call. */
bool semantic_indexed_string_named_call_reads_only(SemanticContext *ctx,
    ASTNode *call, ASTNode *callee, size_t ordinal);
/* The declared callee only reads or returns the String at `ordinal`, which
 * belongs to the Array bound to `owner`; no other argument can change an
 * Array; and the loan proof admits the call's result as a loan of `owner`
 * in `routine`, or finds it staying once copies already selected in this
 * analysis are made (the analysis is then refused by their owner, and the
 * next one proves the loan with them). */
bool semantic_indexed_string_named_call_lends(SemanticContext *ctx, ASTNode *routine,
    ASTNode *call, ASTNode *callee, size_t ordinal, uint32_t owner);
void semantic_indexed_string_named_call_destroy(IndexedStringNamedCallStore *store);

#endif
