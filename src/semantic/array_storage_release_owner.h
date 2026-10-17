#ifndef PERGYRA_ARRAY_STORAGE_RELEASE_OWNER_H
#define PERGYRA_ARRAY_STORAGE_RELEASE_OWNER_H

#include "symbol_table.h"

typedef struct SemanticContext SemanticContext;
typedef struct ArrayStorageResultBirthReceipt ArrayStorageResultBirthReceipt;
typedef struct ArrayStoragePendingCallReceipt ArrayStoragePendingCallReceipt;
typedef struct ArrayStorageOwnedReturnReceipt ArrayStorageOwnedReturnReceipt;

bool semantic_array_storage_independent_call_result(const ASTNode *call,
                                                     SemanticContext *ctx);
bool semantic_array_storage_independent_physical_hof_result(const ASTNode *call,
    SemanticContext *ctx);
bool semantic_array_storage_has_pending(const Symbol *binding);
bool semantic_array_storage_result_birth_current(SemanticContext *ctx,
    const ArrayStorageResultBirthReceipt *birth);
bool semantic_array_storage_pending_call_current(SemanticContext *ctx,
    const ArrayStoragePendingCallReceipt *call);
unsigned semantic_array_storage_pending_call_failures(SemanticContext *ctx,
    const ArrayStoragePendingCallReceipt *call);
bool semantic_array_storage_pending_call_elements_required(
    const ArrayStoragePendingCallReceipt *call);
bool semantic_array_storage_pending_call_consumer_current(SemanticContext *ctx,
    const ArrayStoragePendingCallReceipt *call, const ASTNode *consumer,
    uint32_t consumer_id, uint32_t binding_id, size_t sequence);
bool semantic_array_storage_return_source_current(SemanticContext *ctx,
    const ArrayStorageOwnedReturnReceipt *receipt,
    ASTNode **callee_out, const Type **callee_result_out);
bool semantic_array_storage_admit_mutation(ASTNode *receiver,
                                          SemanticContext *ctx);
void semantic_array_storage_initialize(Symbol *binding, const ASTNode *initializer,
                                       SemanticContext *ctx);
void semantic_array_storage_escape(ASTNode *source, const Type *type,
                                   SemanticContext *ctx);
void semantic_array_storage_call_argument(ASTNode *source, const Type *type,
    const Type *result_type, ParamMode mode, bool constructor,
    ASTNode *call, ASTNode *callee_decl, size_t ordinal, SemanticContext *ctx);
void semantic_array_storage_assignment(ASTNode *target, const Type *target_type,
    ASTNode *value, const Type *value_type, SemanticContext *ctx);
bool semantic_array_storage_admit_drop(ASTNode *receiver, Type *array_type,
                                      SemanticContext *ctx);

#endif
