#ifndef PERGYRA_ARRAY_STORAGE_ELEMENT_LIFETIME_OWNER_H
#define PERGYRA_ARRAY_STORAGE_ELEMENT_LIFETIME_OWNER_H
#include "type_system.h"
#include <stddef.h>
#include <stdint.h>
typedef struct SemanticContext SemanticContext;
typedef struct ASTNode ASTNode;
typedef struct Symbol Symbol;
typedef struct IndexedStringDeferredRead IndexedStringDeferredRead;
bool semantic_array_storage_deferred_read_current(
    const IndexedStringDeferredRead *row, SemanticContext *ctx);
bool semantic_array_storage_deferred_read_assignment(IndexedStringDeferredRead *row,
    ASTNode *assignment, ASTNode *value, uint32_t source_id, SemanticContext *ctx);
bool semantic_array_storage_deferred_read_failure(ASTNode *site,
    SemanticContext *ctx, const char *reason);
bool semantic_array_storage_record_deferred_string_read(ASTNode *use,
    uint32_t source_id, Symbol *binding, bool array_receiver,
    bool owner_release, SemanticContext *ctx);
void semantic_array_storage_deferred_string_read_assignment(Symbol *binding,
    uint32_t source_id, bool array_receiver, ASTNode *assignment,
    ASTNode *value, SemanticContext *ctx);
typedef struct PgyCollectionOwnershipFact PgyCollectionOwnershipFact;

/* Recursively plain value elements need no destructor or borrowed lifetime. */
bool semantic_array_storage_plain_element(const Type *type, SemanticContext *ctx);
/* A non-NULL result source is provisional: its recursive element certificate
 * remains a required dependency, not an owned-element permission by itself. */
bool semantic_array_storage_return_elements_owned(ASTNode *value,
    SemanticContext *ctx, const ASTNode **result_source_out);
typedef struct {
    uint32_t function_id, binding_id;
    bool owned;
    const ASTNode *dependency_call;
    uint32_t dependency_call_id;
} ArrayStorageReturnElementReceipt;
/* A consumed RETURN binding is checked by its issued fact, never by
 * treating the consumed Symbol as a fresh element owner. */
bool semantic_array_storage_return_elements_current(ASTNode *value,
    SemanticContext *ctx, const ArrayStorageReturnElementReceipt *receipt,
    const ASTNode **result_source_out);
/* Captured before an own/deep release consumes the binding. Currentness
 * replays that exact use, not a new permission from a consumed Symbol. */
bool semantic_array_storage_capture_element_use(ASTNode *value,
    SemanticContext *ctx, ArrayStorageReturnElementReceipt *receipt);
bool semantic_array_storage_element_use_current(ASTNode *value,
    SemanticContext *ctx, const ArrayStorageReturnElementReceipt *receipt);
/* A nominal constructor consumes the source descriptor after this receipt is
 * issued. The constructor snapshot, not the retired source binding, then owns
 * the element premise. This query still replays the exact source/dependency;
 * it never upgrades a non-owned receipt. */
bool semantic_array_storage_transferred_element_use_current(ASTNode *value,
    SemanticContext *ctx, const ArrayStorageReturnElementReceipt *receipt);
/* A String actual can borrow one Array<String> element without transferring
 * its descriptor; the source identity and callee effect remain separate. */
bool semantic_array_storage_payload_call_capture(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *argument, ASTNode *callee_decl,
    size_t ordinal, uint32_t *source_id);
bool semantic_array_storage_payload_callee_preserves(SemanticContext *ctx,
    ASTNode *argument, ASTNode *callee_decl, size_t ordinal,
    const Type *array_type, uint32_t source_id);
void semantic_array_storage_initialize_return_elements(PgyCollectionOwnershipFact *fact,
    const Symbol *binding, const ASTNode *initializer, SemanticContext *ctx);
void semantic_array_storage_revoke_return_elements(ASTNode *value, SemanticContext *ctx);
bool semantic_array_storage_record_return_element_effect(ASTNode *receiver,
    uint32_t kind, bool current, SemanticContext *ctx);
#endif
