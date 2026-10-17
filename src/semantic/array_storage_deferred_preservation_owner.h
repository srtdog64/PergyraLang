#ifndef PERGYRA_ARRAY_STORAGE_DEFERRED_PRESERVATION_OWNER_H
#define PERGYRA_ARRAY_STORAGE_DEFERRED_PRESERVATION_OWNER_H

/* Order-independent descriptor preservation for public Array release.
 *
 * A preservation summary reads the semantic call identities that a callee's
 * body check records. A callee declared after its caller has none yet, so a
 * summary taken during Pass 2 would refuse by declaration order alone. During
 * Pass 2 a handoff that preserves release authority is recorded, not decided;
 * exclusivity stays provisional. After every body is checked, each recorded
 * handoff is decided once over a fresh summary store, and a release or own
 * handoff that relied on an unproved one is refused with its original
 * diagnostic. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "symbol_table.h"
#include "array_storage_element_lifetime_owner.h"

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;
typedef struct FunctionParamFlowIndexClaims FunctionParamFlowIndexClaims;

typedef struct ArrayStorageResultBirthReceipt {
    ASTNode *caller, *call;
    uint32_t caller_id, call_id, binding_id, callee_id;
    const Type *type;
    size_t sequence;
} ArrayStorageResultBirthReceipt;

typedef struct ArrayStoragePendingCallReceipt {
    uint32_t binding_id;
    ASTNode *call;
    uint32_t call_id;
    ASTNode *argument;
    uint32_t argument_id;
    ASTNode *callee_decl;
    uint32_t callee_id;
    const Type *binding_type;
    size_t ordinal;
    size_t formal_ordinal;
    uint32_t formal_id;
    size_t sequence;
    bool payload;
    uint32_t source_id;
} ArrayStoragePendingCallReceipt;

typedef enum {
    ARRAY_RESULT_UNPROVED,
    ARRAY_RESULT_LITERAL,
    ARRAY_RESULT_INDEPENDENT_CALL,
    ARRAY_RESULT_OWNED_BINDING,
    ARRAY_RESULT_DECLARED_CALL
} ArrayStorageReturnSource;

/* An issued RETURN event is immutable evidence; the private deferred store
 * alone owns its ordering and all-path completion. */
typedef struct ArrayStorageOwnedReturnReceipt {
    ASTNode *decl, *statement, *value;
    uint32_t decl_id, statement_id, value_id, binding_id, callee_id;
    ArrayStorageReturnElementReceipt elements;
    const Type *type;
    size_t sequence;
    ArrayStorageReturnSource source;
} ArrayStorageOwnedReturnReceipt;

/* The original RETURN row is borrowed only for this synchronous callback.
 * The observer must copy stable identities before it returns. */
typedef bool (*ArrayStorageLiteralResultObserver)(SemanticContext *ctx,
    ASTNode *decl, const FunctionParamFlowIndexClaims *claims,
    const ArrayStorageOwnedReturnReceipt *event,
    const ASTNode *literal_declaration_or_null, const ASTNode *literal,
    void *user);

typedef enum {
    ARRAY_STORAGE_RELEASE_DROP_STORAGE,
    ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS,
    ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS,
    ARRAY_STORAGE_MUTATE_STORAGE,
    ARRAY_STORAGE_MUTATE_OWNED_STRINGS,
    ARRAY_STORAGE_CALL_AFTER_PENDING
} ArrayStorageReleaseOperation;

bool semantic_array_storage_deferral_begin(SemanticContext *ctx);
bool semantic_array_storage_deferral_active(const SemanticContext *ctx);

/* Records one authority-bearing handoff of `binding` to `callee_decl`. */
bool semantic_array_storage_record_pending_call(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *callee_decl, size_t ordinal);
bool semantic_array_storage_record_pending_payload(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *callee_decl, size_t ordinal);
bool semantic_array_storage_record_pending_call_consumer(SemanticContext *ctx,
    const Symbol *binding, ASTNode *call, size_t actual_ordinal);
bool semantic_array_storage_has_pending_elements(SemanticContext *ctx,
    uint32_t binding_id);

/* A returned Array descriptor is separate from its elements' backing. A
 * call-result birth stays provisional until every reachable return in the
 * selected routine has a current storage certificate. */
bool semantic_array_storage_record_pending_result(SemanticContext *ctx,
    Symbol *binding, ASTNode *call);
bool semantic_array_storage_record_owned_return(SemanticContext *ctx,
    ASTNode *ret, ASTNode *value, const Type *type);
bool semantic_array_storage_owned_result_current(SemanticContext *ctx,
    const ASTNode *call);
/* String-element lifetime is independent of the outer Array descriptor. */
bool semantic_array_storage_owned_string_result_current(SemanticContext *ctx,
    const ASTNode *call);
bool semantic_array_storage_function_result_owned(SemanticContext *ctx,
    ASTNode *decl, const Type *result);
bool semantic_array_storage_literal_result_prepare(SemanticContext *ctx,
    const ASTNode *result_call, ArrayStorageLiteralResultObserver observer,
    void *user);
bool semantic_array_storage_literal_result_current(SemanticContext *ctx,
    const ASTNode *result_call, ArrayStorageLiteralResultObserver observer,
    void *user);

/* Records a storage-dependent use after prior handoffs;
 * a missing identity or record fails closed at the caller. */
bool semantic_array_storage_record_pending_drop(SemanticContext *ctx,
    ASTNode *receiver, const Symbol *binding, ArrayStorageReleaseOperation operation);

/* Ordering point for consumers that snapshot exclusivity during Pass 2. */
size_t semantic_array_storage_pending_sequence(const SemanticContext *ctx);

/* Valid after finalize: whether every handoff of `binding_id` recorded before
 * `sequence` preserved the descriptor. */
bool semantic_array_storage_pending_preserved(SemanticContext *ctx,
    uint32_t binding_id, size_t sequence);
bool semantic_array_storage_pending_owned_elements(SemanticContext *ctx,
    uint32_t binding_id, size_t sequence);

/* Decides every recorded handoff and refuses each release that relied on an
 * unproved one. Call after Pass 2 and before consumers of the decisions. */
bool semantic_array_storage_deferral_finalize(SemanticContext *ctx);

void semantic_array_storage_deferral_destroy(SemanticContext *ctx);

#endif
