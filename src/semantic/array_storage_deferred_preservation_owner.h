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

typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;

typedef enum {
    ARRAY_STORAGE_RELEASE_DROP_STORAGE,
    ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS,
    ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
} ArrayStorageReleaseOperation;

bool semantic_array_storage_deferral_begin(SemanticContext *ctx);
bool semantic_array_storage_deferral_active(const SemanticContext *ctx);

/* Records one authority-bearing handoff of `binding` to `callee_decl`. */
bool semantic_array_storage_record_pending_call(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *callee_decl, size_t ordinal);

/* Records a release or move consuming storage authority after prior handoffs;
 * a missing identity or record fails closed at the caller. */
bool semantic_array_storage_record_pending_drop(SemanticContext *ctx,
    ASTNode *receiver, const Symbol *binding, ArrayStorageReleaseOperation operation);

/* Ordering point for consumers that snapshot exclusivity during Pass 2. */
size_t semantic_array_storage_pending_sequence(const SemanticContext *ctx);

/* Valid after finalize: whether every handoff of `binding_id` recorded before
 * `sequence` preserved the descriptor. */
bool semantic_array_storage_pending_preserved(SemanticContext *ctx,
    uint32_t binding_id, size_t sequence);

/* Decides every recorded handoff and refuses each release that relied on an
 * unproved one. Call after Pass 2 and before consumers of the decisions. */
bool semantic_array_storage_deferral_finalize(SemanticContext *ctx);

void semantic_array_storage_deferral_destroy(SemanticContext *ctx);

#endif
