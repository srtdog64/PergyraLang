#ifndef PERGYRA_INDEXED_STRING_BORROW_OWNER_H
#define PERGYRA_INDEXED_STRING_BORROW_OWNER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
/* Conditional local-risk exclusion; no String source or cleanup grant. */
bool semantic_indexed_string_nonowner_record(SemanticContext *ctx,
    const Symbol *candidate, ASTNode *value, ASTNode *sink, size_t actual_ordinal);
typedef enum {
    INDEXED_STRING_PUSH_NOT_APPLICABLE,
    INDEXED_STRING_PUSH_RECORDED,
    INDEXED_STRING_PUSH_FAILED
} IndexedStringPushRecord;
IndexedStringPushRecord semantic_indexed_string_record_private_push(
    SemanticContext *ctx, ASTNode *call, ASTNode *source,
    ASTNode *receiver, uint32_t risk_binding_id);
bool semantic_indexed_string_private_push_current(SemanticContext *ctx,
    const ASTNode *call);
bool semantic_indexed_string_prepare_private_push(SemanticContext *ctx, const ASTNode *call);
void semantic_indexed_string_private_push_destroy(void *receipt);
/* Query at an actual successfully typed String publication/binding seam.
 * Independence does not prove exact payload copying or allocation success. */
bool semantic_indexed_string_independent_result(const ASTNode *expression,
                                               const SemanticContext *ctx);
bool semantic_indexed_string_borrow_publish_source(ASTNode *expression,
    SemanticContext *ctx);
bool semantic_indexed_string_borrow_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id);
bool semantic_indexed_string_field_record(SemanticContext *ctx,
    ASTNode *value, ASTNode *sink, size_t ordinal);
bool semantic_indexed_string_field_source_current(SemanticContext *ctx,
    ASTNode *value, uint32_t *source_id);
bool semantic_indexed_string_borrow_result_dependency_current(SemanticContext *ctx,
    ASTNode *value, const ASTNode **dependency);
bool semantic_indexed_string_borrow_finalize(SemanticContext *ctx);
bool semantic_indexed_string_borrow_prepare(SemanticContext *ctx);
bool semantic_indexed_string_borrow_storage_source_current(ASTNode *expression,
    SemanticContext *ctx, uint32_t *source_id);
/* A NULL callee covers shallow stores and builtin Array mutators, including
 * a non-NULL call site; named handoffs also consume the source relation. */
void semantic_indexed_string_borrow_call_argument(ASTNode *expression,
    ASTNode *call, ASTNode *callee, size_t ordinal, bool constructor,
    SemanticContext *ctx);
/* The String an ArrayPush stores. Where no owner proves that String alive
 * for the receiver, *copy_landing is set instead of a refusal: the push must
 * store an owned copy. Every other refusal is reported as for any argument. */
void semantic_indexed_string_borrow_push_argument(ASTNode *expression,
    ASTNode *call, SemanticContext *ctx, bool *copy_landing);
/* The String an ArraySet stores, checked as any argument. Where no owner
 * proves it alive, the refusal stands and *copy_wanted reports that an owned
 * copy could stand in for it. */
void semantic_indexed_string_borrow_set_argument(ASTNode *expression,
    ASTNode *call, SemanticContext *ctx, bool *copy_wanted);

bool semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *declaration, ASTNode *initializer, SemanticContext *ctx);
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
bool semantic_indexed_string_borrow_array_binding_source(const Symbol *binding,
    uint32_t *source_id);
bool semantic_indexed_string_borrow_array_receiver_source(ASTNode *receiver,
    SemanticContext *ctx, bool *tracked, uint32_t *source_id);

#endif
