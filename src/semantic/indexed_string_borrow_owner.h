#ifndef PERGYRA_INDEXED_STRING_BORROW_OWNER_H
#define PERGYRA_INDEXED_STRING_BORROW_OWNER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
/* Query at an actual successfully typed String publication/binding seam.
 * Independence does not prove exact payload copying or allocation success. */
bool semantic_indexed_string_independent_result(const ASTNode *expression,
                                               const SemanticContext *ctx);
bool semantic_indexed_string_borrow_publish_source(ASTNode *expression,
    SemanticContext *ctx);
bool semantic_indexed_string_borrow_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id);
bool semantic_indexed_string_borrow_storage_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id);
/* A NULL call and callee denote an actual shallow store, not an unknown
 * function call. Only independent String payloads may cross that boundary. */
void semantic_indexed_string_borrow_call_argument(ASTNode *expression,
    ASTNode *call, ASTNode *callee, size_t ordinal, bool constructor,
    SemanticContext *ctx);

void semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *initializer, SemanticContext *ctx);
void semantic_indexed_string_borrow_record_assignment(
    Symbol *binding, ASTNode *value, ASTNode *assignment,
    SemanticContext *ctx);
void semantic_indexed_string_borrow_invalidate_deep_drop(
    ASTNode *receiver, SemanticContext *ctx);
bool semantic_indexed_string_borrow_validate_use(
    Symbol *binding, ASTNode *use, SemanticContext *ctx);
bool semantic_indexed_string_borrow_owner_transfer_allowed(
    ASTNode *receiver, SemanticContext *ctx);
bool semantic_indexed_string_borrow_deferred_release_current(SemanticContext *ctx,
    uint32_t binding_id, uint32_t source_id, bool *reserved);

#endif
